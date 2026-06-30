#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_menu_flyout.h"
#include "fluxent/controls/flux_nav_view_data.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI NavigationView (left nav). See flux_nav_view_data.h for the layout model.
 * Geometry (pane width, content offset, minimal slide, scrim) and the selection
 * indicator are recomputed on a shared 60fps timer while animating. */

#define NAV_PANE_ANIM_MS     250.0f /* Expanded<->Compact inline width. */
#define NAV_MINIMAL_OPEN_MS  350.0f
#define NAV_MINIMAL_CLOSE_MS 120.0f
#define NAV_IND_MS           600.0f
#define NAV_IND_OP_MS        200.0f
#define NAV_SCRIM_ALPHA      0x4d /* SmokeFillColorDefault (#4D000000), matching ContentDialog's scrim. */
#define NAV_ADAPT_COMPACT    641.0f
#define NAV_ADAPT_EXPANDED   1008.0f
#define NAV_EXPAND_MS        200.0f /* inline children reveal */
#define NAV_CHEVRON_ZONE     40.0f  /* right-edge press region that only toggles */
#define NAV_CHILD_INDENT     31.0f  /* NavigationViewItemBase c_itemIndentation */

/* Standard WinUI offset spline; the indicator's leading edge uses it directly and
 * its trailing edge a slower-starting variant, so the pill stretches then settles.
 * Solver shared via flux_cubic_bezier (render/flux_anim.h). */
static float nav_ease_lead(float t) { return flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f); }

static float nav_ease_trail(float t) { return flux_cubic_bezier(t, 0.55f, 0.0f, 0.2f, 1.0f); }

static void  nav_tween_init(FluxNavTween *tw, float v) {
	tw->current = tw->start = tw->target = v;
	tw->active                           = false;
}

static void nav_tween_to(FluxNavTween *tw, float target, float duration_ms) {
	if (fabsf(target - tw->target) < 0.01f) return;
	tw->start    = tw->current;
	tw->target   = target;
	tw->start_ms = GetTickCount();
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

static FluxNavViewData *nav_data(FluxNodeStore *store, XentNodeId nav) {
	FluxNodeData *nd = flux_node_store_get(store, nav);
	if (!nd || nd->component_type != FLUX_CONTROL_NAV_VIEW) return NULL;
	return ( FluxNavViewData * ) nd->component_data;
}

static void nav_repaint(FluxNavViewData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float nav_item_height(FluxNavItemKind kind);
static void  nav_apply_selection(FluxNavViewData *d, int index);
static bool  nav_children_inline(FluxNavViewData const *d);
static bool  nav_tick_expand(FluxNavViewData *d, DWORD now);
static void  nav_snap_children(FluxNavViewData *d);
static void  nav_show_child_flyout(FluxNavViewData *d, int parent);
static void  nav_toggle_expand(FluxNavViewData *d, int idx);

static void  nav_set_abs(FluxNavViewData *d, XentNodeId node, float x, float y, float w, float h) {
	xent_set_absolute_position(d->ctx, node, (XentPoint) {x, y});
	xent_set_size(d->ctx, node, (XentSize) {w, h});
}

/* Update section header heights when label visibility changes. */
static void nav_sync_header_heights(FluxNavViewData *d) {
	float h = d->labels_visible ? FLUX_NAV_HEADER_H : 0.0f;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].kind == FLUX_NAV_ITEM_HEADER)
			xent_set_size(d->ctx, d->items [i].node, (XentSize) {NAN, h});
}

static void nav_set_scrim_alpha(FluxNavViewData *d, int alpha) {
	FluxNodeData *sn = flux_node_store_get(d->store, d->scrim);
	if (sn) sn->visuals.background = flux_color_rgba(0, 0, 0, alpha);
}

static void nav_apply_geometry_top(FluxNavViewData *d) {
	d->labels_visible = true;
	xent_set_size(d->ctx, d->toggle, (XentSize) {0.0f, 0.0f});
	nav_set_abs(d, d->pane, 0.0f, 0.0f, d->width, FLUX_NAV_TOP_H);
	nav_set_abs(d, d->content, 0.0f, FLUX_NAV_TOP_H, d->width, flux_maxf(0.0f, d->height - FLUX_NAV_TOP_H));
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
	nav_set_scrim_alpha(d, 0);
}

