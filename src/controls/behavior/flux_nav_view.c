#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_menu.h"
#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_title_bar_data.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

#define NAV_PANE_ANIM_MS     250.0f
#define NAV_MINIMAL_OPEN_MS  350.0f
#define NAV_MINIMAL_CLOSE_MS 120.0f
#define NAV_IND_MS           600.0f
#define NAV_IND_OP_MS        200.0f
#define NAV_SCRIM_ALPHA      0x4d
#define NAV_ADAPT_COMPACT    641.0f
#define NAV_ADAPT_EXPANDED   1008.0f
#define NAV_EXPAND_MS        200.0f
#define NAV_CHEVRON_ZONE     40.0f
#define NAV_CHILD_INDENT     31.0f

static float nav_ease_lead(float t) {
	if (t >= 1.0f / 3.0f) return 1.0f;
	return flux_cubic_bezier(t * 3.0f, 0.9f, 0.1f, 1.0f, 0.2f);
}

static float nav_ease_trail(float t) {
	if (t < 1.0f / 3.0f) return 0.0f;
	return flux_cubic_bezier((t - 1.0f / 3.0f) * 1.5f, 0.1f, 0.9f, 0.2f, 1.0f);
}

static void nav_tween_init(FluxNavTween *tw, float v) {
	tw->current = tw->start = tw->target = v;
	tw->active                           = false;
}

static void nav_tween_to(FluxNavTween *tw, float target, float duration_ms) {
	if (fabsf(target - tw->target) < 0.01f) return;
	tw->start    = tw->current;
	tw->target   = target;
	tw->start_ms = flux_anim_now();
	tw->duration = duration_ms;
	tw->active   = true;
}

static void nav_tween_snap(FluxNavTween *tw, float v) {
	tw->current = tw->start = tw->target = v;
	tw->active                           = false;
}

static bool nav_tween_tick(FluxNavTween *tw, DWORD now, float (*ease)(float)) {
	if (!tw->active) return false;
	float t = tw->duration > 0.0f ? ( float ) (now - tw->start_ms) / tw->duration : 1.0f;
	if (t >= 1.0f) {
		tw->current = tw->target;
		tw->active  = false;
		return false;
	}
	tw->current = tw->start + (tw->target - tw->start) * ease(t < 0.0f ? 0.0f : t);
	return true;
}

static FluxNavViewData *nav_data(FluxStore *store, XentNodeId nav) {
	FluxNodeData *nd = flux_store_find(store, nav);
	if (!nd || nd->component_type != FLUX_CONTROL_NAV_VIEW) return NULL;
	return ( FluxNavViewData * ) nd->component_data;
}

static void nav_repaint(FluxNavViewData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float nav_item_height(FluxNavItemKind kind);
static void  nav_apply_selection(FluxNavViewData *d, int index, bool notify);
static bool  nav_children_inline(FluxNavViewData const *d);
static bool  nav_tick_expand(FluxNavViewData *d, DWORD now);
static void  nav_snap_children(FluxNavViewData *d);
static void  nav_show_child_flyout(FluxNavViewData *d, int parent);
static void  nav_toggle_expand(FluxNavViewData *d, int idx);

static void  nav_set_abs(FluxNavViewData *d, XentNodeId node, float x, float y, float w, float h) {
	xent_setpos(d->ctx, node, (XentPoint) {x, y});
	xent_setsize(d->ctx, node, (XentSize) {w, h});
}

static void nav_sync_header_heights(FluxNavViewData *d) {
	float h = d->labels_visible ? FLUX_NAV_HEADER_H : 0.0f;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].kind == FLUX_NAV_ITEM_HEADER) xent_setsize(d->ctx, d->items [i].node, (XentSize) {NAN, h});
}

static void nav_set_scrim_alpha(FluxNavViewData *d, int alpha) {
	FluxNodeData *sn = flux_store_find(d->store, d->scrim);
	if (sn) sn->visuals.background = flux_color_rgba(0, 0, 0, alpha);
}

static void nav_apply_geometry_top(FluxNavViewData *d) {
	d->labels_visible = true;
	xent_setsize(d->ctx, d->toggle, (XentSize) {0.0f, 0.0f});
	nav_set_abs(d, d->pane, 0.0f, 0.0f, d->width, FLUX_NAV_TOP_H);
	nav_set_abs(d, d->content, 0.0f, FLUX_NAV_TOP_H, d->width, flux_maxf(0.0f, d->height - FLUX_NAV_TOP_H));
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
	nav_set_scrim_alpha(d, 0);
}

