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

typedef struct FluxWindow     FluxWindow;
typedef struct FluxGraphics   FluxGraphics;
typedef struct FluxMenuFlyout FluxMenuFlyout;

typedef void                  (*FluxRenderCallback)(void *ctx);
typedef void                  (*FluxResizeCallback)(void *ctx, int width, int height);
typedef void                  (*FluxDpiChangedCallback)(void *ctx, FluxDpiInfo dpi);
/** @brief Unified pointer callback for down, move, up, wheel, context menu, and cancel events. */
typedef void                  (*FluxPointerCallback)(void *ctx, FluxPointerEvent const *ev);
typedef void                  (*FluxKeyCallback)(void *ctx, unsigned int vk, bool down);
typedef void                  (*FluxCharCallback)(void *ctx, wchar_t ch);
typedef void                  (*FluxImeCompositionCallback)(void *ctx, wchar_t const *text, int cursor_pos);
typedef void                  (*FluxSettingChangedCallback)(void *ctx);

typedef struct FluxWindowConfig {
	wchar_t const *title;
	int            width;
	int            height;
	bool           dark_mode;
	int            backdrop;
	bool           resizable;
} FluxWindowConfig;

/** @brief Create a window from the provided configuration. */
HRESULT         flux_window_create(FluxWindowConfig const *cfg, FluxWindow **out);
/** @brief Destroy a window and release its resources. */
void            flux_window_destroy(FluxWindow *win);

/** @brief Get the native HWND. */
HWND            flux_window_hwnd(FluxWindow const *win);
/** @brief Get the window client size in pixels. */
FluxSize        flux_window_client_size(FluxWindow const *win);
/** @brief Get the current window DPI. */
FluxDpiInfo     flux_window_dpi(FluxWindow const *win);
/** @brief Get the graphics context attached to the window. */
FluxGraphics   *flux_window_get_graphics(FluxWindow *win);

/** @brief Set the render callback. */
void            flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx);
/** @brief Set the resize callback. */
void            flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx);
/** @brief Set the DPI changed callback. */
void            flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx);
/** @brief Set the unified pointer callback. */
void            flux_window_set_pointer_callback(FluxWindow *win, FluxPointerCallback cb, void *ctx);
/** @brief Set the key callback. */
void            flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx);
/** @brief Set the character input callback. */
void            flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx);
/** @brief Set the IME composition callback. */
void            flux_window_set_ime_composition_callback(FluxWindow *win, FluxImeCompositionCallback cb, void *ctx);
/** @brief Set the settings/theme changed callback. */
void            flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx);

/** @brief Set the DWM backdrop type. */
void            flux_window_set_backdrop(FluxWindow *win, int backdrop_type);
/** @brief Set whether the window uses dark-mode DWM attributes. */
void            flux_window_set_dark_mode(FluxWindow *win, bool enabled);
/** @brief Set the window title. */
void            flux_window_set_title(FluxWindow *win, wchar_t const *title);

/** @brief Position the IME composition window. */
void            flux_window_set_ime_position(FluxWindow *win, int x, int y, int height);
/** @brief Set the cursor type used over the client area. */
void            flux_window_set_cursor(FluxWindow *win, int cursor_type);

/** @brief Abandon tracking of a touch pointer after it has been handed to another subsystem. */
void            flux_window_abandon_pointer(FluxWindow *win, uint32_t pointer_id);
/** @brief Set the active menu leaf for this window. */
void            flux_window_set_active_menu(FluxWindow *win, FluxMenuFlyout *menu);
/** @brief Get the active menu leaf for this window. */
FluxMenuFlyout *flux_window_get_active_menu(FluxWindow *win);

#define FLUX_CURSOR_ARROW 0
#define FLUX_CURSOR_IBEAM 1
#define FLUX_CURSOR_HAND  2
/** @brief Request rendering on the next frame. */
void flux_window_request_render(FluxWindow *win);
/** @brief Run the window message/render loop. */
int  flux_window_run(FluxWindow *win);
/** @brief Close the window. */
void flux_window_close(FluxWindow *win);

#ifdef __cplusplus
}
#endif

#endif