static void nav_apply_geometry_minimal(FluxNavViewData *d) {
	float top    = FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD;
	float pane_x = -(1.0f - d->minimal_t.current) * FLUX_NAV_PANE_EXPANDED;
	nav_set_abs(d, d->content, 0.0f, top, d->width, flux_maxf(0.0f, d->height - top));
	nav_set_abs(d, d->pane, pane_x, 0.0f, FLUX_NAV_PANE_EXPANDED, d->height);
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, d->width, d->height);
	nav_set_scrim_alpha(d, ( int ) (NAV_SCRIM_ALPHA * d->minimal_t.current));
}

static void nav_apply_geometry_inline(FluxNavViewData *d) {
	float flow = d->pane_w.current; /* animates between 320 and 48 */
	nav_set_abs(d, d->pane, 0.0f, 0.0f, flow, d->height);
	nav_set_abs(d, d->content, flow, 0.0f, flux_maxf(0.0f, d->width - flow), d->height);
	nav_set_abs(d, d->scrim, 0.0f, 0.0f, 0.0f, 0.0f);
	nav_set_scrim_alpha(d, 0);
}

/* Place pane + content + scrim from the current mode and animated values. */
static void nav_apply_geometry(FluxNavViewData *d) {
	/* Labels track the animated pane width, not the mode flag: the moment mode
	 * flips to Compact the pane still has to *animate* shut, so gating labels on
	 * mode blanks them while the pane is still wide (wide-rail-no-labels). */
	bool labels_was   = d->labels_visible;
	d->labels_visible = d->pane_w.current > (FLUX_NAV_PANE_COMPACT + FLUX_NAV_ICON_BOX);
	if (d->labels_visible != labels_was && d->mode != FLUX_NAV_TOP)
		nav_sync_header_heights(d);

	if (d->mode == FLUX_NAV_TOP) { nav_apply_geometry_top(d); return; }

	xent_set_size(d->ctx, d->toggle, (XentSize) {FLUX_NAV_TOGGLE_W, FLUX_NAV_TOGGLE_H});

	if (d->mode == FLUX_NAV_MINIMAL) { nav_apply_geometry_minimal(d); return; }

	nav_apply_geometry_inline(d);
}

/* Pane-local center of the selected item along the pane axis (Y for the left
 * pane, X for the Top bar), or -1 if unavailable. */
/* The item the indicator should sit on: the selection itself, or — when it is
 * hidden inside a collapsed/flyout-only parent — its nearest visible ancestor
 * (WinUI IsChildSelected on the parent chain). */
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
	if (!xent_get_layout_rect(d->ctx, d->items [ind].node, &ir)) return -1.0f;
	if (!xent_get_layout_rect(d->ctx, d->pane, &pr)) return -1.0f;
	if (ir.h <= 0.0f) return -1.0f;
	/* Layout rects are scroll-agnostic; the snapshot shifts the pill by the live
	 * viewport offset for menu items (footer items sit outside the viewport). */
	d->ind_in_scroll = (d->items [ind].kind != FLUX_NAV_ITEM_FOOTER);
	if (d->mode == FLUX_NAV_TOP) return (ir.x + ir.w * 0.5f) - pr.x;
	return (ir.y + ir.h * 0.5f) - pr.y;
}