static void nav_apply_geometry_minimal(FluxNavViewData *d) {
	float top      = d->window_title_bar ? FLUX_TB_HEIGHT : (FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD);
	float pane_x   = -(1.0f - d->minimal_t.current) * FLUX_NAV_PANE_EXPANDED;
	bool  scrim_on = d->minimal_t.current > 0.01f;
	nav_set_abs(d, d->content, 0.0f, top, d->width, flux_maxf(0.0f, d->height - top));
	nav_set_abs(d, d->pane, pane_x, 0.0f, FLUX_NAV_PANE_EXPANDED, d->height);
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, scrim_on ? d->width : 0.0f, scrim_on ? d->height : 0.0f);
	nav_set_scrim_alpha(d, ( int ) (NAV_SCRIM_ALPHA * d->minimal_t.current));
}

static void nav_apply_geometry_inline(FluxNavViewData *d) {
	float flow = d->pane_w.current;
	float tb   = d->window_title_bar ? FLUX_TB_HEIGHT : 0.0f;
	nav_set_abs(d, d->pane, 0.0f, 0.0f, flow, d->height);
	nav_set_abs(d, d->content, flow, tb, flux_maxf(0.0f, d->width - flow), flux_maxf(0.0f, d->height - tb));
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
	nav_set_scrim_alpha(d, 0);
}

static void nav_apply_geometry(FluxNavViewData *d) {
	bool labels_was   = d->labels_visible;
	d->labels_visible = d->pane_w.current > (FLUX_NAV_PANE_COMPACT + FLUX_NAV_ICON_BOX);
	if (d->labels_visible != labels_was && d->mode != FLUX_NAV_TOP) {
		nav_sync_header_heights(d);
		nav_snap_children(d);
		d->ind_index  = -1;
		d->ind_primed = false;
	}

	if (d->mode == FLUX_NAV_TOP) {
		nav_apply_geometry_top(d);
		return;
	}

	if (d->title_bar != XENT_NODE_INVALID) nav_set_abs(d, d->title_bar, 0.0f, 0.0f, d->width, FLUX_TB_HEIGHT);
	xent_setsize(
	  d->ctx, d->toggle,
	  d->window_title_bar ? (XentSize) {0.0f, 0.0f} : (XentSize) {FLUX_NAV_TOGGLE_W, FLUX_NAV_TOGGLE_H}
	);

	if (d->mode == FLUX_NAV_MINIMAL) {
		nav_apply_geometry_minimal(d);
		return;
	}

	nav_apply_geometry_inline(d);
}

static int nav_indicator_item(FluxNavViewData *d) {
	int cur = d->selected;
	for (int p = (cur >= 0) ? d->items [cur].parent : -1; p >= 0; p = d->items [p].parent)
		if (!nav_children_inline(d) || !d->items [p].expanded) cur = p;
	return cur;
}

static float nav_selected_center(FluxNavViewData *d) {
	if (d->selected < 0 || d->selected >= d->count) return -1.0f;
	int      ind = nav_indicator_item(d);
	XentRect ir  = {0};
	XentRect pr  = {0};
	if (!xent_layout_rect(d->ctx, d->items [ind].node, &ir)) return -1.0f;
	if (!xent_layout_rect(d->ctx, d->pane, &pr)) return -1.0f;
	if (ir.h <= 0.0f) return -1.0f;
	d->ind_visible   = ind;
	d->ind_in_scroll = (d->items [ind].kind != FLUX_NAV_ITEM_FOOTER);
	if (d->mode == FLUX_NAV_TOP) return (ir.x + ir.w * 0.5f) - pr.x;
	return (ir.y + ir.h * 0.5f) - pr.y;
}

static void nav_update_indicator(FluxNavViewData *d) {
	bool  pane_visible = (d->mode != FLUX_NAV_MINIMAL) || d->minimal_open;
	float center       = pane_visible ? nav_selected_center(d) : -1.0f;
	if (center < 0.0f) {
		nav_tween_to(&d->ind_op, 0.0f, NAV_IND_OP_MS);
		return;
	}
	float half = (d->mode == FLUX_NAV_TOP ? FLUX_NAV_TOP_IND_W : FLUX_NAV_INDICATOR_H) * 0.5f;
	float top  = center - half;
	float bot  = center + half;
	nav_tween_to(&d->ind_op, 1.0f, NAV_IND_OP_MS);

	if (d->selected != d->ind_index) {
		bool down       = (top > d->ind_top.current);
		d->ind_top.lead = !down;
		d->ind_bot.lead = down;
		if (!d->ind_primed) {
			nav_tween_snap(&d->ind_top, top);
			nav_tween_snap(&d->ind_bot, bot);
			d->ind_primed = true;
		}
		else {
			nav_tween_to(&d->ind_top, top, NAV_IND_MS);
			nav_tween_to(&d->ind_bot, bot, NAV_IND_MS);
		}
		d->ind_index = d->selected;
		return;
	}
	if (!d->ind_top.active && fabsf(d->ind_top.target - top) > 0.5f) nav_tween_snap(&d->ind_top, top);
	if (!d->ind_bot.active && fabsf(d->ind_bot.target - bot) > 0.5f) nav_tween_snap(&d->ind_bot, bot);
}

