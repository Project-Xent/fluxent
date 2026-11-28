#include "hot/flux_hot_internal.h"

#include <wchar.h>

enum
{
	EV_STOP,
	EV_CHANGE,
	EV_NOISE,
	EV_IDLE,
	EV_BUILT,
	EV_ERROR,
};

typedef struct Watch {
	FluxHotLoader *hot;
	bool           source;
	HANDLE         dir;
	OVERLAPPED     ov;
	HANDLE         job;
	HANDLE         proc;
	DWORD          buf [4096];
} Watch;

static wchar_t const *const k_skip [] = {
  L"build",
  L".hot",
  L".git",
  L".xmake",
  L".zig-cache",
  L"zig-out",
  L"target",
  L"__pycache__",
  L"dist-newstyle",
};

static bool component_ignored(wchar_t const *s, size_t n) {
	for (size_t i = 0; i < sizeof k_skip / sizeof *k_skip; i++)
		if (wcslen(k_skip [i]) == n && _wcsnicmp(s, k_skip [i], n) == 0) return true;
	return false;
}

static bool path_ignored(wchar_t const *name, size_t len) {
	size_t start = 0;
	for (size_t i = 0; i <= len; i++) {
		if (i < len && name [i] != L'\\') continue;
		if (component_ignored(name + start, i - start)) return true;
		start = i + 1;
	}
	return false;
}

static bool name_relevant(Watch const *w, FILE_NOTIFY_INFORMATION const *info) {
	size_t len = info->FileNameLength / sizeof(wchar_t);
	if (w->source) return !path_ignored(info->FileName, len);
	return len == wcslen(w->hot->module_name) && _wcsnicmp(info->FileName, w->hot->module_name, len) == 0;
}

static bool batch_relevant(Watch const *w, DWORD bytes) {
	if (!bytes) return true;
	unsigned char const *p = ( unsigned char const * ) w->buf;
	for (;;) {
		FILE_NOTIFY_INFORMATION const *info = ( FILE_NOTIFY_INFORMATION const * ) ( void const * ) p;
		if (name_relevant(w, info)) return true;
		if (!info->NextEntryOffset) return false;
		p += info->NextEntryOffset;
	}
}

static bool watch_arm(Watch *w) {
	ResetEvent(w->ov.hEvent);
	DWORD filter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE;
	return ReadDirectoryChangesW(w->dir, w->buf, sizeof w->buf, w->source, filter, NULL, &w->ov, NULL);
}

static int watch_collect(Watch *w) {
	DWORD bytes = 0;
	if (!GetOverlappedResult(w->dir, &w->ov, &bytes, FALSE)) return EV_ERROR;
	bool hit = batch_relevant(w, bytes);
	if (!watch_arm(w)) return EV_ERROR;
	return hit ? EV_CHANGE : EV_NOISE;
}

static int watch_wait(Watch *w, DWORD timeout) {
	HANDLE handles [3] = {w->hot->stop, w->ov.hEvent, w->proc};
	DWORD  r           = WaitForMultipleObjects(w->proc ? 3u : 2u, handles, FALSE, timeout);
	if (r == WAIT_OBJECT_0) return EV_STOP;
	if (r == WAIT_TIMEOUT) return EV_IDLE;
	if (r == WAIT_OBJECT_0 + 1) return watch_collect(w);
	return r == WAIT_OBJECT_0 + 2 ? EV_BUILT : EV_ERROR;
}

static bool file_ready(wchar_t const *path) {
	HANDLE file = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	CloseHandle(file);
	return true;
}

static HANDLE build_log(Watch const *w) {
	wchar_t path [HOT_PATH];
	swprintf(path, HOT_PATH, L"%ls\\build.log", w->hot->copy_dir);
	SECURITY_ATTRIBUTES sa = {.nLength = sizeof sa, .bInheritHandle = TRUE};
	return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void build_start(Watch *w) {
	wchar_t cmd [HOT_PATH * 2];
	swprintf(cmd, sizeof cmd / sizeof *cmd, L"cmd.exe /d /s /c \"%ls\"", w->hot->cfg.build);
	HANDLE              log   = build_log(w);
	STARTUPINFOW        si    = {.cb = sizeof si, .dwFlags = STARTF_USESTDHANDLES, .hStdOutput = log, .hStdError = log};
	PROCESS_INFORMATION pi    = {0};
	DWORD               flags = CREATE_SUSPENDED | CREATE_NO_WINDOW;
	BOOL                ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, flags, NULL, w->hot->cfg.build_dir, &si, &pi);
	if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
	if (!ok) {
		hot_post_status(w->hot, FLUX_HOT_BUILD_FAILED, GetLastError());
		return;
	}
	AssignProcessToJobObject(w->job, pi.hProcess);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	w->proc = pi.hProcess;
	hot_post_status(w->hot, FLUX_HOT_BUILDING, 0u);
}

