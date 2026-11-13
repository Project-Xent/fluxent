#ifndef FLUXENT_FLUX_APP_H
#define FLUXENT_FLUX_APP_H

#include <xtk/xtk.h>
#include <xtk/xtk_reconcile.h>

#include <stdbool.h>
#include <stddef.h>

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <windows.h>

typedef struct FluxApp     FluxApp;
typedef struct FluxWindow  FluxWindow;
typedef struct FluxInput   FluxInput;
typedef struct FluxEngine  FluxEngine;
typedef struct FluxTheme   FluxTheme;
typedef struct FluxTooltip FluxTooltip;
typedef struct FluxText    FluxText;
typedef struct FluxStore   FluxStore;
typedef struct FluxGfx     FluxGfx;
typedef struct FluxMenu    FluxMenu;

typedef enum FluxBackdropType
{
	FLUX_BACKDROP_NONE = 0,
	FLUX_BACKDROP_MICA,
	FLUX_BACKDROP_MICA_ALT,
	FLUX_BACKDROP_ACRYLIC,
} FluxBackdropType;

typedef struct FluxAppCfg {
	wchar_t const   *title;
	int              width;
	int              height;
	bool             dark_mode;
	FluxBackdropType backdrop;
	void             (*on_start)(FluxApp *app, void *user);
	void             (*on_stop)(FluxApp *app, void *user);
	void            *user;
} FluxAppCfg;

XENT_NODISCARD FluxApp *flux_app_create(FluxAppCfg const *cfg);
void                    flux_app_destroy(FluxApp *app);
void                    flux_app_setroot(FluxApp *app, XentCtx *ctx, XentNodeId root, FluxStore *store);
int                     flux_app_run(FluxApp *app);

FluxEngine             *flux_app_engine(FluxApp *app);
FluxWindow             *flux_app_window(FluxApp *app);
FluxInput              *flux_app_input(FluxApp *app);
FluxTheme              *flux_app_theme(FluxApp *app);
FluxTooltip            *flux_app_tooltip(FluxApp *app);
FluxText               *flux_app_text(FluxApp *app);
XentCtx                *flux_app_ctx(FluxApp *app);
FluxStore              *flux_app_store(FluxApp *app);
XentNodeId              flux_app_root(FluxApp *app);
XentNodeId              flux_app_overlay(FluxApp *app);

void                    flux_app_redraw(FluxApp *app);

void                    flux_app_onframe(FluxApp *app, void (*cb)(void *ctx), void *ctx);

FluxGfx                *flux_app_gfx(FluxApp *app);

FluxMenu               *flux_app_menu(FluxApp *app);

HWND                    flux_app_hwnd(FluxApp *app);

XtkPostStatus           flux_app_post(FluxApp *app, XtkMsg msg);
XtkPostStatus           flux_app_poststring(FluxApp *app, XtkMsg msg, char const *data, uint32_t len);

XtkRt                  *flux_app_createruntime(FluxApp *app, void *model, XtkUpdateFn update, XtkViewFn view);

int                     flux_run(FluxAppCfg const *cfg, void *model, XtkUpdateFn update, XtkViewFn view);

#endif
