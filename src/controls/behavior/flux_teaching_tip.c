#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "render/flux_fluent.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/data/flux_teaching_tip_data.h"
#include "controls/draw/flux_teachtip_internal.h"
#include "controls/factory/flux_init_popup.h"
#include "render/flux_icon.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TIP_CARD_W       320.0f
#define TIP_MIN_H        40.0f
#define TIP_MAX_H        520.0f
#define TIP_PAD          12.0f
#define TIP_TAIL_BAND    8.0f
#define TIP_ICON_SIZE    16.0f
#define TIP_ICON_GAP     12.0f
#define TIP_X_SIZE       40.0f
#define TIP_X_RESERVE    28.0f
#define TIP_BTN_H        32.0f
#define TIP_BTN_GUTTER   4.0f
#define TIP_BTN_TOP      12.0f
#define TIP_EDGE_MARGIN  24.0f
#define TIP_TAIL_CENTER  28.0f
#define TIP_SCALE_CORNER 19.0f
#define TIP_EXPAND_MS    300.0f
#define TIP_CONTRACT_MS  200.0f

typedef struct FluxTipRuntime {
	FluxTeachingTipData        model;
	FluxStore                 *store;
	XentNodeId                 node;
	XentNodeId                 target;
	XentCtx                   *ctx;
	FluxWindow                *window;
	FluxText                  *text;
	FluxTheme                 *theme;
	FluxInput                 *input;
	FluxPopup                 *popup;

	float                      tip_w, tip_h;
	uint8_t                    tail_edge;
	float                      tail_center;
	float                      scale_cx, scale_cy;
	FluxRect                   action_rect;
	FluxRect                   close_rect;
	FluxRect                   x_rect;
	int                        hot_zone;
	int                        pressed_zone;

	bool                       anim_expand;
	bool                       anim_contract;
	DWORD                      anim_start;
	float                      from_sx, from_sy;
	float                      to_sx, to_sy;
	float                      cur_sx, cur_sy;
	bool                       pending_hide;
	FluxTeachingTipCloseReason close_reason;
	FluxTeachingTipCloseReason pending_reason;

	XentNodeId                 prev_focus;
} FluxTipRuntime;

static void            tip_open(FluxTipRuntime *rt);
static void            tip_close(FluxTipRuntime *rt, FluxTeachingTipCloseReason reason);

static FluxTipRuntime *tip_runtime(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TEACHING_TIP || !nd->component_data) return NULL;
	return ( FluxTipRuntime * ) nd->component_data;
}

static void tip_route(FluxTipRuntime *rt, XtkEventId id, XtkEventPayload payload, XtkEventCause cause) {
	XtkHost *host = flux_store_host(rt->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) rt->node,
	  .source         = ( XtkCtrl ) rt->node,
	  .current_target = ( XtkCtrl ) rt->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = cause,
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static void tip_route_close(FluxTipRuntime *rt, FluxTeachingTipCloseReason reason) {
	XtkEventCause cause = reason == FLUX_TIP_CLOSE_PROGRAMMATIC ? XTK_EVENT_CAUSE_PROGRAM : XTK_EVENT_CAUSE_USER;
	tip_route(rt, XTK_EVENT_CLOSE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = reason}, cause);
}

