#include "a11y/flux_uia_ids.h"
#include "controls/flux_control_uia.h"
#include "controls/flux_module.h"
#include "controls/factory/flux_factory.h"
#include "render/flux_fluent.h"
#include "store/flux_node_data.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"
#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_window.h"
#include "window/flux_window_internal.h"
#include <xtk/xtk_catalog.h>
#include <xtk/xtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static FluxTitleBarData *titlebar_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TITLE_BAR || !nd->component_data) return NULL;
	return ( FluxTitleBarData * ) nd->component_data;
}

static float titlebar_text_width(FluxTitleBarData const *d, char const *text) {
	if (!text || !text [0]) return 0.0f;
	XentTextMetrics    tm  = {0};
	XentTextMeasureReq req = {
	  .text              = text,
	  .font_size         = FLUX_TB_TEXT_SIZE,
	  .line_break_policy = XENT_LINEBREAK_NOWRAP,
	  .width_mode        = XENT_MEASURE_UNDEFINED};
	return xent_text_measure(d->ctx, &req, &tm) ? tm.width : 0.0f;
}

static void titlebar_measure_title(FluxTitleBarData *d) {
	d->title_w    = titlebar_text_width(d, d->title);
	d->subtitle_w = titlebar_text_width(d, d->subtitle);
}

static void titlebar_set_icon(FluxTitleBarData *d, char const *name) {
	d->icon_glyph [0] = '\0';
	if (!name || !name [0]) return;
	wchar_t const *cp = flux_icon_lookup(name);
	if (cp) flux_icon_to_utf8(cp, d->icon_glyph, sizeof(d->icon_glyph));
	else {
		size_t n = strlen(name);
		if (n < sizeof(d->icon_glyph)) memcpy(d->icon_glyph, name, n + 1);
	}
}

static FluxRect titlebar_window_rect(XentRect const *node, FluxRect const *part, bool rtl) {
	float x = rtl ? node->w - part->x - part->w : part->x;
	return (FluxRect) {node->x + x, node->y + part->y, part->w, part->h};
}

static bool tb_in(FluxRect const *r, float x, float y) {
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static bool titlebar_can_focus(FluxTitleBarData const *d, int part) {
	if (part == FLUX_TB_REGION_BACK) return d->show_back && !d->back_disabled;
	return part == FLUX_TB_REGION_PANE && d->show_pane_toggle;
}

static void titlebar_set_focus_part(FluxTitleBarData *d, int part) {
	d->focus_part   = part;
	d->activate_key = FLUX_KEY_NONE;
}

static void titlebar_update_focusable(FluxTitleBarData *d) {
	bool can_tab = titlebar_can_focus(d, FLUX_TB_REGION_BACK) || titlebar_can_focus(d, FLUX_TB_REGION_PANE);
	xent_set_focusable(d->ctx, d->node, can_tab);
	if (!titlebar_can_focus(d, d->focus_part)) titlebar_set_focus_part(d, FLUX_TB_REGION_NONE);
}

static void titlebar_tab_to(FluxTitleBarData *d, int part) {
	titlebar_set_focus_part(d, part);
	FluxNodeData *nd = flux_store_find(d->store, d->node);
	if (!nd) return;
	nd->state.focused        = 1;
	nd->state.suppress_focus = 0;
}

static bool titlebar_tab(void *ctx, bool shift, bool entering) {
	FluxTitleBarData *d = ctx;
	if (!xent_sem_enabled(d->ctx, d->node)) return false;
	int first = shift ? FLUX_TB_REGION_PANE : FLUX_TB_REGION_BACK;
	int last  = shift ? FLUX_TB_REGION_BACK : FLUX_TB_REGION_PANE;
	if ((entering || d->focus_part == FLUX_TB_REGION_NONE) && titlebar_can_focus(d, first)) {
		titlebar_tab_to(d, first);
		return true;
	}
	if ((!entering && d->focus_part == last) || !titlebar_can_focus(d, last)) return false;
	titlebar_tab_to(d, last);
	return true;
}

static void titlebar_blur(void *ctx) { titlebar_set_focus_part(ctx, FLUX_TB_REGION_NONE); }

static int  titlebar_hit(FluxTitleBarData const *d, float x, float y) {
	XentRect r = {0};
	xent_layout_rect(d->ctx, d->node, &r);
	FluxRect           b = {0.0f, 0.0f, r.w, r.h};
	FluxTitleBarLayout l
	  = flux_titlebar_layout(&b, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', &d->frame);
	if (l.has_back && !d->back_disabled && tb_in(&l.back, x, y)) return FLUX_TB_REGION_BACK;
	if (l.has_toggle && tb_in(&l.toggle, x, y)) return FLUX_TB_REGION_PANE;
	if (!l.has_caption) return FLUX_TB_REGION_NONE;
	if (tb_in(&l.min, x, y)) return FLUX_TB_REGION_MIN;
	if (tb_in(&l.max, x, y)) return FLUX_TB_REGION_MAX;
	if (tb_in(&l.close, x, y)) return FLUX_TB_REGION_CLOSE;
	return FLUX_TB_REGION_NONE;
}

static char const *titlebar_tooltip_at(void *ctx, float x, float y) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	switch (titlebar_hit(d, x, y)) {
	case FLUX_TB_REGION_BACK  : return "Back";
	case FLUX_TB_REGION_PANE  : return "Toggle Navigation";
	case FLUX_TB_REGION_MIN   : return "Minimize";
	case FLUX_TB_REGION_MAX   : return NULL;
	case FLUX_TB_REGION_CLOSE : return "Close";
	default                   : return NULL;
	}
}

static void titlebar_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	d->pressed          = titlebar_hit(d, x, y);
	bool nav            = titlebar_can_focus(d, d->pressed);
	titlebar_set_focus_part(d, nav ? d->pressed : FLUX_TB_REGION_NONE);
	FluxNodeData *nd = flux_store_find(d->store, d->node);
	if (!nd) return;
	nd->state.focused        = nav;
	nd->state.suppress_focus = nav;
}

