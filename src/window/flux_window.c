#include "flux_window_internal.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_popup.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <objbase.h>
#include <imm.h>
#ifdef _MSC_VER
  #pragma comment(lib, "imm32.lib")
#endif

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <dwmapi.h>
#include <windowsx.h>

/* Windows touch gesture + feedback APIs. Both live in user32.lib which
 * is implicitly linked. `GESTURECONFIG` / `SetGestureConfig` require
 * `WINVER >= 0x0601`; `SetWindowFeedbackSetting` requires `WINVER >= 0x0602`. */
#ifndef WINVER
  #define WINVER 0x0602
#endif
#include <winuser.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
  #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
  #define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
  #define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif
#ifndef WS_EX_NOREDIRECTIONBITMAP
  #define WS_EX_NOREDIRECTIONBITMAP 0x00200000l
#endif
#ifndef PT_TOUCHPAD
  #define PT_TOUCHPAD 5
#endif

#define DWMSBT_NONE            0
#define DWMSBT_MAINWINDOW      2
#define DWMSBT_TRANSIENTWINDOW 3
#define DWMSBT_TABBEDWINDOW    4
#define FLUX_WINDOW_DEFAULT_W  800
#define FLUX_WINDOW_DEFAULT_H  600

static wchar_t const    FLUX_WND_CLASS [] = L"FluxentWindowClass";
static bool             class_registered  = false;

static LRESULT CALLBACK flux_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

typedef struct WindowMessage {
	FluxWindow *win;
	HWND        hwnd;
	UINT        msg;
	WPARAM      wp;
	LPARAM      lp;
} WindowMessage;

typedef struct WindowContactPointer {
	FluxWindow       *win;
	HWND              hwnd;
	UINT              msg;
	UINT32            flags;
	UINT32            pid;
	FluxPointerEvent *ev;
} WindowContactPointer;

static void setup_dwm_backdrop(FluxWindow *win) {
	BOOL dark = win->config.dark_mode ? TRUE : FALSE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

	BOOL host = TRUE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_HOSTBACKDROPBRUSH, &host, sizeof(host));

	flux_window_set_backdrop(win, win->config.backdrop);
}

static float window_dpi_scale(FluxWindow const *win) {
	float scale = win->dpi.dpi_x / FLUX_DPI_BASE;
	return scale > 0.0f ? scale : 1.0f;
}

static void window_emit_pointer(FluxWindow *win, FluxPointerEvent const *ev) {
	if (win->on_pointer) win->on_pointer(win->pointer_ctx, ev);
}

static HCURSOR window_cursor_handle(FluxWindow *win, int cursor_type) {
	switch (cursor_type) {
	case FLUX_CURSOR_IBEAM : return win->cursor_ibeam;
	case FLUX_CURSOR_HAND  : return win->cursor_hand;
	default                : return win->cursor_arrow;
	}
}

static FluxPointerType window_pointer_device(POINTER_INPUT_TYPE type) {
	if (type == PT_TOUCH) return FLUX_POINTER_TOUCH;
	if (type == PT_PEN) return FLUX_POINTER_PEN;
	return FLUX_POINTER_MOUSE;
}

static uint32_t window_pointer_buttons(UINT32 flags) {
	uint32_t buttons = 0;
	if (flags & POINTER_FLAG_FIRSTBUTTON) buttons |= FLUX_POINTER_BUTTON_LEFT;
	if (flags & POINTER_FLAG_SECONDBUTTON) buttons |= FLUX_POINTER_BUTTON_RIGHT;
	if (flags & POINTER_FLAG_THIRDBUTTON) buttons |= FLUX_POINTER_BUTTON_MIDDLE;
	return buttons;
}

static void window_client_point_from_lparam(HWND hwnd, LPARAM lp, FluxWindow const *win, float *out_x, float *out_y) {
	POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
	ScreenToClient(hwnd, &pt);
	float scale = window_dpi_scale(win);
	*out_x      = ( float ) pt.x / scale;
	*out_y      = ( float ) pt.y / scale;
}

static void window_enable_pointer_input(HWND hwnd) {
	typedef BOOL(WINAPI * PFN_EnableMouseInPointer)(BOOL);

	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	if (user32) {
		PFN_EnableMouseInPointer p_emip
		  = ( PFN_EnableMouseInPointer ) ( void * ) GetProcAddress(user32, "EnableMouseInPointer");
		if (p_emip) p_emip(TRUE);
	}

	GESTURECONFIG gc = {0, 0, GC_ALLGESTURES};
	SetGestureConfig(hwnd, 0, 1, &gc, sizeof(GESTURECONFIG));
}

