#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/behavior/flux_nav_internal.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/draw/flux_control_draw.h"
#include "input/flux_input_internal.h"
#include "render/flux_icon.h"
#include "store/flux_node_data.h"

#include <math.h>
#include <stdio.h>

#define TOP_ICON_ONLY_W        40.0f
#define TOP_RECOVER_PAD        5.0f
#define TOP_ROOT_MARGIN        4.0f
#define TOP_ICON_LEAD          12.0f
#define TOP_ICON_SIZE          16.0f
#define TOP_TEXT_LEAD          8.0f
#define TOP_TEXT_TRAIL         12.0f
#define TOP_HEADER_PAD         12.0f
#define TOP_CHEVRON_WIDTH      40.0f
#define TOP_CHEVRON_ICON_INSET -16.0f
#define TOP_CHEVRON_TEXT_INSET -12.0f

typedef struct TopList {
	int n;
	int at [FLUX_NAV_VIEW_MAX_ITEMS];
} TopList;

static void top_menu_host_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	XentCtx         *ctx = flux_store_ctx(store);
	FluxNavViewData *d   = NULL;
	for (XentNodeId p = node; p != XENT_NODE_INVALID && !d; p = xent_node_parent(ctx, p)) {
		FluxNodeData *nd = flux_store_find(store, p);
		if (nd && nd->component_type == FLUX_CONTROL_NAV_VIEW) d = ( FluxNavViewData * ) nd->component_data;
	}
	char const *name = d ? flux_uia_apptext(store, d->root, FLUX_A11Y_NAME) : NULL;
	if (name) snprintf(out, size, "%s", name);
}

static FluxUiaHooks const top_uia_menu_host = {.role = FLUX_UIA_GROUP, .name = top_menu_host_name};
static FluxUiaHooks const top_uia_repeater  = {.role = FLUX_UIA_GROUP};
static FluxUiaHooks const top_uia_primary
  = {.role = FLUX_UIA_GROUP, .landmark = FLUX_UIA_LANDMARK_NAVIGATION, .name = top_menu_host_name};
static FluxUiaHooks const top_uia_footer = {.role = FLUX_UIA_GROUP, .landmark = FLUX_UIA_LANDMARK_NAVIGATION};
static FluxUiaHooks const top_uia_hidden = {.role = FLUX_UIA_HIDDEN};
static FluxUiaHooks const top_uia_more
  = {.role = FLUX_UIA_BUTTON, .patterns = FLUX_UIA_P_INVOKE, .invoked = true, .klass = L"Button"};

static bool top_entry(FluxNavViewItem const *it) { return it->parent < 0 && !it->footer; }

static void top_focus_edge(FluxNavViewItem *it, bool last) {
	FluxNavViewData *d    = it->nav;
	XentNodeId       host = xent_node_parent(d->ctx, it->node);
	XentNodeId       node = last ? xent_node_last(d->ctx, host) : xent_node_first(d->ctx, host);
	while (node != XENT_NODE_INVALID) {
		bool active
		  = xent_get_focusable(d->ctx, node) && xent_sem_enabled(d->ctx, node) && !xent_collapsed(d->ctx, node);
		if (active) {
			flux_input_setfocus(d->input, node, false);
			return;
		}
		node = last ? xent_node_prev(d->ctx, node) : xent_node_next(d->ctx, node);
	}
}

static FluxKey top_physical_arrow(FluxNavViewItem const *it, FluxKey key) {
	if (xent_resolved_dir(it->nav->ctx, it->node) != XENT_DIRECTION_RTL) return key;
	if (key == FLUX_KEY_LEFT) return FLUX_KEY_RIGHT;
	if (key == FLUX_KEY_RIGHT) return FLUX_KEY_LEFT;
	return key;
}