/* Drive the indicator: animate on selection change, snap on layout drift. */
static void nav_update_indicator(FluxNavViewData *d) {
	/* The pill lives on the pane; hide it whenever the pane is off-screen (Minimal,
	 * closed) so it never floats over the content. */
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
	/* Same item: keep the pill glued to it if layout shifted (no animation). */
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
	/* Keep ticking until the indicator can prime: at startup the selected item's
	 * layout rect is not ready on the first tick, so center is unknown and nothing
	 * else is animating -- without this the timer would stop before the pill appears. */
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
	if (d->mode == mode || mode == FLUX_NAV_TOP || d->mode == FLUX_NAV_TOP)
		return; /* Top transitions via the pane-display API */
	FluxNavDisplayMode prev = d->mode;
	d->mode                 = mode;

	if (mode == FLUX_NAV_MINIMAL) {
		d->minimal_open = false;
		nav_tween_snap(&d->minimal_t, 0.0f); /* pane starts off-screen left */
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

/* WinUI: Escape light-dismisses the open Minimal overlay pane (NavigationView.cpp
 * OnKeyDown, DisplayMode==Minimal && IsPaneOpen). Reaches us because clicking or
 * keyboard-activating the toggle/item focuses it, so the framework routes the key
 * to the focused node's on_key. ctx is the owning FluxNavViewData. */
static bool nav_escape_key(void *ctx, unsigned int vk, bool down) {
	if (!down || vk != VK_ESCAPE) return false;
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode != FLUX_NAV_MINIMAL || !d->minimal_open) return false;
	d->minimal_open = false;
	nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	nav_anim_start(d);
	return true;
}

static void nav_apply_selection(FluxNavViewData *d, int index) {
	if (index < 0 || index >= d->count) return;
	d->selected = index;
	xent_set_semantic_selected(d->ctx, d->items [index].node, true);
	/* Light-dismiss the overlay pane only for leaf items: parents expand in place
	 * so a child can be picked (WinUI ClosePaneIfNeccessaryAfterItemIsClicked
	 * requires !DoesNavigationViewItemHaveChildren). */
	if (d->mode == FLUX_NAV_MINIMAL && d->minimal_open && !d->items [index].has_children) {
		d->minimal_open = false;
		nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	}
	nav_anim_start(d);
	if (d->on_selection_changed) d->on_selection_changed(d->on_sel_ctx, index);
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
		if (chevron) return; /* chevron press toggles without selecting */
	}
	nav_apply_selection(d, it->index);
}

static void nav_item_down(void *ctx, float x, float y, int clicks) {
	( void ) y;
	( void ) clicks;
	FluxNavViewItem *it = ( FluxNavViewItem * ) ctx;
	XentRect         r  = {0};
	if (!it->has_children || !xent_get_layout_rect(it->nav->ctx, it->node, &r)) return;
	it->press_on_chevron = x >= r.w - NAV_CHEVRON_ZONE;
}

static void nav_scrim_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->mode != FLUX_NAV_MINIMAL || !d->minimal_open) return;
	d->minimal_open = false;
	nav_tween_to(&d->minimal_t, 0.0f, NAV_MINIMAL_CLOSE_MS);
	nav_anim_start(d);
}

/* Children render inline only where the full pane is visible; Compact rails and
 * the Top bar show them in a flyout (TabViewItem::ShouldRepeaterShowInFlyout). */
static bool nav_children_inline(FluxNavViewData const *d) {
	return d->mode == FLUX_NAV_EXPANDED || d->mode == FLUX_NAV_MINIMAL;
}

static float nav_children_content_h(FluxNavViewData *d, int parent) {
	int n = 0;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].parent == parent) n++;
	return n > 0 ? ( float ) n * (FLUX_NAV_ITEM_HEIGHT + FLUX_NAV_ITEM_GAP) - FLUX_NAV_ITEM_GAP : 0.0f;
}

/* Rebuild items_host child order so each parent's children-host follows it. */
/* A children-host is in the flex flow only while inline-shown; otherwise it would
 * still consume items_host's gap on both sides and make a collapsed parent taller
 * than a leaf (WinUI keeps every item at NavigationViewItemOnLeftMinHeight=36). */
static bool nav_host_shown(FluxNavViewData const *d, FluxNavViewItem const *it) {
	return it->children_host != XENT_NODE_INVALID
	    && nav_children_inline(d)
	    && (it->expanded || it->expand_t.active || it->expand_t.current > 0.001f);
}

/* Only menu-list roots live in items_host; footer items stay in footer_host. */
static bool nav_in_items_host(FluxNavViewItem const *it) { return it->parent < 0 && it->kind != FLUX_NAV_ITEM_FOOTER; }

static void nav_items_restack(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!nav_in_items_host(it)) continue;
		xent_remove_child(d->ctx, d->items_host, it->node);
		if (it->children_host != XENT_NODE_INVALID) xent_remove_child(d->ctx, d->items_host, it->children_host);
	}
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!nav_in_items_host(it)) continue;
		xent_append_child(d->ctx, d->items_host, it->node);
		if (nav_host_shown(d, it)) xent_append_child(d->ctx, d->items_host, it->children_host);
	}
}

