/**
 * @file flux_window.h
 * @brief Window creation, rendering, and event handling.
 *
 * FluxWindow wraps a Win32 HWND with Direct2D rendering and provides
 * a callback-based interface for input events, resize, and DPI changes.
 */

#ifndef FLUX_WINDOW_H
#define FLUX_WINDOW_H

#include "flux_types.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxWindow   FluxWindow;
typedef struct FluxGraphics FluxGraphics;

typedef void                (*FluxRenderCallback)(void *ctx);
typedef void                (*FluxResizeCallback)(void *ctx, int width, int height);
typedef void                (*FluxDpiChangedCallback)(void *ctx, FluxDpiInfo dpi);
/* Unified pointer callback.  One entry point for down/move/up/wheel/
 * context-menu/cancel — the event kind is inside the struct.  The
 * window layer is a pure translation of Win32 messages; policy (focus,
 * capture semantics, gesture recognition, dismissal) lives upstream. */
typedef void                (*FluxPointerCallback)(void *ctx, FluxPointerEvent const *ev);
typedef void                (*FluxKeyCallback)(void *ctx, unsigned int vk, bool down);
typedef void                (*FluxCharCallback)(void *ctx, wchar_t ch);
typedef void                (*FluxImeCompositionCallback)(void *ctx, wchar_t const *text, int cursor_pos);
typedef void                (*FluxSettingChangedCallback)(void *ctx);

typedef struct FluxWindowConfig {
	wchar_t const *title;
	int            width;
	int            height;
	bool           dark_mode;
	int            backdrop;
	bool           resizable;
} FluxWindowConfig;

HRESULT       flux_window_create(FluxWindowConfig const *cfg, FluxWindow **out);
void          flux_window_destroy(FluxWindow *win);

HWND          flux_window_hwnd(FluxWindow const *win);
FluxSize      flux_window_client_size(FluxWindow const *win);
FluxDpiInfo   flux_window_dpi(FluxWindow const *win);
FluxGraphics *flux_window_get_graphics(FluxWindow *win);

void          flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx);
void          flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx);
void          flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx);
void          flux_window_set_pointer_callback(FluxWindow *win, FluxPointerCallback cb, void *ctx);
void          flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx);
void          flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx);
void          flux_window_set_ime_composition_callback(FluxWindow *win, FluxImeCompositionCallback cb, void *ctx);
void          flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx);

void          flux_window_set_backdrop(FluxWindow *win, int backdrop_type);
void          flux_window_set_dark_mode(FluxWindow *win, bool enabled);
void          flux_window_set_title(FluxWindow *win, wchar_t const *title);

void          flux_window_set_ime_position(FluxWindow *win, int x, int y, int height);
void          flux_window_set_cursor(FluxWindow *win, int cursor_type);

/* Abandon tracking of a touch pointer whose contact has been handed off
 * to another subsystem (e.g. DirectManipulation's SetContact).  Clears
 * the primary-touch latch and releases Win32 capture so the next touch
 * contact \u2014 which will carry a new pointer id \u2014 is accepted normally.
 * No-op for non-touch pointers or unmatched ids. */
void          flux_window_abandon_pointer(FluxWindow *win, uint32_t pointer_id);

#define FLUX_CURSOR_ARROW 0
#define FLUX_CURSOR_IBEAM 1
#define FLUX_CURSOR_HAND  2
void flux_window_request_render(FluxWindow *win);
int  flux_window_run(FluxWindow *win);
void flux_window_close(FluxWindow *win);

#ifdef __cplusplus
}
#endif

#endif