static LRESULT window_on_set_cursor(FluxWindow *win, LPARAM lp) {
	if (LOWORD(lp) != HTCLIENT) return 0;
	SetCursor(window_cursor_handle(win, win->cursor_type));
	return TRUE;
}

static LRESULT window_on_size(FluxWindow *win, LPARAM lp) {
	int w = LOWORD(lp);
	int h = HIWORD(lp);
	if (w <= 0 || h <= 0) return 0;

	if (win->gfx) flux_graphics_resize(win->gfx, w, h);
	if (win->on_resize) win->on_resize(win->resize_ctx, w, h);
	for (int i = 0; i < win->resize_observer_count; i++)
		win->resize_observers [i](win->resize_observer_ctx [i], w, h);
	if (win->on_render) win->on_render(win->render_ctx);
	win->render_requested = false;
	return 0;
}

static LRESULT window_on_dpi_changed(FluxWindow *win, HWND hwnd, WPARAM wp, LPARAM lp) {
	UINT dpi       = HIWORD(wp);
	win->dpi.dpi_x = ( float ) dpi;
	win->dpi.dpi_y = ( float ) dpi;
	if (win->gfx) flux_graphics_set_dpi(win->gfx, win->dpi);

	RECT *suggested = ( RECT * ) lp;
	if (suggested) {
		SetWindowPos(
		  hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
		  suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE
		);
	}

	if (win->on_dpi_changed) win->on_dpi_changed(win->dpi_changed_ctx, win->dpi);
	return 0;
}

static LRESULT window_on_paint(FluxWindow *win, HWND hwnd) {
	PAINTSTRUCT ps;
	BeginPaint(hwnd, &ps);
	EndPaint(hwnd, &ps);
	win->render_requested = true;
	return 0;
}

static bool window_accept_touch_pointer(FluxWindow *win, UINT msg, UINT32 pid, bool is_touch) {
	if (!is_touch) return true;

	if (msg == WM_POINTERDOWN) {
		if (win->primary_touch_pid != 0 && win->primary_touch_pid != pid) return false;
		win->primary_touch_pid = pid;
		return true;
	}

	return win->primary_touch_pid == pid;
}

static FluxPointerEvent
window_pointer_base_event(FluxWindow *win, HWND hwnd, LPARAM lp, POINTER_INPUT_TYPE ptype, UINT32 pid) {
	FluxPointerEvent ev = {0};
	ev.device           = window_pointer_device(ptype);
	ev.pointer_id       = pid;
	window_client_point_from_lparam(hwnd, lp, win, &ev.x, &ev.y);
	return ev;
}

static void window_emit_mouse_context_menu(FluxWindow *win, FluxPointerEvent *ev, uint32_t bit) {
	if (bit != FLUX_POINTER_BUTTON_RIGHT) return;
	ev->kind           = FLUX_POINTER_CONTEXT_MENU;
	ev->changed_button = 0;
	window_emit_pointer(win, ev);
}

static void
window_emit_mouse_pointer(FluxWindow *win, HWND hwnd, UINT msg, FluxPointerEvent *ev, uint32_t new_buttons) {
	uint32_t old_buttons = win->mouse_buttons;
	uint32_t pressed     = new_buttons & ~old_buttons;
	uint32_t released    = old_buttons & ~new_buttons;

	for (uint32_t b = pressed; b; b &= b - 1) {
		uint32_t bit       = b & (0u - b);
		ev->kind           = FLUX_POINTER_DOWN;
		ev->buttons        = old_buttons | bit;
		ev->changed_button = bit;
		if (bit == FLUX_POINTER_BUTTON_LEFT) SetCapture(hwnd);
		window_emit_pointer(win, ev);
		old_buttons |= bit;
	}

	for (uint32_t b = released; b; b &= b - 1) {
		uint32_t bit       = b & (0u - b);
		ev->kind           = FLUX_POINTER_UP;
		ev->buttons        = old_buttons & ~bit;
		ev->changed_button = bit;
		window_emit_pointer(win, ev);
		old_buttons &= ~bit;
		window_emit_mouse_context_menu(win, ev, bit);
		if (bit == FLUX_POINTER_BUTTON_LEFT && old_buttons == 0) ReleaseCapture();
	}

	win->mouse_buttons = new_buttons;
	if (msg != WM_POINTERUPDATE || pressed != 0 || released != 0) return;

	ev->kind           = FLUX_POINTER_MOVE;
	ev->buttons        = new_buttons;
	ev->changed_button = 0;
	window_emit_pointer(win, ev);
}

