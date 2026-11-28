#include "hot/flux_hot_internal.h"


#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef struct HotStatusPost {
	FluxHotLoader *hot;
	FluxHotEvent   event;
	uint32_t       exit_code;
} HotStatusPost;

static FluxHotLoader *volatile g_hot;

static char const *event_text(FluxHotEvent event) {
	static char const *const text [] = {
	  [FLUX_HOT_LOADED]        = "loaded",
	  [FLUX_HOT_RELOADED]      = "reloaded",
	  [FLUX_HOT_BUILDING]      = "building",
	  [FLUX_HOT_BUILD_FAILED]  = "build failed",
	  [FLUX_HOT_LOAD_FAILED]   = "module failed to load",
	  [FLUX_HOT_FRAME_FAILED]  = "new view failed, kept the previous one",
	  [FLUX_HOT_ABI_CHANGED]   = "module ABI changed, restart required",
	  [FLUX_HOT_MODEL_CHANGED] = "Model layout changed, restart required",
	  [FLUX_HOT_LIMIT]         = "reload limit reached, restart required",
	  [FLUX_HOT_UNCHANGED]     = "build finished, module unchanged",
	};
	char const *t = ( unsigned ) event < sizeof text / sizeof *text ? text [event] : NULL;
	return t ? t : "unknown";
}