static void titlebar_route(FluxTitleBarData *d, XtkEventId id) {
	XtkHost *host = flux_store_host(d->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) d->node,
	  .source         = ( XtkCtrl ) d->node,
	  .current_target = ( XtkCtrl ) d->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	};
	flux_host_route(host, &event);
}

static void titlebar_invoke(FluxTitleBarData *d, int region) {
	HWND hwnd = d->window ? flux_window_hwnd(d->window) : NULL;
	switch (region) {
	case FLUX_TB_REGION_BACK :
		titlebar_route(d, XTK_EVENT_INVOKE);
		if (d->on_back) d->on_back(d->on_back_ctx);
		break;
	case FLUX_TB_REGION_PANE :
		titlebar_route(d, XTK_EVENT_TOGGLE);
		if (d->on_pane_toggle) d->on_pane_toggle(d->on_pane_toggle_ctx);
		break;
	case FLUX_TB_REGION_MIN :
		if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
		break;
	case FLUX_TB_REGION_MAX :
		if (hwnd) ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
		break;
	case FLUX_TB_REGION_CLOSE :
		if (hwnd) PostMessageW(hwnd, WM_CLOSE, 0, 0);
		break;
	default : break;
	}
}

static void titlebar_click(void *ctx) {
	FluxTitleBarData *d   = ( FluxTitleBarData * ) ctx;
	FluxNodeData     *nd  = flux_store_find(d->store, d->node);
	int               hit = nd ? titlebar_hit(d, nd->hover_local_x, nd->hover_local_y) : FLUX_TB_REGION_NONE;
	int               was = d->pressed;
	d->pressed            = FLUX_TB_REGION_NONE;
	if (hit == was && hit != FLUX_TB_REGION_NONE) titlebar_invoke(d, hit);
}