static void nav_make_children_host(FluxNavViewData *d, FluxNavViewItem *par) {
	par->children_host = flux_factory_create_node(d->ctx, d->store, d->items_host, FLUX_CONTROL_CONTAINER);
	xent_set_protocol(d->ctx, par->children_host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, par->children_host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, par->children_host, XENT_FLEX_ALIGN_STRETCH);
	xent_set_gap(d->ctx, par->children_host, FLUX_NAV_ITEM_GAP);
	xent_set_flex_shrink(d->ctx, par->children_host, 0.0f);
	xent_set_size(d->ctx, par->children_host, (XentSize) {NAN, 0.0f});
	FluxNodeData *hn = flux_node_store_get(d->store, par->children_host);
	if (hn) hn->clips_children = true; /* the reveal animation clips children */
	nav_tween_init(&par->expand_t, 0.0f);
	nav_items_restack(d);
}

/* Snap every children-host to its resting height for the current mode. */
static void nav_snap_children(FluxNavViewData *d) {
	bool inline_ok = nav_children_inline(d);
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!it->has_children) continue;
		bool open = inline_ok && it->expanded;
		nav_tween_snap(&it->expand_t, open ? 1.0f : 0.0f);
		float w = (d->mode == FLUX_NAV_TOP) ? 0.0f : NAN;
		xent_set_size(d->ctx, it->children_host, (XentSize) {w, open ? nav_children_content_h(d, i) : 0.0f});
	}
	nav_items_restack(d);
}

static bool nav_tick_expand_item(FluxNavViewData *d, FluxNavViewItem *it, int index, DWORD now) {
	bool was = it->expand_t.active;
	bool running = nav_tween_tick(&it->expand_t, now, nav_ease_lead);
	if (was)
		xent_set_size(d->ctx, it->children_host, (XentSize) {NAN, it->expand_t.current * nav_children_content_h(d, index)});
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

/* UTF-8 of a single Segoe Fluent glyph for the flyout item definition. */
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
	nav_apply_selection(it->nav, it->index);
	nav_repaint(it->nav);
}

/* Compact/Top: children open in a flyout beside (rail) or under (bar) the parent. */
/* Flyout dismissed (light-dismiss or child picked): drop the parent's expanded
 * state so its chevron rotates back down (WinUI OnFlyoutClosing -> IsExpanded(false)). */
static void nav_child_flyout_dismissed(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->flyout_parent >= 0 && d->flyout_parent < d->count) d->items [d->flyout_parent].expanded = false;
	d->flyout_parent = -1;
	nav_repaint(d);
}

static void nav_show_child_flyout(FluxNavViewData *d, int parent) {
	if (!d->child_flyout) {
		d->child_flyout = flux_menu_flyout_create(d->window);
		if (!d->child_flyout) return;
		flux_menu_flyout_set_theme_manager(d->child_flyout, d->theme);
		flux_menu_flyout_set_text_renderer(d->child_flyout, d->text);
		flux_menu_flyout_set_dismiss_callback(d->child_flyout, nav_child_flyout_dismissed, d);
	}
	flux_menu_flyout_clear(d->child_flyout);
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
		flux_menu_flyout_add_item(d->child_flyout, &def);
	}
	d->items [parent].expanded = true; /* chevron points open while the flyout is up */
	d->flyout_parent           = parent;
	FluxRect anchor            = flux_binding_screen_anchor(d->window, d->ctx, d->store, d->items [parent].node);
	flux_menu_flyout_show(
	  d->child_flyout, anchor, d->mode == FLUX_NAV_TOP ? FLUX_PLACEMENT_BOTTOM : FLUX_PLACEMENT_RIGHT
	);
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
	nav_items_restack(d); /* host enters the flow on open; tick detaches it after collapse */
	nav_tween_to(&it->expand_t, it->expanded ? 1.0f : 0.0f, NAV_EXPAND_MS);
	nav_anim_start(d);
}

static void nav_pane_axis(FluxNavViewData *d, XentFlexDirection dir, XentFlexAlign align) {
	bool row = (dir == XENT_FLEX_ROW);
	xent_set_flex_direction(d->ctx, d->pane, dir);
	xent_set_flex_align_items(d->ctx, d->pane, align);
	xent_set_flex_direction(d->ctx, d->items_scroll, dir);
	xent_set_flex_align_items(d->ctx, d->items_scroll, align);
	xent_set_flex_direction(d->ctx, d->items_host, dir);
	xent_set_flex_align_items(d->ctx, d->items_host, align);
	xent_set_wrap_content(d->ctx, d->items_host, row, !row); /* content-sized along the pane axis */
	xent_set_flex_direction(d->ctx, d->footer_host, dir);
	xent_set_flex_align_items(d->ctx, d->footer_host, align);
	xent_set_wrap_content(d->ctx, d->footer_host, row, !row);
}

