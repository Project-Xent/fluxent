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
typedef void              (*FluxPointerFn)(void *ctx, FluxPointerEvent const *ev);
typedef void              (*FluxKeyFn)(void *ctx, FluxKey key, bool down);
typedef void              (*FluxCharFn)(void *ctx, wchar_t ch);
typedef void              (*FluxImeCompositionFn)(void *ctx, wchar_t const *text, int cursor_pos);
typedef void              (*FluxSettingChangedFn)(void *ctx);
typedef void             *(*FluxUiaProviderFn)(void *ctx, void *hwnd);

typedef struct FluxWindowCfg {
	wchar_t const *title;
	int            width;
	int            height;
	bool           dark_mode;
	int            backdrop;
	bool           resizable;
} FluxWindowCfg;

XENT_NODISCARD HRESULT flux_window_create(FluxWindowCfg const *cfg, FluxWindow **out);
void                   flux_window_destroy(FluxWindow *win);

HWND                   flux_window_hwnd(FluxWindow const *win);
FluxSize               flux_window_size(FluxWindow const *win);
FluxDpi                flux_window_dpi(FluxWindow const *win);
FluxGfx               *flux_window_gfx(FluxWindow *win);

void                   flux_window_onrender(FluxWindow *win, FluxRenderFn cb, void *ctx);
void                   flux_window_onresize(FluxWindow *win, FluxResizeFn cb, void *ctx);
void                   flux_window_addresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx);
void                   flux_window_delresizeobs(FluxWindow *win, FluxResizeFn cb, void *ctx);
void                   flux_window_ondpi(FluxWindow *win, FluxDpiChangedFn cb, void *ctx);
void                   flux_window_onpointer(FluxWindow *win, FluxPointerFn cb, void *ctx);
void                   flux_window_onkey(FluxWindow *win, FluxKeyFn cb, void *ctx);
void                   flux_window_onchar(FluxWindow *win, FluxCharFn cb, void *ctx);
void                   flux_window_onime(FluxWindow *win, FluxImeCompositionFn cb, void *ctx);
void                   flux_window_onsettings(FluxWindow *win, FluxSettingChangedFn cb, void *ctx);
void                   flux_window_onuia(FluxWindow *win, FluxUiaProviderFn cb, void *ctx);

void                   flux_window_setbackdrop(FluxWindow *win, int backdrop_type);
int                    flux_window_backdrop(FluxWindow const *win);
void                   flux_window_setdark(FluxWindow *win, bool enabled);
void                   flux_window_settitle(FluxWindow *win, wchar_t const *title);

#define FLUX_WINDOW_MAX_TITLE_BAR_PASS 8

void      flux_window_settitlebar(FluxWindow *win, FluxRect drag, FluxRect const *passthrough, int count);

void      flux_window_extendtitlebar(FluxWindow *win, bool enabled);

bool      flux_window_titlebar(FluxWindow const *win);

void      flux_window_setimepos(FluxWindow *win, int x, int y, int height);
void      flux_window_setcursor(FluxWindow *win, int cursor_type);

void      flux_window_abandonpointer(FluxWindow *win, uint32_t pointer_id);
void      flux_window_setmenu(FluxWindow *win, FluxMenu *menu);
FluxMenu *flux_window_menu(FluxWindow *win);

#define FLUX_CURSOR_ARROW 0
#define FLUX_CURSOR_IBEAM 1
#define FLUX_CURSOR_HAND  2
void flux_window_redraw(FluxWindow *win);
int  flux_window_run(FluxWindow *win);
void flux_window_close(FluxWindow *win);

#endif
