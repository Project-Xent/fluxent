#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_object.h"
#include "controls/flux_module.h"
#include "popup/flux_popup_tree.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_container.h"
#include "controls/behavior/flux_nav_internal.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/draw/flux_control_draw.h"
#include "input/flux_input_internal.h"
#include "render/flux_icon.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <math.h>

#define FLY_BORDER      1.0f
#define FLY_PAD_V       2.0f
#define FLY_LEFT_MARGIN 4.0f
#define FLY_LEFT_INSET  2.0f
#define FLY_LEFT_TRAIL  8.0f
#define FLY_CHEVRON     28.0f
#define FLY_INDENT      31.0f
#define FLY_OVF_ICON    44.0f
#define FLY_OVF_NOICON  16.0f
#define FLY_OVF_TRAIL   34.0f
#define FLY_ANCHOR_LIFT 4.0f

static FluxUiaHooks const fly_uia_overflow = {.role = FLUX_UIA_GROUP};

static bool               fly_left_style(FluxNavViewData const *d) { return d->mode != FLUX_NAV_TOP; }

static float              fly_row_w(FluxNavViewData const *d, FluxNavViewItem const *it) {
	float text = flux_nav_text_w(d, it->label, 400u);
	float chev = it->has_children ? FLY_CHEVRON : 0.0f;
	if (fly_left_style(d)) {
		float indent = ( float ) flux_nav_draw_depth(d, it) * FLY_INDENT;
		return 2.0f * FLY_LEFT_MARGIN + FLY_LEFT_INSET + indent + FLUX_NAV_ICON_BOX + text + FLY_LEFT_TRAIL + chev;
	}
	bool icon = flux_icon_lookup(it->icon_name) != NULL;
	return (icon ? FLY_OVF_ICON : FLY_OVF_NOICON) + text + FLY_OVF_TRAIL + chev;
}

static float fly_rows_w(FluxNavViewData const *d, int parent) {
	float w = 0.0f;
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem const *c = &d->items [i];
		if (c->parent != parent) continue;
		w = fmaxf(w, fly_row_w(d, c));
		if (c->has_children && c->expanded && !flux_nav_flyout_repeater(d, c)) w = fmaxf(w, fly_rows_w(d, i));
	}
	return w;
}

static void fly_overflow_size(FluxNavViewData const *d, float *w, float *h) {
	*w = *h = 0.0f;
	for (int i = 0; i < d->count; i++) {
		FluxNavViewItem const *it = &d->items [i];
		if (it->parent >= 0 || !it->in_overflow) continue;
		*w  = fmaxf(*w, fly_row_w(d, it));
		*h += FLUX_NAV_OVF_ROW_H;
		if (!it->has_children || !it->expanded) continue;
		*w  = fmaxf(*w, fly_rows_w(d, i));
		*h += it->expand_t.current * flux_nav_host_full_h(d, i);
	}
}

static XentInsets fly_padding(FluxNavViewData const *d) {
	float side = FLY_BORDER + (fly_left_style(d) ? FLY_LEFT_MARGIN : 0.0f);
	float v    = FLY_BORDER + FLY_PAD_V + (fly_left_style(d) ? FLY_PAD_V : 0.0f);
	return (XentInsets) {side, v, side, v};
}

static void fly_measure(FluxNavViewData const *d, float *w, float *h) {
	if (d->fly_owner == FLUX_NAV_FLY_OVERFLOW) fly_overflow_size(d, w, h);
	else {
		*w = fly_rows_w(d, d->fly_owner);
		*h = flux_nav_host_full_h(d, d->fly_owner);
	}
	XentInsets p = fly_padding(d);
	if (fly_left_style(d)) *w -= 2.0f * FLY_LEFT_MARGIN;
	*w = fminf(fmaxf(*w + p.left + p.right, FLUX_FLYOUT_MIN_W), FLUX_FLYOUT_MAX_W);
	*h = fminf(*h + p.top + p.bottom, FLUX_FLYOUT_MAX_H);
}