static float nav_top_item_width(FluxNavViewData *d, FluxNavViewItem const *it) {
	float w = FLUX_NAV_TOP_ITEM_PAD_H * 2.0f + (it->has_children ? 20.0f : 0.0f);
	if (it->icon_name) w += FLUX_NAV_ICON_GLYPH + (it->label ? FLUX_NAV_TOP_ICON_GAP : 0.0f);
	if (it->label && d->text) {
		FluxTextStyle ts  = {0};
		ts.font_size      = FLUX_NAV_LABEL_FONT;
		w                += flux_text_measure(d->text, it->label, &ts, 1e9f).w;
	}
	return w;
}

/* Re-orient the pane into a 48px horizontal bar with measured-width items (xent
 * has no wrap-content); footer items end up trailing via items_host's grow. */
static void nav_enter_top(FluxNavViewData *d) {
	d->mode         = FLUX_NAV_TOP;
	d->minimal_open = false;
	nav_tween_snap(&d->minimal_t, 0.0f);
	nav_pane_axis(d, XENT_FLEX_ROW, XENT_FLEX_ALIGN_CENTER);
	xent_set_padding(d->ctx, d->pane, (XentInsets) {FLUX_NAV_TOP_ITEM_PAD_H, 0.0f, FLUX_NAV_TOP_ITEM_PAD_H, 0.0f});
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (it->parent >= 0) continue; /* children live in collapsed hosts (flyout-only) */
		if (it->kind == FLUX_NAV_ITEM_SEPARATOR)
			xent_set_size(d->ctx, it->node, (XentSize) {FLUX_NAV_SEP_TOP_W, FLUX_NAV_SEP_TOP_LINE_H});
		else xent_set_size(d->ctx, it->node, (XentSize) {nav_top_item_width(d, it), FLUX_NAV_TOP_ITEM_H});
	}
	d->ind_primed = false; /* re-prime the indicator along the new axis */
	nav_snap_children(d);
	nav_apply_geometry(d);
	nav_anim_start(d);
}

static void nav_leave_top(FluxNavViewData *d) {
	if (d->mode != FLUX_NAV_TOP) return;
	nav_pane_axis(d, XENT_FLEX_COLUMN, XENT_FLEX_ALIGN_STRETCH);
	xent_set_padding(
	  d->ctx, d->pane,
	  (XentInsets) {FLUX_NAV_ITEM_MARGIN_H, FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD, FLUX_NAV_ITEM_MARGIN_H, 4.0f}
	);
	for (int i = 0; i < d->count; i++)
		if (d->items [i].parent < 0)
			xent_set_size(d->ctx, d->items [i].node, (XentSize) {NAN, nav_item_height(d->items [i].kind)});
	d->ind_primed = false;
	d->mode       = FLUX_NAV_EXPANDED; /* concrete left mode applied by the caller */
	nav_tween_snap(&d->pane_w, FLUX_NAV_PANE_EXPANDED);
	nav_snap_children(d);
}

static FluxNavDisplayMode nav_mode_for_width(float w) {
	if (w < NAV_ADAPT_COMPACT) return FLUX_NAV_MINIMAL;
	if (w < NAV_ADAPT_EXPANDED) return FLUX_NAV_COMPACT;
	return FLUX_NAV_EXPANDED;
}

static float nav_dip_scale(FluxNavViewData *d) {
	FluxDpiInfo dpi = flux_window_dpi(d->window);
	return dpi.dpi_x > 0.0f ? dpi.dpi_x / 96.0f : 1.0f;
}

/* Window client width in DIPs — the metric WinUI's adaptive thresholds use. */
static float nav_window_dip_width(FluxNavViewData *d) {
	FluxSize cs = flux_window_client_size(d->window);
	return cs.w / nav_dip_scale(d);
}