static bool titlebar_key(void *ctx, FluxKey key, bool down) {
	FluxTitleBarData *d = ctx;
	if (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE) return false;
	if (!titlebar_can_focus(d, d->focus_part)) return false;
	if (!xent_sem_enabled(d->ctx, d->node)) {
		d->activate_key = FLUX_KEY_NONE;
		return false;
	}
	if (down && d->activate_key == FLUX_KEY_NONE && d->pressed == FLUX_TB_REGION_NONE) d->activate_key = key;
	if (down || d->activate_key != key) return true;
	d->activate_key = FLUX_KEY_NONE;
	titlebar_invoke(d, d->focus_part);
	return true;
}

static void titlebar_cancel(void *ctx) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) ctx;
	d->pressed          = FLUX_TB_REGION_NONE;
}

static void titlebar_destroy(void *component_data) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) component_data;
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->subtitle);
	free(d);
}

XentNodeId flux_titlebar_init(FluxTitleBarDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TITLE_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, node);
	FluxTitleBarData *d  = nd ? ( FluxTitleBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx                = info->ctx;
	d->store              = info->store;
	d->node               = node;
	d->window             = info->window;
	d->title              = info->title ? flux_str_dup(info->title) : NULL;
	d->subtitle           = info->subtitle ? flux_str_dup(info->subtitle) : NULL;
	d->show_back          = info->show_back;
	d->back_disabled      = info->back_disabled;
	d->show_pane_toggle   = info->show_pane_toggle;
	d->integrate_window   = info->integrate_window;
	d->frame.integrated   = info->integrate_window;
	d->pressed            = FLUX_TB_REGION_NONE;
	d->on_back            = info->on_back;
	d->on_back_ctx        = info->back_ctx;
	d->on_pane_toggle     = info->on_pane_toggle;
	d->on_pane_toggle_ctx = info->pane_toggle_ctx;
	titlebar_set_icon(d, info->icon);
	titlebar_measure_title(d);

	nd->component_data               = d;
	nd->destroy_component_data       = titlebar_destroy;
	nd->behavior.on_pointer_down     = titlebar_pointer_down;
	nd->behavior.on_pointer_down_ctx = d;
	nd->behavior.on_click            = titlebar_click;
	nd->behavior.on_click_ctx        = d;
	nd->behavior.tooltip_at          = titlebar_tooltip_at;
	nd->behavior.tooltip_at_ctx      = d;
	nd->behavior.on_cancel           = titlebar_cancel;
	nd->behavior.on_cancel_ctx       = d;
	nd->behavior.on_tab              = titlebar_tab;
	nd->behavior.on_tab_ctx          = d;
	nd->behavior.on_key              = titlebar_key;
	nd->behavior.on_key_ctx          = d;
	nd->behavior.on_blur             = titlebar_blur;
	nd->behavior.on_blur_ctx         = d;

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	titlebar_update_focusable(d);
	xent_setproto(info->ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_seth(info->ctx, node, FLUX_TB_HEIGHT);
	xent_setself(info->ctx, node, XENT_FLEX_ALIGN_STRETCH);
	xent_setgrow(info->ctx, node, 0.0f);

	if (d->integrate_window && d->window) flux_window_extendtitlebar(d->window, true);
	return node;
}

static FluxTitleBarFrame titlebar_system_frame(FluxWindow *window, XentRect const *bar, bool rtl) {
	FluxTitleBarFrame frame   = {.integrated = true};
	FluxRect          buttons = {0};
	frame.native_buttons      = flux_window_caption_bounds(window, &buttons);
	if (!frame.native_buttons || buttons.y >= bar->y + bar->h || buttons.y + buttons.h <= bar->y) return frame;
	float inset = fminf(bar->w, fmaxf(0.0f, bar->x + bar->w - buttons.x));
	if (rtl) frame.leading_inset = inset;
	else frame.trailing_inset = inset;
	return frame;
}

static bool titlebar_slots(FluxTitleBarData *d, XentNodeId slots [3]) {
	bool present = false;
	for (XentNodeId node = xent_node_first(d->ctx, d->node); node; node = xent_node_next(d->ctx, node)) {
		FluxNodeData const *nd = flux_store_find(d->store, node);
		if (!nd || nd->nav_slot < XTK_TITLE_BAR_SLOT_LEFT_HEADER || nd->nav_slot > XTK_TITLE_BAR_SLOT_RIGHT_HEADER)
			continue;
		if (!xent_node_first(d->ctx, node)) continue;
		slots [nd->nav_slot - XTK_TITLE_BAR_SLOT_LEFT_HEADER] = node;
		present                                               = true;
	}
	return present;
}

static XentSize titlebar_slot_size(XentCtx *ctx, XentNodeId node, float height) {
	XentSize size = {0};
	xent_layout_desired_size(ctx, xent_node_first(ctx, node), (XentSize) {INFINITY, height}, &size);
	return size;
}

static float titlebar_text_extent(FluxTitleBarData const *d) {
	float title    = d->title_w > 0.0f ? d->title_w + FLUX_TB_TITLE_MARGIN : 0.0f;
	float subtitle = d->subtitle_w > 0.0f ? d->subtitle_w + FLUX_TB_SUBTITLE_MARGIN : 0.0f;
	return title + subtitle;
}

static bool titlebar_place_slot(FluxTitleBarData *d, unsigned slot, XentNodeId node, FluxRect box) {
	if (d->slot_node [slot] == node && memcmp(&d->slot_rect [slot], &box, sizeof(box)) == 0) return false;
	d->slot_node [slot] = node;
	d->slot_rect [slot] = box;
	if (!node) return true;
	FluxNodeData *nd = flux_store_find(d->store, node);
	if (nd) nd->clips_children = true;
	xent_setpos(d->ctx, node, (XentPoint) {box.x, box.y});
	xent_setsize(d->ctx, node, (XentSize) {box.w, box.h});
	return true;
}

static bool titlebar_layout_slots(FluxTitleBarData *d, XentNodeId const slots [3], FluxRect bounds) {
	XentSize left               = titlebar_slot_size(d->ctx, slots [0], bounds.h);
	XentSize center             = titlebar_slot_size(d->ctx, slots [1], bounds.h);
	XentSize right              = titlebar_slot_size(d->ctx, slots [2], bounds.h);
	d->frame.left_header_width  = left.w;
	d->frame.right_header_width = right.w;
	FluxTitleBarLayout layout
	  = flux_titlebar_layout(&bounds, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', &d->frame);
	float text_w          = titlebar_text_extent(d);
	float free_w          = fmaxf(0.0f, layout.right_header_x - layout.title_x);
	d->frame.compact_text = slots [1] && center.w + text_w >= free_w;
	float content_x       = layout.title_x + (d->frame.compact_text ? 0.0f : text_w);
	float available       = fmaxf(0.0f, layout.right_header_x - content_x - (d->frame.compact_text ? 16.0f : 0.0f));
	center.w              = fminf(center.w, available);
	if (!d->frame.compact_text) content_x += (available - center.w) * 0.5f;
	bool changed = titlebar_place_slot(
	  d, 0, slots [0], (FluxRect) {layout.left_header_x, (bounds.h - left.h) * 0.5f, left.w, left.h}
	);
	changed
	  |= titlebar_place_slot(d, 1, slots [1], (FluxRect) {content_x, (bounds.h - center.h) * 0.5f, center.w, center.h});
	changed |= titlebar_place_slot(
	  d, 2, slots [2], (FluxRect) {layout.right_header_x, (bounds.h - right.h) * 0.5f, right.w, right.h}
	);
	return changed;
}

uint32_t flux_titlebar_frame(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) store;
	FluxTitleBarData *d    = nd->component_data;
	XentRect          rect = {0};
	if (!d || !xent_layout_rect(ctx, node, &rect)) return 0u;
	XentNodeId slots [3] = {0};
	bool       present   = titlebar_slots(d, slots);
	bool       changed   = d->has_slots != present;
	d->has_slots         = present;
	if (changed) xent_seth(ctx, node, present ? FLUX_TB_TALL_HEIGHT : FLUX_TB_HEIGHT);
	if (d->integrate_window && d->window) {
		bool rtl = xent_resolved_dir(ctx, node) == XENT_DIRECTION_RTL;
		d->frame = titlebar_system_frame(d->window, &rect, rtl);
	}
	float height = present ? FLUX_TB_TALL_HEIGHT : rect.h;
	if (present || changed) changed |= titlebar_layout_slots(d, slots, (FluxRect) {0, 0, rect.w, height});
	return changed ? FLUX_FRAME_REDRAW : 0u;
}

static int titlebar_add_slot_regions(FluxTitleBarData const *d, XentRect const *r, bool rtl, FluxRect *pass) {
	int n = 0;
	for (unsigned i = 0; i < 3; i++)
		if (d->slot_node [i]) pass [n++] = titlebar_window_rect(r, &d->slot_rect [i], rtl);
	return n;
}

static void titlebar_sync_hit_regions(FluxTitleBarData *d, XentRect const *r, bool rtl) {
	FluxRect           bounds = {r->x, r->y, r->w, r->h};
	FluxRect           local  = {0.0f, 0.0f, r->w, r->h};
	FluxTitleBarLayout l
	  = flux_titlebar_layout(&local, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', &d->frame);

	FluxRect pass [FLUX_WINDOW_MAX_TITLE_BAR_PASS];
	int      n = titlebar_add_slot_regions(d, r, rtl, pass);
	if (l.has_back) pass [n++] = titlebar_window_rect(r, &l.back, rtl);
	if (l.has_toggle) pass [n++] = titlebar_window_rect(r, &l.toggle, rtl);
	if (l.has_caption) {
		pass [n++] = titlebar_window_rect(r, &l.min, rtl);
		pass [n++] = titlebar_window_rect(r, &l.max, rtl);
		pass [n++] = titlebar_window_rect(r, &l.close, rtl);
	}
	flux_window_settitlebar(d->window, bounds, pass, n);
	flux_window_setmaxbutton(d->window, titlebar_window_rect(r, &l.max, rtl));
}

void flux_titlebar_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd) {
	FluxTitleBarData *d = ( FluxTitleBarData * ) nd->component_data;
	if (!d || !d->integrate_window || !d->window) return;
	HWND hwnd    = flux_window_hwnd(d->window);
	d->maximized = hwnd && IsZoomed(hwnd);
	XentRect r   = {0};
	if (!xent_layout_rect(ctx, node, &r) || r.w <= 0.0f) return;
	bool rtl = xent_resolved_dir(ctx, node) == XENT_DIRECTION_RTL;
	titlebar_sync_hit_regions(d, &r, rtl);
}

void flux_titlebar_set_title(FluxStore *store, XentNodeId id, char const *title, char const *subtitle) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->subtitle);
	d->title    = title ? flux_str_dup(title) : NULL;
	d->subtitle = subtitle ? flux_str_dup(subtitle) : NULL;
	titlebar_measure_title(d);
}

