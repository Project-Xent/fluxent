#include "fluxent/flux_taskbar.h"
#include "flux_window_internal.h"

#include <shobjidl.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct FluxTaskbar {
	ITaskbarList3      *api;
	FluxTaskbarProgress progress;
	uint64_t            completed;
	uint64_t            total;
	HICON               overlay;
	wchar_t            *description;
	THUMBBUTTON         buttons [FLUX_TASKBAR_MAX_BUTTONS];
	uint32_t            button_count;
	FluxTaskbarButtonFn callback;
	void               *callback_ctx;
	bool                buttons_installed;
};

static TBPFLAG const progress_flags [] = {TBPF_NOPROGRESS, TBPF_INDETERMINATE, TBPF_NORMAL, TBPF_PAUSED, TBPF_ERROR};

static FluxTaskbar  *taskbar_state(FluxWindow *win) {
	if (!win->taskbar) win->taskbar = calloc(1, sizeof(*win->taskbar));
	return win->taskbar;
}

static void taskbar_disconnect(FluxTaskbar *state) {
	if (state->api) ITaskbarList3_Release(state->api);
	state->api               = NULL;
	state->buttons_installed = false;
}

static HRESULT taskbar_connect(FluxWindow *win) {
	if (!win->taskbar_ready) return S_FALSE;
	FluxTaskbar *state = win->taskbar;
	if (state->api) return S_OK;
	HRESULT hr
	  = CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, ( void ** ) &state->api);
	if (FAILED(hr)) return hr;
	hr = ITaskbarList3_HrInit(state->api);
	if (FAILED(hr)) taskbar_disconnect(state);
	return hr;
}

static HRESULT taskbar_progress(FluxWindow *win) {
	FluxTaskbar *state = win->taskbar;
	HRESULT      hr    = ITaskbarList3_SetProgressState(state->api, win->hwnd, progress_flags [state->progress]);
	if (FAILED(hr) || state->progress <= FLUX_TASKBAR_PROGRESS_INDETERMINATE) return hr;
	return ITaskbarList3_SetProgressValue(state->api, win->hwnd, state->completed, state->total);
}

static HRESULT taskbar_overlay(FluxWindow *win) {
	FluxTaskbar *state = win->taskbar;
	return ITaskbarList3_SetOverlayIcon(state->api, win->hwnd, state->overlay, state->description);
}

static HRESULT taskbar_buttons(FluxWindow *win) {
	FluxTaskbar *state = win->taskbar;
	if (state->buttons_installed)
		return ITaskbarList3_ThumbBarUpdateButtons(state->api, win->hwnd, FLUX_TASKBAR_MAX_BUTTONS, state->buttons);
	if (!state->button_count) return S_OK;
	HRESULT hr = ITaskbarList3_ThumbBarAddButtons(state->api, win->hwnd, FLUX_TASKBAR_MAX_BUTTONS, state->buttons);
	state->buttons_installed = SUCCEEDED(hr);
	return hr;
}

HRESULT flux_taskbar_set_progress(FluxWindow *win, FluxTaskbarProgress state, uint64_t completed, uint64_t total) {
	if (!win || ( unsigned ) state >= ARRAYSIZE(progress_flags)) return E_INVALIDARG;
	if (state > FLUX_TASKBAR_PROGRESS_INDETERMINATE && (total == 0 || completed > total)) return E_INVALIDARG;
	FluxTaskbar *taskbar = taskbar_state(win);
	if (!taskbar) return E_OUTOFMEMORY;
	taskbar->progress  = state;
	taskbar->completed = completed;
	taskbar->total     = total;
	HRESULT hr         = taskbar_connect(win);
	return hr == S_OK ? taskbar_progress(win) : hr;
}

static HRESULT taskbar_replace_overlay(FluxTaskbar *state, HICON icon, wchar_t const *description) {
	HICON copy = icon ? CopyIcon(icon) : NULL;
	if (icon && !copy) return E_OUTOFMEMORY;
	wchar_t *text = icon && description ? _wcsdup(description) : NULL;
	if (icon && description && !text) {
		DestroyIcon(copy);
		return E_OUTOFMEMORY;
	}
	if (state->overlay) DestroyIcon(state->overlay);
	free(state->description);
	state->overlay     = copy;
	state->description = text;
	return S_OK;
}