static void window_emit_contact_pointer(WindowContactPointer const *contact) {
	if (contact->msg == WM_POINTERDOWN) {
		SetCapture(contact->hwnd);
		contact->ev->kind           = FLUX_POINTER_DOWN;
		contact->ev->buttons        = FLUX_POINTER_BUTTON_LEFT;
		contact->ev->changed_button = FLUX_POINTER_BUTTON_LEFT;
		window_emit_pointer(contact->win, contact->ev);
		return;
	}

	if (contact->msg == WM_POINTERUP) {
		contact->ev->kind           = FLUX_POINTER_UP;
		contact->ev->buttons        = 0;
		contact->ev->changed_button = FLUX_POINTER_BUTTON_LEFT;
		window_emit_pointer(contact->win, contact->ev);
		ReleaseCapture();
		if (contact->ev->device == FLUX_POINTER_TOUCH && contact->win->primary_touch_pid == contact->pid)
			contact->win->primary_touch_pid = 0;
		return;
	}

	contact->ev->kind           = FLUX_POINTER_MOVE;
	contact->ev->buttons        = (contact->flags & POINTER_FLAG_INCONTACT) ? FLUX_POINTER_BUTTON_LEFT : 0;
	contact->ev->changed_button = 0;
	window_emit_pointer(contact->win, contact->ev);
}

/* Touch contacts are fully consumed here: handing them to DefWindowProc would
 * make the system promote the tap into legacy mouse messages delivered to
 * whichever window sits under the contact by then — a popup opened by that
 * very tap immediately eats a ghost press (a ComboBox would auto-select its
 * first item, then flicker open/closed on later taps). The press-and-hold
 * context-menu gesture that promotion would otherwise provide is synthesized below. */
#define WINDOW_TOUCH_HOLD_TIMER 0xF1
#define WINDOW_TOUCH_HOLD_MS    500
#define WINDOW_TOUCH_HOLD_SLOP  10.0f

static void window_track_touch_hold(FluxWindow *win, HWND hwnd, UINT msg, FluxPointerEvent const *ev) {
	if (msg == WM_POINTERDOWN) {
		win->touch_hold_x = ev->x;
		win->touch_hold_y = ev->y;
		SetTimer(hwnd, WINDOW_TOUCH_HOLD_TIMER, WINDOW_TOUCH_HOLD_MS, NULL);
		return;
	}
	if (msg == WM_POINTERUP
		|| fabsf(ev->x - win->touch_hold_x) > WINDOW_TOUCH_HOLD_SLOP
		|| fabsf(ev->y - win->touch_hold_y) > WINDOW_TOUCH_HOLD_SLOP)
		KillTimer(hwnd, WINDOW_TOUCH_HOLD_TIMER);
}

/* Press-and-hold fired: the contact stops being a tap (cancel the press) and
 * surfaces a context menu at the hold position, matching the system gesture. */
static LRESULT window_on_touch_hold_timer(FluxWindow *win, HWND hwnd) {
	KillTimer(hwnd, WINDOW_TOUCH_HOLD_TIMER);
	if (win->primary_touch_pid == 0) return 0;
	win->primary_touch_pid = 0;
	ReleaseCapture();

	FluxPointerEvent ev = {0};
	ev.device           = FLUX_POINTER_TOUCH;
	ev.x                = win->touch_hold_x;
	ev.y                = win->touch_hold_y;
	ev.kind             = FLUX_POINTER_CANCEL;
	window_emit_pointer(win, &ev);
	ev.kind = FLUX_POINTER_CONTEXT_MENU;
	window_emit_pointer(win, &ev);
	return 0;
}

static LRESULT window_on_pointer_contact(FluxWindow *win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	UINT32             pid   = GET_POINTERID_WPARAM(wp);
	POINTER_INPUT_TYPE ptype = PT_POINTER;
	GetPointerType(pid, &ptype);
	if (ptype == PT_TOUCHPAD) return 0;

	bool is_touch = (ptype == PT_TOUCH);
	bool is_mouse = (ptype == PT_MOUSE);
	if (!window_accept_touch_pointer(win, msg, pid, is_touch)) return 0;

	POINTER_INFO pi = {0};
	if (!GetPointerInfo(pid, &pi)) return 0;
	UINT32 flags = pi.pointerFlags;
	if (!is_mouse && !(flags & POINTER_FLAG_INCONTACT) && msg == WM_POINTERUPDATE) return 0;

	FluxPointerEvent ev = window_pointer_base_event(win, hwnd, lp, ptype, pid);
	if (is_mouse) {
		window_emit_mouse_pointer(win, hwnd, msg, &ev, window_pointer_buttons(flags));
		return 0;
	}

	WindowContactPointer contact = {win, hwnd, msg, flags, pid, &ev};
	window_emit_contact_pointer(&contact);
	if (is_touch) window_track_touch_hold(win, hwnd, msg, &ev);
	return 0;
}