static bool top_move_focus(FluxNavViewItem *it, FluxKey key) {
	FluxNavViewData *d = it->nav;
	if (key == FLUX_KEY_HOME || key == FLUX_KEY_END) {
		if (it->kind == FLUX_NAV_ITEM_MORE) return false;
		top_focus_edge(it, key == FLUX_KEY_END);
		return true;
	}
	if (key != FLUX_KEY_LEFT && key != FLUX_KEY_RIGHT && key != FLUX_KEY_UP && key != FLUX_KEY_DOWN) return false;
	flux_input_xyfocus(d->input, d->pane, top_physical_arrow(it, key));
	return true;
}

static bool top_activate_key(FluxNavViewItem *it, FluxKey key, bool down) {
	FluxInput *input = it->nav->input;
	if (it->kind == FLUX_NAV_ITEM_MORE) return flux_input_keyactivate(input, key, down);
	if (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE) return false;
	if (down && !flux_input_keyrepeated(input) && xent_sem_enabled(it->nav->ctx, it->node)) flux_input_activate(input);
	return true;
}

bool flux_nav_top_key(void *ctx, FluxKey key, bool down) {
	FluxNavViewItem *it     = ctx;
	FluxNavViewData *d      = it->nav;
	bool             in_fly = flux_nav_fly_shows(d, it) && input_within(d->ctx, it->node, d->fly_root);
	if (!d->input || (d->mode != FLUX_NAV_TOP && !in_fly)) return false;
	if (top_activate_key(it, key, down)) return true;
	if (!down) return false;
	return in_fly ? flux_nav_fly_key(it, key) : top_move_focus(it, key);
}

static void top_more_click(void *ctx) {
	FluxNavViewData *d = ( FluxNavViewData * ) ctx;
	if (d->fly_owner == FLUX_NAV_FLY_OVERFLOW) flux_nav_fly_close(d);
	else flux_nav_fly_open(d, FLUX_NAV_FLY_OVERFLOW);
}

static void top_make_more(FluxNavViewData *d) {
	d->more = flux_factory_create_node(d->ctx, d->store, d->top_grid, FLUX_CONTROL_NAV_VIEW_ITEM);
	xent_setsize(d->ctx, d->more, (XentSize) {FLUX_NAV_TOP_MORE_W, FLUX_NAV_TOP_MORE_W});
	xent_setself(d->ctx, d->more, XENT_FLEX_ALIGN_CENTER);
	xent_setshrink(d->ctx, d->more, 0.0f);
	xent_setcollapsed(d->ctx, d->more, true);
	FluxNodeData *nd = flux_store_find(d->store, d->more);
	if (!nd) return;
	d->more_item              = (FluxNavViewItem) {d, d->more, NULL, "More", FLUX_NAV_ITEM_MORE, -1};
	d->more_item.place        = FLUX_NAV_PLACE_TOP;
	nd->component_data        = &d->more_item;
	nd->behavior.on_click     = top_more_click;
	nd->behavior.on_click_ctx = d;
	nd->behavior.on_key       = flux_nav_top_key;
	nd->behavior.on_key_ctx   = &d->more_item;
	nd->uia                   = &top_uia_more;
	xent_set_focusable(d->ctx, d->more, true);
	xent_sem_setlabel(d->ctx, d->more, "More");
	flux_uia_settext(d->store, d->more, FLUX_A11Y_ID, "TopNavOverflowButton");
}

static XentNodeId top_measure_node(XentCtx *ctx, XentNodeId parent) {
	XentNodeId node = xent_node_create(ctx);
	if (node) xent_node_append(ctx, parent, node);
	return node;
}