static void fly_layout(FluxNavViewData *d) {
	float w = 0.0f, h = 0.0f;
	fly_measure(d, &w, &h);
	h = flux_popup_fit_content_height(d->fly_popup, h);
	xent_setp(d->ctx, d->fly_root, fly_padding(d));
	xent_setsize(d->ctx, d->fly_root, (XentSize) {w, h});
	xent_layout(d->ctx, d->fly_root, w, h);
	flux_popup_setsize(d->fly_popup, w, h);
}

static XentNodeId fly_anchor_node(FluxNavViewData const *d, int owner) {
	if (owner == FLUX_NAV_FLY_OVERFLOW) return d->more;
	return owner >= 0 && owner < d->count ? d->items [owner].node : XENT_NODE_INVALID;
}

static void fly_restore_focus(FluxNavViewData *d, int owner) {
	XentNodeId focus = flux_input_focus(d->input);
	if (!input_within(d->ctx, focus, d->fly_root)) return;
	XentNodeId anchor = fly_anchor_node(d, owner);
	if (anchor == XENT_NODE_INVALID || xent_collapsed(d->ctx, anchor)) {
		flux_input_setfocus(d->input, XENT_NODE_INVALID, false);
		return;
	}
	FluxNodeData *nd = flux_store_find(d->store, focus);
	flux_input_setfocus(d->input, anchor, !nd || !nd->state.focused || nd->state.suppress_focus);
}

static void fly_dismissed(void *ctx) {
	FluxNavViewData *d     = ( FluxNavViewData * ) ctx;
	int              owner = d->fly_owner;
	fly_restore_focus(d, owner);
	d->fly_owner = FLUX_NAV_FLY_NONE;
	if (owner >= 0 && owner < d->count) {
		d->items [owner].expanded = false;
		flux_nav_home_host(d, &d->items [owner]);
	}
	if (d->overflow_host != XENT_NODE_INVALID) xent_setcollapsed(d->ctx, d->overflow_host, true);
	flux_nav_sync_hosts(d);
	flux_nav_anim_start(d);
	flux_nav_repaint(d);
}

static XentNodeId fly_column(FluxNavViewData *d, XentNodeId parent, FluxControlType type) {
	XentNodeId n = flux_factory_create_node(d->ctx, d->store, parent, type);
	if (n == XENT_NODE_INVALID) return n;
	xent_setproto(d->ctx, n, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, n, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, n, XENT_FLEX_ALIGN_STRETCH);
	return n;
}

static bool fly_build_tree(FluxNavViewData *d) {
	d->fly_root = fly_column(d, XENT_NODE_INVALID, FLUX_CONTROL_FLYOUT);
	if (d->fly_root == XENT_NODE_INVALID) return false;
	d->fly_scroll = flux_scroll_init(&(FluxContainerDesc) {d->ctx, d->store, d->fly_root});
	if (d->fly_scroll == XENT_NODE_INVALID) return false;
	xent_setitems(d->ctx, d->fly_scroll, XENT_FLEX_ALIGN_STRETCH);
	xent_setbasis(d->ctx, d->fly_scroll, 0.0f);
	xent_setgrow(d->ctx, d->fly_scroll, 1.0f);
	d->overflow_host = fly_column(d, d->fly_scroll, FLUX_CONTROL_CONTAINER);
	return d->overflow_host != XENT_NODE_INVALID;
}

static bool fly_owner_press(void *ctx, XentNodeId node) {
	FluxNavViewData const *d = ctx;
	if (d->mode != FLUX_NAV_TOP) return false;
	FluxNodeData const *nd = flux_store_find(d->store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_NAV_VIEW_ITEM || !nd->behavior.on_click) return false;
	FluxNavViewItem const *it = nd->component_data;
	return it && it->nav == d && it->place == FLUX_NAV_PLACE_TOP && xent_sem_enabled(d->ctx, node);
}