static LRESULT window_on_pointer_wheel(FluxWindow *win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	UINT32             pid   = GET_POINTERID_WPARAM(wp);
	POINTER_INPUT_TYPE ptype = PT_POINTER;
	GetPointerType(pid, &ptype);
	if (ptype == PT_TOUCHPAD) return 0;

	FluxPointerEvent ev = window_pointer_base_event(win, hwnd, lp, ptype, pid);
	float            d  = ( float ) GET_WHEEL_DELTA_WPARAM(wp) / ( float ) WHEEL_DELTA;
	ev.kind             = FLUX_POINTER_WHEEL;
	if (msg == WM_POINTERHWHEEL) ev.wheel_dx = d;
	else ev.wheel_dy = d;
	window_emit_pointer(win, &ev);
	return 0;
}

static LRESULT window_on_capture_changed(FluxWindow *win, HWND hwnd) {
	FluxPointerEvent ev = {0};
	ev.kind             = FLUX_POINTER_CANCEL;
	ev.device           = FLUX_POINTER_MOUSE;

	POINT cur;
	GetCursorPos(&cur);
	ScreenToClient(hwnd, &cur);
	float scale            = window_dpi_scale(win);
	ev.x                   = ( float ) cur.x / scale;
	ev.y                   = ( float ) cur.y / scale;
	win->primary_touch_pid = 0;
	win->mouse_buttons     = 0;
	window_emit_pointer(win, &ev);
	return 0;
}

static LRESULT window_on_context_menu(FluxWindow *win, HWND hwnd, LPARAM lp) {
	int              sx = GET_X_LPARAM(lp);
	int              sy = GET_Y_LPARAM(lp);
	FluxPointerEvent ev = {0};
	ev.kind             = FLUX_POINTER_CONTEXT_MENU;
	ev.device           = FLUX_POINTER_TOUCH;

	if (sx != -1 || sy != -1) {
		POINT scr;
		scr.x = sx;
		scr.y = sy;
		ScreenToClient(hwnd, &scr);
		float scale = window_dpi_scale(win);
		ev.x        = ( float ) scr.x / scale;
		ev.y        = ( float ) scr.y / scale;
	}
	else { ev.device = FLUX_POINTER_MOUSE; }

	window_emit_pointer(win, &ev);
	return 0;
}

static wchar_t *window_ime_read_string(HIMC hImc, DWORD kind, LONG *out_len) {
	*out_len   = 0;
	LONG bytes = ImmGetCompositionStringW(hImc, kind, NULL, 0);
	if (bytes <= 0) return NULL;

	wchar_t *buffer = ( wchar_t * ) malloc(( size_t ) bytes + sizeof(wchar_t));
	if (!buffer) return NULL;
	if (ImmGetCompositionStringW(hImc, kind, buffer, ( DWORD ) bytes) < 0) {
		free(buffer);
		return NULL;
	}
	*out_len          = bytes / ( LONG ) sizeof(wchar_t);
	buffer [*out_len] = 0;
	return buffer;
}

static void window_emit_ime_result(FluxWindow *win, wchar_t const *text, LONG len) {
	if (!win->on_char || !text || len <= 0) return;
	for (LONG i = 0; i < len; i++) win->on_char(win->char_ctx, text [i]);
}

static void window_apply_ime_position(FluxWindow *win, HIMC hImc) {
	if (!win || !win->hwnd || !hImc || !win->ime_position_valid) return;
	if (win->ime_applying_position) return;
	win->ime_applying_position = true;

	COMPOSITIONFORM cf;
	memset(&cf, 0, sizeof(cf));
	cf.dwStyle        = CFS_POINT;
	cf.ptCurrentPos.x = win->ime_x;
	cf.ptCurrentPos.y = win->ime_y;
	ImmSetCompositionWindow(hImc, &cf);

	CANDIDATEFORM cand;
	memset(&cand, 0, sizeof(cand));
	cand.dwIndex        = 0;
	cand.dwStyle        = CFS_EXCLUDE;
	cand.ptCurrentPos.x = win->ime_x;
	cand.ptCurrentPos.y = win->ime_y;
	cand.rcArea.left    = win->ime_x;
	cand.rcArea.top     = win->ime_y;
	cand.rcArea.right   = win->ime_x + 1;
	cand.rcArea.bottom  = win->ime_y + win->ime_h;
	ImmSetCandidateWindow(hImc, &cand);

	win->ime_applying_position = false;
}

