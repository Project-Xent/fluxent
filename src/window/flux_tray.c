#include "fluxent/flux_tray.h"
#include "flux_window_internal.h"

#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>
#include <windowsx.h>

#define FLUX_WM_TRAY (WM_APP + 0x52)

struct FluxTrayIcon {
	FluxWindow        *window;
	FluxTrayIcon      *next;
	NOTIFYICONDATAW    data;
	HMENU              menu;
	FluxTrayActivateFn activate;
	FluxTrayCommandFn  command;
	void              *ctx;
	bool               installed;
};

static bool tray_install(NOTIFYICONDATAW *data) {
	if (!Shell_NotifyIconW(NIM_ADD, data)) return false;
	data->uVersion = NOTIFYICON_VERSION_4;
	if (Shell_NotifyIconW(NIM_SETVERSION, data)) return true;
	Shell_NotifyIconW(NIM_DELETE, data);
	return false;
}

static HRESULT tray_set(FluxTrayIcon *tray, FluxTrayDesc const *desc) {
	if (!desc || !desc->icon) return E_INVALIDARG;
	NOTIFYICONDATAW data = tray->data;
	data.hIcon           = CopyIcon(desc->icon);
	if (!data.hIcon) return E_OUTOFMEMORY;
	wcsncpy(data.szTip, desc->tooltip ? desc->tooltip : L"", ARRAYSIZE(data.szTip) - 1u);
	bool ok = tray->installed ? Shell_NotifyIconW(NIM_MODIFY, &data) : tray_install(&data);
	if (!ok) {
		DestroyIcon(data.hIcon);
		return E_FAIL;
	}
	if (tray->data.hIcon) DestroyIcon(tray->data.hIcon);
	tray->data      = data;
	tray->menu      = desc->menu;
	tray->activate  = desc->activate;
	tray->command   = desc->command;
	tray->ctx       = desc->ctx;
	tray->installed = true;
	return S_OK;
}

HRESULT flux_tray_create(FluxWindow *win, FluxTrayDesc const *desc, FluxTrayIcon **out) {
	if (!out) return E_INVALIDARG;
	*out = NULL;
	if (!win || !win->hwnd || !IsWindow(win->hwnd)) return E_INVALIDARG;
	if (win->tray_next_id == UINT16_MAX) return E_OUTOFMEMORY;
	FluxTrayIcon *tray = calloc(1, sizeof(*tray));
	if (!tray) return E_OUTOFMEMORY;
	tray->window = win;
	tray->data   = (NOTIFYICONDATAW) {
	  .cbSize           = sizeof(NOTIFYICONDATAW),
	  .hWnd             = win->hwnd,
	  .uID              = win->tray_next_id + 1u,
	  .uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
	  .uCallbackMessage = FLUX_WM_TRAY};
	HRESULT hr = tray_set(tray, desc);
	if (FAILED(hr)) {
		free(tray);
		return hr;
	}
	win->tray_next_id = tray->data.uID;
	tray->next        = win->tray_icons;
	win->tray_icons   = tray;
	*out              = tray;
	return S_OK;
}

HRESULT flux_tray_update(FluxTrayIcon *tray, FluxTrayDesc const *desc) {
	return tray ? tray_set(tray, desc) : E_INVALIDARG;
}

void flux_tray_destroy(FluxTrayIcon *tray) {
	if (!tray) return;
	FluxTrayIcon **link = &tray->window->tray_icons;
	while (*link != tray) link = &(*link)->next;
	*link = tray->next;
	if (tray->installed) Shell_NotifyIconW(NIM_DELETE, &tray->data);
	DestroyIcon(tray->data.hIcon);
	free(tray);
}

void flux_tray_destroy_window(FluxWindow *win) {
	while (win->tray_icons) flux_tray_destroy(win->tray_icons);
}

static FluxTrayIcon *tray_find(FluxWindow *win, UINT id) {
	for (FluxTrayIcon *tray = win->tray_icons; tray; tray = tray->next)
		if (tray->data.uID == id) return tray;
	return NULL;
}

static void tray_context_menu(FluxTrayIcon *tray, POINT point) {
	if (!tray->menu) return;
	HWND hwnd = tray->data.hWnd;
	UINT id   = tray->data.uID;
	SetForegroundWindow(hwnd);
	UINT command = ( UINT ) TrackPopupMenuEx(
	  tray->menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, hwnd, NULL
	);
	FluxWindow *win = ( FluxWindow * ) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (!win) return;
	PostMessageW(hwnd, WM_NULL, 0, 0);
	tray = tray_find(win, id);
	if (!tray) return;
	if (!command) Shell_NotifyIconW(NIM_SETFOCUS, &tray->data);
	else if (tray->command) tray->command(tray->ctx, command);
}

static void tray_reinstall_all(FluxWindow *win) {
	for (FluxTrayIcon *tray = win->tray_icons; tray; tray = tray->next) tray->installed = tray_install(&tray->data);
}

bool flux_tray_message(FluxWindow *win, UINT msg, WPARAM wp, LPARAM lp) {
	if (win->taskbar_created && msg == win->taskbar_created) {
		tray_reinstall_all(win);
		return true;
	}
	if (msg != FLUX_WM_TRAY) return false;
	FluxTrayIcon *tray = tray_find(win, HIWORD(lp));
	if (!tray) return true;
	UINT event = LOWORD(lp);
	if (event == WM_CONTEXTMENU) tray_context_menu(tray, (POINT) {GET_X_LPARAM(wp), GET_Y_LPARAM(wp)});
	else if ((event == NIN_SELECT || event == NIN_KEYSELECT) && tray->activate) tray->activate(tray->ctx);
	return true;
}