static bool fly_bind_tree(FluxNavViewData *d) {
	FluxNodeData *root = flux_store_find(d->store, d->fly_root);
	if (!root) return false;
	root->focus_passive  = true;
	root->clips_children = true;
	xent_setshrink(d->ctx, d->overflow_host, 0.0f);
	xent_setfit(d->ctx, d->overflow_host, false, true);
	xent_setcollapsed(d->ctx, d->overflow_host, true);
	flux_uia_part_hooks(d->store, d->overflow_host, &fly_uia_overflow);
	flux_popup_setanim(d->fly_popup, FLUX_POPUP_ANIM_FLYOUT);
	flux_popup_ondismiss(d->fly_popup, fly_dismissed, d);
	flux_popup_onownerpress(d->fly_popup, fly_owner_press, d);
	return flux_popup_setcontent(d->fly_popup, d->ctx, d->store, d->fly_root)
	    && (!d->input || flux_popup_sethost(d->fly_popup, d->input, d->pane_clip));
}

bool flux_nav_fly_ensure(FluxNavViewData *d) {
	if (d->fly_popup) return true;
	d->fly_popup = flux_popup_create(d->window);
	if (!d->fly_popup) return false;
	if (fly_build_tree(d) && fly_bind_tree(d)) return true;
	flux_nav_fly_destroy(d);
	return false;
}

static FluxRect fly_anchor(FluxNavViewData *d, XentNodeId node, FluxPlacement *placement) {
	FluxRect a = flux_binding_screen_anchor(d->window, d->ctx, d->store, node);
	if (d->fly_owner == FLUX_NAV_FLY_OVERFLOW) {
		*placement = FLUX_PLACEMENT_BOTTOM_EDGE_RIGHT;
		return a;
	}
	if (d->mode == FLUX_NAV_TOP) {
		*placement = FLUX_PLACEMENT_BOTTOM_EDGE_LEFT;
		return a;
	}
	FluxDpi dpi    = flux_window_dpi(d->window);
	float   scale  = dpi.dpi_x > 0.0f ? dpi.dpi_x / 96.0f : 1.0f;
	float   margin = FLUX_NAV_ITEM_MARGIN_H * scale;
	*placement     = FLUX_PLACEMENT_RIGHT_EDGE_TOP;
	return (FluxRect) {a.x - margin, a.y - FLY_ANCHOR_LIFT * scale, a.w + 2.0f * margin, a.h};
}

static XentNodeId fly_take_content(FluxNavViewData *d, int owner) {
	int view = owner == FLUX_NAV_FLY_OVERFLOW ? XTK_ACCESS_VIEW_CONTENT : XTK_ACCESS_VIEW_RAW;
	flux_uia_setnum(d->store, d->fly_scroll, FLUX_A11Y_VIEW, view);
	if (owner == FLUX_NAV_FLY_OVERFLOW) {
		xent_setcollapsed(d->ctx, d->overflow_host, false);
		return d->more;
	}
	FluxNavViewItem *it = &d->items [owner];
	xent_setcollapsed(d->ctx, d->overflow_host, true);
	flux_nav_graft_host(d, it, false);
	flux_nav_place(d, d->fly_scroll, it->children_host, XENT_NODE_INVALID);
	it->expanded = true;
	return it->node;
}

void flux_nav_fly_open(FluxNavViewData *d, int owner) {
	if (d->fly_owner == owner || !flux_nav_fly_ensure(d)) return;
	if (d->fly_owner != FLUX_NAV_FLY_NONE) flux_nav_fly_close(d);
	XentNodeId anchor_node = fly_take_content(d, owner);
	d->fly_owner           = owner;
	flux_nav_sync_hosts(d);
	FluxPlacement placement = FLUX_PLACEMENT_BOTTOM;
	FluxRect      anchor    = fly_anchor(d, anchor_node, &placement);
	flux_popup_setrtl(d->fly_popup, xent_resolved_dir(d->ctx, anchor_node) == XENT_DIRECTION_RTL);
	fly_layout(d);
	flux_popup_rebind(d->fly_popup, anchor_node);
	flux_popup_show(d->fly_popup, anchor, placement);
	if (flux_input_focus(d->input) == anchor_node) {
		FluxNodeData *nd = flux_store_find(d->store, anchor_node);
		flux_input_focuswithin(d->input, d->fly_root, !nd || !nd->state.focused || nd->state.suppress_focus);
	}
	flux_nav_anim_start(d);
	flux_nav_repaint(d);
}