static void window_update_system_caret(FluxWindow *win) {
	if (!win || !win->hwnd || !win->ime_position_valid) return;
	CreateCaret(win->hwnd, NULL, 1, win->ime_h > 0 ? win->ime_h : 1);
	SetCaretPos(win->ime_x, win->ime_y);
	HideCaret(win->hwnd);
}

static bool window_on_ime_request(FluxWindow *win, WPARAM wp, LPARAM lp, LRESULT *result) {
	if (wp != IMR_QUERYCHARPOSITION || !lp || !win || !win->ime_position_valid) return false;

	IMECHARPOSITION *pos = ( IMECHARPOSITION * ) lp;
	memset(pos, 0, sizeof(*pos));
	pos->dwSize      = sizeof(*pos);
	pos->dwCharPos   = 0;
	pos->pt.x        = win->ime_x;
	pos->pt.y        = win->ime_y;
	pos->cLineHeight = ( UINT ) (win->ime_h > 0 ? win->ime_h : 1);
	ClientToScreen(win->hwnd, &pos->pt);
	GetClientRect(win->hwnd, &pos->rcDocument);
	MapWindowPoints(win->hwnd, NULL, ( POINT * ) &pos->rcDocument, 2);
	*result = 1;
	return true;
}

static bool window_on_ime_notify(FluxWindow *win, WPARAM wp) {
	if (!win || !win->ime_position_valid) return false;
	if (wp != IMN_OPENCANDIDATE && wp != IMN_CHANGECANDIDATE) return false;

	HIMC hImc = ImmGetContext(win->hwnd);
	if (!hImc) return false;
	window_apply_ime_position(win, hImc);
	ImmReleaseContext(win->hwnd, hImc);
	return true;
}

static bool window_on_ime_composition(FluxWindow *win, HWND hwnd, LPARAM lp) {
	if (!win->on_ime_composition && !win->on_char) return false;

	HIMC hImc = ImmGetContext(hwnd);
	if (!hImc) return false;
	window_apply_ime_position(win, hImc);

	bool handled = false;
	if (lp & GCS_RESULTSTR) {
		LONG     result_len = 0;
		wchar_t *result     = window_ime_read_string(hImc, GCS_RESULTSTR, &result_len);
		if (win->on_ime_composition) win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		window_emit_ime_result(win, result, result_len);
		free(result);
		handled = true;
	}

	if (win->on_ime_composition && (lp & (GCS_COMPSTR | GCS_CURSORPOS))) {
		LONG     comp_len = 0;
		wchar_t *comp     = window_ime_read_string(hImc, GCS_COMPSTR, &comp_len);
		int      cursor   = ( int ) ImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
		if (comp && comp_len > 0) win->on_ime_composition(win->ime_composition_ctx, comp, cursor);
		else win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		free(comp);
		handled = true;
	}
	else if (!handled && win->on_ime_composition) {
		win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		handled = true;
	}

	ImmReleaseContext(hwnd, hImc);
	return handled;
}

static LRESULT window_on_mouse_wheel(FluxWindow *win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	FluxPointerEvent ev = {0};
	ev.kind             = FLUX_POINTER_WHEEL;
	ev.device           = FLUX_POINTER_MOUSE;
	window_client_point_from_lparam(hwnd, lp, win, &ev.x, &ev.y);

	float d = ( float ) GET_WHEEL_DELTA_WPARAM(wp) / ( float ) WHEEL_DELTA;
	if (msg == WM_MOUSEHWHEEL) ev.wheel_dx = d;
	else ev.wheel_dy = d;
	window_emit_pointer(win, &ev);
	return 0;
}

static LRESULT window_on_display_change(FluxWindow *win) {
	if (!win->gfx || flux_graphics_is_device_current(win->gfx)) return 0;

	flux_graphics_handle_device_change(win->gfx);
	win->render_requested = true;
	return 0;
}

static HRESULT window_register_class(HINSTANCE hinst) {
	if (class_registered) return S_OK;

	WNDCLASSEXW wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize        = sizeof(wc);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = flux_wnd_proc;
	wc.hInstance     = hinst;
	wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
	wc.lpszClassName = FLUX_WND_CLASS;

	if (!RegisterClassExW(&wc)) return HRESULT_FROM_WIN32(GetLastError());

	class_registered = true;
	return S_OK;
}

static bool window_dispatch_message(MSG const *msg, int *exit_code) {
	if (msg->message == WM_QUIT) {
		*exit_code = ( int ) msg->wParam;
		return false;
	}

	TranslateMessage(msg);
	DispatchMessageW(msg);
	return true;
}