static void tip_repaint_owner(FluxTipRuntime *rt) {
	HWND h = flux_window_hwnd(rt->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void tip_repaint_popup(FluxTipRuntime *rt) {
	HWND h = flux_popup_hwnd(rt->popup);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void tip_resolve_theme(FluxTipRuntime *rt, FluxThemeColors const **theme, bool *is_dark) {
	if (rt->theme) {
		*theme          = flux_theme_colors(rt->theme);
		FluxThemeMode m = flux_theme_mode(rt->theme);
		*is_dark        = (m == FLUX_THEME_DARK) || (m == FLUX_THEME_SYSTEM && flux_theme_systemdark());
		return;
	}
	*theme   = flux_theme_defaults();
	*is_dark = false;
}

static float tip_owner_scale(FluxTipRuntime const *rt) {
	FluxDpi dpi = flux_window_dpi(rt->window);
	return dpi.dpi_x > 0.0f ? dpi.dpi_x / FLUX_DPI_BASE : 1.0f;
}

static bool tip_has_action(FluxTeachingTipData const *m) { return m->action_text && m->action_text [0]; }

static bool tip_has_close(FluxTeachingTipData const *m) { return m->close_text && m->close_text [0]; }

static bool tip_shows_x(FluxTeachingTipData const *m) { return !tip_has_close(m) && !m->light_dismiss; }

static void tip_layout_buttons(FluxTipRuntime *rt, float inner_w, float btn_y) {
	bool action = tip_has_action(&rt->model);
	bool close  = tip_has_close(&rt->model);
	if (action && close) {
		float col       = inner_w * 0.5f;
		rt->action_rect = (FluxRect) {TIP_PAD, btn_y, col - TIP_BTN_GUTTER, TIP_BTN_H};
		rt->close_rect  = (FluxRect) {TIP_PAD + col + TIP_BTN_GUTTER, btn_y, col - TIP_BTN_GUTTER, TIP_BTN_H};
		return;
	}
	if (action) rt->action_rect = (FluxRect) {TIP_PAD, btn_y, inner_w, TIP_BTN_H};
	else if (close) rt->close_rect = (FluxRect) {TIP_PAD, btn_y, inner_w, TIP_BTN_H};
}

static void tip_layout(FluxTipRuntime *rt) {
	FluxTeachingTipData const *m       = &rt->model;
	float                      inner_w = TIP_CARD_W - 2.0f * TIP_PAD;
	float                      text_w
	  = inner_w - (m->icon_glyph ? TIP_ICON_SIZE + TIP_ICON_GAP : 0.0f) - (tip_shows_x(m) ? TIP_X_RESERVE : 0.0f);

	float title_h    = flux_teachtip_text_height(rt->text, m->title, true, text_w);
	float subtitle_h = flux_teachtip_text_height(rt->text, m->subtitle, false, text_w);
	float body_h     = flux_maxf(title_h + subtitle_h, m->icon_glyph ? TIP_ICON_SIZE : 0.0f);

	rt->action_rect  = (FluxRect) {0};
	rt->close_rect   = (FluxRect) {0};
	rt->x_rect       = (FluxRect) {0};

	float card_h     = TIP_PAD + body_h + TIP_PAD;
	if (tip_has_action(m) || tip_has_close(m)) {
		float btn_y = TIP_PAD + body_h + TIP_BTN_TOP;
		card_h      = btn_y + TIP_BTN_H + TIP_PAD;
		tip_layout_buttons(rt, inner_w, btn_y);
	}
	card_h = flux_clampf(card_h, TIP_MIN_H, TIP_MAX_H);
	if (tip_shows_x(m)) rt->x_rect = (FluxRect) {TIP_CARD_W - TIP_X_SIZE, 0.0f, TIP_X_SIZE, TIP_X_SIZE};

	rt->tip_w          = TIP_CARD_W + 2.0f * TIP_TAIL_BAND;
	rt->tip_h          = card_h + 2.0f * TIP_TAIL_BAND;

	FluxRect *rects [] = {&rt->action_rect, &rt->close_rect, &rt->x_rect};
	for (int i = 0; i < 3; i++) {
		if (rects [i]->w <= 0.0f) continue;
		rects [i]->x += TIP_TAIL_BAND;
		rects [i]->y += TIP_TAIL_BAND;
	}
}

typedef struct TipGeom {
	FluxRect win;
	FluxRect tgt;
	float    tip_w, tip_h;
	float    margin;
} TipGeom;

static FluxTeachingTipPlacement const kTipOrderBase [13] = {
  FLUX_TIP_PLACE_TOP, FLUX_TIP_PLACE_BOTTOM, FLUX_TIP_PLACE_LEFT, FLUX_TIP_PLACE_RIGHT, FLUX_TIP_PLACE_TOP_LEFT,
  FLUX_TIP_PLACE_TOP_RIGHT, FLUX_TIP_PLACE_BOTTOM_LEFT, FLUX_TIP_PLACE_BOTTOM_RIGHT, FLUX_TIP_PLACE_LEFT_TOP,
  FLUX_TIP_PLACE_LEFT_BOTTOM, FLUX_TIP_PLACE_RIGHT_TOP, FLUX_TIP_PLACE_RIGHT_BOTTOM, FLUX_TIP_PLACE_CENTER};

static FluxTeachingTipPlacement const kTipOrderBottom [13] = {
  FLUX_TIP_PLACE_BOTTOM, FLUX_TIP_PLACE_TOP, FLUX_TIP_PLACE_LEFT, FLUX_TIP_PLACE_RIGHT, FLUX_TIP_PLACE_BOTTOM_LEFT,
  FLUX_TIP_PLACE_BOTTOM_RIGHT, FLUX_TIP_PLACE_TOP_LEFT, FLUX_TIP_PLACE_TOP_RIGHT, FLUX_TIP_PLACE_LEFT_TOP,
  FLUX_TIP_PLACE_LEFT_BOTTOM, FLUX_TIP_PLACE_RIGHT_TOP, FLUX_TIP_PLACE_RIGHT_BOTTOM, FLUX_TIP_PLACE_CENTER};

static FluxTeachingTipPlacement const kTipOrderLeft [13] = {
  FLUX_TIP_PLACE_LEFT, FLUX_TIP_PLACE_RIGHT, FLUX_TIP_PLACE_TOP, FLUX_TIP_PLACE_BOTTOM, FLUX_TIP_PLACE_LEFT_TOP,
  FLUX_TIP_PLACE_LEFT_BOTTOM, FLUX_TIP_PLACE_RIGHT_TOP, FLUX_TIP_PLACE_RIGHT_BOTTOM, FLUX_TIP_PLACE_TOP_LEFT,
  FLUX_TIP_PLACE_TOP_RIGHT, FLUX_TIP_PLACE_BOTTOM_LEFT, FLUX_TIP_PLACE_BOTTOM_RIGHT, FLUX_TIP_PLACE_CENTER};

static FluxTeachingTipPlacement const kTipOrderRight [13] = {
  FLUX_TIP_PLACE_RIGHT, FLUX_TIP_PLACE_LEFT, FLUX_TIP_PLACE_TOP, FLUX_TIP_PLACE_BOTTOM, FLUX_TIP_PLACE_RIGHT_TOP,
  FLUX_TIP_PLACE_RIGHT_BOTTOM, FLUX_TIP_PLACE_LEFT_TOP, FLUX_TIP_PLACE_LEFT_BOTTOM, FLUX_TIP_PLACE_TOP_LEFT,
  FLUX_TIP_PLACE_TOP_RIGHT, FLUX_TIP_PLACE_BOTTOM_LEFT, FLUX_TIP_PLACE_BOTTOM_RIGHT, FLUX_TIP_PLACE_CENTER};

static bool tip_bottom_family(FluxTeachingTipPlacement p) {
	return p == FLUX_TIP_PLACE_BOTTOM || p == FLUX_TIP_PLACE_BOTTOM_LEFT || p == FLUX_TIP_PLACE_BOTTOM_RIGHT;
}

static bool tip_left_family(FluxTeachingTipPlacement p) {
	return p == FLUX_TIP_PLACE_LEFT || p == FLUX_TIP_PLACE_LEFT_TOP || p == FLUX_TIP_PLACE_LEFT_BOTTOM;
}

static bool tip_right_family(FluxTeachingTipPlacement p) {
	return p == FLUX_TIP_PLACE_RIGHT || p == FLUX_TIP_PLACE_RIGHT_TOP || p == FLUX_TIP_PLACE_RIGHT_BOTTOM;
}

static FluxTeachingTipPlacement const *tip_family_order(FluxTeachingTipPlacement pref) {
	if (tip_bottom_family(pref)) return kTipOrderBottom;
	if (tip_left_family(pref)) return kTipOrderLeft;
	if (tip_right_family(pref)) return kTipOrderRight;
	return kTipOrderBase;
}

static void tip_priority(FluxTeachingTipPlacement pref, FluxTeachingTipPlacement order [13]) {
	memcpy(order, tip_family_order(pref), 13 * sizeof(order [0]));
	if (pref == FLUX_TIP_PLACE_AUTO) return;

	int idx = 0;
	while (idx < 13 && order [idx] != pref) idx++;
	for (; idx > 0; idx--) order [idx] = order [idx - 1];
	order [0] = pref;
}

static void tip_prune(bool avail [14], FluxTeachingTipPlacement const *set, int count) {
	for (int i = 0; i < count; i++) avail [set [i]] = false;
}

static void tip_prune_offscreen(bool avail [14], TipGeom const *g) {
	FluxRect const                *w      = &g->win;
	FluxRect const                *t      = &g->tgt;

	FluxTeachingTipPlacement const top [] = {FLUX_TIP_PLACE_TOP, FLUX_TIP_PLACE_TOP_LEFT, FLUX_TIP_PLACE_TOP_RIGHT};
	FluxTeachingTipPlacement const bot []
	  = {FLUX_TIP_PLACE_BOTTOM, FLUX_TIP_PLACE_BOTTOM_LEFT, FLUX_TIP_PLACE_BOTTOM_RIGHT};
	FluxTeachingTipPlacement const lef [] = {FLUX_TIP_PLACE_LEFT, FLUX_TIP_PLACE_LEFT_TOP, FLUX_TIP_PLACE_LEFT_BOTTOM};
	FluxTeachingTipPlacement const rig []
	  = {FLUX_TIP_PLACE_RIGHT, FLUX_TIP_PLACE_RIGHT_TOP, FLUX_TIP_PLACE_RIGHT_BOTTOM};

	if (t->y < w->y) tip_prune(avail, top, 3);
	if (t->y + t->h > w->y + w->h) tip_prune(avail, bot, 3);
	if (t->x < w->x) tip_prune(avail, lef, 3);
	if (t->x + t->w > w->x + w->w) tip_prune(avail, rig, 3);

	float cx = t->x + t->w * 0.5f;
	float cy = t->y + t->h * 0.5f;
	if (cx < w->x || cx > w->x + w->w) {
		tip_prune(avail, top, 3);
		tip_prune(avail, bot, 3);
		avail [FLUX_TIP_PLACE_CENTER] = false;
	}
	if (cy < w->y || cy > w->y + w->h) {
		tip_prune(avail, lef, 3);
		tip_prune(avail, rig, 3);
		avail [FLUX_TIP_PLACE_CENTER] = false;
	}
}

static void tip_prune_fit_vertical(bool avail [14], TipGeom const *g, float above, float below, float half_h) {
	float content_h = g->tip_h;
	float tip_h     = content_h + TIP_TAIL_BAND;

	if (tip_h > above)
		avail [FLUX_TIP_PLACE_TOP] = avail [FLUX_TIP_PLACE_TOP_LEFT] = avail [FLUX_TIP_PLACE_TOP_RIGHT] = false;
	if (tip_h > above + half_h) avail [FLUX_TIP_PLACE_CENTER] = false;
	if (content_h - TIP_TAIL_CENTER > above + half_h)
		avail [FLUX_TIP_PLACE_RIGHT_TOP] = avail [FLUX_TIP_PLACE_LEFT_TOP] = false;
	if (content_h * 0.5f > above + half_h || content_h * 0.5f > below + half_h)
		avail [FLUX_TIP_PLACE_LEFT] = avail [FLUX_TIP_PLACE_RIGHT] = false;
	if (content_h - TIP_TAIL_CENTER > below + half_h)
		avail [FLUX_TIP_PLACE_RIGHT_BOTTOM] = avail [FLUX_TIP_PLACE_LEFT_BOTTOM] = false;
	if (tip_h > below) {
		avail [FLUX_TIP_PLACE_BOTTOM] = avail [FLUX_TIP_PLACE_BOTTOM_LEFT] = avail [FLUX_TIP_PLACE_BOTTOM_RIGHT]
		  = false;
	}
}

static void tip_prune_fit_horizontal(bool avail [14], TipGeom const *g, float left, float right, float half_w) {
	float content_w = g->tip_w;
	float tip_w     = content_w + TIP_TAIL_BAND;

	if (tip_w > left)
		avail [FLUX_TIP_PLACE_LEFT] = avail [FLUX_TIP_PLACE_LEFT_TOP] = avail [FLUX_TIP_PLACE_LEFT_BOTTOM] = false;
	if (content_w - TIP_TAIL_CENTER > left + half_w)
		avail [FLUX_TIP_PLACE_TOP_LEFT] = avail [FLUX_TIP_PLACE_BOTTOM_LEFT] = false;
	if (content_w * 0.5f > left + half_w || content_w * 0.5f > right + half_w)
		avail [FLUX_TIP_PLACE_TOP] = avail [FLUX_TIP_PLACE_BOTTOM] = avail [FLUX_TIP_PLACE_CENTER] = false;
	if (content_w - TIP_TAIL_CENTER > right + half_w)
		avail [FLUX_TIP_PLACE_TOP_RIGHT] = avail [FLUX_TIP_PLACE_BOTTOM_RIGHT] = false;
	if (tip_w > right)
		avail [FLUX_TIP_PLACE_RIGHT] = avail [FLUX_TIP_PLACE_RIGHT_TOP] = avail [FLUX_TIP_PLACE_RIGHT_BOTTOM] = false;
}

static FluxTeachingTipPlacement tip_solve(TipGeom const *g, FluxTeachingTipPlacement pref) {
	bool avail [14];
	for (int i = 0; i < 14; i++) avail [i] = true;

	tip_prune_offscreen(avail, g);
	float above = g->tgt.y - g->win.y;
	float below = (g->win.y + g->win.h) - (g->tgt.y + g->tgt.h);
	float left  = g->tgt.x - g->win.x;
	float right = (g->win.x + g->win.w) - (g->tgt.x + g->tgt.w);
	tip_prune_fit_vertical(avail, g, above, below, g->tgt.h * 0.5f);
	tip_prune_fit_horizontal(avail, g, left, right, g->tgt.w * 0.5f);

	FluxTeachingTipPlacement order [13];
	tip_priority(pref, order);
	for (int i = 0; i < 13; i++)
		if (avail [order [i]]) return order [i];
	return FLUX_TIP_PLACE_AUTO;
}

static FluxPoint tip_origin_targeted(TipGeom const *g, FluxTeachingTipPlacement p) {
	FluxRect const *t       = &g->tgt;
	float           w       = g->tip_w;
	float           h       = g->tip_h;
	float           m       = g->margin;
	float           cx      = t->x + t->w * 0.5f;
	float           cy      = t->y + t->h * 0.5f;
	float           top_y   = t->y - h - m;
	float           bot_y   = t->y + t->h + m;
	float           left_x  = t->x - w - m;
	float           right_x = t->x + t->w + m;

	switch (p) {
	case FLUX_TIP_PLACE_TOP          : return (FluxPoint) {cx - w * 0.5f, top_y};
	case FLUX_TIP_PLACE_BOTTOM       : return (FluxPoint) {cx - w * 0.5f, bot_y};
	case FLUX_TIP_PLACE_LEFT         : return (FluxPoint) {left_x, cy - h * 0.5f};
	case FLUX_TIP_PLACE_RIGHT        : return (FluxPoint) {right_x, cy - h * 0.5f};
	case FLUX_TIP_PLACE_TOP_RIGHT    : return (FluxPoint) {cx - TIP_TAIL_CENTER, top_y};
	case FLUX_TIP_PLACE_TOP_LEFT     : return (FluxPoint) {cx - w + TIP_TAIL_CENTER, top_y};
	case FLUX_TIP_PLACE_BOTTOM_RIGHT : return (FluxPoint) {cx - TIP_TAIL_CENTER, bot_y};
	case FLUX_TIP_PLACE_BOTTOM_LEFT  : return (FluxPoint) {cx - w + TIP_TAIL_CENTER, bot_y};
	case FLUX_TIP_PLACE_LEFT_TOP     : return (FluxPoint) {left_x, cy - h + TIP_TAIL_CENTER};
	case FLUX_TIP_PLACE_LEFT_BOTTOM  : return (FluxPoint) {left_x, cy - TIP_TAIL_CENTER};
	case FLUX_TIP_PLACE_RIGHT_TOP    : return (FluxPoint) {right_x, cy - h + TIP_TAIL_CENTER};
	case FLUX_TIP_PLACE_RIGHT_BOTTOM : return (FluxPoint) {right_x, cy - TIP_TAIL_CENTER};
	default                          : return (FluxPoint) {cx - w * 0.5f, t->y + t->h * 0.5f - h - m};
	}
}

static FluxPoint tip_origin_untargeted(TipGeom const *g, FluxTeachingTipPlacement p) {
	float e  = TIP_EDGE_MARGIN + g->margin;
	float x0 = g->win.x + e;
	float y0 = g->win.y + e;
	float x1 = g->win.x + g->win.w - g->tip_w - e;
	float y1 = g->win.y + g->win.h - g->tip_h - e;
	float xc = g->win.x + (g->win.w - g->tip_w) * 0.5f;
	float yc = g->win.y + (g->win.h - g->tip_h) * 0.5f;

	switch (p) {
	case FLUX_TIP_PLACE_TOP          : return (FluxPoint) {xc, y0};
	case FLUX_TIP_PLACE_LEFT         : return (FluxPoint) {x0, yc};
	case FLUX_TIP_PLACE_RIGHT        : return (FluxPoint) {x1, yc};
	case FLUX_TIP_PLACE_TOP_LEFT     :
	case FLUX_TIP_PLACE_LEFT_TOP     : return (FluxPoint) {x0, y0};
	case FLUX_TIP_PLACE_TOP_RIGHT    :
	case FLUX_TIP_PLACE_RIGHT_TOP    : return (FluxPoint) {x1, y0};
	case FLUX_TIP_PLACE_BOTTOM_LEFT  :
	case FLUX_TIP_PLACE_LEFT_BOTTOM  : return (FluxPoint) {x0, y1};
	case FLUX_TIP_PLACE_BOTTOM_RIGHT :
	case FLUX_TIP_PLACE_RIGHT_BOTTOM : return (FluxPoint) {x1, y1};
	case FLUX_TIP_PLACE_CENTER       : return (FluxPoint) {xc, yc};
	default                          : return (FluxPoint) {xc, y1};
	}
}

static uint8_t tip_tail_edge_for(FluxTeachingTipPlacement p) {
	switch (p) {
	case FLUX_TIP_PLACE_TOP          :
	case FLUX_TIP_PLACE_TOP_LEFT     :
	case FLUX_TIP_PLACE_TOP_RIGHT    :
	case FLUX_TIP_PLACE_CENTER       : return FLUX_TIP_TAIL_EDGE_BOTTOM;
	case FLUX_TIP_PLACE_BOTTOM       :
	case FLUX_TIP_PLACE_BOTTOM_LEFT  :
	case FLUX_TIP_PLACE_BOTTOM_RIGHT : return FLUX_TIP_TAIL_EDGE_TOP;
	case FLUX_TIP_PLACE_LEFT         :
	case FLUX_TIP_PLACE_LEFT_TOP     :
	case FLUX_TIP_PLACE_LEFT_BOTTOM  : return FLUX_TIP_TAIL_EDGE_RIGHT;
	case FLUX_TIP_PLACE_RIGHT        :
	case FLUX_TIP_PLACE_RIGHT_TOP    :
	case FLUX_TIP_PLACE_RIGHT_BOTTOM : return FLUX_TIP_TAIL_EDGE_LEFT;
	default                          : return FLUX_TIP_TAIL_EDGE_NONE;
	}
}

static float tip_tail_center_for(FluxTipRuntime const *rt, FluxTeachingTipPlacement p) {
	switch (p) {
	case FLUX_TIP_PLACE_TOP_RIGHT    :
	case FLUX_TIP_PLACE_BOTTOM_RIGHT : return TIP_TAIL_CENTER;
	case FLUX_TIP_PLACE_TOP_LEFT     :
	case FLUX_TIP_PLACE_BOTTOM_LEFT  : return rt->tip_w - TIP_TAIL_CENTER;
	case FLUX_TIP_PLACE_LEFT_TOP     :
	case FLUX_TIP_PLACE_RIGHT_TOP    : return rt->tip_h - TIP_TAIL_CENTER;
	case FLUX_TIP_PLACE_LEFT_BOTTOM  :
	case FLUX_TIP_PLACE_RIGHT_BOTTOM : return TIP_TAIL_CENTER;
	case FLUX_TIP_PLACE_LEFT         :
	case FLUX_TIP_PLACE_RIGHT        : return rt->tip_h * 0.5f;
	default                          : return rt->tip_w * 0.5f;
	}
}

static FluxPoint tip_scale_center_for(FluxTipRuntime const *rt, FluxTeachingTipPlacement p, bool tail) {
	float w = rt->tip_w;
	float h = rt->tip_h;
	if (!tail) return (FluxPoint) {w * 0.5f, h * 0.5f};

	switch (p) {
	case FLUX_TIP_PLACE_TOP          :
	case FLUX_TIP_PLACE_CENTER       : return (FluxPoint) {w * 0.5f, h - TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_BOTTOM       : return (FluxPoint) {w * 0.5f, TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_LEFT         : return (FluxPoint) {w - TIP_TAIL_BAND, h * 0.5f};
	case FLUX_TIP_PLACE_RIGHT        : return (FluxPoint) {TIP_TAIL_BAND, h * 0.5f};
	case FLUX_TIP_PLACE_TOP_RIGHT    : return (FluxPoint) {TIP_SCALE_CORNER, h - TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_TOP_LEFT     : return (FluxPoint) {w - TIP_SCALE_CORNER, h - TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_BOTTOM_RIGHT : return (FluxPoint) {TIP_SCALE_CORNER, TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_BOTTOM_LEFT  : return (FluxPoint) {w - TIP_SCALE_CORNER, TIP_TAIL_BAND};
	case FLUX_TIP_PLACE_LEFT_TOP     : return (FluxPoint) {w - TIP_TAIL_BAND, h - TIP_SCALE_CORNER};
	case FLUX_TIP_PLACE_LEFT_BOTTOM  : return (FluxPoint) {w - TIP_TAIL_BAND, TIP_SCALE_CORNER};
	case FLUX_TIP_PLACE_RIGHT_TOP    : return (FluxPoint) {TIP_TAIL_BAND, h - TIP_SCALE_CORNER};
	case FLUX_TIP_PLACE_RIGHT_BOTTOM : return (FluxPoint) {TIP_TAIL_BAND, TIP_SCALE_CORNER};
	default                          : return (FluxPoint) {w * 0.5f, h * 0.5f};
	}
}

static FluxRect tip_window_rect_px(FluxTipRuntime const *rt) {
	HWND hwnd = flux_window_hwnd(rt->window);
	RECT rc   = {0};
	if (!hwnd || !GetClientRect(hwnd, &rc)) return (FluxRect) {0};
	POINT tl = {rc.left, rc.top};
	ClientToScreen(hwnd, &tl);
	return (FluxRect) {( float ) tl.x, ( float ) tl.y, ( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

static bool tip_position(FluxTipRuntime *rt) {
	float    scale                    = tip_owner_scale(rt);
	TipGeom  g                        = {0};
	FluxRect wpx                      = tip_window_rect_px(rt);
	g.win                             = (FluxRect) {wpx.x / scale, wpx.y / scale, wpx.w / scale, wpx.h / scale};
	g.tip_w                           = rt->tip_w;
	g.tip_h                           = rt->tip_h;
	g.margin                          = rt->model.placement_margin;

	bool                     targeted = !rt->model.untargeted && rt->target != XENT_NODE_INVALID;
	FluxTeachingTipPlacement eff      = rt->model.preferred;
	FluxPoint                origin;

	if (targeted) {
		FluxRect a = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->target);
		g.tgt      = (FluxRect) {a.x / scale, a.y / scale, a.w / scale, a.h / scale};
		eff        = tip_solve(&g, rt->model.preferred);
		if (eff == FLUX_TIP_PLACE_AUTO) return false;
		origin = tip_origin_targeted(&g, eff);
	}
	else {
		if (rt->tip_w > g.win.w || rt->tip_h > g.win.h) return false;
		origin = tip_origin_untargeted(&g, eff);
		if (eff == FLUX_TIP_PLACE_AUTO) eff = FLUX_TIP_PLACE_BOTTOM;
	}

	bool tail           = rt->model.tail_visibility == FLUX_TIP_TAIL_VISIBLE
	                   || (rt->model.tail_visibility == FLUX_TIP_TAIL_AUTO && targeted);
	rt->model.effective = eff;
	rt->tail_edge       = tail ? tip_tail_edge_for(eff) : FLUX_TIP_TAIL_EDGE_NONE;
	rt->tail_center     = tip_tail_center_for(rt, eff);
	FluxPoint sc        = tip_scale_center_for(rt, eff, tail);
	rt->scale_cx        = sc.x;
	rt->scale_cy        = sc.y;

	flux_popup_setsize(rt->popup, rt->tip_w, rt->tip_h);
	FluxRect anchor = {origin.x * scale, origin.y * scale, 0.0f, 0.0f};
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
	return true;
}

static void tip_restore_focus(FluxTipRuntime *rt) {
	if (!rt->input || !rt->model.light_dismiss) return;
	if (flux_input_focus(rt->input) == rt->node && rt->prev_focus != XENT_NODE_INVALID)
		flux_input_setfocus(rt->input, rt->prev_focus, false);
}

static void tip_anim_finish(FluxTipRuntime *rt) {
	bool was_contract = rt->anim_contract;
	rt->anim_expand   = false;
	rt->anim_contract = false;
	rt->cur_sx        = 1.0f;
	rt->cur_sy        = 1.0f;

	if (was_contract) {
		FluxStore                 *store                                         = rt->store;
		XentNodeId                 node                                          = rt->node;
		void                       (*notify)(void *, FluxTeachingTipCloseReason) = rt->model.on_close;
		void                      *notify_ctx                                    = rt->model.close_ctx;
		FluxTeachingTipCloseReason reason                                        = rt->close_reason;
		flux_popup_dismiss(rt->popup);
		tip_restore_focus(rt);
		rt = tip_runtime(store, node);
		if (!rt) return;
		tip_repaint_owner(rt);
		tip_route_close(rt, reason);
		if (notify) notify(notify_ctx, reason);
		rt = tip_runtime(store, node);
		if (!rt) return;
	}

	if (rt->pending_hide) {
		rt->pending_hide = false;
		tip_close(rt, rt->pending_reason);
	}
}

static bool tip_anim_step(void *ctx, unsigned long now) {
	FluxTipRuntime *rt  = ( FluxTipRuntime * ) ctx;
	float           dur = rt->anim_expand ? TIP_EXPAND_MS : TIP_CONTRACT_MS;
	float           t   = flux_clamp01(( float ) (( DWORD ) now - rt->anim_start) / dur);
	float           eased
	  = rt->anim_expand ? flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f) : flux_cubic_bezier(t, 0.7f, 0.0f, 1.0f, 0.5f);
	rt->cur_sx = rt->from_sx + (rt->to_sx - rt->from_sx) * eased;
	rt->cur_sy = rt->from_sy + (rt->to_sy - rt->from_sy) * eased;
	tip_repaint_popup(rt);
	if (t < 1.0f) return true;
	tip_anim_finish(rt);
	return false;
}

static void tip_anim_start(FluxTipRuntime *rt, bool expand) {
	rt->anim_expand   = expand;
	rt->anim_contract = !expand;
	rt->anim_start    = flux_anim_now();
	if (expand) {
		rt->from_sx = flux_minf(0.01f, 20.0f / rt->tip_w);
		rt->from_sy = flux_minf(0.01f, 20.0f / rt->tip_h);
		rt->to_sx   = 1.0f;
		rt->to_sy   = 1.0f;
	}
	else {
		rt->from_sx = rt->cur_sx;
		rt->from_sy = rt->cur_sy;
		rt->to_sx   = 20.0f / rt->tip_w;
		rt->to_sy   = 20.0f / rt->tip_h;
	}
	rt->cur_sx = rt->from_sx;
	rt->cur_sy = rt->from_sy;
	flux_anim_register(rt, tip_anim_step);
}

static void tip_open(FluxTipRuntime *rt) {
	if (rt->anim_expand || rt->anim_contract) {
		rt->pending_hide = false;
		return;
	}
	if (rt->model.open || !rt->popup) return;
	if (!rt->model.untargeted) rt->target = xent_node_parent(rt->ctx, rt->node);

	tip_layout(rt);
	flux_popup_setdismissoutside(rt->popup, rt->model.light_dismiss);
	if (!tip_position(rt)) {
		rt->model.effective = FLUX_TIP_PLACE_AUTO;
		tip_route_close(rt, FLUX_TIP_CLOSE_PROGRAMMATIC);
		if (rt->model.on_close) rt->model.on_close(rt->model.close_ctx, FLUX_TIP_CLOSE_PROGRAMMATIC);
		return;
	}

	rt->model.open   = true;
	rt->hot_zone     = FLUX_TIP_ZONE_NONE;
	rt->pressed_zone = FLUX_TIP_ZONE_NONE;
	if (rt->model.light_dismiss && rt->input) {
		FluxStore *store = rt->store;
		XentNodeId node  = rt->node;
		rt->prev_focus   = flux_input_focus(rt->input);
		flux_input_setfocus(rt->input, rt->node, false);
		rt = tip_runtime(store, node);
		if (!rt) return;
	}
	tip_anim_start(rt, true);
	tip_repaint_owner(rt);
}

static void tip_close(FluxTipRuntime *rt, FluxTeachingTipCloseReason reason) {
	if (rt->anim_expand || rt->anim_contract) {
		rt->pending_hide   = rt->anim_expand;
		rt->pending_reason = reason;
		return;
	}
	if (!rt->model.open) return;
	rt->model.open      = false;
	rt->model.effective = FLUX_TIP_PLACE_AUTO;
	rt->close_reason    = reason;
	tip_anim_start(rt, false);
}

static void tip_dismissed(void *ctx) {
	FluxTipRuntime *rt = ( FluxTipRuntime * ) ctx;
	if (!rt || !rt->model.open) return;
	FluxStore *store                                         = rt->store;
	XentNodeId node                                          = rt->node;
	void       (*notify)(void *, FluxTeachingTipCloseReason) = rt->model.on_close;
	void      *notify_ctx                                    = rt->model.close_ctx;
	rt->model.open                                           = false;
	rt->model.effective                                      = FLUX_TIP_PLACE_AUTO;
	rt->anim_expand                                          = false;
	rt->anim_contract                                        = false;
	flux_anim_unregister(rt);
	rt->cur_sx = 1.0f;
	rt->cur_sy = 1.0f;
	tip_restore_focus(rt);
	rt = tip_runtime(store, node);
	if (!rt) return;
	tip_repaint_owner(rt);
	tip_route_close(rt, FLUX_TIP_CLOSE_LIGHT_DISMISS);
	if (notify) notify(notify_ctx, FLUX_TIP_CLOSE_LIGHT_DISMISS);
}

static bool tip_rect_has(FluxRect const *r, float x, float y) {
	return r->w > 0.0f && x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static int tip_zone_at(FluxTipRuntime const *rt, float x, float y) {
	if (tip_rect_has(&rt->x_rect, x, y)) return FLUX_TIP_ZONE_X;
	if (tip_rect_has(&rt->action_rect, x, y)) return FLUX_TIP_ZONE_ACTION;
	if (tip_rect_has(&rt->close_rect, x, y)) return FLUX_TIP_ZONE_CLOSE;
	return FLUX_TIP_ZONE_NONE;
}

static void tip_activate_zone(FluxTipRuntime *rt, int zone) {
	if (zone == FLUX_TIP_ZONE_ACTION) {
		tip_route(rt, XTK_EVENT_INVOKE, (XtkEventPayload) {0}, XTK_EVENT_CAUSE_USER);
		if (rt->model.on_action) rt->model.on_action(rt->model.action_ctx);
		return;
	}
	tip_close(rt, FLUX_TIP_CLOSE_BUTTON);
}

static void tip_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	( void ) popup;
	FluxTipRuntime *rt = ( FluxTipRuntime * ) ctx;
	if (!rt) return;
	int zone = (event == FLUX_POPUP_MOUSE_LEAVE) ? FLUX_TIP_ZONE_NONE : tip_zone_at(rt, x, y);

	if (event == FLUX_POPUP_MOUSE_DOWN) {
		rt->pressed_zone = zone;
		tip_repaint_popup(rt);
		return;
	}
	if (event == FLUX_POPUP_MOUSE_UP) {
		int pressed      = rt->pressed_zone;
		rt->pressed_zone = FLUX_TIP_ZONE_NONE;
		tip_repaint_popup(rt);
		if (zone != FLUX_TIP_ZONE_NONE && zone == pressed) tip_activate_zone(rt, zone);
		return;
	}
	if (zone != rt->hot_zone) {
		rt->hot_zone = zone;
		tip_repaint_popup(rt);
	}
}

static bool tip_on_key(void *ctx, FluxKey key, bool down) {
	FluxTipRuntime *rt = ( FluxTipRuntime * ) ctx;
	if (!rt || !down || !rt->model.open) return false;
	if (key == FLUX_KEY_ESCAPE && rt->model.light_dismiss) {
		tip_close(rt, FLUX_TIP_CLOSE_LIGHT_DISMISS);
		return true;
	}
	if (key == FLUX_KEY_F6 && rt->input && rt->prev_focus != XENT_NODE_INVALID) {
		flux_input_setfocus(rt->input, rt->prev_focus, false);
		return true;
	}
	return false;
}

static XentDl *tip_record(void *ctx, FluxPopup const *popup) {
	FluxTipRuntime        *rt      = ( FluxTipRuntime * ) ctx;
	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	tip_resolve_theme(rt, &theme, &is_dark);

	FluxRenderCtx rc = {
	  .text    = rt->text,
	  .theme   = theme,
	  .is_dark = is_dark,
	};

	FluxColor bg = theme->solid_background_tertiary;
	if (rt->model.light_dismiss) {
		if (is_dark) bg = flux_color_rgb(0x2c, 0x2c, 0x2c);
		bg = flux_popup_acrylictint(popup, bg);
	}

	FluxTeachingTipPaintInfo info = {
	  .model        = &rt->model,
	  .tip_w        = rt->tip_w,
	  .tip_h        = rt->tip_h,
	  .tail_edge    = rt->tail_edge,
	  .tail_center  = rt->tail_center,
	  .background   = bg,
	  .scale_x      = rt->cur_sx,
	  .scale_y      = rt->cur_sy,
	  .scale_cx     = rt->scale_cx,
	  .scale_cy     = rt->scale_cy,
	  .hot_zone     = rt->hot_zone,
	  .pressed_zone = rt->pressed_zone,
	  .action_rect  = rt->action_rect,
	  .close_rect   = rt->close_rect,
	  .x_rect       = rt->x_rect};
	return flux_teachtip_record_surface(&rc, &info);
}

static void tip_destroy(void *component_data) {
	FluxTipRuntime *rt = ( FluxTipRuntime * ) component_data;
	if (!rt) return;
	flux_anim_unregister(rt);
	flux_str_free(rt->model.title);
	flux_str_free(rt->model.subtitle);
	flux_str_free(rt->model.action_text);
	flux_str_free(rt->model.close_text);
	if (rt->popup) flux_popup_destroy(rt->popup);
	free(rt);
}

static void tip_init_runtime(FluxTipRuntime *rt, XentNodeId node, FluxTeachingTipDesc const *info) {
	rt->model.title            = flux_str_dup(info->title);
	rt->model.subtitle         = flux_str_dup(info->subtitle);
	rt->model.icon_glyph       = info->icon_glyph;
	rt->model.action_text      = flux_str_dup(info->action_text);
	rt->model.action_accent    = info->action_accent;
	rt->model.close_text       = flux_str_dup(info->close_text);
	rt->model.preferred        = info->preferred_placement;
	rt->model.effective        = FLUX_TIP_PLACE_AUTO;
	rt->model.tail_visibility  = info->tail_visibility;
	rt->model.light_dismiss    = info->light_dismiss;
	rt->model.untargeted       = info->untargeted;
	rt->model.placement_margin = info->placement_margin;
	rt->model.on_action        = info->on_action;
	rt->model.action_ctx       = info->action_ctx;
	rt->model.on_close         = info->on_close;
	rt->model.close_ctx        = info->close_ctx;
	rt->store                  = info->store;
	rt->node                   = node;
	rt->target                 = info->untargeted ? XENT_NODE_INVALID : info->parent;
	rt->ctx                    = info->ctx;
	rt->window                 = info->window;
	rt->text                   = info->text;
	rt->theme                  = info->theme;
	rt->input                  = info->input;
	rt->cur_sx                 = 1.0f;
	rt->cur_sy                 = 1.0f;
	rt->prev_focus             = XENT_NODE_INVALID;
}

XentNodeId flux_teachtip_init(FluxTeachingTipDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TEACHING_TIP);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_store_find(info->store, node);
	FluxTipRuntime *rt = nd ? ( FluxTipRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return node;
	}
	tip_init_runtime(rt, node, info);

	rt->popup = flux_popup_create(info->window);
	if (rt->popup) {
		flux_popup_setanim(rt->popup, FLUX_POPUP_ANIM_NONE);
		flux_popup_onrecord(rt->popup, tip_record, rt, rt->text);
		flux_popup_onmouse(rt->popup, tip_mouse, rt);
		flux_popup_ondismiss(rt->popup, tip_dismissed, rt);
	}

	nd->component_data         = &rt->model;
	nd->destroy_component_data = tip_destroy;
	nd->behavior.on_key        = tip_on_key;
	nd->behavior.on_key_ctx    = rt;

	xent_setpos(info->ctx, node, (XentPoint) {0.0f, 0.0f});
	xent_setsize(info->ctx, node, (XentSize) {0.0f, 0.0f});
	xent_set_focusable(info->ctx, node, true);
	return node;
}

void flux_teachtip_retarget(XentCtx *ctx, FluxStore *store, XentNodeId tip) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (!rt) return;
	rt->ctx   = ctx;
	rt->store = store;
	rt->node  = tip;
}

void flux_teachtip_show(FluxStore *store, XentNodeId tip) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (rt) tip_open(rt);
}

void flux_teachtip_hide(FluxStore *store, XentNodeId tip) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (rt) tip_close(rt, FLUX_TIP_CLOSE_PROGRAMMATIC);
}

void flux_teachtip_set_target(FluxStore *store, XentNodeId tip, XentNodeId target) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (!rt) return;
	rt->target           = target;
	rt->model.untargeted = target == XENT_NODE_INVALID;
	if (rt->model.open && tip_position(rt)) tip_repaint_popup(rt);
}

void flux_teachtip_set_placement(FluxStore *store, XentNodeId tip, FluxTeachingTipPlacement placement) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (!rt) return;
	rt->model.preferred = placement;
	if (rt->model.open && tip_position(rt)) tip_repaint_popup(rt);
}

void flux_teachtip_set_texts(FluxStore *store, XentNodeId tip, char const *title, char const *subtitle) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (!rt) return;
	flux_str_replace(&rt->model.title, title);
	flux_str_replace(&rt->model.subtitle, subtitle);
	if (!rt->model.open) return;
	tip_layout(rt);
	if (tip_position(rt)) tip_repaint_popup(rt);
}

void flux_teachtip_configure(
  FluxStore *store, XentNodeId tip, char const *icon, char const *action_text, bool action_accent,
  char const *close_text, bool light_dismiss, int tail_visibility, float placement_margin
) {
	FluxTipRuntime *rt = tip_runtime(store, tip);
	if (!rt) return;
	wchar_t const *glyph       = flux_icon_lookup(icon);
	rt->model.icon_glyph       = glyph ? ( uint32_t ) glyph [0] : 0u;
	rt->model.action_accent    = action_accent;
	rt->model.light_dismiss    = light_dismiss;
	rt->model.tail_visibility  = ( int8_t ) tail_visibility;
	rt->model.placement_margin = placement_margin;
	flux_str_replace(&rt->model.action_text, action_text);
	flux_str_replace(&rt->model.close_text, close_text);
	if (!rt->model.open) return;
	tip_layout(rt);
	if (tip_position(rt)) tip_repaint_popup(rt);
}