void flux_titlebar_set_back(FluxStore *store, XentNodeId id, bool show, bool disabled) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d) return;
	d->show_back     = show;
	d->back_disabled = disabled;
	titlebar_update_focusable(d);
}

void flux_titlebar_settoggle(FluxStore *store, XentNodeId id, bool show) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d) return;
	d->show_pane_toggle = show;
	titlebar_update_focusable(d);
}

void flux_titlebar_set_icon(FluxStore *store, XentNodeId id, char const *icon) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (d) titlebar_set_icon(d, icon);
}

void flux_titlebar_set_window_integration(FluxStore *store, XentNodeId id, bool integrate) {
	FluxTitleBarData *d = titlebar_data(store, id);
	if (!d || d->integrate_window == integrate) return;
	d->integrate_window = integrate;
	d->frame            = (FluxTitleBarFrame) {.integrated = integrate};
	if (d->window) flux_window_extendtitlebar(d->window, integrate);
}

enum
{
	TB_UIA_TITLE = FLUX_TB_REGION_CLOSE + 1,
	TB_UIA_SUBTITLE,
};

static int tb_uia_parts(FluxTitleBarData const *d, int parts [4]) {
	int n = 0;
	if (d->show_back) parts [n++] = FLUX_TB_REGION_BACK;
	if (d->show_pane_toggle) parts [n++] = FLUX_TB_REGION_PANE;
	if (d->frame.compact_text) return n;
	if (d->title && d->title [0]) parts [n++] = TB_UIA_TITLE;
	if (d->subtitle && d->subtitle [0]) parts [n++] = TB_UIA_SUBTITLE;
	return n;
}