static bool window_pump_messages(int *exit_code) {
	MSG  msg;
	bool keep_running = true;
	while (keep_running && PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		keep_running = window_dispatch_message(&msg, exit_code);
	return keep_running;
}

static void window_render_if_requested(FluxWindow *win) {
	if (!win->render_requested) return;

	win->render_requested = false;
	if (win->gfx && !flux_graphics_is_device_current(win->gfx)) flux_graphics_handle_device_change(win->gfx);
	if (win->on_render) win->on_render(win->render_ctx);
}

static bool window_run_iteration(FluxWindow *win, int *exit_code) {
	if (!window_pump_messages(exit_code)) return false;

	window_render_if_requested(win);
	if (!win->render_requested) MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
	return true;
}

static FluxWindow *window_bind_or_get(HWND hwnd, UINT msg, LPARAM lp) {
	if (msg != WM_NCCREATE) return ( FluxWindow * ) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	CREATESTRUCTW *cs  = ( CREATESTRUCTW * ) lp;
	FluxWindow    *win = ( FluxWindow * ) cs->lpCreateParams;
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, ( LONG_PTR ) win);
	win->hwnd = hwnd;
	return win;
}

static LRESULT window_on_create(FluxWindow *win, HWND hwnd) {
	win->cursor_arrow = LoadCursorW(NULL, IDC_ARROW);
	win->cursor_ibeam = LoadCursorW(NULL, IDC_IBEAM);
	win->cursor_hand  = LoadCursorW(NULL, IDC_HAND);
	window_enable_pointer_input(hwnd);
	return 0;
}

static LRESULT window_on_key_message(FluxWindow *win, WPARAM wp, bool down) {
	if (win->on_key) win->on_key(win->key_ctx, ( unsigned int ) wp, down);
	return 0;
}

static LRESULT window_on_setting_message(FluxWindow *win) {
	if (win->on_setting_changed) win->on_setting_changed(win->setting_changed_ctx);
	win->render_requested = true;
	return 0;
}

static bool window_handle_close_message(WindowMessage const *m, LRESULT *result) {
	if (m->msg == WM_DESTROY) {
		PostQuitMessage(0);
		*result = 0;
		return true;
	}
	if (m->msg != WM_CLOSE) return false;

	DestroyWindow(m->hwnd);
	*result = 0;
	return true;
}

static bool window_handle_popup_lifecycle(WindowMessage const *m) {
	if (m->msg == WM_ACTIVATE) {
		if (LOWORD(m->wp) == WA_INACTIVE) flux_popup_dismiss_all_for_owner(m->hwnd);
		return true;
	}
	if (m->msg != WM_MOVE && m->msg != WM_ENTERSIZEMOVE) return false;

	flux_popup_dismiss_all_for_owner(m->hwnd);
	return true;
}

static bool window_handle_lifecycle(WindowMessage const *m, LRESULT *result) {
	if (m->msg == WM_CREATE) {
		*result = window_on_create(m->win, m->hwnd);
		return true;
	}
	if (m->msg == WM_SETCURSOR) return window_on_set_cursor(m->win, m->lp) ? (*result = TRUE, true) : false;
	if (window_handle_close_message(m, result)) return true;
	if (window_handle_popup_lifecycle(m)) return false;
	return false;
}

static bool window_handle_layout(WindowMessage const *m, LRESULT *result) {
	switch (m->msg) {
	case WM_SIZE       : *result = window_on_size(m->win, m->lp); return true;
	case WM_DPICHANGED : *result = window_on_dpi_changed(m->win, m->hwnd, m->wp, m->lp); return true;
	case WM_ERASEBKGND : *result = 1; return true;
	case WM_PAINT      : *result = window_on_paint(m->win, m->hwnd); return true;
	case WM_GETOBJECT  : {
		LRESULT uia = flux_window_uia_get_object(m->win, m->hwnd, m->wp, m->lp);
		if (uia) {
			*result = uia;
			return true;
		}
		return false;
	}
	default : return false;
	}
}

static bool window_is_pointer_contact(UINT msg) {
	return msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE || msg == WM_POINTERUP;
}

static bool window_is_pointer_wheel(UINT msg) { return msg == WM_POINTERWHEEL || msg == WM_POINTERHWHEEL; }

static bool window_is_capture_change(UINT msg) { return msg == WM_POINTERCAPTURECHANGED || msg == WM_CAPTURECHANGED; }

static bool window_is_mouse_wheel(UINT msg) { return msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL; }

