/**
 * @file flux_app.h
 * @brief Fluxent application API: FluxApp / FluxWindow / FluxInput, the app
 * config, the flux_app_* lifecycle and accessors, plus the flux_run() XTK
 * bridge entry point.
 */
#ifndef FLUXENT_FLUX_APP_H
#define FLUXENT_FLUX_APP_H

#include <xtk/xtk.h>

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
} FluxAppCfg;

/** @brief Create a Fluxent application, or NULL on failure. */
XENT_NODISCARD FluxApp *flux_app_create(FluxAppCfg const *cfg);
/** @brief Destroy a Fluxent application. */
void                    flux_app_destroy(FluxApp *app);
/** @brief Set the layout root and node store rendered by the app. */
void                    flux_app_setroot(FluxApp *app, XentCtx *ctx, XentNodeId root, FluxStore *store);
/** @brief Run the application message/render loop. */
int                     flux_app_run(FluxApp *app);

/** @brief Get the app render engine. */
FluxEngine             *flux_app_engine(FluxApp *app);
/** @brief Get the app window. */
FluxWindow             *flux_app_window(FluxApp *app);
/** @brief Get the app input router. */
FluxInput              *flux_app_input(FluxApp *app);
/** @brief Get the app theme manager. */
FluxTheme              *flux_app_theme(FluxApp *app);
/** @brief Get the app tooltip manager. */
FluxTooltip            *flux_app_tooltip(FluxApp *app);
/** @brief Get the app text renderer. */
FluxText               *flux_app_text(FluxApp *app);
/** @brief Get the app layout context. */
XentCtx                *flux_app_ctx(FluxApp *app);
/** @brief Get the app node store. */
FluxStore              *flux_app_store(FluxApp *app);
/** @brief Get the app content root node (where the reconciled view mounts). */
XentNodeId              flux_app_root(FluxApp *app);
/** @brief Get the app overlay layer node (modal surfaces: dialogs, flyouts). */
XentNodeId              flux_app_overlay(FluxApp *app);

/** @brief Request rendering for the next compositor frame on the app window. */
void                    flux_app_redraw(FluxApp *app);

/**
 * @brief Install a per-frame callback that runs at frame start, after input
 * dispatch and before layout/paint. The xtk runtime uses this to drain its
 * message queue and reconcile the view. NULL clears it.
 */
void                    flux_app_onframe(FluxApp *app, void (*cb)(void *ctx), void *ctx);

/** @brief Get the Direct2D-backed graphics context owned by the app's window. */
FluxGfx                *flux_app_gfx(FluxApp *app);

/** @brief Deepest visible MenuFlyout leaf for the app's window, if any. */
FluxMenu               *flux_app_menu(FluxApp *app);

/** @brief Native HWND for the application's primary window. */
HWND                    flux_app_hwnd(FluxApp *app);

/**
 * @brief Create the app window and drive update/view until it closes.
 *
 * Each frame: queued control messages are drained through @p update, then
 * (only if anything changed) @p view rebuilds the element tree and the
 * reconciler applies the diff to retained controls. Returns the app's exit
 * code.
 */
int                     flux_run(FluxAppCfg const *cfg, void *model, XtkUpdateFn update, XtkViewFn view);


#endif
