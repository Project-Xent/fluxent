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


typedef struct FluxWindow FluxWindow;
typedef struct FluxGfx    FluxGfx;
typedef struct FluxMenu   FluxMenu;

typedef void              (*FluxRenderFn)(void *ctx);
typedef void              (*FluxResizeFn)(void *ctx, int width, int height);
typedef void              (*FluxDpiChangedFn)(void *ctx, FluxDpi dpi);
/** @brief Unified pointer callback for contact, hold, wheel, context-menu, and cancel events. */
typedef void              (*FluxPointerFn)(void *ctx, FluxPointerEvent const *ev);
typedef void              (*FluxKeyFn)(void *ctx, FluxKey key, bool down);
typedef void              (*FluxCharFn)(void *ctx, wchar_t ch);
typedef void              (*FluxImeCompositionFn)(void *ctx, wchar_t const *text, int cursor_pos);
typedef void              (*FluxSettingChangedFn)(void *ctx);
/** @brief Returns a UIA root provider (IRawElementProviderSimple* as void*) for WM_GETOBJECT;
 *  the caller releases it. Return NULL to fall back to default handling. */
typedef void             *(*FluxUiaProviderFn)(void *ctx, void *hwnd);

typedef struct FluxWindowCfg {
	wchar_t const *title;
	int            width;
	int            height;
	bool           dark_mode;
	int            backdrop;
	bool           resizable;
} FluxWindowCfg;

/** @brief Create a window from the provided configuration. */
XENT_NODISCARD HRESULT flux_window_create(FluxWindowCfg const *cfg, FluxWindow **out);
/** @brief Destroy a window and release its resources. */
void                   flux_window_destroy(FluxWindow *win);

/** @brief Get the native HWND. */
HWND                   flux_window_hwnd(FluxWindow const *win);
/** @brief Get the window client size in pixels. */
FluxSize               flux_window_size(FluxWindow const *win);
/** @brief Get the current window DPI. */
FluxDpi                flux_window_dpi(FluxWindow const *win);
/** @brief Get the graphics context attached to the window. */
FluxGfx               *flux_window_gfx(FluxWindow *win);

/** @brief Set the render callback. */
void                   flux_window_onrender(FluxWindow *win, FluxRenderFn cb, void *ctx);
/** @brief Set the resize callback. */
void                   flux_window_onresize(FluxWindow *win, FluxResizeFn cb, void *ctx);
/**
 * @brief Register an additional resize observer, invoked after the primary callback.
 * Additive (does not replace the primary callback); lets self-contained controls such
 * as NavigationView react to client-size changes. Remove before the @p ctx is freed.
 */
void                   flux_window_addresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx);
/** @brief Remove a previously added resize observer (matched by cb + ctx). */
void                   flux_window_delresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx);
/** @brief Set the DPI changed callback. */
void                   flux_window_ondpi(FluxWindow *win, FluxDpiChangedFn cb, void *ctx);
/** @brief Set the unified pointer callback. */
void                   flux_window_onpointer(FluxWindow *win, FluxPointerFn cb, void *ctx);
/** @brief Set the key callback. */
void                   flux_window_onkey(FluxWindow *win, FluxKeyFn cb, void *ctx);
/** @brief Set the character input callback. */
void                   flux_window_onchar(FluxWindow *win, FluxCharFn cb, void *ctx);
/** @brief Set the IME composition callback. */
void                   flux_window_onime(FluxWindow *win, FluxImeCompositionFn cb, void *ctx);
/** @brief Set the settings/theme changed callback. */
void                   flux_window_onsettings(FluxWindow *win, FluxSettingChangedFn cb, void *ctx);
/** @brief Set the UIA root-provider callback (answers WM_GETOBJECT for accessibility). */
void                   flux_window_onuia(FluxWindow *win, FluxUiaProviderFn cb, void *ctx);

/** @brief Set the DWM backdrop type. */
void                   flux_window_setbackdrop(FluxWindow *win, int backdrop_type);
/** @brief Current DWM backdrop type (FluxBackdropType; 0 = none). */
int                    flux_window_backdrop(FluxWindow const *win);
/** @brief Set whether the window uses dark-mode DWM attributes. */
void                   flux_window_setdark(FluxWindow *win, bool enabled);
/** @brief Set the window title. */
void                   flux_window_settitle(FluxWindow *win, wchar_t const *title);

/** @brief Maximum interactive (passthrough) rects a custom title bar may register. */
#define FLUX_WINDOW_MAX_TITLE_BAR_PASS 8

/**
 * @brief Register a custom title-bar drag region and its interactive passthrough
 * rects (all in client-local DIPs). Points inside @p drag hit-test as caption
 * (the window drags), except points inside a passthrough rect, which stay client
 * (buttons remain clickable). Pass @p drag with zero width to clear.
 */
void      flux_window_settitlebar(FluxWindow *win, FluxRect drag, FluxRect const *passthrough, int count);

/**
 * @brief Extend the client area over the OS title bar (removes the system
 * caption; the app draws its own title bar and caption buttons). Resize borders,
 * DWM shadow, snap, and maximize are preserved. Idempotent.
 */
void      flux_window_extendtitlebar(FluxWindow *win, bool enabled);

/** @brief True while the client area covers the (removed) OS title bar. */
bool      flux_window_titlebar(FluxWindow const *win);

/** @brief Position the IME composition window. */
void      flux_window_setimepos(FluxWindow *win, int x, int y, int height);
/** @brief Set the cursor type used over the client area. */
void      flux_window_setcursor(FluxWindow *win, int cursor_type);

/** @brief Abandon tracking of a touch pointer after it has been handed to another subsystem. */
void      flux_window_abandonpointer(FluxWindow *win, uint32_t pointer_id);
/** @brief Set the active menu leaf for this window. */
void      flux_window_setmenu(FluxWindow *win, FluxMenu *menu);
/** @brief Get the active menu leaf for this window. */
FluxMenu *flux_window_menu(FluxWindow *win);

#define FLUX_CURSOR_ARROW 0
#define FLUX_CURSOR_IBEAM 1
#define FLUX_CURSOR_HAND  2
/** @brief Request rendering on the next frame. */
void flux_window_redraw(FluxWindow *win);
/** @brief Run the window message/render loop. */
int  flux_window_run(FluxWindow *win);
/** @brief Close the window. */
void flux_window_close(FluxWindow *win);


#endif