static bool window_handle_pointer(WindowMessage const *m, LRESULT *result) {
	if (window_is_pointer_contact(m->msg)) {
		*result = window_on_pointer_contact(m->win, m->hwnd, m->msg, m->wp, m->lp);
		return true;
	}
	if (window_is_pointer_wheel(m->msg)) {
		*result = window_on_pointer_wheel(m->win, m->hwnd, m->msg, m->wp, m->lp);
		return true;
	}
	if (window_is_capture_change(m->msg)) {
		*result = window_on_capture_changed(m->win, m->hwnd);
		return true;
	}
	if (m->msg == WM_CONTEXTMENU) {
		*result = window_on_context_menu(m->win, m->hwnd, m->lp);
		return true;
	}
	if (m->msg == WM_TIMER && m->wp == WINDOW_TOUCH_HOLD_TIMER) {
		*result = window_on_touch_hold_timer(m->win, m->hwnd);
		return true;
	}
	if (window_is_mouse_wheel(m->msg)) {
		*result = window_on_mouse_wheel(m->win, m->hwnd, m->msg, m->wp, m->lp);
		return true;
	}
	return false;
}

static bool window_is_key_down(UINT msg) { return msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN; }

static bool window_is_key_up(UINT msg) { return msg == WM_KEYUP || msg == WM_SYSKEYUP; }

static bool window_handle_ime_input(WindowMessage const *m, LRESULT *result) {
	if (m->msg == WM_IME_STARTCOMPOSITION) {
		HIMC hImc = ImmGetContext(m->hwnd);
		if (hImc) {
			window_apply_ime_position(m->win, hImc);
			ImmReleaseContext(m->hwnd, hImc);
		}
		*result = 0;
		return true;
	}
	if (m->msg == WM_IME_COMPOSITION) {
		*result = 0;
		return window_on_ime_composition(m->win, m->hwnd, m->lp);
	}
	if (m->msg != WM_IME_ENDCOMPOSITION) return false;

	if (m->win->on_ime_composition) m->win->on_ime_composition(m->win->ime_composition_ctx, NULL, 0);
	return false;
}

static bool window_handle_text_input(WindowMessage const *m, LRESULT *result) {
	if (window_is_key_down(m->msg)) {
		*result = window_on_key_message(m->win, m->wp, true);
		return true;
	}
	if (window_is_key_up(m->msg)) {
		*result = window_on_key_message(m->win, m->wp, false);
		return true;
	}
	if (m->msg == WM_CHAR) {
		if (m->win->on_char) m->win->on_char(m->win->char_ctx, ( wchar_t ) m->wp);
		*result = 0;
		return true;
	}
	return window_handle_ime_input(m, result);
}

static bool window_handle_system(WindowMessage const *m, LRESULT *result) {
	if (m->msg == WM_IME_REQUEST) return window_on_ime_request(m->win, m->wp, m->lp, result);
	if (m->msg == WM_IME_NOTIFY) return window_on_ime_notify(m->win, m->wp) ? (*result = 0, true) : false;

	switch (m->msg) {
	case WM_SETTINGCHANGE :
	case WM_THEMECHANGED  : *result = window_on_setting_message(m->win); return true;
	case WM_DISPLAYCHANGE : *result = window_on_display_change(m->win); return true;
	default               : return false;
	}
}

static bool window_handle_message(WindowMessage const *m, LRESULT *result) {
	if (window_handle_lifecycle(m, result)) return true;
	if (window_handle_layout(m, result)) return true;
	if (window_handle_pointer(m, result)) return true;
	if (window_handle_text_input(m, result)) return true;
	return window_handle_system(m, result);
}

static LRESULT CALLBACK flux_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	FluxWindow *win = window_bind_or_get(hwnd, msg, lp);
	if (!win) return DefWindowProcW(hwnd, msg, wp, lp);

	LRESULT       result  = 0;
	WindowMessage message = {win, hwnd, msg, wp, lp};
	if (window_handle_message(&message, &result)) return result;
	return DefWindowProcW(hwnd, msg, wp, lp);
}

static void window_apply_config(FluxWindow *win, FluxWindowConfig const *cfg) {
	if (cfg) win->config = *cfg;
	if (!win->config.title) win->config.title = L"Fluxent";
	if (win->config.width <= 0) win->config.width = FLUX_WINDOW_DEFAULT_W;
	if (win->config.height <= 0) win->config.height = FLUX_WINDOW_DEFAULT_H;
}

static HRESULT window_initialize_com(FluxWindow *win) {
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return hr;
	win->com_initialized = (hr == S_OK || hr == S_FALSE);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	return S_OK;
}