void flux_nav_fly_close(FluxNavViewData *d) {
	if (d->fly_owner == FLUX_NAV_FLY_NONE) return;
	if (d->fly_popup && flux_popup_visible(d->fly_popup)) flux_popup_dismiss(d->fly_popup);
	if (d->fly_owner != FLUX_NAV_FLY_NONE) fly_dismissed(d);
}

void flux_nav_fly_refresh(FluxNavViewData *d) {
	if (d->fly_owner == FLUX_NAV_FLY_NONE || !d->fly_popup || !flux_popup_visible(d->fly_popup)) return;
	fly_layout(d);
}

void flux_nav_fly_destroy(FluxNavViewData *d) {
	if (!d->fly_popup) return;
	flux_popup_ondismiss(d->fly_popup, NULL, NULL);
	flux_popup_destroy(d->fly_popup);
	d->fly_popup = NULL;
	if (d->fly_root != XENT_NODE_INVALID) xent_node_destroy(d->ctx, d->fly_root);
	d->fly_root      = XENT_NODE_INVALID;
	d->fly_scroll    = XENT_NODE_INVALID;
	d->overflow_host = XENT_NODE_INVALID;
	d->fly_owner     = FLUX_NAV_FLY_NONE;
}

static bool fly_chain_open(FluxNavViewData const *d, FluxNavViewItem const *it, int stop) {
	for (int p = it->parent; p >= 0 && p != stop; p = d->items [p].parent)
		if (!d->items [p].expanded) return false;
	return true;
}

bool flux_nav_fly_shows(FluxNavViewData const *d, FluxNavViewItem const *it) {
	if (d->fly_owner == FLUX_NAV_FLY_NONE || !d->fly_popup || !flux_popup_visible(d->fly_popup)) return false;
	if (d->fly_owner == FLUX_NAV_FLY_OVERFLOW) {
		FluxNavViewItem const *top = it;
		while (top->parent >= 0) top = &d->items [top->parent];
		return top->in_overflow && fly_chain_open(d, it, -1);
	}
	int p = it->parent;
	while (p >= 0 && p != d->fly_owner) p = d->items [p].parent;
	return p == d->fly_owner && fly_chain_open(d, it, d->fly_owner);
}

static void fly_focus_entry(FluxNavViewData *d, int start, int step) {
	for (int i = start; i >= 0 && i < d->count; i += step) {
		FluxNavViewItem const *entry = &d->items [i];
		XentNodeId             node  = entry->node;
		if (!xent_get_focusable(d->ctx, node) || !xent_sem_enabled(d->ctx, node)) continue;
		if (!flux_nav_fly_shows(d, entry)) continue;
		flux_input_setfocus(d->input, node, false);
		FluxUiaContext info = {.ctx = d->ctx, .store = d->store};
		flux_uia_bring_into_view(&info, node);
		return;
	}
}

bool flux_nav_fly_key(FluxNavViewItem *it, FluxKey key) {
	FluxNavViewData *d = it->nav;
	switch (key) {
	case FLUX_KEY_ESCAPE : flux_nav_fly_close(d); break;
	case FLUX_KEY_HOME   : fly_focus_entry(d, 0, 1); break;
	case FLUX_KEY_END    : fly_focus_entry(d, d->count - 1, -1); break;
	case FLUX_KEY_UP     : fly_focus_entry(d, it->index - 1, -1); break;
	case FLUX_KEY_DOWN   : fly_focus_entry(d, it->index + 1, 1); break;
	default              : return key == FLUX_KEY_LEFT || key == FLUX_KEY_RIGHT;
	}
	return true;
}