static void nav_anim_remove(FluxNavViewData *d) {
	d->anim_active = false;
	flux_anim_unregister(d);
}

static bool nav_tick_one(FluxNavViewData *d, DWORD now) {
	bool active  = false;
	active      |= nav_tween_tick(&d->pane_w, now, flux_ease_out_cubic);
	active      |= nav_tween_tick(&d->minimal_t, now, nav_ease_lead);
	active      |= nav_tween_tick(&d->ind_top, now, d->ind_top.lead ? nav_ease_lead : nav_ease_trail);
	active      |= nav_tween_tick(&d->ind_bot, now, d->ind_bot.lead ? nav_ease_lead : nav_ease_trail);
	active      |= nav_tween_tick(&d->ind_op, now, flux_ease_out_quad);
	active      |= nav_tick_expand(d, now);
	nav_update_indicator(d);
	active |= d->ind_top.active || d->ind_bot.active || d->ind_op.active;
	active |= (d->selected >= 0 && !d->ind_primed);
	nav_apply_geometry(d);
	return active;
}

static bool nav_step(void *ctx, unsigned long now) {
	FluxNavViewData *d      = ( FluxNavViewData * ) ctx;
	bool             active = nav_tick_one(d, ( DWORD ) now);
	nav_repaint(d);
	if (!active) d->anim_active = false;
	return active;
}

static void nav_anim_start(FluxNavViewData *d) {
	d->anim_active = true;
	flux_anim_register(d, nav_step);
}

static void nav_set_mode(FluxNavViewData *d, FluxNavDisplayMode mode) {
	if (d->mode == mode || mode == FLUX_NAV_TOP || d->mode == FLUX_NAV_TOP) return;
	FluxNavDisplayMode prev = d->mode;
	d->mode                 = mode;

	if (mode == FLUX_NAV_MINIMAL) {
		d->minimal_open = false;
		nav_tween_snap(&d->minimal_t, 0.0f);
		nav_tween_snap(&d->pane_w, FLUX_NAV_PANE_EXPANDED);
	}
	else {
		float target = (mode == FLUX_NAV_COMPACT) ? FLUX_NAV_PANE_COMPACT : FLUX_NAV_PANE_EXPANDED;
		if (prev == FLUX_NAV_MINIMAL) nav_tween_snap(&d->pane_w, target);
		else nav_tween_to(&d->pane_w, target, NAV_PANE_ANIM_MS);
	}
	nav_snap_children(d);
	nav_apply_geometry(d);
	nav_anim_start(d);
}

static void nav_toggle_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode == FLUX_NAV_TOP) return;
	if (d->mode == FLUX_NAV_MINIMAL) {
		d->minimal_open = !d->minimal_open;
		nav_tween_to(
		  &d->minimal_t, d->minimal_open ? 1.0f : 0.0f, d->minimal_open ? NAV_MINIMAL_OPEN_MS : NAV_MINIMAL_CLOSE_MS
		);
		nav_anim_start(d);
		return;
	}
	nav_set_mode(d, d->mode == FLUX_NAV_EXPANDED ? FLUX_NAV_COMPACT : FLUX_NAV_EXPANDED);
}

static bool nav_escape_key(void *ctx, FluxKey key, bool down) {
	if (!down || key != FLUX_KEY_ESCAPE) return false;
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode != FLUX_NAV_MINIMAL || !d->minimal_open) return false;
	d->minimal_open = false;
	nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	nav_anim_start(d);
	return true;
}

static void nav_apply_selection(FluxNavViewData *d, int index, bool notify) {
	if (index < 0 || index >= d->count) return;
	if (d->selected == index) return;
	if (d->selected >= 0) xent_sem_setselected(d->ctx, d->items [d->selected].node, false);
	d->selected = index;
	xent_sem_setselected(d->ctx, d->items [index].node, true);
	if (d->mode == FLUX_NAV_MINIMAL && d->minimal_open && !d->items [index].has_children) {
		d->minimal_open = false;
		nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	}
	nav_anim_start(d);
	d->selection_revision++;
	if (notify && d->on_selection_changed) d->on_selection_changed(d->on_sel_ctx, index);
}

