#include "flux_window_internal.h"
#include "fluxent/flux_graphics.h"

#include <objbase.h>
#include <stdlib.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
  #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void flux_window_destroy(FluxWindow *win) {
	if (!win) return;
	if (win->gfx) flux_graphics_destroy(win->gfx);
	if (win->hwnd) DestroyWindow(win->hwnd);
	if (win->com_initialized) CoUninitialize();
	free(win);
}

HWND     flux_window_hwnd(FluxWindow const *win) { return win ? win->hwnd : NULL; }

FluxSize flux_window_client_size(FluxWindow const *win) {
	if (!win || !win->hwnd) return (FluxSize) {0, 0};
	RECT rc;
	GetClientRect(win->hwnd, &rc);
	return (FluxSize) {( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

FluxDpiInfo flux_window_dpi(FluxWindow const *win) {
	if (!win) return (FluxDpiInfo) {FLUX_DPI_BASE, FLUX_DPI_BASE};
	return win->dpi;
}

FluxGraphics *flux_window_get_graphics(FluxWindow *win) { return win ? win->gfx : NULL; }

void          flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx) {
	if (!win) return;
	win->on_render  = cb;
	win->render_ctx = ctx;
}

void flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx) {
	if (!win) return;
	win->on_resize  = cb;
	win->resize_ctx = ctx;
}

void flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx) {
	if (!win) return;
	win->on_dpi_changed  = cb;
	win->dpi_changed_ctx = ctx;
}

void flux_window_set_pointer_callback(FluxWindow *win, FluxPointerCallback cb, void *ctx) {
	if (!win) return;
	win->on_pointer  = cb;
	win->pointer_ctx = ctx;
}

void flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx) {
	if (!win) return;
	win->on_key  = cb;
	win->key_ctx = ctx;
}

void flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx) {
	if (!win) return;
	win->on_char  = cb;
	win->char_ctx = ctx;
}

void flux_window_set_uia_provider_callback(FluxWindow *win, FluxUiaProviderCallback cb, void *ctx) {
	if (!win) return;
	win->on_uia_provider  = cb;
	win->uia_provider_ctx = ctx;
}

void flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx) {
	if (!win) return;
	win->on_setting_changed  = cb;
	win->setting_changed_ctx = ctx;
}

void flux_window_set_ime_composition_callback(FluxWindow *win, FluxImeCompositionCallback cb, void *ctx) {
	if (!win) return;
	win->on_ime_composition  = cb;
	win->ime_composition_ctx = ctx;
}

void flux_window_set_dark_mode(FluxWindow *win, bool enabled) {
	if (!win || !win->hwnd) return;
	BOOL dark = enabled ? TRUE : FALSE;
	DwmSetWindowAttribute(win->hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
	win->config.dark_mode = enabled;
}

void flux_window_set_title(FluxWindow *win, wchar_t const *title) {
	if (!win || !win->hwnd || !title) return;
	SetWindowTextW(win->hwnd, title);
}

void flux_window_request_render(FluxWindow *win) {
	if (win) win->render_requested = true;
}

void flux_window_abandon_pointer(FluxWindow *win, uint32_t pointer_id) {
	if (!win) return;
	if (win->primary_touch_pid == pointer_id) win->primary_touch_pid = 0;
	if (win->hwnd && GetCapture() == win->hwnd) ReleaseCapture();
}

void flux_window_set_active_menu(FluxWindow *win, FluxMenuFlyout *menu) {
	if (win) win->active_menu = menu;
}

FluxMenuFlyout *flux_window_get_active_menu(FluxWindow *win) { return win ? win->active_menu : NULL; }