static void top_measure_setup(FluxNavViewData *d) {
	XentCtx   *ctx  = d->ctx;
	XentNodeId host = top_measure_node(ctx, d->root);
	if (!host) return;
	xent_setcollapsed(ctx, host, true);
	d->top_measure_row = top_measure_node(ctx, host);
	if (!d->top_measure_row) return;
	xent_setproto(ctx, d->top_measure_row, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, d->top_measure_row, XENT_FLEX_ROW);
	xent_setfit(ctx, d->top_measure_row, true, true);
	d->top_measure_icon    = top_measure_node(ctx, d->top_measure_row);
	d->top_measure_label   = top_measure_node(ctx, d->top_measure_row);
	d->top_measure_chevron = top_measure_node(ctx, d->top_measure_row);
	xent_setsize(ctx, d->top_measure_icon, (XentSize) {TOP_ICON_SIZE, TOP_ICON_SIZE});
	xent_setm(ctx, d->top_measure_icon, (XentInsets) {TOP_ICON_LEAD, 0, 0, 0});
	xent_setlinebreak(ctx, d->top_measure_label, XENT_LINEBREAK_NOWRAP);
	xent_setsize(ctx, d->top_measure_chevron, (XentSize) {TOP_CHEVRON_WIDTH, 12.0f});
}

void flux_nav_top_setup(FluxNavViewData *d) {
	d->top_grid = flux_factory_create_node(d->ctx, d->store, d->pane, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->top_grid, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->top_grid, XENT_FLEX_ROW);
	xent_setitems(d->ctx, d->top_grid, XENT_FLEX_ALIGN_CENTER);
	xent_seth(d->ctx, d->top_grid, FLUX_NAV_TOP_H);
	xent_setshrink(d->ctx, d->top_grid, 0.0f);
	flux_uia_part_hooks(d->store, d->top_grid, &top_uia_hidden);
	top_make_more(d);
	d->top_spacer = flux_factory_create_node(d->ctx, d->store, d->top_grid, FLUX_CONTROL_CONTAINER);
	xent_setproto(d->ctx, d->top_spacer, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->top_spacer, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->top_spacer, XENT_FLEX_ALIGN_STRETCH);
	xent_seth(d->ctx, d->top_spacer, FLUX_NAV_TOP_H);
	xent_setgrow(d->ctx, d->top_spacer, 1.0f);
	xent_setbasis(d->ctx, d->top_spacer, 0.0f);
	xent_setminsize(d->ctx, d->top_spacer, (XentSize) {FLUX_NAV_TOP_CUSTOM_MIN, 0.0f});
	xent_setcollapsed(d->ctx, d->top_spacer, true);
	flux_uia_part_hooks(d->store, d->top_spacer, &top_uia_hidden);
	top_measure_setup(d);
}

static float top_layout_width(FluxNavViewData const *d, XentNodeId node) {
	XentSize size = {0};
	xent_layout_desired_size(d->ctx, node, (XentSize) {INFINITY, FLUX_NAV_TOP_H}, &size);
	return size.w;
}

static void top_measure_presenter(FluxNavViewData const *d, FluxNavViewItem const *it, bool icon) {
	bool     header = it->kind == FLUX_NAV_ITEM_HEADER;
	XentCtx *ctx    = d->ctx;
	float    pad    = header ? 0.0f : TOP_ROOT_MARGIN;
	xent_setp(ctx, d->top_measure_row, (XentInsets) {pad, 0, pad, 0});
	xent_setcollapsed(ctx, d->top_measure_icon, !icon);
	xent_setcollapsed(ctx, d->top_measure_chevron, !it->has_children);
	xent_setm(
	  ctx, d->top_measure_chevron, (XentInsets) {icon ? TOP_CHEVRON_ICON_INSET : TOP_CHEVRON_TEXT_INSET, 0, 0, 0}
	);
	float lead = icon ? TOP_TEXT_LEAD : TOP_HEADER_PAD;
	xent_setm(ctx, d->top_measure_label, (XentInsets) {lead, 0, TOP_TEXT_TRAIL, 0});
	xent_setfontsize(ctx, d->top_measure_label, header ? FLUX_NAV_HEADER_FONT : FLUX_NAV_LABEL_FONT);
	xent_setfontweight(ctx, d->top_measure_label, header ? FLUX_FONT_SEMI_BOLD : 400u);
	xent_settext(ctx, d->top_measure_label, it->label);
}