static void nav_item_click(void *ctx) {
	FluxNavViewItem *it = ( FluxNavViewItem * ) ctx;
	FluxNavViewData *d  = it->nav;
	if (it->has_children) {
		bool chevron         = it->press_on_chevron;
		it->press_on_chevron = false;
		if (!nav_children_inline(d)) {
			nav_show_child_flyout(d, it->index);
			return;
		}
		nav_toggle_expand(d, it->index);
		if (chevron) return;
	}
	nav_apply_selection(d, it->index, true);
}

static void nav_item_down(void *ctx, float x, float y, int clicks) {
	( void ) y;
	( void ) clicks;
	FluxNavViewItem *it = ( FluxNavViewItem * ) ctx;
	XentRect         r  = {0};
	if (!it->has_children || !xent_layout_rect(it->nav->ctx, it->node, &r)) return;
	it->press_on_chevron = x >= r.w - NAV_CHEVRON_ZONE;
}

static void nav_scrim_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode != FLUX_NAV_MINIMAL || !d->minimal_open) return;
	d->minimal_open = false;
	nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	nav_anim_start(d);
}

static bool nav_children_inline(FluxNavViewData const *d) {
	if (d->mode == FLUX_NAV_MINIMAL) return true;
	return d->mode != FLUX_NAV_TOP && d->labels_visible;
}

static float nav_children_content_h(FluxNavViewData *d, int parent) {
	int n = 0;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].parent == parent) n++;
	return n > 0 ? ( float ) n * (FLUX_NAV_ITEM_HEIGHT + FLUX_NAV_ITEM_GAP) - FLUX_NAV_ITEM_GAP : 0.0f;
}

static bool nav_host_shown(FluxNavViewData const *d, FluxNavViewItem const *it) {
	return it->children_host != XENT_NODE_INVALID
	    && nav_children_inline(d)
	    && (it->expanded || it->expand_t.active || it->expand_t.current > 0.001f);
}

static bool nav_in_items_host(FluxNavViewItem const *it) { return it->parent < 0 && it->kind != FLUX_NAV_ITEM_FOOTER; }

static void nav_items_restack(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!nav_in_items_host(it)) continue;
		xent_node_remove(d->ctx, d->items_host, it->node);
		if (it->children_host != XENT_NODE_INVALID) xent_node_remove(d->ctx, d->items_host, it->children_host);
	}
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!nav_in_items_host(it)) continue;
		xent_node_append(d->ctx, d->items_host, it->node);
		if (nav_host_shown(d, it)) xent_node_append(d->ctx, d->items_host, it->children_host);
	}
}

static void nav_make_children_host(FluxNavViewData *d, FluxNavViewItem *par) {
	par->children_host = flux_factory_create_node(d->ctx, d->store, d->items_host, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, par->children_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, par->children_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, par->children_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, par->children_host, FLUX_NAV_ITEM_GAP);
	xent_setshrink(d->ctx, par->children_host, 0.0f);
	xent_setsize(d->ctx, par->children_host, (XentSize) {NAN, 0.0f});
	FluxNodeData *hn = flux_store_find(d->store, par->children_host);
	if (hn) hn->clips_children = true;
	nav_tween_init(&par->expand_t, 0.0f);
	nav_items_restack(d);
}

static void nav_snap_children(FluxNavViewData *d) {
	bool inline_ok = nav_children_inline(d);
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!it->has_children) continue;
		bool open = inline_ok && it->expanded;
		nav_tween_snap(&it->expand_t, open ? 1.0f : 0.0f);
		float w = (d->mode == FLUX_NAV_TOP) ? 0.0f : NAN;
		xent_setsize(d->ctx, it->children_host, (XentSize) {w, open ? nav_children_content_h(d, i) : 0.0f});
	}
	nav_items_restack(d);
}

static bool nav_tick_expand_item(FluxNavViewData *d, FluxNavViewItem *it, int index, DWORD now) {
	bool was     = it->expand_t.active;
	bool running = nav_tween_tick(&it->expand_t, now, nav_ease_lead);
	if (was)
		xent_setsize(
		  d->ctx, it->children_host, (XentSize) {NAN, it->expand_t.current * nav_children_content_h(d, index)}
		);
	return running;
}

static bool nav_tick_expand(FluxNavViewData *d, DWORD now) {
	bool active   = false;
	bool finished = false;
	for (int i = 0; i < d->count; i++) {
		if (!d->items [i].has_children) continue;
		bool was  = d->items [i].expand_t.active;
		active   |= nav_tick_expand_item(d, &d->items [i], i, now);
		if (was && !d->items [i].expand_t.active) finished = true;
	}
	if (finished) nav_items_restack(d);
	return active;
}

