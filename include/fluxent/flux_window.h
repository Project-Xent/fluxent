#ifndef FLUX_WINDOW_H
#define FLUX_WINDOW_H

#include "flux_types.h"

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxWindow FluxWindow;
typedef struct FluxGraphics FluxGraphics;

typedef void (*FluxRenderCallback)(void *ctx);
typedef void (*FluxResizeCallback)(void *ctx, int width, int height);
typedef void (*FluxDpiChangedCallback)(void *ctx, FluxDpiInfo dpi);
typedef void (*FluxMouseCallback)(void *ctx, float x, float y, int button, bool down);
typedef void (*FluxKeyCallback)(void *ctx, unsigned int vk, bool down);
typedef void (*FluxCharCallback)(void *ctx, wchar_t ch);
typedef void (*FluxSettingChangedCallback)(void *ctx);

typedef struct FluxWindowConfig {
    const wchar_t *title;
    int            width;
    int            height;
    bool           dark_mode;
    int            backdrop;
    bool           resizable;
} FluxWindowConfig;

HRESULT     flux_window_create(const FluxWindowConfig *cfg, FluxWindow **out);
void        flux_window_destroy(FluxWindow *win);

HWND        flux_window_hwnd(const FluxWindow *win);
FluxSize    flux_window_client_size(const FluxWindow *win);
FluxDpiInfo flux_window_dpi(const FluxWindow *win);
FluxGraphics *flux_window_get_graphics(FluxWindow *win);

void        flux_window_set_render_callback(FluxWindow *win, FluxRenderCallback cb, void *ctx);
void        flux_window_set_resize_callback(FluxWindow *win, FluxResizeCallback cb, void *ctx);
void        flux_window_set_dpi_changed_callback(FluxWindow *win, FluxDpiChangedCallback cb, void *ctx);
void        flux_window_set_mouse_callback(FluxWindow *win, FluxMouseCallback cb, void *ctx);
void        flux_window_set_key_callback(FluxWindow *win, FluxKeyCallback cb, void *ctx);
void        flux_window_set_char_callback(FluxWindow *win, FluxCharCallback cb, void *ctx);
void        flux_window_set_setting_changed_callback(FluxWindow *win, FluxSettingChangedCallback cb, void *ctx);

void        flux_window_set_backdrop(FluxWindow *win, int backdrop_type);
void        flux_window_set_dark_mode(FluxWindow *win, bool enabled);
void        flux_window_set_title(FluxWindow *win, const wchar_t *title);

void        flux_window_request_render(FluxWindow *win);
int         flux_window_run(FluxWindow *win);
void        flux_window_close(FluxWindow *win);

#ifdef __cplusplus
}
#endif

#endif