static float top_measure_item(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (it->kind == FLUX_NAV_ITEM_SETTINGS) return TOP_ICON_ONLY_W;
	if (it->kind == FLUX_NAV_ITEM_SEPARATOR) return FLUX_NAV_SEP_TOP_W;
	bool icon = flux_icon_lookup(it->icon_name) != NULL;
	if (icon && (!it->label || !it->label [0])) return TOP_ICON_ONLY_W;
	top_measure_presenter(d, it, icon);
	return top_layout_width(d, d->top_measure_row);
}

float flux_nav_top_item_w(FluxNavViewData const *d, FluxNavViewItem *it) {
	if (it->top_w <= 0.0f) it->top_w = top_measure_item(d, it);
	return it->top_w;
}

static float top_w(FluxNavViewData *d, int i) { return flux_nav_top_item_w(d, &d->items [i]); }

static float top_footer_w(FluxNavViewData *d) {
	float w = 0.0f;
	for (int i = 0; i < d->count; i++)
		if (d->items [i].parent < 0 && d->items [i].footer) w += top_w(d, i);
	if (d->settings_visible) w += flux_nav_top_item_w(d, &d->settings_item);
	return w;
}

static float top_primary_w(FluxNavViewData *d) {
	float w = 0.0f;
	for (int i = 0; i < d->count; i++)
		if (top_entry(&d->items [i]) && !d->items [i].in_overflow) w += top_w(d, i);
	return w;
}

static float top_desired(FluxNavViewData *d) {
	float more = d->top_more ? d->top_more_width : 0.0f;
	return top_primary_w(d) + more + flux_nav_chrome_width(d) + top_footer_w(d);
}

static float top_avail(FluxNavViewData const *d) {
	return d->width - 2.0f * FLUX_NAV_TOP_GRID_PAD - flux_nav_top_back_width(d);
}

static int top_primary_count(FluxNavViewData const *d) {
	int n = 0;
	for (int i = 0; i < d->count; i++) n += top_entry(&d->items [i]) && !d->items [i].in_overflow;
	return n;
}

static bool top_has_overflow(FluxNavViewData const *d) {
	for (int i = 0; i < d->count; i++)
		if (top_entry(&d->items [i]) && d->items [i].in_overflow) return true;
	return false;
}

static int top_selected_entry(FluxNavViewData const *d) {
	int s = d->selected;
	return s >= 0 && top_entry(&d->items [s]) ? s : -1;
}

static bool top_contains(TopList const *l, int i) {
	for (int k = 0; k < l->n; k++)
		if (l->at [k] == i) return true;
	return false;
}

static void top_push(TopList *l, int i) {
	if (l->n < FLUX_NAV_VIEW_MAX_ITEMS) l->at [l->n++] = i;
}

static float top_list_w(FluxNavViewData *d, TopList const *l) {
	float w = 0.0f;
	for (int k = 0; k < l->n; k++) w += top_w(d, l->at [k]);
	return w;
}

static void top_move(FluxNavViewData *d, TopList const *l, bool out) {
	for (int k = 0; k < l->n; k++) d->items [l->at [k]].in_overflow = out;
}

static void top_keep_one(FluxNavViewData const *d, TopList *l, bool keep_first) {
	if (!l->n || l->n != top_primary_count(d)) return;
	if (!keep_first) {
		l->n--;
		return;
	}
	for (int k = 1; k < l->n; k++) l->at [k - 1] = l->at [k];
	l->n--;
}

static TopList top_beyond(FluxNavViewData *d, float avail, int sel) {
	TopList l        = {0};
	float   required = 0.0f;
	for (int i = 0; i < d->count; i++) {
		if (!top_entry(&d->items [i]) || d->items [i].in_overflow || i == sel) continue;
		bool move = true;
		if (required <= avail) {
			required += top_w(d, i);
			move      = required > avail;
		}
		if (move) top_push(&l, i);
	}
	return l;
}