static void build_cancel(Watch *w) {
	if (!w->proc) return;
	TerminateJobObject(w->job, 1u);
	WaitForSingleObject(w->proc, 5000u);
	CloseHandle(w->proc);
	w->proc = NULL;
}

static void build_finish(Watch *w) {
	DWORD code = 1u;
	GetExitCodeProcess(w->proc, &code);
	CloseHandle(w->proc);
	w->proc = NULL;
	if (code) hot_post_status(w->hot, FLUX_HOT_BUILD_FAILED, code);
	else hot_report_built(w->hot);
}

static bool settle(Watch *w) {
	if (w->source) {
		build_start(w);
		return true;
	}
	if (!file_ready(w->hot->module)) return false;
	hot_queue_reload(w->hot);
	return true;
}

static DWORD debounce(Watch const *w) {
	if (w->hot->cfg.debounce_ms) return w->hot->cfg.debounce_ms;
	return w->source ? HOT_DEBOUNCE_SOURCE : HOT_DEBOUNCE_MODULE;
}

static HANDLE kill_on_close_job(void) {
	HANDLE job = CreateJobObjectW(NULL, NULL);
	if (!job) return NULL;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit = {0};
	limit.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit, sizeof limit);
	return job;
}

static bool watch_open(Watch *w) {
	wchar_t const *dir   = w->source ? w->hot->cfg.watch : w->hot->module_dir;
	DWORD          share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	w->dir               = CreateFileW(
	  dir, FILE_LIST_DIRECTORY, share, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL
	);
	if (w->dir == INVALID_HANDLE_VALUE) return false;
	w->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (w->source) w->job = kill_on_close_job();
	return w->ov.hEvent && (!w->source || w->job) && watch_arm(w);
}

static void watch_close(Watch *w) {
	build_cancel(w);
	if (w->dir != INVALID_HANDLE_VALUE) {
		DWORD bytes = 0;
		if (CancelIoEx(w->dir, &w->ov)) GetOverlappedResult(w->dir, &w->ov, &bytes, TRUE);
		CloseHandle(w->dir);
	}
	if (w->ov.hEvent) CloseHandle(w->ov.hEvent);
	if (w->job) CloseHandle(w->job);
}

static void watch_loop(Watch *w) {
	bool dirty = false;
	for (;;) {
		int event = watch_wait(w, dirty ? debounce(w) : INFINITE);
		if (event == EV_STOP || event == EV_ERROR) return;
		if (event == EV_CHANGE) {
			dirty = true;
			build_cancel(w);
		}
		if (event == EV_BUILT) build_finish(w);
		if (event == EV_IDLE) dirty = !settle(w);
	}
}

static DWORD WINAPI watch_main(void *arg) {
	Watch *w = ( Watch * ) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Watch));
	if (!w) return 1u;
	w->hot    = ( FluxHotLoader * ) arg;
	w->source = w->hot->cfg.build && w->hot->cfg.watch && w->hot->cfg.build_dir;
	if (watch_open(w)) watch_loop(w);
	watch_close(w);
	HeapFree(GetProcessHeap(), 0, w);
	return 0u;
}

bool hot_watch_start(FluxHotLoader *hot) {
	hot->stop = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!hot->stop) return false;
	hot->thread = CreateThread(NULL, 0, watch_main, hot, 0, NULL);
	if (hot->thread) return true;
	CloseHandle(hot->stop);
	hot->stop = NULL;
	return false;
}

void hot_watch_stop(FluxHotLoader *hot) {
	SetEvent(hot->stop);
	WaitForSingleObject(hot->thread, INFINITE);
	CloseHandle(hot->thread);
	CloseHandle(hot->stop);
	hot->thread = NULL;
	hot->stop   = NULL;
}
