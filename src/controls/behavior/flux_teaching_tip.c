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
#include "controls/factory/flux_init_basic.h"
#include "controls/data/flux_button_data.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"
#include "popup/flux_popup_tree.h"
#include "render/flux_render_snapshot.h"
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
	FluxRect                   title_rect;
	FluxRect                   subtitle_rect;
	XentNodeId                 surface;
	XentNodeId                 parts [5];
	FluxTeachingTipPaintInfo   paint;

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

static void tip_layout_texts(FluxTipRuntime *rt, float text_w, float title_h, float subtitle_h) {
	float x           = TIP_PAD + (rt->model.icon_glyph ? TIP_ICON_SIZE + TIP_ICON_GAP : 0.0f);
	rt->title_rect    = (FluxRect) {x, TIP_PAD, title_h > 0.0f ? text_w : 0.0f, title_h};
	rt->subtitle_rect = (FluxRect) {x, TIP_PAD + title_h, subtitle_h > 0.0f ? text_w : 0.0f, subtitle_h};
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
	tip_layout_texts(rt, text_w, title_h, subtitle_h);

	float card_h = TIP_PAD + body_h + TIP_PAD;
	if (tip_has_action(m) || tip_has_close(m)) {
		float btn_y = TIP_PAD + body_h + TIP_BTN_TOP;
		card_h      = btn_y + TIP_BTN_H + TIP_PAD;
		tip_layout_buttons(rt, inner_w, btn_y);
	}
	card_h = flux_clampf(card_h, TIP_MIN_H, TIP_MAX_H);
	if (tip_shows_x(m)) rt->x_rect = (FluxRect) {TIP_CARD_W - TIP_X_SIZE, 0.0f, TIP_X_SIZE, TIP_X_SIZE};

	rt->tip_w          = TIP_CARD_W + 2.0f * TIP_TAIL_BAND;
	rt->tip_h          = card_h + 2.0f * TIP_TAIL_BAND;

	FluxRect *rects [] = {&rt->action_rect, &rt->close_rect, &rt->x_rect, &rt->title_rect, &rt->subtitle_rect};
	for (int i = 0; i < 5; i++) {
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

typedef enum TipAlign
{
	TIP_NEAR,
	TIP_MIDDLE,
	TIP_FAR,
} TipAlign;

typedef struct TipPlacement {
	TipAlign x;
	TipAlign y;
	uint8_t  tail_edge;
} TipPlacement;

static TipPlacement tip_placement(FluxTeachingTipPlacement p) {
	static TipPlacement const placements [] = {
	  [FLUX_TIP_PLACE_TOP]          = {TIP_MIDDLE, TIP_NEAR,   FLUX_TIP_TAIL_EDGE_BOTTOM},
	  [FLUX_TIP_PLACE_BOTTOM]       = {TIP_MIDDLE, TIP_FAR,    FLUX_TIP_TAIL_EDGE_TOP   },
	  [FLUX_TIP_PLACE_LEFT]         = {TIP_NEAR,   TIP_MIDDLE, FLUX_TIP_TAIL_EDGE_RIGHT },
	  [FLUX_TIP_PLACE_RIGHT]        = {TIP_FAR,    TIP_MIDDLE, FLUX_TIP_TAIL_EDGE_LEFT  },
	  [FLUX_TIP_PLACE_TOP_RIGHT]    = {TIP_FAR,    TIP_NEAR,   FLUX_TIP_TAIL_EDGE_BOTTOM},
	  [FLUX_TIP_PLACE_TOP_LEFT]     = {TIP_NEAR,   TIP_NEAR,   FLUX_TIP_TAIL_EDGE_BOTTOM},
	  [FLUX_TIP_PLACE_BOTTOM_RIGHT] = {TIP_FAR,    TIP_FAR,    FLUX_TIP_TAIL_EDGE_TOP   },
	  [FLUX_TIP_PLACE_BOTTOM_LEFT]  = {TIP_NEAR,   TIP_FAR,    FLUX_TIP_TAIL_EDGE_TOP   },
	  [FLUX_TIP_PLACE_LEFT_TOP]     = {TIP_NEAR,   TIP_NEAR,   FLUX_TIP_TAIL_EDGE_RIGHT },
	  [FLUX_TIP_PLACE_LEFT_BOTTOM]  = {TIP_NEAR,   TIP_FAR,    FLUX_TIP_TAIL_EDGE_RIGHT },
	  [FLUX_TIP_PLACE_RIGHT_TOP]    = {TIP_FAR,    TIP_NEAR,   FLUX_TIP_TAIL_EDGE_LEFT  },
	  [FLUX_TIP_PLACE_RIGHT_BOTTOM] = {TIP_FAR,    TIP_FAR,    FLUX_TIP_TAIL_EDGE_LEFT  },
	  [FLUX_TIP_PLACE_CENTER]       = {TIP_MIDDLE, TIP_MIDDLE, FLUX_TIP_TAIL_EDGE_BOTTOM},
	};
	if (p != FLUX_TIP_PLACE_AUTO && ( unsigned ) p < sizeof(placements) / sizeof(placements [0])) return placements [p];
	return (TipPlacement) {TIP_MIDDLE, TIP_FAR, FLUX_TIP_TAIL_EDGE_NONE};
}

static bool tip_tail_on_side(TipPlacement p) {
	return p.tail_edge == FLUX_TIP_TAIL_EDGE_LEFT || p.tail_edge == FLUX_TIP_TAIL_EDGE_RIGHT;
}

static FluxPoint tip_origin_targeted(TipGeom const *g, TipPlacement p) {
	FluxRect const *t = &g->tgt;
	float           w = g->tip_w, h = g->tip_h, m = g->margin;
	float           cx = t->x + t->w * 0.5f, cy = t->y + t->h * 0.5f;
	if (tip_tail_on_side(p)) {
		float const x [] = {t->x - w - m, cx - w * 0.5f, t->x + t->w + m};
		float const y [] = {cy - h + TIP_TAIL_CENTER, cy - h * 0.5f, cy - TIP_TAIL_CENTER};
		return (FluxPoint) {x [p.x], y [p.y]};
	}
	float const x [] = {cx - w + TIP_TAIL_CENTER, cx - w * 0.5f, cx - TIP_TAIL_CENTER};
	float const y [] = {t->y - h - m, cy - h - m, t->y + t->h + m};
	return (FluxPoint) {x [p.x], y [p.y]};
}

static FluxPoint tip_origin_untargeted(TipGeom const *g, TipPlacement p) {
	FluxRect const *win  = &g->win;
	float           e    = TIP_EDGE_MARGIN + g->margin;
	float const     x [] = {win->x + e, win->x + (win->w - g->tip_w) * 0.5f, win->x + win->w - g->tip_w - e};
	float const     y [] = {win->y + e, win->y + (win->h - g->tip_h) * 0.5f, win->y + win->h - g->tip_h - e};
	return (FluxPoint) {x [p.x], y [p.y]};
}

static float tip_tail_offset(float extent, TipAlign align, float inset) {
	float const offsets [] = {extent - inset, extent * 0.5f, inset};
	return offsets [align];
}

static void tip_set_tail(FluxTipRuntime *rt, TipPlacement p, bool visible) {
	float w = rt->tip_w, h = rt->tip_h;
	bool  side      = tip_tail_on_side(p);
	rt->tail_edge   = visible ? p.tail_edge : FLUX_TIP_TAIL_EDGE_NONE;
	rt->tail_center = side ? tip_tail_offset(h, p.y, TIP_TAIL_CENTER) : tip_tail_offset(w, p.x, TIP_TAIL_CENTER);
	rt->scale_cx    = w * 0.5f;
	rt->scale_cy    = h * 0.5f;
	if (rt->tail_edge == FLUX_TIP_TAIL_EDGE_NONE) return;
	if (side) {
		rt->scale_cx = p.tail_edge == FLUX_TIP_TAIL_EDGE_LEFT ? TIP_TAIL_BAND : w - TIP_TAIL_BAND;
		rt->scale_cy = tip_tail_offset(h, p.y, TIP_SCALE_CORNER);
		return;
	}
	rt->scale_cx = tip_tail_offset(w, p.x, TIP_SCALE_CORNER);
	rt->scale_cy = p.tail_edge == FLUX_TIP_TAIL_EDGE_TOP ? TIP_TAIL_BAND : h - TIP_TAIL_BAND;
}

static FluxTeachingTipPlacement tip_flow_placement(FluxTeachingTipPlacement p) {
	static FluxTeachingTipPlacement const mirrored [] = {
	  [FLUX_TIP_PLACE_AUTO]         = FLUX_TIP_PLACE_AUTO,
	  [FLUX_TIP_PLACE_TOP]          = FLUX_TIP_PLACE_TOP,
	  [FLUX_TIP_PLACE_BOTTOM]       = FLUX_TIP_PLACE_BOTTOM,
	  [FLUX_TIP_PLACE_LEFT]         = FLUX_TIP_PLACE_RIGHT,
	  [FLUX_TIP_PLACE_RIGHT]        = FLUX_TIP_PLACE_LEFT,
	  [FLUX_TIP_PLACE_TOP_RIGHT]    = FLUX_TIP_PLACE_TOP_LEFT,
	  [FLUX_TIP_PLACE_TOP_LEFT]     = FLUX_TIP_PLACE_TOP_RIGHT,
	  [FLUX_TIP_PLACE_BOTTOM_RIGHT] = FLUX_TIP_PLACE_BOTTOM_LEFT,
	  [FLUX_TIP_PLACE_BOTTOM_LEFT]  = FLUX_TIP_PLACE_BOTTOM_RIGHT,
	  [FLUX_TIP_PLACE_LEFT_TOP]     = FLUX_TIP_PLACE_RIGHT_TOP,
	  [FLUX_TIP_PLACE_LEFT_BOTTOM]  = FLUX_TIP_PLACE_RIGHT_BOTTOM,
	  [FLUX_TIP_PLACE_RIGHT_TOP]    = FLUX_TIP_PLACE_LEFT_TOP,
	  [FLUX_TIP_PLACE_RIGHT_BOTTOM] = FLUX_TIP_PLACE_LEFT_BOTTOM,
	  [FLUX_TIP_PLACE_CENTER]       = FLUX_TIP_PLACE_CENTER,
	};
	return ( unsigned ) p < sizeof(mirrored) / sizeof(mirrored [0]) ? mirrored [p] : p;
}

static void tip_logical_tail(FluxTipRuntime *rt) {
	if (rt->tail_edge == FLUX_TIP_TAIL_EDGE_LEFT) rt->tail_edge = FLUX_TIP_TAIL_EDGE_RIGHT;
	else if (rt->tail_edge == FLUX_TIP_TAIL_EDGE_RIGHT) rt->tail_edge = FLUX_TIP_TAIL_EDGE_LEFT;
	else rt->tail_center = rt->tip_w - rt->tail_center;
	rt->scale_cx = rt->tip_w - rt->scale_cx;
}

static FluxRect tip_window_rect_px(FluxTipRuntime const *rt) {
	HWND hwnd = flux_window_hwnd(rt->window);
	RECT rc   = {0};
	if (!hwnd || !GetClientRect(hwnd, &rc)) return (FluxRect) {0};
	POINT tl = {rc.left, rc.top};
	ClientToScreen(hwnd, &tl);
	return (FluxRect) {( float ) tl.x, ( float ) tl.y, ( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

static void               tip_uia_close(FluxStore *store, XentNodeId node);
static FluxUiaHooks const tip_window_uia;
static FluxUiaHooks const tip_pane_uia;

static void               tip_place(FluxTipRuntime *rt, XentNodeId node, FluxRect r) {
	bool shown = r.w > 0.0f && r.h > 0.0f;
	xent_setcollapsed(rt->ctx, node, !shown);
	if (!shown) return;
	xent_setpos(rt->ctx, node, (XentPoint) {r.x, r.y});
	xent_setsize(rt->ctx, node, (XentSize) {r.w, r.h});
}

static void tip_place_parts(FluxTipRuntime *rt) {
	FluxTeachingTipData const *m = &rt->model;
	flux_prop_text_content(rt->store, rt->parts [0], m->title ? m->title : "");
	flux_prop_text_content(rt->store, rt->parts [1], m->subtitle ? m->subtitle : "");
	flux_prop_button_label(rt->store, rt->parts [2], m->action_text ? m->action_text : "");
	flux_prop_button_style(rt->store, rt->parts [2], m->action_accent ? FLUX_BUTTON_ACCENT : FLUX_BUTTON_STANDARD);
	flux_prop_button_label(rt->store, rt->parts [3], m->close_text ? m->close_text : "");
	FluxRect const rects [5] = {rt->title_rect, rt->subtitle_rect, rt->action_rect, rt->close_rect, rt->x_rect};
	for (int i = 0; i < 5; i++) tip_place(rt, rt->parts [i], rects [i]);
	xent_setpos(rt->ctx, rt->surface, (XentPoint) {0.0f, 0.0f});
	xent_setsize(rt->ctx, rt->surface, (XentSize) {rt->tip_w, rt->tip_h});
	xent_layout(rt->ctx, rt->surface, rt->tip_w, rt->tip_h);
	FluxNodeData *nd = flux_store_find(rt->store, rt->surface);
	if (nd) nd->uia = m->light_dismiss ? &tip_window_uia : &tip_pane_uia;
}

static FluxTeachingTipPlacement tip_effective_placement(
  FluxTipRuntime *rt, TipGeom *g, float scale, FluxTeachingTipPlacement preferred, bool targeted
) {
	if (targeted) {
		FluxRect a = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->target);
		g->tgt     = (FluxRect) {a.x / scale, a.y / scale, a.w / scale, a.h / scale};
		return tip_solve(g, preferred);
	}
	if (g->tip_w > g->win.w || g->tip_h > g->win.h) return FLUX_TIP_PLACE_AUTO;
	return preferred == FLUX_TIP_PLACE_AUTO ? FLUX_TIP_PLACE_BOTTOM : preferred;
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
	bool                     rtl      = xent_resolved_dir(rt->ctx, rt->node) == XENT_DIRECTION_RTL;
	FluxTeachingTipPlacement pref     = rtl ? tip_flow_placement(rt->model.preferred) : rt->model.preferred;
	FluxTeachingTipPlacement eff      = tip_effective_placement(rt, &g, scale, pref, targeted);
	if (eff == FLUX_TIP_PLACE_AUTO) return false;

	TipPlacement p      = tip_placement(eff);
	FluxPoint    origin = targeted ? tip_origin_targeted(&g, p) : tip_origin_untargeted(&g, p);
	bool         tail   = rt->model.tail_visibility == FLUX_TIP_TAIL_VISIBLE
	                   || (rt->model.tail_visibility == FLUX_TIP_TAIL_AUTO && targeted);
	rt->model.effective = eff;
	tip_set_tail(rt, p, tail);
	if (rtl) tip_logical_tail(rt);

	tip_place_parts(rt);
	flux_popup_setsize(rt->popup, rt->tip_w, rt->tip_h);
	flux_popup_setrtl(rt->popup, rtl);
	FluxRect anchor = {origin.x * scale, origin.y * scale, 0.0f, 0.0f};
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
	return true;
}

static void tip_uia_opened(FluxTipRuntime *rt) {
	char text [512];
	snprintf(text, sizeof(text), "Press F6 to go to new notification, %s.", rt->model.title ? rt->model.title : "");
	flux_uia_notify(
	  rt->store, rt->surface, FLUX_UIA_NOTE_OTHER, FLUX_UIA_PROC_CURRENT_RECENT, text, "TeachingTipOpenedActivityId"
	);
	if (rt->model.light_dismiss) flux_uia_note(rt->store, rt->surface, -1, FLUX_UIA_EVT_WINDOW_OPENED);
}

static void tip_uia_closed(FluxTipRuntime const *rt) {
	if (rt->model.light_dismiss) flux_uia_note(rt->store, rt->surface, -1, FLUX_UIA_EVT_WINDOW_CLOSED);
}

static void tip_restore_focus(FluxTipRuntime *rt) {
	if (!rt->input || !rt->model.light_dismiss) return;
	if (flux_input_focus(rt->input) == rt->node && rt->prev_focus != XENT_NODE_INVALID)
		flux_input_setfocus(rt->input, rt->prev_focus, false);
}

static void tip_apply_scale(FluxTipRuntime *rt) {
	float cx = flux_popup_rtl(rt->popup) ? rt->tip_w - rt->scale_cx : rt->scale_cx;
	flux_popup_setscale(rt->popup, rt->cur_sx, rt->cur_sy, cx, rt->scale_cy);
}

static bool tip_anim_finish(FluxTipRuntime *rt) {
	bool was_contract = rt->anim_contract;
	rt->anim_expand   = false;
	rt->anim_contract = false;
	rt->cur_sx        = 1.0f;
	rt->cur_sy        = 1.0f;
	tip_apply_scale(rt);

	if (was_contract) {
		FluxStore                 *store                                         = rt->store;
		XentNodeId                 node                                          = rt->node;
		void                       (*notify)(void *, FluxTeachingTipCloseReason) = rt->model.on_close;
		void                      *notify_ctx                                    = rt->model.close_ctx;
		FluxTeachingTipCloseReason reason                                        = rt->close_reason;
		flux_popup_dismiss(rt->popup);
		tip_uia_closed(rt);
		tip_restore_focus(rt);
		rt = tip_runtime(store, node);
		if (!rt) return false;
		tip_repaint_owner(rt);
		tip_route_close(rt, reason);
		if (notify) notify(notify_ctx, reason);
		rt = tip_runtime(store, node);
		if (!rt) return false;
	}

	if (!rt->pending_hide) return false;
	rt->pending_hide = false;
	tip_close(rt, rt->pending_reason);
	return rt->anim_contract;
}

static bool tip_anim_step(void *ctx, unsigned long now) {
	FluxTipRuntime *rt  = ( FluxTipRuntime * ) ctx;
	float           dur = rt->anim_expand ? TIP_EXPAND_MS : TIP_CONTRACT_MS;
	float           t   = flux_clamp01(( float ) (( DWORD ) now - rt->anim_start) / dur);
	float           eased
	  = rt->anim_expand ? flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f) : flux_cubic_bezier(t, 0.7f, 0.0f, 1.0f, 0.5f);
	rt->cur_sx = rt->from_sx + (rt->to_sx - rt->from_sx) * eased;
	rt->cur_sy = rt->from_sy + (rt->to_sy - rt->from_sy) * eased;
	tip_apply_scale(rt);
	tip_repaint_popup(rt);
	if (t < 1.0f) return true;
	return tip_anim_finish(rt);
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
	tip_apply_scale(rt);
	flux_anim_register(rt, tip_anim_step);
}

static void tip_open_failed(FluxTipRuntime *rt) {
	void  (*notify)(void *, FluxTeachingTipCloseReason) = rt->model.on_close;
	void *notify_ctx                                    = rt->model.close_ctx;
	rt->model.effective                                 = FLUX_TIP_PLACE_AUTO;
	tip_route_close(rt, FLUX_TIP_CLOSE_PROGRAMMATIC);
	if (notify) notify(notify_ctx, FLUX_TIP_CLOSE_PROGRAMMATIC);
}

static FluxTipRuntime *tip_focus_on_open(FluxTipRuntime *rt) {
	if (!rt->model.light_dismiss || !rt->input) return rt;
	FluxStore *store = rt->store;
	XentNodeId node  = rt->node;
	rt->prev_focus   = flux_input_focus(rt->input);
	flux_input_setfocus(rt->input, node, false);
	return tip_runtime(store, node);
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
		tip_open_failed(rt);
		return;
	}

	rt->model.open = true;
	tip_uia_opened(rt);
	rt = tip_focus_on_open(rt);
	if (!rt) return;
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
	tip_uia_closed(rt);
	tip_restore_focus(rt);
	rt = tip_runtime(store, node);
	if (!rt) return;
	tip_repaint_owner(rt);
	tip_route_close(rt, FLUX_TIP_CLOSE_LIGHT_DISMISS);
	if (notify) notify(notify_ctx, FLUX_TIP_CLOSE_LIGHT_DISMISS);
}

static void tip_action_click(void *ctx) {
	FluxTipRuntime *rt = ( FluxTipRuntime * ) ctx;
	tip_route(rt, XTK_EVENT_INVOKE, (XtkEventPayload) {0}, XTK_EVENT_CAUSE_USER);
	if (rt->model.on_action) rt->model.on_action(rt->model.action_ctx);
}

static void tip_close_click(void *ctx) { tip_close(( FluxTipRuntime * ) ctx, FLUX_TIP_CLOSE_BUTTON); }

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

void flux_snap_teachtip(FluxSnapCtx const *ctx) {
	FluxTipRuntime *rt           = ( FluxTipRuntime * ) ctx->data;
	ctx->snap->u.teach_tip.paint = NULL;
	if (!rt || ctx->node != rt->surface) return;
	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	tip_resolve_theme(rt, &theme, &is_dark);
	FluxColor bg = theme->solid_background_tertiary;
	if (!theme->contrast.active && rt->model.light_dismiss)
		bg = flux_popup_acrylictint(rt->popup, is_dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : bg);
	rt->paint = (FluxTeachingTipPaintInfo) {
	  .model       = &rt->model,
	  .tip_w       = rt->tip_w,
	  .tip_h       = rt->tip_h,
	  .tail_edge   = rt->tail_edge,
	  .tail_center = rt->tail_center,
	  .background  = bg};
	ctx->snap->u.teach_tip.paint = &rt->paint;
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
	if (rt->surface) xent_node_destroy(rt->ctx, rt->surface);
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

static void tip_uia_close(FluxStore *store, XentNodeId node) {
	FluxNodeData const *nd = flux_store_find(store, node);
	if (nd && nd->component_data) tip_close(( FluxTipRuntime * ) nd->component_data, FLUX_TIP_CLOSE_PROGRAMMATIC);
}

static FluxUiaHooks const tip_window_uia
  = {.role = FLUX_UIA_WINDOW, .patterns = FLUX_UIA_P_WINDOW, .close = tip_uia_close, .modal = true};
static FluxUiaHooks const tip_pane_uia = {.role = FLUX_UIA_PANE, .patterns = FLUX_UIA_P_WINDOW, .close = tip_uia_close};

static void               tip_press_noop(void *ctx, float x, float y, int clicks) {
	( void ) ctx;
	( void ) x;
	( void ) y;
	( void ) clicks;
}

static XentNodeId tip_text_part(FluxTipRuntime *rt, bool title) {
	XentNodeId n = flux_text_init(&(FluxTextDesc) {
	  .ctx = rt->ctx, .store = rt->store, .parent = rt->surface, .content = "", .font_size = 14.0f, .text = rt->text});
	if (n == XENT_NODE_INVALID) return n;
	flux_prop_text_wrap(rt->store, n, true);
	flux_prop_text_weight(rt->store, n, title ? FLUX_FONT_SEMI_BOLD : FLUX_FONT_REGULAR);
	return n;
}

static XentNodeId tip_button_part(FluxTipRuntime *rt, void (*click)(void *), char const *icon) {
	XentNodeId n = flux_button_init(&(FluxButtonDesc) {
	  .ctx = rt->ctx, .store = rt->store, .parent = rt->surface, .on_click = click, .userdata = rt});
	if (n == XENT_NODE_INVALID || !icon) return n;
	flux_prop_button_style(rt->store, n, FLUX_BUTTON_SUBTLE);
	flux_prop_button_icon(rt->store, n, icon);
	FluxNodeData *nd = flux_store_find(rt->store, n);
	if (nd && nd->component_data) {
		FluxButtonData *button     = ( FluxButtonData * ) nd->component_data;
		button->teaching_tip_close = true;
		button->font_size          = 16.0f / FLUX_BTN_ICON_SIZE_MUL;
	}
	return n;
}

static bool tip_build_content(FluxTipRuntime *rt) {
	rt->surface      = flux_factory_create_node(rt->ctx, rt->store, XENT_NODE_INVALID, FLUX_CONTROL_TEACHING_TIP);
	FluxNodeData *nd = rt->surface ? flux_store_find(rt->store, rt->surface) : NULL;
	if (!nd) return false;
	xent_setproto(rt->ctx, rt->surface, XENT_PROTOCOL_ABSOLUTE);
	nd->component_data           = &rt->model;
	nd->behavior.on_pointer_down = tip_press_noop;
	rt->parts [0]                = tip_text_part(rt, true);
	rt->parts [1]                = tip_text_part(rt, false);
	rt->parts [2]                = tip_button_part(rt, tip_action_click, NULL);
	rt->parts [3]                = tip_button_part(rt, tip_close_click, NULL);
	rt->parts [4]                = tip_button_part(rt, tip_close_click, "Cancel");
	for (int i = 0; i < 5; i++)
		if (rt->parts [i] == XENT_NODE_INVALID) return false;
	return flux_popup_setcontent(rt->popup, rt->ctx, rt->store, rt->surface)
	    && (!rt->input || flux_popup_sethost(rt->popup, rt->input, rt->node));
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
		flux_popup_ondismiss(rt->popup, tip_dismissed, rt);
	}
	if (rt->popup && !tip_build_content(rt)) {
		flux_popup_destroy(rt->popup);
		rt->popup = NULL;
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