static HRESULT window_create_hwnd(FluxWindow *win, HINSTANCE hinst) {
	DWORD style    = WS_OVERLAPPEDWINDOW;
	DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;

	if (!win->config.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

	UINT  sys_dpi = GetDpiForSystem();
	float scale   = ( float ) sys_dpi / FLUX_DPI_BASE;

	int   cw      = ( int ) (win->config.width * scale + 0.5f);
	int   ch      = ( int ) (win->config.height * scale + 0.5f);

	RECT  rect    = {0, 0, cw, ch};
	AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, sys_dpi);

	int ww    = rect.right - rect.left;
	int wh    = rect.bottom - rect.top;

	win->hwnd = CreateWindowExW(
	  ex_style, FLUX_WND_CLASS, win->config.title, style, CW_USEDEFAULT, CW_USEDEFAULT, ww, wh, NULL, NULL, hinst, win
	);
	if (!win->hwnd) return HRESULT_FROM_WIN32(GetLastError());

	UINT dpi       = GetDpiForWindow(win->hwnd);
	win->dpi.dpi_x = ( float ) dpi;
	win->dpi.dpi_y = ( float ) dpi;

	setup_dwm_backdrop(win);
	return S_OK;
}

static HRESULT window_create_graphics(FluxWindow *win) {
	win->gfx = flux_graphics_create();
	if (!win->gfx) return E_FAIL;

	HRESULT hr = flux_graphics_attach(win->gfx, win->hwnd);
	if (FAILED(hr)) return hr;

	flux_graphics_set_dpi(win->gfx, win->dpi);
	return S_OK;
}

static void window_dispose_failed_create(FluxWindow *win) {
	if (win->gfx) flux_graphics_destroy(win->gfx);
	if (win->hwnd) DestroyWindow(win->hwnd);
	if (win->com_initialized) CoUninitialize();
	free(win);
}

HRESULT flux_window_create(FluxWindowConfig const *cfg, FluxWindow **out) {
	if (!out) return E_INVALIDARG;
	*out            = NULL;

	FluxWindow *win = ( FluxWindow * ) calloc(1, sizeof(*win));
	if (!win) return E_OUTOFMEMORY;

	window_apply_config(win, cfg);

	HRESULT hr = window_initialize_com(win);
	if (FAILED(hr)) {
		free(win);
		return hr;
	}

	HINSTANCE hinst = GetModuleHandleW(NULL);
	hr              = window_register_class(hinst);
	if (FAILED(hr)) {
		window_dispose_failed_create(win);
		return hr;
	}

	hr = window_create_hwnd(win, hinst);
	if (SUCCEEDED(hr)) hr = window_create_graphics(win);
	if (FAILED(hr)) {
		window_dispose_failed_create(win);
		return hr;
	}

	ShowWindow(win->hwnd, SW_SHOW);
	UpdateWindow(win->hwnd);

	*out = win;
	return S_OK;
}

void flux_window_set_backdrop(FluxWindow *win, int backdrop_type) {
	if (!win || !win->hwnd) return;
	DWORD bd = DWMSBT_NONE;
	switch (backdrop_type) {
	case 1  : bd = DWMSBT_MAINWINDOW; break;
	case 2  : bd = DWMSBT_TABBEDWINDOW; break;
	case 3  : bd = DWMSBT_TRANSIENTWINDOW; break;
	default : bd = DWMSBT_NONE; break;
	}
	DwmSetWindowAttribute(win->hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &bd, sizeof(bd));
	win->config.backdrop = backdrop_type;
}

int flux_window_backdrop(FluxWindow const *win) { return win ? win->config.backdrop : 0; }

void flux_window_set_ime_position(FluxWindow *win, int x, int y, int height) {
	if (!win || !win->hwnd) return;

	win->ime_x              = x;
	win->ime_y              = y;
	win->ime_h              = height > 0 ? height : 1;
	win->ime_position_valid = true;
	window_update_system_caret(win);

	HIMC hImc = ImmGetContext(win->hwnd);
	if (!hImc) return;
	window_apply_ime_position(win, hImc);
	ImmReleaseContext(win->hwnd, hImc);
}

void flux_window_set_cursor(FluxWindow *win, int cursor_type) {
	if (!win) return;
	if (win->cursor_type == cursor_type) return;
	win->cursor_type = cursor_type;
	if (win->hwnd) SetCursor(window_cursor_handle(win, cursor_type));
}

int flux_window_run(FluxWindow *win) {
	if (!win) return -1;

	int exit_code = -1;
	while (window_run_iteration(win, &exit_code)) { }
	return exit_code;
}

void flux_window_close(FluxWindow *win) {
	if (win && win->hwnd) PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
}