static TopList top_from_end(FluxNavViewData *d, float at_least, int exclude) {
	TopList l = {0};
	for (int i = d->count - 1; i >= 0 && at_least > 0.0f; i--) {
		if (!top_entry(&d->items [i]) || d->items [i].in_overflow || i == exclude) continue;
		top_push(&l, i);
		at_least -= top_w(d, i);
	}
	return l;
}

static TopList top_recover(FluxNavViewData *d, float avail, int include) {
	TopList l = {0};
	if (include >= 0) {
		top_push(&l, include);
		avail -= top_w(d, include);
	}
	int i = 0;
	for (; i < d->count && avail > 0.0f; i++) {
		FluxNavViewItem const *it = &d->items [i];
		if (!top_entry(it) || !it->in_overflow || i == include) continue;
		if (avail < top_w(d, i)) break;
		top_push(&l, i);
		avail -= top_w(d, i);
	}
	if (i == d->count && l.n) l.n--;
	return l;
}

static void top_shrink(FluxNavViewData *d, float desired, float avail) {
	int   sel      = top_selected_entry(d);
	float possible = top_primary_w(d) - (desired - avail);
	if (possible >= 0.0f) {
		TopList rm = top_beyond(d, possible, sel);
		top_keep_one(d, &rm, true);
		top_move(d, &rm, true);
	}
	float at_least = top_desired(d) - avail;
	if (at_least <= 0.0f) return;
	TopList rm = top_from_end(d, at_least, sel);
	top_keep_one(d, &rm, false);
	top_move(d, &rm, true);
}

static void top_clear_overflow(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) d->items [i].in_overflow = false;
	d->top_more = false;
}

static void top_measure_more(FluxNavViewData *d) {
	xent_setcollapsed(d->ctx, d->more, true);
	float hidden = top_layout_width(d, d->top_grid);
	xent_setcollapsed(d->ctx, d->more, false);
	d->top_more_width = top_layout_width(d, d->top_grid) - hidden;
}

static void top_reset_and_arrange(FluxNavViewData *d, float avail) {
	top_clear_overflow(d);
	if (top_desired(d) < avail) return;
	top_measure_more(d);
	d->top_more = true;
	top_shrink(d, top_desired(d), avail);
}

static void top_measure_overflow(FluxNavViewData *d, float avail) {
	float desired = top_desired(d);
	if (desired > avail) {
		top_shrink(d, desired, avail);
		return;
	}
	if (desired >= avail) return;
	TopList all = {0};
	for (int i = 0; i < d->count; i++)
		if (top_entry(&d->items [i]) && d->items [i].in_overflow) top_push(&all, i);
	float recover = fmaxf(0.0f, top_list_w(d, &all) - d->top_more_width);
	if (avail >= desired + recover + TOP_RECOVER_PAD) {
		top_reset_and_arrange(d, avail);
		return;
	}
	TopList mv = top_recover(d, avail - desired, -1);
	top_move(d, &mv, false);
}

static void top_restack(FluxNavViewData *d) {
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem *it = &d->items [i];
		if (!top_entry(it)) continue;
		XentNodeId host = it->in_overflow && d->overflow_host != XENT_NODE_INVALID ? d->overflow_host : d->items_host;
		flux_nav_place(d, host, it->node, XENT_NODE_INVALID);
		if (it->has_children && d->fly_owner != i) flux_nav_home_host(d, it);
	}
}

static void top_commit(FluxNavViewData *d) {
	top_restack(d);
	xent_setcollapsed(d->ctx, d->more, d->mode != FLUX_NAV_TOP || !d->top_more);
	flux_nav_refresh_items(d);
	if (d->fly_owner == FLUX_NAV_FLY_OVERFLOW && !top_has_overflow(d)) flux_nav_fly_close(d);
}