static char const *nav_glyph_utf8(char const *icon_name, char buf [4]) {
	wchar_t const *g = flux_icon_lookup(icon_name);
	if (!g || !g [0]) return NULL;
	unsigned c = ( unsigned ) g [0];
	buf [0]    = ( char ) (0xe0 | (c >> 12));
	buf [1]    = ( char ) (0x80 | ((c >> 6) & 0x3f));
	buf [2]    = ( char ) (0x80 | (c & 0x3f));
	buf [3]    = 0;
	return buf;
}

static void nav_flyout_child_click(void *ctx) {
	FluxNavViewItem *it = ( FluxNavViewItem * ) ctx;
	nav_repaint(it->nav);
	nav_apply_selection(it->nav, it->index, true);
}

static void nav_child_flyout_dismissed(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->flyout_parent >= 0 && d->flyout_parent < d->count) d->items [d->flyout_parent].expanded = false;
	d->flyout_parent = -1;
	nav_repaint(d);
}

static void nav_show_child_flyout(FluxNavViewData *d, int parent) {
	if (!d->child_flyout) {
		d->child_flyout = flux_menu_create(d->window);
		if (!d->child_flyout) return;
		flux_menu_setthememanager(d->child_flyout, d->theme);
		flux_menu_settext(d->child_flyout, d->text);
		flux_menu_ondismiss(d->child_flyout, nav_child_flyout_dismissed, d);
		flux_menu_setminh(d->child_flyout, FLUX_NAV_ITEM_HEIGHT + FLUX_NAV_ITEM_GAP);
		flux_menu_setanim(d->child_flyout, FLUX_POPUP_ANIM_FLYOUT);
	}
	flux_menu_clear(d->child_flyout);
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (it->parent != parent) continue;
		char            gbuf [4];
		FluxMenuItemDef def = {
		  .type         = FLUX_MENU_ITEM_NORMAL,
		  .label        = it->label,
		  .icon_glyph   = nav_glyph_utf8(it->icon_name, gbuf),
		  .enabled      = true,
		  .checked      = (d->selected == i),
		  .on_click     = nav_flyout_child_click,
		  .on_click_ctx = it,
		};
		flux_menu_additem(d->child_flyout, &def);
	}
	d->items [parent].expanded = true;
	d->flyout_parent           = parent;
	FluxRect anchor            = flux_binding_screen_anchor(d->window, d->ctx, d->store, d->items [parent].node);
	flux_menu_show(d->child_flyout, anchor, d->mode == FLUX_NAV_TOP ? FLUX_PLACEMENT_BOTTOM : FLUX_PLACEMENT_RIGHT);
	nav_repaint(d);
}

static void nav_toggle_expand(FluxNavViewData *d, int idx) {
	FluxNavViewItem *it = &d->items [idx];
	if (!it->has_children) return;
	if (!nav_children_inline(d)) {
		nav_show_child_flyout(d, idx);
		return;
	}
	it->expanded = !it->expanded;
	nav_items_restack(d);
	nav_tween_to(&it->expand_t, it->expanded ? 1.0f : 0.0f, NAV_EXPAND_MS);
	nav_anim_start(d);
}

static FluxNavDisplayMode nav_mode_for_width(float w) {
	if (w < NAV_ADAPT_COMPACT) return FLUX_NAV_MINIMAL;
	if (w < NAV_ADAPT_EXPANDED) return FLUX_NAV_COMPACT;
	return FLUX_NAV_EXPANDED;
}

static float nav_dip_scale(FluxNavViewData *d) {
	FluxDpi dpi = flux_window_dpi(d->window);
	return dpi.dpi_x > 0.0f ? dpi.dpi_x / 96.0f : 1.0f;
}

static float nav_window_dip_width(FluxNavViewData *d) {
	FluxSize cs = flux_window_size(d->window);
	return cs.w / nav_dip_scale(d);
}

#define NAV_HOST_INSET 72.0f

static float nav_fit_width(FluxNavViewData *d, float window_dip) {
	if (d->fill_window) return flux_maxf(window_dip, FLUX_NAV_PANE_EXPANDED);
	return flux_clampf(window_dip - NAV_HOST_INSET, FLUX_NAV_PANE_EXPANDED, d->max_width);
}

static void nav_apply_window(FluxNavViewData *d, float window_dip) {
	float w = nav_fit_width(d, window_dip);
	if (fabsf(w - d->width) > 0.5f) {
		d->width = w;
		xent_setsize(d->ctx, d->root, (XentSize) {d->width, d->height});
	}
	if (d->adaptive && d->mode != FLUX_NAV_TOP) nav_set_mode(d, nav_mode_for_width(window_dip));
	nav_apply_geometry(d);
	nav_anim_start(d);
}