/* Width that keeps the control inside the visible window so the content region
 * never runs off the right edge: track the window minus the host's left/right
 * inset, capped at the creation width. The display *mode* still uses the raw
 * window width (so the 641/1008 thresholds match WinUI), but the laid-out width
 * shrinks with the window. */
#define NAV_HOST_INSET 72.0f

static float nav_fit_width(FluxNavViewData *d, float window_dip) {
	if (d->fill_window) return flux_maxf(window_dip, FLUX_NAV_PANE_EXPANDED);
	return flux_clampf(window_dip - NAV_HOST_INSET, FLUX_NAV_PANE_EXPANDED, d->max_width);
}

static void nav_apply_window(FluxNavViewData *d, float window_dip) {
	float w = nav_fit_width(d, window_dip);
	if (fabsf(w - d->width) > 0.5f) {
		d->width = w;
		xent_set_size(d->ctx, d->root, (XentSize) {d->width, d->height});
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
			xent_set_size(d->ctx, d->root, (XentSize) {d->width, d->height});
		}
	}
	nav_apply_window(d, width_px / scale);
}

static void nav_destroy(void *component_data) {
	FluxNavViewData *d = ( FluxNavViewData * ) component_data;
	if (!d) return;
	if (d->child_flyout) flux_menu_flyout_destroy(d->child_flyout);
	nav_anim_remove(d);
	flux_window_remove_resize_observer(d->window, nav_on_window_resize, d);
	for (int i = 0; i < d->count; i++) {
		flux_str_free(d->items [i].label);
		flux_str_free(d->items [i].icon_name);
	}
	free(d);
}

/* The pane holds only the item list + footer; the hamburger lives at root level
 * (see nav_make_toggle) so it stays visible in Minimal when the pane is off-screen.
 * The top padding reserves the hamburger row so the first item clears it. */
static void nav_setup_pane(FluxNavViewData *d) {
	xent_set_protocol(d->ctx, d->pane, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->pane, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, d->pane, XENT_FLEX_ALIGN_STRETCH);
	xent_set_gap(d->ctx, d->pane, FLUX_NAV_ITEM_GAP);
	xent_set_padding(
	  d->ctx, d->pane,
	  (XentInsets) {FLUX_NAV_ITEM_MARGIN_H, FLUX_NAV_TOGGLE_H + FLUX_NAV_PANE_TOP_PAD, FLUX_NAV_ITEM_MARGIN_H, 4.0f}
	);

	/* The menu list lives inside a scroll viewport: a zero-base grow child that
	 * eats the pane's free space (pinning the footer to the trailing edge) and
	 * clips + scrolls the list when it outgrows the pane. Wheel/scrollbar input
	 * and the auto-derived extent are the standard FLUX_CONTROL_SCROLL paths. */
	d->items_scroll = flux_create_scroll(&(FluxContainerCreateInfo) {d->ctx, d->store, d->pane});
	xent_set_flex_align_items(d->ctx, d->items_scroll, XENT_FLEX_ALIGN_STRETCH);
	xent_set_flex_grow(d->ctx, d->items_scroll, 1.0f);
	xent_set_flex_basis(d->ctx, d->items_scroll, 0.0f);
	xent_set_flex_shrink(d->ctx, d->items_scroll, 0.0f);
	FluxNodeData *sn     = flux_node_store_get(d->store, d->items_scroll);
	d->items_scroll_data = sn ? ( struct FluxScrollData * ) sn->component_data : NULL;

	d->items_host        = flux_factory_create_node(d->ctx, d->store, d->items_scroll, FLUX_CONTROL_CONTAINER);
	xent_set_protocol(d->ctx, d->items_host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->items_host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, d->items_host, XENT_FLEX_ALIGN_STRETCH);
	xent_set_gap(d->ctx, d->items_host, FLUX_NAV_ITEM_GAP);
	/* Content-sized, never compressed: xent treats auto as fill-available and its
	 * flex shrink compresses child POSITIONS while they draw full-size (overlap). */
	xent_set_wrap_content(d->ctx, d->items_host, false, true);
	xent_set_flex_shrink(d->ctx, d->items_host, 0.0f);

	d->footer_host = flux_factory_create_node(d->ctx, d->store, d->pane, FLUX_CONTROL_CONTAINER);
	xent_set_protocol(d->ctx, d->footer_host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->footer_host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, d->footer_host, XENT_FLEX_ALIGN_STRETCH);
	xent_set_gap(d->ctx, d->footer_host, FLUX_NAV_ITEM_GAP);
	xent_set_wrap_content(d->ctx, d->footer_host, false, true);
	xent_set_flex_shrink(d->ctx, d->footer_host, 0.0f);
}