void flux_nav_top_arrange(FluxNavViewData *d) {
	if (d->mode != FLUX_NAV_TOP) return;
	if (top_has_overflow(d) && !flux_nav_fly_ensure(d)) return;
	bool  was   = top_has_overflow(d);
	float avail = top_avail(d);
	if (was) top_measure_overflow(d, avail);
	else if (top_layout_width(d, d->top_grid) > avail) top_reset_and_arrange(d, avail);
	if (top_has_overflow(d) && !flux_nav_fly_ensure(d)) {
		top_reset_and_arrange(d, 1.0e9f);
		return;
	}
	d->top_more = top_has_overflow(d);
	top_commit(d);
}

void flux_nav_top_bring(FluxNavViewData *d, int index) {
	if (d->mode != FLUX_NAV_TOP || index < 0) return;
	int top = index;
	while (d->items [top].parent >= 0) top = d->items [top].parent;
	if (!top_entry(&d->items [top]) || !d->items [top].in_overflow) return;
	float   at_least = top_desired(d) + top_w(d, top) - top_avail(d);
	TopList rm       = top_from_end(d, at_least, -1);
	float   recover  = top_list_w(d, &rm) - at_least;
	TopList add      = top_recover(d, recover, top);
	if (!top_contains(&add, top)) top_push(&add, top);
	top_move(d, &add, false);
	top_move(d, &rm, true);
	d->top_more = top_has_overflow(d);
	top_commit(d);
	flux_nav_top_arrange(d);
	d->ind_layout_pending = true;
}

static void top_host_axis(FluxNavViewData *d, XentNodeId host, bool top) {
	xent_setflexdir(d->ctx, host, top ? XENT_FLEX_ROW : XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, host, top ? XENT_FLEX_ALIGN_CENTER : XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(d->ctx, host, top ? 0.0f : FLUX_NAV_ITEM_GAP);
	xent_setp(
	  d->ctx, host, top ? (XentInsets) {0} : (XentInsets) {0, FLUX_NAV_ITEM_GAP * 0.5f, 0, FLUX_NAV_ITEM_GAP * 0.5f}
	);
	xent_setfit(d->ctx, host, top, true);
}

static void top_place_items_host(FluxNavViewData *d, bool top) {
	XentNodeId want = top ? d->top_grid : d->items_scroll;
	if (xent_node_parent(d->ctx, d->items_host) == want) return;
	flux_nav_place(d, want, d->items_host, top ? d->more : XENT_NODE_INVALID);
}

static void top_place_footer_host(FluxNavViewData *d, bool top) {
	XentNodeId want = top ? d->top_grid : d->footer_scroll;
	if (xent_node_parent(d->ctx, d->footer_host) != want) flux_nav_place(d, want, d->footer_host, XENT_NODE_INVALID);
	xent_setcollapsed(d->ctx, d->footer_scroll, top);
	if (top) xent_setcollapsed(d->ctx, d->pane_separator, true);
}

void flux_nav_top_layout(FluxNavViewData *d, bool top) {
	xent_setcollapsed(d->ctx, d->top_grid, !top);
	top_place_items_host(d, top);
	top_place_footer_host(d, top);
	xent_setcollapsed(d->ctx, d->items_scroll, top);
	top_host_axis(d, d->items_host, top);
	top_host_axis(d, d->footer_host, top);
	xent_setcollapsed(d->ctx, d->top_spacer, !top);
	flux_uia_part_hooks(d->store, d->items_host, top ? &top_uia_primary : &top_uia_menu_host);
	flux_uia_part_hooks(d->store, d->footer_host, top ? &top_uia_footer : &top_uia_repeater);
	if (d->title_bar != XENT_NODE_INVALID) flux_titlebar_settoggle(d->store, d->title_bar, !top);
	if (!top) top_clear_overflow(d);
	for (int i = 0; i < d->count; i++) d->items [i].top_w = 0.0f;
	top_restack(d);
	xent_setcollapsed(d->ctx, d->more, !top || !d->top_more);
	flux_nav_chrome_sync(d);
}
