#ifndef FLUXENT_FLUX_APP_H
#define FLUXENT_FLUX_APP_H

#include <assert.h>

#include "flux_theme.h"

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
	FluxThemeMode    theme;
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

/** Borrows `ctx` until this callback is replaced or the app is destroyed. */
void                    flux_app_onframe(FluxApp *app, void (*cb)(void *ctx), void *ctx);

FluxGfx                *flux_app_gfx(FluxApp *app);

FluxMenu               *flux_app_menu(FluxApp *app);

HWND                    flux_app_hwnd(FluxApp *app);

/** Window thread only. Applied after the next layout; falls back to the first focusable descendant in tab order. */
bool                    flux_app_setfocus(FluxApp *app, XtkCtrl control);
/** The control that has keyboard focus, or `XTK_CTRL_INVALID`. */
XtkCtrl                 flux_app_focus(FluxApp *app);

/** Borrows `ctx` until `fn` runs on the window thread. */
bool                    flux_app_invoke(FluxApp *app, void (*fn)(void *ctx), void *ctx);
XtkPostStatus           flux_app_post(FluxApp *app, XtkMsg msg);
/** Copies `data` and delivers it on the window thread. The caller may free `data`. `len > 0` with NULL `data` is rejected. */
XtkPostStatus           flux_app_poststring(FluxApp *app, XtkMsg msg, char const *data, uint32_t len);

typedef enum FluxAutomationEvent
{
	FLUX_AUTOMATION_TOOLTIP_OPENED              = 0,
	FLUX_AUTOMATION_TOOLTIP_CLOSED              = 1,
	FLUX_AUTOMATION_MENU_OPENED                 = 2,
	FLUX_AUTOMATION_MENU_CLOSED                 = 3,
	FLUX_AUTOMATION_FOCUS_CHANGED               = 4,
	FLUX_AUTOMATION_INVOKED                     = 5,
	FLUX_AUTOMATION_ADDED_TO_SELECTION          = 6,
	FLUX_AUTOMATION_REMOVED_FROM_SELECTION      = 7,
	FLUX_AUTOMATION_ELEMENT_SELECTED            = 8,
	FLUX_AUTOMATION_SELECTION_INVALIDATED       = 9,
	FLUX_AUTOMATION_TEXT_SELECTION_CHANGED      = 10,
	FLUX_AUTOMATION_TEXT_CHANGED                = 11,
	FLUX_AUTOMATION_ASYNC_CONTENT_LOADED        = 12,
	FLUX_AUTOMATION_PROPERTY_CHANGED            = 13,
	FLUX_AUTOMATION_STRUCTURE_CHANGED           = 14,
	FLUX_AUTOMATION_DRAG_START                  = 15,
	FLUX_AUTOMATION_DRAG_CANCEL                 = 16,
	FLUX_AUTOMATION_DRAG_COMPLETE               = 17,
	FLUX_AUTOMATION_DRAG_ENTER                  = 18,
	FLUX_AUTOMATION_DRAG_LEAVE                  = 19,
	FLUX_AUTOMATION_DROPPED                     = 20,
	FLUX_AUTOMATION_LIVE_REGION_CHANGED         = 21,
	FLUX_AUTOMATION_INPUT_REACHED_TARGET        = 22,
	FLUX_AUTOMATION_INPUT_REACHED_OTHER_ELEMENT = 23,
	FLUX_AUTOMATION_INPUT_DISCARDED             = 24,
	FLUX_AUTOMATION_WINDOW_CLOSED               = 25,
	FLUX_AUTOMATION_WINDOW_OPENED               = 26,
	FLUX_AUTOMATION_CONVERSION_TARGET_CHANGED   = 27,
	FLUX_AUTOMATION_TEXT_EDIT_TEXT_CHANGED      = 28,
	FLUX_AUTOMATION_LAYOUT_INVALIDATED          = 29,
	FLUX_AUTOMATION_NOTIFICATION                = 30,
} FluxAutomationEvent;

typedef enum FluxNotificationKind
{
	FLUX_NOTIFICATION_ITEM_ADDED       = 0,
	FLUX_NOTIFICATION_ITEM_REMOVED     = 1,
	FLUX_NOTIFICATION_ACTION_COMPLETED = 2,
	FLUX_NOTIFICATION_ACTION_ABORTED   = 3,
	FLUX_NOTIFICATION_OTHER            = 4,
} FluxNotificationKind;

typedef enum FluxNotificationProcessing
{
	FLUX_NOTIFICATION_IMPORTANT_ALL            = 0,
	FLUX_NOTIFICATION_IMPORTANT_MOST_RECENT    = 1,
	FLUX_NOTIFICATION_ALL                      = 2,
	FLUX_NOTIFICATION_MOST_RECENT              = 3,
	FLUX_NOTIFICATION_CURRENT_THEN_MOST_RECENT = 4,
} FluxNotificationProcessing;

bool flux_app_raiseevent(FluxApp *app, XtkCtrl control, FluxAutomationEvent event);
bool flux_app_raisenotification(
  FluxApp *app, XtkCtrl control, FluxNotificationKind kind, FluxNotificationProcessing processing, char const *text,
  char const *activity
);

XtkRt *flux_app_createruntime(FluxApp *app, void *model, XtkUpdateFn update, XtkViewFn view);

int    flux_run(FluxAppCfg const *cfg, void *model, XtkUpdateFn update, XtkViewFn view);

static_assert(sizeof(FluxBackdropType) == 4, "FluxBackdropType crosses the ABI as int32");

#endif