static void nav_on_window_resize(void *ctx, int width_px, int height_px) {
	FluxNavViewData *d     = ( FluxNavViewData * ) ctx;
	float            scale = nav_dip_scale(d);
	if (d->fill_window) {
		float h = height_px / scale;
		if (fabsf(h - d->height) > 0.5f) {
			d->height = h;
			xent_setsize(d->ctx, d->root, (XentSize) {d->width, d->height});
		}
	}
	nav_apply_window(d, width_px / scale);
}

static void nav_destroy(void *component_data) {
	FluxNavViewData *d = ( FluxNavViewData * ) component_data;
	if (!d) return;
	if (d->child_flyout) flux_menu_destroy(d->child_flyout);
	nav_anim_remove(d);
	flux_window_delresizeobs(d->window, nav_on_window_resize, d);
	flux_str_free(d->app_title);
	for (int i = 0; i < d->count; i++) {
		flux_str_free(d->items [i].label);
		flux_str_free(d->items [i].icon_name);
	}
	free(d);
}

static void nav_set_pane_padding(FluxNavViewData *d) {
	float top_pad = d->window_title_bar ? FLUX_TB_HEIGHT : (FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD);
	xent_setp(d->ctx, d->pane, (XentInsets) {FLUX_NAV_ITEM_MARGIN_H, top_pad, FLUX_NAV_ITEM_MARGIN_H, 4.0f});
}

static void nav_setup_pane(FluxNavViewData *d) {
	xent_setproto(d->ctx, d->pane, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->pane, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->pane, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, d->pane, FLUX_NAV_ITEM_GAP);
	nav_set_pane_padding(d);

	d->items_scroll = flux_scroll_init(&(FluxContainerDesc) {d->ctx, d->store, d->pane});
	xent_setitems(d->ctx, d->items_scroll, XENT_FLEX_ALIGN_STRETCH);
	xent_setgrow(d->ctx, d->items_scroll, 1.0f);
	xent_setbasis(d->ctx, d->items_scroll, 0.0f);
	xent_setshrink(d->ctx, d->items_scroll, 0.0f);
	FluxNodeData *sn     = flux_store_find(d->store, d->items_scroll);
	d->items_scroll_data = sn ? ( struct FluxScrollData * ) sn->component_data : NULL;

	d->items_host        = flux_factory_create_node(d->ctx, d->store, d->items_scroll, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->items_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->items_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->items_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, d->items_host, FLUX_NAV_ITEM_GAP);
	xent_setfit(d->ctx, d->items_host, false, true);
	xent_setshrink(d->ctx, d->items_host, 0.0f);

	d->footer_host = flux_factory_create_node(d->ctx, d->store, d->pane, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->footer_host, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->footer_host, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->footer_host, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, d->footer_host, FLUX_NAV_ITEM_GAP);
	xent_setfit(d->ctx, d->footer_host, false, true);
	xent_setshrink(d->ctx, d->footer_host, 0.0f);
}

static void nav_make_toggle(FluxNavViewData *d) {
	d->toggle = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW_ITEM);
	xent_setpos(d->ctx, d->toggle, (XentPoint) {FLUX_NAV_ITEM_MARGIN_H, FLUX_NAV_PANE_TOP_PAD});
	xent_setsize(d->ctx, d->toggle, (XentSize) {FLUX_NAV_TOGGLE_W, FLUX_NAV_TOGGLE_H});

	FluxNodeData *tn = flux_store_find(d->store, d->toggle);
	if (tn) {
		d->toggle_item            = (FluxNavViewItem) {d, d->toggle, NULL, "GlobalNavButton", FLUX_NAV_ITEM_TOGGLE, -1};
		tn->component_data        = &d->toggle_item;
		tn->behavior.on_click     = nav_toggle_click;
		tn->behavior.on_click_ctx = d;
		tn->behavior.on_key       = nav_escape_key;
		tn->behavior.on_key_ctx   = d;
	}
	xent_set_focusable(d->ctx, d->toggle, true);
}

static XentNodeId nav_make_titlebar(FluxNavViewData *d, char const *title) {
	return flux_titlebar_init(&(FluxTitleBarDesc) {
	  .ctx              = d->ctx,
	  .store            = d->store,
	  .parent           = d->root,
	  .window           = d->window,
	  .title            = title,
	  .show_pane_toggle = true,
	  .integrate_window = true,
	  .on_pane_toggle   = nav_toggle_click,
	  .pane_toggle_ctx  = d});
}