/* The pane-toggle (hamburger) is an absolutely-positioned root child drawn last,
 * so it floats above the pane/content and stays put in every mode — in Minimal it
 * is the only affordance to open the overlay pane. */
static void nav_make_toggle(FluxNavViewData *d) {
	d->toggle = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW_ITEM);
	xent_set_absolute_position(d->ctx, d->toggle, (XentPoint) {FLUX_NAV_ITEM_MARGIN_H, FLUX_NAV_PANE_TOP_PAD});
	xent_set_size(d->ctx, d->toggle, (XentSize) {FLUX_NAV_TOGGLE_W, FLUX_NAV_TOGGLE_H});

	FluxNodeData *tn = flux_node_store_get(d->store, d->toggle);
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

static void nav_init_data(FluxNavViewData *d, FluxNavViewCreateInfo const *info, XentNodeId root) {
	d->ctx                  = info->ctx;
	d->store                = info->store;
	d->window               = info->window;
	d->text                 = info->text;
	d->root                 = root;
	d->selected             = -1;
	d->ind_index            = -1;
	d->adaptive             = info->adaptive;
	d->mode                 = ( FluxNavDisplayMode ) info->mode;
	d->fill_window          = info->width <= 0.0f;
	d->max_width            = info->width;
	d->width                = info->width;
	d->height               = info->height;
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

/* Build the ABSOLUTE root with content (under), scrim, pane (on top), toggle. */
static void nav_build_tree(FluxNavViewData *d) {
	xent_set_protocol(d->ctx, d->root, XENT_PROTOCOL_ABSOLUTE);
	xent_set_size(d->ctx, d->root, (XentSize) {d->width, d->height});

	d->content        = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_CONTAINER);

	d->scrim          = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_CONTAINER);
	FluxNodeData *scn = flux_node_store_get(d->store, d->scrim);
	if (scn) {
		scn->behavior.on_click     = nav_scrim_click;
		scn->behavior.on_click_ctx = d;
	}

	d->pane          = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_NAV_VIEW);
	/* Borrowed (root owns d's lifetime): lets the selection-indicator snapshot run on
	 * the pane, so the pill tracks the pane and slides off-screen with it in Minimal
	 * instead of being drawn at the always-on-screen root's left edge. */
	FluxNodeData *pn = flux_node_store_get(d->store, d->pane);
	if (pn) pn->component_data = d;
	nav_setup_pane(d);
	nav_make_toggle(d); /* last root child: floats above pane + content in every mode */
}

/* Fit the nav view to the current window and apply initial geometry. */
static void nav_apply_initial_window(FluxNavViewData *d) {
	float win_dip = nav_window_dip_width(d);
	if (d->fill_window) {
		FluxSize cs = flux_window_client_size(d->window);
		d->height   = cs.h / nav_dip_scale(d);
	}
	if (d->adaptive) d->mode = nav_mode_for_width(win_dip);
	d->width = nav_fit_width(d, win_dip);
	xent_set_size(d->ctx, d->root, (XentSize) {d->width, d->height});
	nav_tween_snap(&d->pane_w, d->mode == FLUX_NAV_COMPACT ? FLUX_NAV_PANE_COMPACT : FLUX_NAV_PANE_EXPANDED);
	nav_apply_geometry(d);
	flux_window_add_resize_observer(d->window, nav_on_window_resize, d);
}