static int tb_uia_part(FluxStore *store, XentNodeId node, int sub, FluxTitleBarData **out) {
	FluxTitleBarData *d = titlebar_data(store, node);
	int               parts [4];
	int               n = d ? tb_uia_parts(d, parts) : 0;
	int               i = sub - FLUX_UIA_SUB_HEAD;
	*out                = d;
	return i >= 0 && i < n ? parts [i] : FLUX_TB_REGION_NONE;
}

int flux_titlebar_uia_heads(FluxStore *store, XentNodeId node) {
	FluxTitleBarData *d = titlebar_data(store, node);
	int               parts [4];
	return d ? tb_uia_parts(d, parts) : 0;
}

int flux_titlebar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	bool              text = part == TB_UIA_TITLE || part == TB_UIA_SUBTITLE;
	*patterns              = text ? 0u : FLUX_UIA_P_INVOKE;
	return text ? FLUX_UIA_TEXT : FLUX_UIA_BUTTON;
}

bool flux_titlebar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	if (part == FLUX_TB_REGION_NONE) return false;
	FluxRect           b = {0.0f, 0.0f, rect->w, rect->h};
	FluxTitleBarLayout l
	  = flux_titlebar_layout(&b, d->show_back, d->show_pane_toggle, d->icon_glyph [0] != '\0', &d->frame);
	bool     subtitle = part == TB_UIA_SUBTITLE;
	FluxRect p        = part == FLUX_TB_REGION_BACK ? l.back
	                  : part == FLUX_TB_REGION_PANE
	                    ? l.toggle
	                    : flux_titlebar_text_rect(
							&l, rect->h, d->title_w, subtitle ? d->subtitle_w : d->title_w, d->title && d->title [0], subtitle
						  );
	*rect             = (XentRect) {rect->x + p.x, rect->y + p.y, p.w, p.h};
	return true;
}