static void nav_init_data(FluxNavViewData *d, FluxNavViewDesc const *info, XentNodeId root) {
	d->ctx                  = info->ctx;
	d->store                = info->store;
	d->window               = info->window;
	d->text                 = info->text;
	d->root                 = root;
	d->selected             = -1;
	d->ind_index            = -1;
	d->ind_visible          = -1;
	d->adaptive             = info->adaptive;
	d->mode                 = ( FluxNavDisplayMode ) info->mode;
	d->fill_window          = info->width <= 0.0f;
	d->max_width            = info->width;
	d->width                = info->width;
	d->height               = info->height;
	d->window_title_bar     = info->window_title_bar;
	d->app_title            = info->app_title ? flux_str_dup(info->app_title) : NULL;
	d->title_bar            = XENT_NODE_INVALID;
	d->theme                = info->theme;
	d->flyout_parent        = -1;
	d->on_selection_changed = info->on_selection_changed;
	d->on_sel_ctx           = info->userdata;
	nav_tween_init(&d->pane_w, d->mode == FLUX_NAV_COMPACT ? FLUX_NAV_PANE_COMPACT : FLUX_NAV_PANE_EXPANDED);
	nav_tween_init(&d->minimal_t, 0.0f);
	nav_tween_init(&d->ind_top, 0.0f);
	nav_tween_init(&d->ind_bot, 0.0f);
	nav_tween_init(&d->ind_op, 0.0f);
}

static void nav_build_tree(FluxNavViewData *d) {
	xent_setproto(d->ctx, d->root, XENT_PROTOCOL_ABSOLUTE);
	xent_setsize(d->ctx, d->root, (XentSize) {d->width, d->height});

	d->content        = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_CONTAINER);

	d->scrim          = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_CONTAINER);
	FluxNodeData *scn = flux_store_find(d->store, d->scrim);
	if (scn) {
		scn->behavior.on_click     = nav_scrim_click;
		scn->behavior.on_click_ctx = d;
	}

	d->pane          = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW);
	FluxNodeData *pn = flux_store_find(d->store, d->pane);
	if (pn) pn->component_data = d;
	nav_setup_pane(d);
	nav_make_toggle(d);

	if (d->window_title_bar) d->title_bar = nav_make_titlebar(d, d->app_title);
}

static void nav_apply_initial_window(FluxNavViewData *d) {
	float win_dip = nav_window_dip_width(d);
	if (d->fill_window) {
		FluxSize cs = flux_window_size(d->window);
		d->height   = cs.h / nav_dip_scale(d);
	}
	if (d->adaptive) d->mode = nav_mode_for_width(win_dip);
	d->width = nav_fit_width(d, win_dip);
	xent_setsize(d->ctx, d->root, (XentSize) {d->width, d->height});
	nav_tween_snap(&d->pane_w, d->mode == FLUX_NAV_COMPACT ? FLUX_NAV_PANE_COMPACT : FLUX_NAV_PANE_EXPANDED);
	nav_apply_geometry(d);
	flux_window_addresizeobs(d->window, nav_on_window_resize, d);
}

XentNodeId flux_nav_init(FluxNavViewDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_NAV_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_store_find(info->store, root);
	FluxNavViewData *d  = nd ? ( FluxNavViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	nav_init_data(d, info, root);
	nd->component_data         = d;
	nd->destroy_component_data = nav_destroy;
	nd->clips_children         = true;
	nav_build_tree(d);
	nav_apply_initial_window(d);

	if (info->out_content) *info->out_content = d->content;
	return root;
}

bool flux_nav_set_chrome(FluxStore *store, XentNodeId nav, bool window_title_bar, char const *app_title) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d) return false;
	char *title = app_title ? flux_str_dup(app_title) : NULL;
	if (app_title && !title) return false;
	if (window_title_bar && d->title_bar == XENT_NODE_INVALID) {
		d->title_bar = nav_make_titlebar(d, title);
		if (d->title_bar == XENT_NODE_INVALID) {
			flux_str_free(title);
			return false;
		}
	}
	if (!window_title_bar && d->title_bar != XENT_NODE_INVALID) {
		flux_tree_drop(store, d->title_bar);
		d->title_bar = XENT_NODE_INVALID;
		flux_window_extendtitlebar(d->window, false);
	}
	flux_str_free(d->app_title);
	d->app_title        = title;
	d->window_title_bar = window_title_bar;
	if (d->title_bar != XENT_NODE_INVALID) flux_titlebar_set_title(store, d->title_bar, title, NULL);
	nav_set_pane_padding(d);
	nav_apply_geometry(d);
	return true;
}

static float nav_item_height(FluxNavItemKind kind) {
	if (kind == FLUX_NAV_ITEM_SEPARATOR) return FLUX_NAV_SEP_H;
	if (kind == FLUX_NAV_ITEM_HEADER) return FLUX_NAV_HEADER_H;
	return FLUX_NAV_ITEM_HEIGHT;
}

