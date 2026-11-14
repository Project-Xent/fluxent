#include "wizard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

static char *dup_str(char const *s) { return s ? _strdup(s) : NULL; }

static void  replace_str(char **slot, char const *s) {
	free(*slot);
	*slot = dup_str(s);
}

void wizard_init(Model *m) {
	memset(m, 0, sizeof(*m));
	m->dir         = dup_str(kFolders [0]);
	m->browse_sel  = 0;
	m->channel_sel = 0;
	m->channel     = dup_str(kChannels [0]);
	m->update_date = xtk_date(2026, 12, 1);
	m->cache_mb    = 512.0f;
	m->log_follow  = true;
	for (int i = 0; i < COMP_COUNT; i++) {
		m->checked [i]  = kComps [i].parent >= 0 && i != 5;
		m->expanded [i] = kComps [i].parent < 0 && i != 6;
	}
	m->checked [9] = true;
}

static void free_files(Model *m) {
	for (int i = 0; i < m->file_count; i++) free(m->files [i]);
	free(m->files);
	m->files      = NULL;
	m->file_count = 0;
	m->file_done  = 0;
}

void wizard_free(Model *m) {
	free_files(m);
	free(m->dir);
	free(m->channel);
	free(m->log);
}

static bool comp_leaf(int c) { return c + 1 >= COMP_COUNT || kComps [c + 1].parent != c; }

int         wizard_required_mb(Model const *m) {
	int mb = 0;
	for (int i = 0; i < COMP_COUNT; i++)
		if (comp_leaf(i) && m->checked [i]) mb += kComps [i].mb;
	return mb;
}

bool wizard_can_next(Model const *m) {
	switch (m->page) {
	case PAGE_LICENSE    : return m->accepted;
	case PAGE_LOCATION   : return m->dir && m->dir [0];
	case PAGE_COMPONENTS : return wizard_required_mb(m) > 0;
	case PAGE_PROGRESS   : return m->finished;
	default              : return true;
	}
}

int wizard_flat_comp(Model const *m, int flat) {
	int row = 0;
	for (int i = 0; i < COMP_COUNT; i++) {
		int p = kComps [i].parent;
		if (p >= 0 && !m->expanded [p]) continue;
		if (row++ == flat) return i;
	}
	return -1;
}

static void log_append(Model *m, char const *line) {
	size_t n = strlen(line);
	if (m->log_len + n + 2u > m->log_cap) {
		size_t cap = m->log_cap ? m->log_cap * 2u : 4096u;
		while (cap < m->log_len + n + 2u) cap *= 2u;
		char *grown = ( char * ) realloc(m->log, cap);
		if (!grown) return;
		m->log     = grown;
		m->log_cap = cap;
	}
	if (m->log_len) m->log [m->log_len++] = '\n';
	memcpy(m->log + m->log_len, line, n + 1u);
	m->log_len += n;
}

static void build_files(Model *m) {
	free_files(m);
	int total = 0;
	for (int i = 0; i < COMP_COUNT; i++)
		if (comp_leaf(i) && m->checked [i]) total += kComps [i].mb / 4 + 3;
	m->files = ( char ** ) calloc(( size_t ) (total ? total : 1), sizeof(char *));
	if (!m->files) return;
	for (int i = 0; i < COMP_COUNT; i++) {
		if (!comp_leaf(i) || !m->checked [i]) continue;
		WizardComp const *c = &kComps [i];
		for (int k = 0; k < c->mb / 4 + 3; k++) {
			char path [MAX_PATH * 2];
			snprintf(
			  path, sizeof(path), "%s\\%s\\%s\\x64\\release\\%s_component_%03d.%s", m->dir, c->dir,
			  m->channel ? m->channel : "Stable", c->stem, k + 1, c->ext
			);
			m->files [m->file_count++] = dup_str(path);
		}
	}
}

static DWORD WINAPI worker_main(void *arg) {
	WizardWorker *w = ( WizardWorker * ) arg;
	for (int i = 0; i < w->count; i++) {
		while (InterlockedCompareExchange(&w->paused, 0, 0) && !InterlockedCompareExchange(&w->cancel, 0, 0)) Sleep(20);
		if (InterlockedCompareExchange(&w->cancel, 0, 0)) {
			flux_app_post(w->app, (XtkMsg) {.tag = MSG_DONE, .b = false});
			return 0;
		}
		Sleep(40);
		flux_app_post(w->app, xtk_msg_i(MSG_TICK, i));
	}
	flux_app_post(w->app, (XtkMsg) {.tag = MSG_DONE, .b = true});
	return 0;
}

static void worker_join(WizardWorker *w) {
	if (!w->thread) return;
	InterlockedExchange(&w->cancel, 1);
	WaitForSingleObject(( HANDLE ) w->thread, INFINITE);
	CloseHandle(( HANDLE ) w->thread);
	w->thread = NULL;
}

static void install_start(Model *m) {
	build_files(m);
	m->log_len = 0;
	if (m->log) m->log [0] = '\0';
	char line [MAX_PATH * 2];
	snprintf(line, sizeof(line), "Installing to %s (%s channel)", m->dir, m->channel ? m->channel : "Stable");
	log_append(m, line);
	m->installing    = true;
	m->finished      = false;
	m->cancelled     = false;
	m->worker.cancel = 0;
	m->worker.paused = 0;
	m->worker.count  = m->file_count;
	m->worker.thread = CreateThread(NULL, 0, worker_main, &m->worker, 0, NULL);
	if (!m->worker.thread) {
		log_append(m, "Could not start the installer thread.");
		m->installing = false;
		m->cancelled  = true;
	}
}

static void close_window(Model *m) {
	if (m->worker.app) PostMessageW(flux_app_hwnd(m->worker.app), WM_CLOSE, 0, 0);
}