void flux_titlebar_uia_subname(FluxStore *store, XentNodeId node, int sub, char *out, size_t size) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	char const       *text = part == FLUX_TB_REGION_BACK ? "Back"
	                       : part == FLUX_TB_REGION_PANE ? "Toggle Navigation"
	                       : part == TB_UIA_TITLE        ? d->title
	                       : part == TB_UIA_SUBTITLE     ? d->subtitle
	                                                     : NULL;
	if (text) snprintf(out, size, "%s", text);
}

bool flux_titlebar_uia_enabled(FluxStore *store, XentNodeId node, int sub) {
	FluxTitleBarData *d = NULL;
	return tb_uia_part(store, node, sub, &d) != FLUX_TB_REGION_BACK || !d->back_disabled;
}

bool flux_titlebar_uia_focusable(FluxStore *store, XentNodeId node, int sub) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	return d && titlebar_can_focus(d, part);
}

bool flux_titlebar_uia_focused(FluxStore *store, XentNodeId node, int sub) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	return d && titlebar_can_focus(d, part) && d->focus_part == part;
}

void flux_titlebar_uia_focus(FluxStore *store, XentNodeId node, int sub) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	if (d && titlebar_can_focus(d, part)) titlebar_set_focus_part(d, part);
}

bool flux_titlebar_uia_invoke(FluxStore *store, XentNodeId node, int sub) {
	FluxTitleBarData *d    = NULL;
	int               part = tb_uia_part(store, node, sub, &d);
	if (part != FLUX_TB_REGION_BACK && part != FLUX_TB_REGION_PANE) return false;
	if (part == FLUX_TB_REGION_BACK && d->back_disabled) return false;
	titlebar_invoke(d, part);
	return true;
}

void flux_titlebar_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	FluxTitleBarData *d = titlebar_data(store, node);
	if (d && d->title) snprintf(out, size, "%s", d->title);
}