static int nav_add(FluxNavViewData *d, XentNodeId host, char const *icon, char const *label, FluxNavItemKind kind) {
	if (d->count >= FLUX_NAV_VIEW_MAX_ITEMS) {
		OutputDebugStringA("fluxent: NavigationView item capacity exceeded; item dropped\n");
		return -1;
	}
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, host, FLUX_CONTROL_NAV_VIEW_ITEM);
	if (node == XENT_NODE_INVALID) return -1;
	xent_setsize(d->ctx, node, (XentSize) {NAN, nav_item_height(kind)});
	xent_setshrink(d->ctx, node, 0.0f);

	FluxNavViewItem *slot = &d->items [d->count];
	*slot                 = (FluxNavViewItem) {d, node, flux_str_dup(label), flux_str_dup(icon), kind, d->count};
	slot->parent          = -1;
	slot->children_host   = XENT_NODE_INVALID;

	FluxNodeData *in      = flux_store_find(d->store, node);
	bool          inert   = (kind == FLUX_NAV_ITEM_SEPARATOR || kind == FLUX_NAV_ITEM_HEADER);
	if (in && !inert) {
		in->component_data               = slot;
		in->behavior.on_click            = nav_item_click;
		in->behavior.on_click_ctx        = slot;
		in->behavior.on_pointer_down     = nav_item_down;
		in->behavior.on_pointer_down_ctx = slot;
		in->behavior.on_key              = nav_escape_key;
		in->behavior.on_key_ctx          = d;
	}
	else if (in) in->component_data = slot;
	if (!inert) {
		xent_set_focusable(d->ctx, node, true);
		xent_sem_setrole(d->ctx, node, XENT_SEM_ROLE_BUTTON);
		if (label) xent_sem_setlabel(d->ctx, node, label);
	}

	int index = d->count++;
	if (d->selected < 0 && kind == FLUX_NAV_ITEM_MENU) nav_apply_selection(d, index, false);
	return index;
}

bool flux_nav_reset_items(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d) return false;
	if (d->child_flyout) {
		flux_menu_destroy(d->child_flyout);
		d->child_flyout = NULL;
	}
	nav_anim_remove(d);
	for (int i = 0; i < d->count; i++) {
		flux_str_free(d->items [i].label);
		flux_str_free(d->items [i].icon_name);
		xent_node_destroy(d->ctx, d->items [i].node);
	}
	for (int i = 0; i < d->count; i++)
		if (d->items [i].children_host != XENT_NODE_INVALID) xent_node_destroy(d->ctx, d->items [i].children_host);
	memset(d->items, 0, sizeof(d->items));
	d->count         = 0;
	d->selected      = -1;
	d->ind_index     = -1;
	d->ind_visible   = -1;
	d->flyout_parent = -1;
	d->ind_in_scroll = false;
	d->ind_primed    = false;
	nav_tween_snap(&d->ind_top, 0.0f);
	nav_tween_snap(&d->ind_bot, 0.0f);
	nav_tween_snap(&d->ind_op, 0.0f);
	return true;
}

int flux_nav_add_item(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, icon_name, label, FLUX_NAV_ITEM_MENU) : -1;
}

int flux_nav_addfooter(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->footer_host, icon_name, label, FLUX_NAV_ITEM_FOOTER) : -1;
}

int flux_nav_add_separator(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, NULL, FLUX_NAV_ITEM_SEPARATOR) : -1;
}

int flux_nav_add_header(FluxStore *store, XentNodeId nav, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, label, FLUX_NAV_ITEM_HEADER) : -1;
}

int flux_nav_addchild(FluxStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d || parent_index < 0 || parent_index >= d->count) return -1;
	FluxNavViewItem *par = &d->items [parent_index];
	if (par->kind != FLUX_NAV_ITEM_MENU && par->kind != FLUX_NAV_ITEM_FOOTER) return -1;
	if (!par->has_children) {
		par->has_children = true;
		nav_make_children_host(d, par);
	}
	int idx = nav_add(d, par->children_host, icon_name, label, FLUX_NAV_ITEM_MENU);
	if (idx < 0) return -1;
	d->items [idx].parent = parent_index;
	d->items [idx].depth  = par->depth + 1;
	if (nav_children_inline(d) && par->expanded)
		xent_setsize(d->ctx, par->children_host, (XentSize) {NAN, nav_children_content_h(d, parent_index)});
	return idx;
}

void flux_nav_select(FluxStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (d) nav_apply_selection(d, index, true);
}

void flux_nav_set_selected(FluxStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (d) nav_apply_selection(d, index, false);
}

XentNodeId flux_nav_content_node(FluxStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? d->content : XENT_NODE_INVALID;
}
