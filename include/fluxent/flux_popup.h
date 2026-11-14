#ifndef FLUX_POPUP_H
#define FLUX_POPUP_H

#include <assert.h>

#include "flux_types.h"
#include "flux_window.h"
#include <stdbool.h>
#include "flux_gfx.h"
#include "flux_node_store.h"
#include "flux_input.h"
#include <xent/xent.h>
#include <xent/xent_display.h>

typedef enum FluxPlacement
{
	FLUX_PLACEMENT_BOTTOM,
	FLUX_PLACEMENT_TOP,
	FLUX_PLACEMENT_LEFT,
	FLUX_PLACEMENT_RIGHT,
	FLUX_PLACEMENT_AUTO,
} FluxPlacement;

typedef struct FluxPopup  FluxPopup;
typedef struct FluxText   FluxText;

XENT_NODISCARD FluxPopup *flux_popup_create(FluxWindow *owner);

void                      flux_popup_destroy(FluxPopup *popup);

bool flux_popup_setcontent(FluxPopup *popup, XentCtx *ctx, FluxStore *store, XentNodeId content_root);

void flux_popup_show(FluxPopup *popup, FluxRect anchor, FluxPlacement placement);

typedef enum FluxPopupAnimStyle
{
	FLUX_POPUP_ANIM_MENU,
	FLUX_POPUP_ANIM_FLYOUT,
	FLUX_POPUP_ANIM_TOOLTIP,
	FLUX_POPUP_ANIM_NONE,
	FLUX_POPUP_ANIM_SPLIT,
	FLUX_POPUP_ANIM_PICKER,
} FluxPopupAnimStyle;

void flux_popup_setanim(FluxPopup *popup, FluxPopupAnimStyle style);

typedef struct FluxPopupMenuTransition {
	bool  active;
	float border_scale_y;
	float border_center_y;
	float content_translate_y;
	float clip_translate_y;
} FluxPopupMenuTransition;

FluxPopupMenuTransition flux_popup_menutransition(FluxPopup const *popup, float opened_length);

void                    flux_popup_dismiss(FluxPopup *popup);

void                    flux_popup_reposition(FluxPopup *popup);

bool                    flux_popup_visible(FluxPopup const *popup);

void                    flux_popup_setsize(FluxPopup *popup, float width, float height);

void                    flux_popup_setdismissoutside(FluxPopup *popup, bool dismiss);

typedef void            (*FluxPopupDismissFn)(void *ctx);
void                    flux_popup_ondismiss(FluxPopup *popup, FluxPopupDismissFn cb, void *ctx);

typedef void            (*FluxPopupPaintFn)(void *ctx, FluxPopup *popup);
void                    flux_popup_onpaint(FluxPopup *popup, FluxPopupPaintFn cb, void *ctx);

typedef XentDl         *(*FluxPopupRecordFn)(void *ctx, FluxPopup const *popup);
void                    flux_popup_onrecord(FluxPopup *popup, FluxPopupRecordFn cb, void *ctx, FluxText *text);

typedef enum FluxPopupMouseEvent
{
	FLUX_POPUP_MOUSE_MOVE,
	FLUX_POPUP_MOUSE_DOWN,
	FLUX_POPUP_MOUSE_UP,
	FLUX_POPUP_MOUSE_LEAVE,
	FLUX_POPUP_MOUSE_WHEEL,
} FluxPopupMouseEvent;

typedef void (*FluxPopupMouseFn)(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y);
void         flux_popup_onmouse(FluxPopup *popup, FluxPopupMouseFn cb, void *ctx);

FluxGfx     *flux_popup_gfx(FluxPopup *popup);

HWND         flux_popup_hwnd(FluxPopup *popup);

bool         flux_popup_lasttouch(FluxPopup const *popup);

bool         flux_popup_acrylicactive(FluxPopup const *popup);

FluxColor    flux_popup_acrylictint(FluxPopup const *popup, FluxColor fill);

void         flux_popup_setmaxh(FluxPopup *popup, float max_h);
void         flux_popup_setshadow(FluxPopup *popup, bool shadow, float corner);
FluxPoint    flux_popup_contentorigin(FluxPopup const *popup);
void         flux_popup_setsplit(FluxPopup *popup, float closed_length, float offset_from_center);

void         flux_popup_dismissowner(HWND owner_hwnd);

bool         flux_popup_lightdismissvisible(HWND owner_hwnd);

static_assert(sizeof(FluxPlacement) == 4, "FluxPlacement crosses the ABI as int32");
static_assert(sizeof(FluxPopupAnimStyle) == 4, "FluxPopupAnimStyle crosses the ABI as int32");
static_assert(sizeof(FluxPopupMouseEvent) == 4, "FluxPopupMouseEvent crosses the ABI as int32");

#endif
