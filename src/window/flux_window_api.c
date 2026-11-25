#include "flux_window_internal.h"
#include "controls/data/flux_title_bar_data.h"
#include "theme/flux_theme_contrast.h"
#include "fluxent/flux_gfx.h"

#include <objbase.h>
#include <stdlib.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
  #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void flux_window_destroy(FluxWindow *win) {
	if (!win) return;
	if (win->gfx) flux_gfx_destroy(win->gfx);
	if (win->hwnd) DestroyWindow(win->hwnd);
	if (win->ole_initialized) OleUninitialize();
	free(win);
}

HWND flux_window_hwnd(FluxWindow const *win) { return win ? win->hwnd : NULL; }

bool flux_window_setaccelerators(FluxWindow *win, ACCEL const *entries, int count, FluxAcceleratorFn cb, void *ctx) {
	if (!win || count < 0 || (count && (!entries || !cb))) return false;
	HACCEL table = count ? CreateAcceleratorTableW(( ACCEL * ) entries, count) : NULL;
	if (count && !table) return false;
	if (win->accelerators) DestroyAcceleratorTable(win->accelerators);
	win->accelerators    = table;
	win->on_accelerator  = table ? cb : NULL;
	win->accelerator_ctx = table ? ctx : NULL;
	return true;
}