HRESULT flux_taskbar_set_overlay(FluxWindow *win, HICON icon, wchar_t const *description) {
	if (!win) return E_INVALIDARG;
	FluxTaskbar *state = taskbar_state(win);
	if (!state) return E_OUTOFMEMORY;
	HRESULT hr = taskbar_replace_overlay(state, icon, description);
	if (FAILED(hr)) return hr;
	hr = taskbar_connect(win);
	return hr == S_OK ? taskbar_overlay(win) : hr;
}

static void taskbar_free_buttons(THUMBBUTTON *buttons) {
	for (uint32_t i = 0; i < FLUX_TASKBAR_MAX_BUTTONS; i++)
		if (buttons [i].hIcon) DestroyIcon(buttons [i].hIcon);
}

static HRESULT taskbar_copy_button(THUMBBUTTON *out, uint32_t index, FluxTaskbarButton const *source) {
	*out = (THUMBBUTTON) {.dwMask = THB_FLAGS, .iId = index, .dwFlags = THBF_HIDDEN};
	if (!source) return S_OK;
	if (!source->icon) return E_INVALIDARG;
	out->hIcon = CopyIcon(source->icon);
	if (!out->hIcon) return E_OUTOFMEMORY;
	out->dwMask  |= THB_ICON | THB_TOOLTIP;
	out->dwFlags  = source->disabled ? THBF_DISABLED : THBF_ENABLED;
	if (source->tooltip) wcsncpy(out->szTip, source->tooltip, ARRAYSIZE(out->szTip) - 1u);
	return S_OK;
}

static HRESULT taskbar_copy_buttons(THUMBBUTTON *copy, FluxTaskbarButton const *buttons, uint32_t count) {
	for (uint32_t i = 0; i < FLUX_TASKBAR_MAX_BUTTONS; i++) {
		HRESULT hr = taskbar_copy_button(&copy [i], i, i < count ? &buttons [i] : NULL);
		if (FAILED(hr)) {
			taskbar_free_buttons(copy);
			return hr;
		}
	}
	return S_OK;
}

HRESULT flux_taskbar_set_buttons(
  FluxWindow *win, FluxTaskbarButton const *buttons, uint32_t count, FluxTaskbarButtonFn callback, void *ctx
) {
	if (!win || count > FLUX_TASKBAR_MAX_BUTTONS || (count && !buttons)) return E_INVALIDARG;
	FluxTaskbar *state = taskbar_state(win);
	if (!state) return E_OUTOFMEMORY;
	THUMBBUTTON copy [FLUX_TASKBAR_MAX_BUTTONS] = {0};
	HRESULT     hr                              = taskbar_copy_buttons(copy, buttons, count);
	if (FAILED(hr)) return hr;
	taskbar_free_buttons(state->buttons);
	memcpy(state->buttons, copy, sizeof(copy));
	state->button_count = count;
	state->callback     = callback;
	state->callback_ctx = ctx;
	hr                  = taskbar_connect(win);
	return hr == S_OK ? taskbar_buttons(win) : hr;
}

static void taskbar_recreated(FluxWindow *win) {
	win->taskbar_ready = true;
	if (!win->taskbar) return;
	taskbar_disconnect(win->taskbar);
	if (taskbar_connect(win) != S_OK) return;
	( void ) taskbar_progress(win);
	( void ) taskbar_overlay(win);
	( void ) taskbar_buttons(win);
}

bool flux_taskbar_message(FluxWindow *win, UINT msg, WPARAM wp) {
	if (win->taskbar_button_created && msg == win->taskbar_button_created) {
		taskbar_recreated(win);
		return true;
	}
	if (msg != WM_COMMAND || HIWORD(wp) != THBN_CLICKED) return false;
	FluxTaskbar *state = win->taskbar;
	uint32_t     i     = LOWORD(wp);
	if (state && state->callback && i < state->button_count && !(state->buttons [i].dwFlags & THBF_DISABLED))
		state->callback(state->callback_ctx, i);
	return true;
}

void flux_taskbar_destroy(FluxWindow *win) {
	FluxTaskbar *state = win->taskbar;
	win->taskbar       = NULL;
	win->taskbar_ready = false;
	if (!state) return;
	taskbar_disconnect(state);
	taskbar_free_buttons(state->buttons);
	if (state->overlay) DestroyIcon(state->overlay);
	free(state->description);
	free(state);
}