static HANDLE console_out(void) {
	static HANDLE out;
	static bool   tried;
	if (tried) return out;
	tried    = true;
	HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
	if ((!h || h == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
		h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	out = h == INVALID_HANDLE_VALUE ? NULL : h;
	return out;
}

static void console_bytes(HANDLE out, char const *bytes, int len) {
	DWORD mode = 0, n = 0;
	if (len <= 0) return;
	if (!GetConsoleMode(out, &mode)) {
		WriteFile(out, bytes, ( DWORD ) len, &n, NULL);
		return;
	}
	wchar_t wide [4096];
	int     w = MultiByteToWideChar(CP_UTF8, 0, bytes, len, wide, 4096);
	if (w > 0) WriteConsoleW(out, wide, ( DWORD ) w, &n, NULL);
}

static char *read_build_log(FluxHotLoader *hot, DWORD *len) {
	wchar_t path [HOT_PATH];
	swprintf(path, HOT_PATH, L"%ls\\build.log", hot->copy_dir);
	HANDLE log = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (log == INVALID_HANDLE_VALUE) return NULL;
	DWORD size = GetFileSize(log, NULL);
	if (size == INVALID_FILE_SIZE || size > (1u << 20)) size = 1u << 20;
	char *buf = ( char * ) malloc(( size_t ) size + 1u);
	DWORD got = 0;
	if (buf && !ReadFile(log, buf, size, &got, NULL)) got = 0;
	CloseHandle(log);
	if (buf) buf [got] = '\0';
	*len = got;
	return buf;
}

static bool runtime_name(char *out, int cap) {
	HMODULE self  = NULL;
	DWORD   flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
	wchar_t path [HOT_PATH];
	if (!GetModuleHandleExW(flags, ( LPCWSTR ) ( uintptr_t ) &runtime_name, &self)) return false;
	DWORD          n    = GetModuleFileNameW(self, path, HOT_PATH);
	wchar_t const *base = n && n < HOT_PATH ? wcsrchr(path, L'\\') : NULL;
	return base && WideCharToMultiByte(CP_UTF8, 0, base + 1, -1, out, cap, NULL, NULL) > 0;
}

static bool mentions_file(char const *log, char const *name) {
	size_t len = strlen(name);
	for (char const *at = strstr(log, name); at; at = strstr(at + 1, name))
		if (at [len] != '.' && at [len] != '_' && !isalnum(( unsigned char ) at [len])) return true;
	return false;
}

static void dump_build_log(FluxHotLoader *hot, HANDLE out) {
	DWORD len = 0;
	char *log = read_build_log(hot, &len);
	if (!log) return;
	for (DWORD off = 0; off < len; off += 4096u)
		console_bytes(out, log + off, ( int ) (len - off < 4096u ? len - off : 4096u));
	char name [MAX_PATH];
	char hint [MAX_PATH + 96];
	if (runtime_name(name, MAX_PATH) && mentions_file(log, name)) {
		int n = snprintf(
		  hint, sizeof hint, "[hot] the build relinks %s, which this process has loaded; restart for runtime changes\n",
		  name
		);
		console_bytes(out, hint, n < ( int ) sizeof hint ? n : ( int ) sizeof hint - 1);
	}
	free(log);
}

static int status_line(FluxHotLoader const *hot, FluxHotEvent event, uint32_t exit_code, char *line, size_t cap) {
	if (event == FLUX_HOT_RELOADED)
		return snprintf(
		  line, cap, "[hot] reloaded, generation %u, %u module(s) loaded\n", hot->generation, hot->lib_count
		);
	if (event == FLUX_HOT_BUILD_FAILED)
		return snprintf(line, cap, "[hot] build failed (exit %d):\n", ( int ) exit_code);
	if (event == FLUX_HOT_LOADED)
		return snprintf(line, cap, "[hot] loaded, %s\n", hot->cfg.manual ? "manual reload" : "watching for changes");
	return snprintf(line, cap, "[hot] %s\n", event_text(event));
}

static void report_console(FluxHotLoader *hot, HANDLE out, FluxHotEvent event, uint32_t exit_code) {
	char line [192];
	int  n = status_line(hot, event, exit_code, line, sizeof line);
	console_bytes(out, line, n < ( int ) sizeof line ? n : ( int ) sizeof line - 1);
	if (event == FLUX_HOT_BUILD_FAILED) dump_build_log(hot, out);
}

static void show_status(FluxHotLoader *hot, FluxHotEvent event, uint32_t exit_code) {
	HANDLE out = console_out();
	if (out) report_console(hot, out, event, exit_code);
	FluxHotStatus status = {
	  .event      = event,
	  .generation = hot->generation,
	  .loaded     = hot->lib_count,
	  .exit_code  = exit_code,
	};
	if (hot->cfg.on_status) hot->cfg.on_status(hot->cfg.user, &status);
}

static void status_ui(void *ctx) {
	HotStatusPost *post = ( HotStatusPost * ) ctx;
	if (g_hot == post->hot) show_status(post->hot, post->event, post->exit_code);
	free(post);
}

void hot_post_status(FluxHotLoader *hot, FluxHotEvent event, uint32_t exit_code) {
	HotStatusPost *post = ( HotStatusPost * ) malloc(sizeof(*post));
	if (!post) return;
	*post = (HotStatusPost) {.hot = hot, .event = event, .exit_code = exit_code};
	if (!flux_app_invoke(hot->app, status_ui, post)) free(post);
}

typedef struct PeView {
	unsigned char *buf;
	DWORD          size;
	DWORD          nt;
} PeView;

static bool  pe_range(PeView const *pe, DWORD off, DWORD len) { return off <= pe->size && len <= pe->size - off; }

static DWORD pe_rva_offset(PeView const *pe, DWORD rva) {
	IMAGE_FILE_HEADER fh;
	memcpy(&fh, pe->buf + pe->nt + 4u, sizeof fh);
	DWORD sec = pe->nt + 4u + ( DWORD ) sizeof fh + fh.SizeOfOptionalHeader;
	for (WORD i = 0; i < fh.NumberOfSections; i++, sec += ( DWORD ) sizeof(IMAGE_SECTION_HEADER)) {
		IMAGE_SECTION_HEADER sh;
		if (!pe_range(pe, sec, sizeof sh)) return 0;
		memcpy(&sh, pe->buf + sec, sizeof sh);
		if (rva >= sh.VirtualAddress && rva - sh.VirtualAddress < sh.Misc.VirtualSize)
			return sh.PointerToRawData + (rva - sh.VirtualAddress);
	}
	return 0;
}

static void pe_mask_debug_entry(PeView const *pe, DWORD at) {
	IMAGE_DEBUG_DIRECTORY d;
	if (!pe_range(pe, at, sizeof d)) return;
	memcpy(&d, pe->buf + at, sizeof d);
	memset(pe->buf + at + offsetof(IMAGE_DEBUG_DIRECTORY, TimeDateStamp), 0, 4);
	if (d.Type == IMAGE_DEBUG_TYPE_CODEVIEW && pe_range(pe, d.PointerToRawData, 24u))
		memset(pe->buf + d.PointerToRawData + 4u, 0, 20);
}

static void pe_mask_debug(PeView const *pe) {
	DWORD opt   = pe->nt + 24u;
	WORD  magic = 0;
	if (!pe_range(pe, opt, sizeof magic)) return;
	memcpy(&magic, pe->buf + opt, sizeof magic);
	DWORD dd = opt + (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC ? 112u : 96u) + IMAGE_DIRECTORY_ENTRY_DEBUG * 8u;
	IMAGE_DATA_DIRECTORY dir = {0};
	if (!pe_range(pe, dd, sizeof dir)) return;
	memcpy(&dir, pe->buf + dd, sizeof dir);
	DWORD off = dir.VirtualAddress ? pe_rva_offset(pe, dir.VirtualAddress) : 0;
	for (DWORD i  = 0; off && i + sizeof(IMAGE_DEBUG_DIRECTORY) <= dir.Size;
	  i          += ( DWORD ) sizeof(IMAGE_DEBUG_DIRECTORY))
		pe_mask_debug_entry(pe, off + i);
}

static void pe_mask_link_stamp(unsigned char *buf, DWORD size) {
	PeView pe = {.buf = buf, .size = size};
	if (size < 0x40u || buf [0] != 'M' || buf [1] != 'Z') return;
	memcpy(&pe.nt, buf + 0x3c, sizeof pe.nt);
	if (size < 92u || pe.nt > size - 92u || memcmp(buf + pe.nt, "PE\0\0", 4) != 0) return;
	memset(buf + pe.nt + 8u, 0, 4);
	memset(buf + pe.nt + 24u + 64u, 0, 4);
	pe_mask_debug(&pe);
}

static bool file_hash(wchar_t const *path, uint64_t *out) {
	HANDLE file
	  = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) return false;
	DWORD          size = GetFileSize(file, NULL);
	unsigned char *buf  = size != INVALID_FILE_SIZE ? ( unsigned char * ) malloc(( size_t ) size + 1u) : NULL;
	DWORD          got  = 0;
	bool           ok   = buf && ReadFile(file, buf, size, &got, NULL) && got == size;
	CloseHandle(file);
	if (ok) pe_mask_link_stamp(buf, size);
	uint64_t h = 1469598103934665603ull;
	for (DWORD i = 0; ok && i < size; i++) h = (h ^ buf [i]) * 1099511628211ull;
	free(buf);
	*out = h;
	return ok;
}

static bool lib_reserve(FluxHotLoader *hot) {
	if (hot->lib_count < hot->lib_cap) return true;
	uint32_t cap  = hot->lib_cap ? hot->lib_cap * 2u : 8u;
	HotLib  *libs = ( HotLib * ) realloc(hot->libs, sizeof(*libs) * cap);
	if (!libs) return false;
	hot->libs    = libs;
	hot->lib_cap = cap;
	return true;
}

static void lib_close(FluxHotLoader *hot, HotLib *lib) {
	if (lib->loaded && hot->hooks.unload) hot->hooks.unload(hot->hooks.ctx, lib->lib);
	lib->loaded = false;
	if (lib->lib) FreeLibrary(lib->lib);
	lib->lib = NULL;
	DeleteFileW(lib->copy);
}

static bool lib_open(FluxHotLoader *hot, HotLib *out) {
	swprintf(
	  out->copy, HOT_PATH, L"%ls\\%ls.%lu.%u.dll", hot->copy_dir, hot->module_name, GetCurrentProcessId(),
	  ++hot->copy_seq
	);
	if (!CopyFileW(hot->module, out->copy, FALSE)) return false;
	out->lib = LoadLibraryExW(out->copy, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	if (out->lib) return true;
	DeleteFileW(out->copy);
	return false;
}

void flux_hot_print(char const *text) {
	HANDLE out = text ? console_out() : NULL;
	if (out) console_bytes(out, text, ( int ) strlen(text));
}

void *flux_hot_symbol(void *lib, char const *name) {
	return lib && name ? ( void * ) ( uintptr_t ) GetProcAddress(( HMODULE ) lib, name) : NULL;
}

static HotLib *current_lib(FluxHotLoader *hot) { return &hot->libs [hot->lib_count - 1u]; }

void          *flux_hot_loader_current(FluxHotLoader const *hot) { return hot->libs [hot->lib_count - 1u].lib; }

static void    retire_previous(FluxHotLoader *hot) {
	if (hot->lib_count < 2u || !hot->hooks.retire) return;
	HotLib *prev = &hot->libs [hot->lib_count - 2u];
	HotLib *cur  = current_lib(hot);
	if (!hot->hooks.retire(hot->hooks.ctx, prev->lib, cur->lib)) return;
	lib_close(hot, prev);
	*prev = *cur;
	hot->lib_count--;
}

static uint32_t max_loaded(FluxHotLoader const *hot) {
	return hot->cfg.max_loaded ? hot->cfg.max_loaded : HOT_MAX_LOADED;
}

static FluxHotEvent load_next(FluxHotLoader *hot, void *prev) {
	HotLib next = {0};
	if (!lib_reserve(hot) || !lib_open(hot, &next)) return FLUX_HOT_LOAD_FAILED;
	FluxHotEvent event = hot->hooks.swap(hot->hooks.ctx, prev, next.lib);
	if (event != FLUX_HOT_RELOADED) {
		lib_close(hot, &next);
		return event;
	}
	next.loaded                  = true;
	hot->libs [hot->lib_count++] = next;
	return event;
}

static FluxHotEvent reload_module(FluxHotLoader *hot) {
	retire_previous(hot);
	if (hot->lib_count >= max_loaded(hot)) return FLUX_HOT_LIMIT;
	FluxHotEvent event = load_next(hot, current_lib(hot)->lib);
	if (event != FLUX_HOT_RELOADED) return event;
	hot->generation++;
	flux_app_redraw(hot->app);
	return event;
}

static void reload_ui(void *ctx) {
	FluxHotLoader *hot = ( FluxHotLoader * ) ctx;
	if (g_hot != hot) return;
	InterlockedExchange(&hot->reload_queued, 0);
	bool     built = InterlockedExchange(&hot->built, 0) != 0;
	uint64_t hash  = 0;
	if (!file_hash(hot->module, &hash)) return;
	if (hash == hot->hash || hash == hot->rejected) {
		if (built) show_status(hot, FLUX_HOT_UNCHANGED, 0u);
		return;
	}
	FluxHotEvent event = reload_module(hot);
	if (event == FLUX_HOT_RELOADED) hot->hash = hash;
	else hot->rejected = hash;
	show_status(hot, event, 0u);
}

void hot_queue_reload(FluxHotLoader *hot) {
	if (InterlockedExchange(&hot->reload_queued, 1)) return;
	if (!flux_app_invoke(hot->app, reload_ui, hot)) InterlockedExchange(&hot->reload_queued, 0);
}

void hot_report_built(FluxHotLoader *hot) {
	InterlockedExchange(&hot->built, 1);
	hot_queue_reload(hot);
}

bool flux_hot_reload(void) {
	FluxHotLoader *hot = g_hot;
	if (!hot) return false;
	hot_queue_reload(hot);
	return true;
}

static bool is_absolute(wchar_t const *path) {
	return (path [0] && path [1] == L':') || (path [0] == L'\\' && path [1] == L'\\');
}

static bool join_module(wchar_t const *module, wchar_t *out) {
	if (is_absolute(module)) return swprintf(out, HOT_PATH, L"%ls", module) > 0;
	wchar_t  exe [HOT_PATH];
	DWORD    n     = GetModuleFileNameW(NULL, exe, HOT_PATH);
	wchar_t *slash = n && n < HOT_PATH ? wcsrchr(exe, L'\\') : NULL;
	if (!slash) return false;
	*slash = L'\0';
	return swprintf(out, HOT_PATH, L"%ls\\%ls", exe, module) > 0;
}

static bool resolve_paths(FluxHotLoader *hot) {
	wchar_t joined [HOT_PATH];
	if (!join_module(hot->cfg.module, joined) || !GetFullPathNameW(joined, HOT_PATH, hot->module, NULL)) return false;
	wcscpy(hot->module_dir, hot->module);
	wchar_t *slash = wcsrchr(hot->module_dir, L'\\');
	if (!slash) return false;
	*slash           = L'\0';
	hot->module_name = hot->module + (slash - hot->module_dir) + 1;
	swprintf(hot->copy_dir, HOT_PATH, L"%ls\\.hot", hot->module_dir);
	return CreateDirectoryW(hot->copy_dir, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static void clean_copies(FluxHotLoader *hot) {
	wchar_t pattern [HOT_PATH];
	swprintf(pattern, HOT_PATH, L"%ls\\%ls.*.dll", hot->copy_dir, hot->module_name);
	WIN32_FIND_DATAW found;
	HANDLE           find = FindFirstFileW(pattern, &found);
	if (find == INVALID_HANDLE_VALUE) return;
	do {
		wchar_t path [HOT_PATH];
		swprintf(path, HOT_PATH, L"%ls\\%ls", hot->copy_dir, found.cFileName);
		DeleteFileW(path);
	}
	while (FindNextFileW(find, &found));
	FindClose(find);
}

static void loader_free(FluxHotLoader *hot) {
	for (uint32_t i = hot->lib_count; i-- > 0;) lib_close(hot, &hot->libs [i]);
	free(hot->libs);
	free(hot);
}

static bool loader_ready(FluxHotLoader *hot) {
	if (!resolve_paths(hot)) return false;
	clean_copies(hot);
	return file_hash(hot->module, &hot->hash) && load_next(hot, NULL) == FLUX_HOT_RELOADED;
}

FluxHotLoader *flux_hot_loader_open(FluxApp *app, FluxHotCfg const *cfg, FluxHotHooks const *hooks) {
	if (!app || !cfg || !cfg->module || !hooks || !hooks->swap || g_hot) return NULL;
	FluxHotLoader *hot = ( FluxHotLoader * ) calloc(1, sizeof(*hot));
	if (!hot) return NULL;
	hot->app   = app;
	hot->cfg   = *cfg;
	hot->hooks = *hooks;
	if (!loader_ready(hot)) {
		loader_free(hot);
		return NULL;
	}
	g_hot         = hot;
	hot->watching = !hot->cfg.manual && hot_watch_start(hot);
	show_status(hot, FLUX_HOT_LOADED, 0u);
	return hot;
}

void flux_hot_loader_stop(FluxHotLoader *hot) {
	if (!hot) return;
	if (g_hot == hot) g_hot = NULL;
	if (hot->watching) hot_watch_stop(hot);
	hot->watching = false;
}

void flux_hot_loader_close(FluxHotLoader *hot) {
	if (!hot) return;
	flux_hot_loader_stop(hot);
	loader_free(hot);
}