FluxSize flux_window_size(FluxWindow const *win) {
	if (!win || !win->hwnd) return (FluxSize) {0, 0};
	RECT rc;
	GetClientRect(win->hwnd, &rc);
	return (FluxSize) {( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

FluxDpi flux_window_dpi(FluxWindow const *win) {
	if (!win) return (FluxDpi) {FLUX_DPI_BASE, FLUX_DPI_BASE};
	return win->dpi;
}

FluxGfx *flux_window_gfx(FluxWindow *win) { return win ? win->gfx : NULL; }

void     flux_window_onrender(FluxWindow *win, FluxRenderFn cb, void *ctx) {
	if (!win) return;
	win->on_render  = cb;
	win->render_ctx = ctx;
}

void flux_window_onresize(FluxWindow *win, FluxResizeFn cb, void *ctx) {
	if (!win) return;
	win->on_resize  = cb;
	win->resize_ctx = ctx;
}

void flux_window_addresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx) {
	if (!win || !cb || win->resize_observer_count >= FLUX_WINDOW_MAX_RESIZE_OBSERVERS) return;
	win->resize_observers [win->resize_observer_count]    = cb;
	win->resize_observer_ctx [win->resize_observer_count] = ctx;
	win->resize_observer_count++;
}

void flux_window_delresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx) {
	if (!win) return;
	for (int i = 0; i < win->resize_observer_count; i++) {
		if (win->resize_observers [i] != cb || win->resize_observer_ctx [i] != ctx) continue;
		win->resize_observers [i]    = win->resize_observers [win->resize_observer_count - 1];
		win->resize_observer_ctx [i] = win->resize_observer_ctx [win->resize_observer_count - 1];
		win->resize_observer_count--;
		return;
	}
}

void flux_window_ondpi(FluxWindow *win, FluxDpiChangedFn cb, void *ctx) {
	if (!win) return;
	win->on_dpi_changed  = cb;
	win->dpi_changed_ctx = ctx;
}

void flux_window_onpointer(FluxWindow *win, FluxPointerFn cb, void *ctx) {
	if (!win) return;
	win->on_pointer  = cb;
	win->pointer_ctx = ctx;
}

void flux_window_onkey(FluxWindow *win, FluxKeyFn cb, void *ctx) {
	if (!win) return;
	win->on_key  = cb;
	win->key_ctx = ctx;
}

void flux_window_onchar(FluxWindow *win, FluxCharFn cb, void *ctx) {
	if (!win) return;
	win->on_char  = cb;
	win->char_ctx = ctx;
}

void flux_window_onuia(FluxWindow *win, FluxUiaProviderFn cb, void *ctx) {
	if (!win) return;
	win->on_uia_provider  = cb;
	win->uia_provider_ctx = ctx;
}

void flux_window_onsettings(FluxWindow *win, FluxSettingChangedFn cb, void *ctx) {
	if (!win) return;
	win->on_setting_changed  = cb;
	win->setting_changed_ctx = ctx;
}

void flux_window_onime(FluxWindow *win, FluxImeCompositionFn cb, void *ctx) {
	if (!win) return;
	win->on_ime_composition  = cb;
	win->ime_composition_ctx = ctx;
}

void flux_window_setdark(FluxWindow *win, bool enabled) {
	if (!win || !win->hwnd) return;
	BOOL dark = enabled && !flux_theme_systemcontrast();
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	win->config.dark_mode = enabled;
}

void flux_window_settitle(FluxWindow *win, wchar_t const *title) {
	if (!win || !win->hwnd || !title) return;
	SetWindowTextW(win->hwnd, title);
}

void flux_window_setmaxbutton(FluxWindow *win, FluxRect bounds) {
	if (win) win->title_bar_max = bounds;
}

bool flux_window_caption_bounds(FluxWindow *win, FluxRect *bounds) {
	win->title_bar_native_buttons = false;
	if (!win->title_bar_extended || IsIconic(win->hwnd) || !IsWindowVisible(win->hwnd)) return false;
	RECT    caption = {0}, window = {0};
	HRESULT hr = DwmGetWindowAttribute(win->hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &caption, sizeof(caption));
	if (FAILED(hr) || IsRectEmpty(&caption) || !GetWindowRect(win->hwnd, &window)) return false;
	POINT start = {window.left + caption.left, window.top + caption.top};
	if (!ScreenToClient(win->hwnd, &start)) return false;
	UINT dpi = GetDpiForWindow(win->hwnd);
	if (!dpi) return false;
	float scale = FLUX_DPI_BASE / ( float ) dpi;
	*bounds     = (FluxRect) {
	  start.x * scale, start.y * scale, (caption.right - caption.left) * scale, (caption.bottom - caption.top) * scale};
	win->title_bar_native_buttons = true;
	win->title_bar_max_pressed    = false;
	return true;
}

static float window_titlebar_height(FluxWindow const *win) {
	return win->title_bar_active ? win->title_bar_drag.h : FLUX_TB_HEIGHT;
}

static void window_set_titlebar_regions(FluxWindow *win, FluxRect drag, FluxRect const *passthrough, int count) {
	if (drag.w <= 0.0f || drag.h <= 0.0f) {
		win->title_bar_active     = false;
		win->title_bar_pass_count = 0;
		return;
	}
	if (count < 0) count = 0;
	if (count > FLUX_WINDOW_MAX_TITLE_BAR_PASS) count = FLUX_WINDOW_MAX_TITLE_BAR_PASS;
	win->title_bar_active     = true;
	win->title_bar_drag       = drag;
	win->title_bar_pass_count = count;
	for (int i = 0; i < count; i++) win->title_bar_pass [i] = passthrough [i];
}

void flux_window_settitlebar(FluxWindow *win, FluxRect drag, FluxRect const *passthrough, int count) {
	if (!win) return;
	float previous = window_titlebar_height(win);
	window_set_titlebar_regions(win, drag, passthrough, count);
	if (win->shown && win->title_bar_extended && window_titlebar_height(win) != previous)
		flux_window_applyframe(win, true);
}

void flux_window_applyframe(FluxWindow *win, bool deferred) {
	if (!win || !win->hwnd) return;
	/* DWM animates minimize and maximize only for a WS_CAPTION window, so keep the caption. Drop WS_SYSMENU instead:
	 * with it, the window has system caption buttons, and Snap Layouts opens only over those, which the one-pixel frame
	 * below leaves no room for. Without it, the flyout follows HTMAXBUTTON from the hit test. */
	MARGINS  m          = win->title_bar_extended ? (MARGINS) {0, 0, 1, 0} : (MARGINS) {0, 0, 0, 0};
	LONG_PTR style      = GetWindowLongPtrW(win->hwnd, GWL_STYLE);
	LONG_PTR next_style = (win->title_bar_extended ? style & ~WS_SYSMENU : style | WS_SYSMENU) | WS_CAPTION;
	if (next_style != style) SetWindowLongPtrW(win->hwnd, GWL_STYLE, next_style);
	DwmExtendFrameIntoClientArea(win->hwnd, &m);
	if (deferred) {
		PostMessageW(win->hwnd, FLUX_WM_APPLY_FRAME, 0, 0);
		return;
	}
	SetWindowPos(
	  win->hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
	);
}

void flux_window_extendtitlebar(FluxWindow *win, bool enabled) {
	if (!win || win->title_bar_extended == enabled) return;
	win->title_bar_extended       = enabled;
	win->title_bar_native_buttons = false;
	win->title_bar_max_pressed    = false;
	if (win->shown) flux_window_applyframe(win, true);
}

bool flux_window_titlebar(FluxWindow const *win) { return win && win->title_bar_extended; }

void flux_window_redraw(FluxWindow *win) {
	if (win) win->render_requested = true;
}

void flux_window_redraw_after(FluxWindow *win, double seconds) {
	if (!win || !win->hwnd || seconds <= 0.0) return;
	UINT      delay = ( UINT ) (seconds * 1000.0 + 0.999);
	ULONGLONG due   = GetTickCount64() + delay;
	if (win->redraw_due && win->redraw_due <= due) return;
	if (SetTimer(win->hwnd, FLUX_WINDOW_REDRAW_TIMER, delay, NULL)) win->redraw_due = due;
}

void flux_window_abandonpointer(FluxWindow *win, uint32_t pointer_id) {
	if (!win) return;
	if (win->primary_touch_pid == pointer_id) win->primary_touch_pid = 0;
}

void flux_window_setmenu(FluxWindow *win, FluxMenu *menu) {
	if (win) win->active_menu = menu;
}

FluxMenu *flux_window_menu(FluxWindow *win) { return win ? win->active_menu : NULL; }