XentNodeId flux_create_nav_view(FluxNavViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_NAV_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData    *nd = flux_node_store_get(info->store, root);
	FluxNavViewData *d  = nd ? ( FluxNavViewData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	nav_init_data(d, info, root);
	nd->component_data         = d;
	nd->destroy_component_data = nav_destroy;
	nd->clips_children         = true; /* contain the slid-away Minimal pane within the control */
	nav_build_tree(d);
	nav_apply_initial_window(d);

	if (info->out_content) *info->out_content = d->content;
	return root;
}

static float nav_item_height(FluxNavItemKind kind) {
	if (kind == FLUX_NAV_ITEM_SEPARATOR) return FLUX_NAV_SEP_H;
	if (kind == FLUX_NAV_ITEM_HEADER) return FLUX_NAV_HEADER_H;
	return FLUX_NAV_ITEM_HEIGHT;
}

static int nav_add(FluxNavViewData *d, XentNodeId host, char const *icon, char const *label, FluxNavItemKind kind) {
	if (d->count >= FLUX_NAV_VIEW_MAX_ITEMS) return -1;
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, host, FLUX_CONTROL_NAV_VIEW_ITEM);
	if (node == XENT_NODE_INVALID) return -1;
	/* Width is auto (NAN) so the host's align-stretch sizes it to the pane content
	 * width — 312 in Expanded, 40 in Compact — keeping labels visible when open. */
	xent_set_size(d->ctx, node, (XentSize) {NAN, nav_item_height(kind)});
	xent_set_flex_shrink(d->ctx, node, 0.0f);

	FluxNavViewItem *slot = &d->items [d->count];
	*slot                 = (FluxNavViewItem) {d, node, flux_str_dup(label), flux_str_dup(icon), kind, d->count};
	slot->parent          = -1;
	slot->children_host   = XENT_NODE_INVALID;

	FluxNodeData *in      = flux_node_store_get(d->store, node);
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
		xent_set_semantic_role(d->ctx, node, XENT_SEMANTIC_BUTTON);
		if (label) xent_set_semantic_label(d->ctx, node, label);
	}

	int index = d->count++;
	if (d->selected < 0 && kind == FLUX_NAV_ITEM_MENU) nav_apply_selection(d, index);
	return index;
}

int flux_nav_view_add_item(FluxNodeStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, icon_name, label, FLUX_NAV_ITEM_MENU) : -1;
}

int flux_nav_view_add_footer_item(FluxNodeStore *store, XentNodeId nav, char const *icon_name, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->footer_host, icon_name, label, FLUX_NAV_ITEM_FOOTER) : -1;
}

int flux_nav_view_add_separator(FluxNodeStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, NULL, FLUX_NAV_ITEM_SEPARATOR) : -1;
}

int flux_nav_view_add_header(FluxNodeStore *store, XentNodeId nav, char const *label) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? nav_add(d, d->items_host, NULL, label, FLUX_NAV_ITEM_HEADER) : -1;
}

int flux_nav_view_add_child_item(
  FluxNodeStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label
) {
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
		xent_set_size(d->ctx, par->children_host, (XentSize) {NAN, nav_children_content_h(d, parent_index)});
	return idx;
}

char const *flux_nav_view_item_label(FluxNodeStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d || index < 0 || index >= d->count) return NULL;
	return d->items [index].label;
}

void flux_nav_view_select(FluxNodeStore *store, XentNodeId nav, int index) {
	FluxNavViewData *d = nav_data(store, nav);
	if (d) nav_apply_selection(d, index);
}

XentNodeId flux_nav_view_content_node(FluxNodeStore *store, XentNodeId nav) {
	FluxNavViewData *d = nav_data(store, nav);
	return d ? d->content : XENT_NODE_INVALID;
}

void flux_nav_view_set_pane_display_mode(FluxNodeStore *store, XentNodeId nav, int mode) {
	FluxNavViewData *d = nav_data(store, nav);
	if (!d) return;
	if (mode == FLUX_NAV_PANE_TOP) {
		d->adaptive = false;
		if (d->mode != FLUX_NAV_TOP) nav_enter_top(d);
		nav_repaint(d);
		return;
	}
	nav_leave_top(d);
	if (mode == FLUX_NAV_PANE_AUTO) {
		d->adaptive = true;
		nav_set_mode(d, nav_mode_for_width(nav_window_dip_width(d)));
	}
	else {
		d->adaptive          = false;
		FluxNavDisplayMode m = (mode == FLUX_NAV_PANE_LEFT_COMPACT) ? FLUX_NAV_COMPACT
		                     : (mode == FLUX_NAV_PANE_LEFT_MINIMAL) ? FLUX_NAV_MINIMAL
		                                                            : FLUX_NAV_EXPANDED;
		nav_set_mode(d, m);
	}
	nav_apply_geometry(d);
	nav_anim_start(d);
	nav_repaint(d);
}