static void msg_next(Model *m, XtkMsg msg) {
	( void ) msg;
	if (!wizard_can_next(m)) return;
	if (m->page == PAGE_FINISH) {
		close_window(m);
		return;
	}
	m->page++;
	if (m->page == PAGE_PROGRESS) install_start(m);
}

static void msg_back(Model *m, XtkMsg msg) {
	( void ) msg;
	if (m->page > PAGE_LICENSE && m->page < PAGE_PROGRESS) m->page--;
}

static void msg_cancel(Model *m, XtkMsg msg) {
	( void ) msg;
	if (m->page == PAGE_FINISH || (m->page == PAGE_PROGRESS && !m->installing)) {
		close_window(m);
		return;
	}
	m->exit_open = true;
	if (m->installing) InterlockedExchange(&m->worker.paused, 1);
}

static void msg_exit_result(Model *m, XtkMsg msg) {
	m->exit_open = false;
	if (msg.i == 1) {
		if (m->installing) InterlockedExchange(&m->worker.cancel, 1);
		else close_window(m);
	}
	InterlockedExchange(&m->worker.paused, 0);
}

static void msg_comp_select(Model *m, XtkMsg msg) {
	int c = wizard_flat_comp(m, msg.i);
	if (c < 0) return;
	m->checked [c] = msg.b;
	for (int i = c + 1; i < COMP_COUNT && kComps [i].parent == c; i++) m->checked [i] = msg.b;
}

static void msg_comp_expand(Model *m, XtkMsg msg) {
	int c = wizard_flat_comp(m, msg.i);
	if (c >= 0) m->expanded [c] = msg.b;
}

static void msg_tick(Model *m, XtkMsg msg) {
	if (msg.i < 0 || msg.i >= m->file_count) return;
	m->file_done = msg.i + 1;
	char line [MAX_PATH * 2 + 16];
	snprintf(line, sizeof(line), "Copied %s", m->files [msg.i]);
	log_append(m, line);
}

static void msg_done(Model *m, XtkMsg msg) {
	worker_join(&m->worker);
	m->installing = false;
	m->cancelled  = !msg.b;
	m->finished   = msg.b;
	log_append(m, msg.b ? "Installation completed successfully." : "Installation was cancelled. No changes were kept.");
}

static void msg_accept(Model *m, XtkMsg msg) { m->accepted = msg.b; }

static void msg_dir(Model *m, XtkMsg msg) { replace_str(&m->dir, ( char const * ) msg.ptr); }

static void msg_browse(Model *m, XtkMsg msg) {
	( void ) msg;
	m->browse_open = true;
}

static void msg_browse_pick(Model *m, XtkMsg msg) { m->browse_sel = msg.i; }

static void msg_browse_result(Model *m, XtkMsg msg) {
	m->browse_open = false;
	if (msg.i == 1 && m->browse_sel >= 0 && m->browse_sel < FOLDER_COUNT)
		replace_str(&m->dir, kFolders [m->browse_sel]);
}

static void msg_channel_select(Model *m, XtkMsg msg) {
	m->channel_sel = msg.i;
	if (msg.i >= 0 && msg.i < CHANNEL_COUNT) replace_str(&m->channel, kChannels [msg.i]);
}

static void msg_channel_submit(Model *m, XtkMsg msg) {
	m->channel_sel = -1;
	replace_str(&m->channel, ( char const * ) msg.ptr);
	for (int i = 0; m->channel && i < CHANNEL_COUNT; i++)
		if (strcmp(m->channel, kChannels [i]) == 0) m->channel_sel = i;
}

static void msg_update_date(Model *m, XtkMsg msg) { m->update_date = msg.i; }

static void msg_cache(Model *m, XtkMsg msg) { m->cache_mb = msg.f; }

static void msg_log_scroll(Model *m, XtkMsg msg) {
	m->log_follow = msg.b;
	m->log_y      = msg.f;
}

typedef void             (*WizardMsgFn)(Model *m, XtkMsg msg);

static WizardMsgFn const kMsgFn [] = {
  [MSG_BACK]           = msg_back,
  [MSG_NEXT]           = msg_next,
  [MSG_CANCEL]         = msg_cancel,
  [MSG_EXIT_RESULT]    = msg_exit_result,
  [MSG_ACCEPT]         = msg_accept,
  [MSG_DIR]            = msg_dir,
  [MSG_BROWSE]         = msg_browse,
  [MSG_BROWSE_PICK]    = msg_browse_pick,
  [MSG_BROWSE_RESULT]  = msg_browse_result,
  [MSG_COMP_SELECT]    = msg_comp_select,
  [MSG_COMP_EXPAND]    = msg_comp_expand,
  [MSG_CHANNEL_SELECT] = msg_channel_select,
  [MSG_CHANNEL_SUBMIT] = msg_channel_submit,
  [MSG_UPDATE_DATE]    = msg_update_date,
  [MSG_CACHE]          = msg_cache,
  [MSG_TICK]           = msg_tick,
  [MSG_DONE]           = msg_done,
  [MSG_LOG_SCROLL]     = msg_log_scroll,
};

void wizard_update(void *model, XtkMsg msg) {
	if (msg.tag < 0 || ( size_t ) msg.tag >= sizeof(kMsgFn) / sizeof(kMsgFn [0]) || !kMsgFn [msg.tag]) return;
	kMsgFn [msg.tag](( Model * ) model, msg);
}

void wizard_start(FluxApp *app, void *user) {
	Model *m      = ( Model * ) user;
	m->worker.app = app;
}

void wizard_stop(FluxApp *app, void *user) {
	( void ) app;
	Model *m = ( Model * ) user;
	worker_join(&m->worker);
	m->worker.app = NULL;
}
