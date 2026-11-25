#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/behavior/flux_nav_internal.h"
#include "controls/flux_module.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_basic.h"
#include "fluxent/flux_control_type.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_theme.h"
#include "host/flux_prop_apply.h"
#include "runtime/flux_str.h"

#include <math.h>
#include <xtk/xtk.h>

#define NAV_TOP_SEARCH_MIN_WIDTH   216.0f
#define NAV_LEFT_SEARCH_MIN_HEIGHT 40.0f
// measured 40 in WinUI Gallery: NavigationView reads PaneToggleButtonHeight from Application.Resources with a 40 fallback
#define NAV_TOGGLE_ROW_MIN_HEIGHT  40.0f
#define NAV_GRID_OUTSET            -4.0f
#define NAV_LEFT_SEARCH_MARGIN     12.0f

static FluxUiaHooks const chrome_uia_hidden = {.role = FLUX_UIA_HIDDEN};
static FluxUiaHooks const chrome_uia_search
  = {.role = FLUX_UIA_BUTTON, .patterns = FLUX_UIA_P_INVOKE, .invoked = true, .klass = L"Button"};

static bool chrome_filled(FluxNavViewData const *d, XentNodeId node) {
	return node != XENT_NODE_INVALID && xent_node_first(d->ctx, node) != XENT_NODE_INVALID;
}

static void chrome_put(FluxNavViewData *d, XentNodeId parent, XentNodeId node, XentNodeId before) {
	if (node == XENT_NODE_INVALID || parent == XENT_NODE_INVALID) return;
	if (xent_node_parent(d->ctx, node) == parent && xent_node_next(d->ctx, node) == before) return;
	flux_nav_place(d, parent, node, before);
}

static void chrome_axis(FluxNavViewData *d, XentNodeId node, bool top) {
	if (node == XENT_NODE_INVALID) return;
	xent_setfit(d->ctx, node, top, !top);
	xent_setshrink(d->ctx, node, 0.0f);
	xent_setself(d->ctx, node, top ? XENT_FLEX_ALIGN_CENTER : XENT_FLEX_ALIGN_STRETCH);
	xent_setsize(d->ctx, node, (XentSize) {NAN, top ? FLUX_NAV_TOP_H : NAN});
}

static bool chrome_title_on(FluxNavViewData const *d) {
	if (d->mode == FLUX_NAV_TOP || !d->labels_visible) return false;
	if (d->mode == FLUX_NAV_MINIMAL && !d->pane_open) return false;
	return d->pane_title && d->pane_title [0];
}

static bool chrome_header_on(FluxNavViewData const *d) {
	if (!chrome_filled(d, d->chrome_header)) return false;
	return d->mode == FLUX_NAV_TOP || d->labels_visible;
}

static void chrome_search_axis(FluxNavViewData *d, bool top) {
	XentNodeId area      = d->chrome_search_area;
	XentNodeId presenter = d->chrome_search;
	chrome_axis(d, area, top);
	xent_setjustify(d->ctx, area, XENT_FLEX_JUSTIFY_CENTER);
	xent_setminsize(d->ctx, area, (XentSize) {0.0f, top ? 0.0f : NAV_LEFT_SEARCH_MIN_HEIGHT});
	xent_setm(d->ctx, area, top ? (XentInsets) {0} : (XentInsets) {0, 0, 0, 8});
	xent_setm(
	  d->ctx, presenter,
	  top ? (XentInsets) {4, 0, 4, 0} : (XentInsets) {NAV_LEFT_SEARCH_MARGIN, 0, NAV_LEFT_SEARCH_MARGIN, 0}
	);
	xent_setfit(d->ctx, presenter, false, true);
	xent_setminsize(d->ctx, presenter, (XentSize) {top ? NAV_TOP_SEARCH_MIN_WIDTH : 0.0f, 0.0f});
}

static void chrome_show(FluxNavViewData *d, XentNodeId node, bool on) {
	if (node == XENT_NODE_INVALID || xent_collapsed(d->ctx, node) == !on) return;
	xent_setcollapsed(d->ctx, node, !on);
}

static void chrome_header_spacing(FluxNavViewData *d, bool toggle) {
	float outset = toggle ? NAV_GRID_OUTSET : 0.0f;
	float inset  = toggle ? FLUX_NAV_TOGGLE_W + flux_nav_overlay_button_width(d) : 0.0f;
	xent_setm(d->ctx, d->chrome_column, (XentInsets) {outset, 0, outset, 0});
	xent_setm(d->ctx, d->chrome_header, (XentInsets) {inset, 0, 0, 0});
}

static void chrome_header_layout(FluxNavViewData *d, bool top, bool title, bool header) {
	bool       toggle = !top && !d->window_title_bar;
	XentNodeId parent = toggle ? d->toggle : d->chrome_column;
	if (!toggle && xent_node_parent(d->ctx, d->chrome_title) != parent)
		xent_setsize(d->ctx, d->chrome_title, (XentSize) {NAN, NAN});
	chrome_put(d, parent, d->chrome_title, toggle ? XENT_NODE_INVALID : d->chrome_header);
	chrome_show(d, d->chrome_column, toggle || title || header);
	xent_setminsize(d->ctx, d->chrome_column, (XentSize) {0.0f, toggle ? NAV_TOGGLE_ROW_MIN_HEIGHT : 0.0f});
	chrome_header_spacing(d, toggle);
}

static void chrome_apply_axis(FluxNavViewData *d, bool top) {
	uint8_t want = top ? 2u : 1u;
	if (d->chrome_laid == want) return;
	d->chrome_laid = want;
	chrome_axis(d, d->chrome_column, top);
	chrome_search_axis(d, top);
	chrome_axis(d, d->chrome_custom, top);
	xent_setself(d->ctx, d->chrome_custom, XENT_FLEX_ALIGN_STRETCH);
	xent_setm(d->ctx, d->chrome_custom, top ? (XentInsets) {0} : (XentInsets) {NAV_GRID_OUTSET, 0, NAV_GRID_OUTSET, 0});
	chrome_axis(d, d->chrome_footer, top);
	xent_setm(d->ctx, d->chrome_footer, top ? (XentInsets) {0} : (XentInsets) {NAV_GRID_OUTSET, 0, NAV_GRID_OUTSET, 4});
}

static void chrome_visibility(FluxNavViewData *d) {
	bool top    = d->mode == FLUX_NAV_TOP;
	bool closed = flux_nav_closed_compact(d);
	bool search = chrome_filled(d, d->chrome_search);
	bool title  = chrome_title_on(d);
	bool header = chrome_header_on(d);
	chrome_show(d, d->chrome_title, title);
	chrome_show(d, d->chrome_header, header);
	chrome_header_layout(d, top, title, header);
	chrome_show(d, d->chrome_search_area, search);
	chrome_show(d, d->chrome_search, search && !closed);
	chrome_show(d, d->chrome_search_button, search && closed);
	chrome_show(d, d->chrome_custom, chrome_filled(d, d->chrome_custom));
	chrome_show(d, d->chrome_footer, chrome_filled(d, d->chrome_footer));
	chrome_apply_axis(d, top);
}

static void chrome_order_top(FluxNavViewData *d) {
	chrome_put(d, d->top_grid, d->chrome_column, d->items_host);
	chrome_put(d, d->top_spacer, d->chrome_custom, XENT_NODE_INVALID);
	chrome_put(d, d->top_grid, d->chrome_search_area, d->footer_host);
	chrome_put(d, d->top_grid, d->chrome_footer, d->footer_host);
}

static void chrome_order_left(FluxNavViewData *d) {
	chrome_put(d, d->pane, d->chrome_column, d->items_scroll);
	chrome_put(d, d->pane, d->chrome_search_area, d->items_scroll);
	chrome_put(d, d->pane, d->chrome_custom, d->items_scroll);
	chrome_put(d, d->pane, d->chrome_footer, d->footer_scroll);
	chrome_put(d, d->pane, d->pane_separator, d->chrome_footer);
}

static void chrome_settings_tip(FluxNavViewData *d) {
	FluxNodeData *nd = flux_store_find(d->store, d->settings);
	if (!nd) return;
	flux_str_replace(&nd->tooltip_text, d->mode == FLUX_NAV_TOP || !d->labels_visible ? "Settings" : NULL);
}

void flux_nav_chrome_sync(FluxNavViewData *d) {
	if (!d || d->chrome_column == XENT_NODE_INVALID) return;
	chrome_visibility(d);
	if (d->mode == FLUX_NAV_TOP) chrome_order_top(d);
	else chrome_order_left(d);
	chrome_settings_tip(d);
}

static float chrome_desired_width(FluxNavViewData const *d, XentNodeId node) {
	XentSize size = {0};
	xent_layout_desired_size(d->ctx, node, (XentSize) {INFINITY, FLUX_NAV_TOP_H}, &size);
	return size.w;
}

static bool chrome_widths_changed(FluxNavViewData const *d, float hw, float sw, float cw, float fw) {
	return hw != d->chrome_header_w || sw != d->chrome_search_w || cw != d->chrome_custom_w || fw != d->chrome_footer_w;
}

static float chrome_desired_height(FluxNavViewData const *d, XentNodeId node, float width) {
	XentSize size = {0};
	xent_layout_desired_size(d->ctx, node, (XentSize) {width, INFINITY}, &size);
	return size.h;
}

static float chrome_footer_cap(float available, float menu, float footer, float custom, bool *separated) {
	float half  = available * 0.5f;
	float group = footer + custom;
	*separated  = footer > 0.0f && menu > 0.0f && available < menu + group;
	if (footer <= 0.0f) return 0.0f;
	if (menu <= 0.0f) return available;
	if (!*separated || group <= half) return footer;
	return menu <= half ? available - menu : half;
}

static void chrome_separator(FluxNavViewData *d, bool visible) {
	chrome_show(d, d->pane_separator, visible);
	FluxNodeData *nd = flux_store_find(d->store, d->pane_separator);
	if (!nd) return;
	FluxThemeColors const *colors = d->theme ? flux_theme_colors(d->theme) : flux_theme_defaults();
	nd->visuals.background = colors->contrast.active ? colors->contrast.window_text : colors->divider_stroke_default;
}

static bool chrome_layout_footer(FluxNavViewData *d) {
	XentRect pane = {0}, menu = {0}, custom = {0};
	if (!xent_layout_rect(d->ctx, d->pane, &pane) || !xent_layout_rect(d->ctx, d->items_scroll, &menu)) return false;
	if (chrome_filled(d, d->chrome_footer)) xent_layout_rect(d->ctx, d->chrome_footer, &custom);
	float available = fmaxf(0.0f, pane.y + pane.h - FLUX_NAV_GRID_MARGIN_Y - menu.y);
	float menu_h    = chrome_desired_height(d, d->items_host, menu.w);
	float footer_h  = chrome_desired_height(d, d->footer_host, menu.w);
	float custom_h  = chrome_filled(d, d->chrome_footer) ? custom.h + 4.0f : 0.0f;
	bool  separated = false;
	float cap       = chrome_footer_cap(available, menu_h, footer_h, custom_h, &separated);
	float height    = fminf(footer_h, fmaxf(0.0f, cap));
	chrome_separator(d, separated);
	if (height == d->footer_viewport_h) return false;
	d->footer_viewport_h = height;
	xent_seth(d->ctx, d->footer_scroll, height);
	return true;
}

bool flux_nav_chrome_sample(FluxNavViewData *d) {
	flux_nav_chrome_sync(d);
	if (!d) return false;
	if (d->mode != FLUX_NAV_TOP) return chrome_layout_footer(d);
	float hw           = chrome_desired_width(d, d->chrome_column);
	float sw           = chrome_desired_width(d, d->chrome_search_area);
	float cw           = fmaxf(chrome_desired_width(d, d->chrome_custom), FLUX_NAV_TOP_CUSTOM_MIN);
	float fw           = chrome_desired_width(d, d->chrome_footer);
	bool  changed      = chrome_widths_changed(d, hw, sw, cw, fw);
	d->chrome_header_w = hw;
	d->chrome_search_w = sw;
	d->chrome_custom_w = cw;
	d->chrome_footer_w = fw;
	if (changed) xent_setminsize(d->ctx, d->top_spacer, (XentSize) {cw, 0.0f});
	return changed;
}

float flux_nav_chrome_width(FluxNavViewData const *d) {
	if (!d || d->mode != FLUX_NAV_TOP) return 0.0f;
	return d->chrome_header_w + d->chrome_custom_w + d->chrome_search_w + d->chrome_footer_w;
}

static XentNodeId chrome_host_for(FluxNavViewData *d, uint32_t slot) {
	if (slot == XTK_NAV_SLOT_HEADER) return d->chrome_header;
	if (slot == XTK_NAV_SLOT_CUSTOM) return d->chrome_custom;
	if (slot == XTK_NAV_SLOT_FOOTER) return d->chrome_footer;
	if (slot == XTK_NAV_SLOT_SEARCH) return d->chrome_search;
	if (slot == XTK_NAV_SLOT_PAGE_HEADER) return d->page_header;
	if (slot == XTK_NAV_SLOT_CONTENT_OVERLAY) return d->content_overlay;
	return d->content;
}

XentNodeId flux_nav_slot(FluxStore *store, XentNodeId nav, uint32_t slot) {
	FluxNavViewData *d = flux_nav_data(store, nav);
	return d ? chrome_host_for(d, slot) : XENT_NODE_INVALID;
}

static bool chrome_is_child_host(FluxNavViewData const *d, XentNodeId host) {
	return host == d->content
	    || host == d->chrome_header
	    || host == d->chrome_custom
	    || host == d->chrome_footer
	    || host == d->chrome_search
	    || host == d->page_header
	    || host == d->content_overlay;
}

void flux_nav_slot_changed(FluxStore *store, XentNodeId node) {
	XentCtx      *ctx = flux_store_ctx(store);
	FluxNodeData *nd  = flux_store_find(store, node);
	if (!nd) return;
	XentNodeId parent = xent_node_parent(ctx, node);
	for (XentNodeId p = parent; p != XENT_NODE_INVALID; p = xent_node_parent(ctx, p)) {
		FluxNavViewData *d = flux_nav_data(store, p);
		if (!d || p != d->root) continue;
		if (!chrome_is_child_host(d, parent)) return;
		XentNodeId host = chrome_host_for(d, nd->nav_slot);
		if (host != XENT_NODE_INVALID && parent != host) flux_nav_place(d, host, node, XENT_NODE_INVALID);
		flux_nav_chrome_sync(d);
		return;
	}
}

void flux_nav_set_pane_title(FluxStore *store, XentNodeId nav, char const *title) {
	FluxNavViewData *d = flux_nav_data(store, nav);
	if (!d) return;
	char const *text = title && title [0] ? title : NULL;
	flux_str_replace(&d->pane_title, text);
	if (d->chrome_title != XENT_NODE_INVALID) flux_prop_text_content(store, d->chrome_title, text ? text : "");
	flux_nav_chrome_sync(d);
}

static XentNodeId chrome_make(FluxNavViewData *d, XentNodeId parent) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, parent, FLUX_CONTROL_CONTAINER);
	if (node == XENT_NODE_INVALID) return node;
	xent_setproto(d->ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, node, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, node, XENT_FLEX_ALIGN_STRETCH);
	xent_setfit(d->ctx, node, false, true);
	xent_setshrink(d->ctx, node, 0.0f);
	xent_setcollapsed(d->ctx, node, true);
	xent_set_focusable(d->ctx, node, false);
	flux_uia_part_hooks(d->store, node, &chrome_uia_hidden);
	return node;
}

static void chrome_make_title(FluxNavViewData *d) {
	d->chrome_title = flux_text_init(&(FluxTextDesc) {
	  .ctx       = d->ctx,
	  .store     = d->store,
	  .parent    = d->chrome_column,
	  .content   = "",
	  .font_size = 14.0f,
	  .text      = d->text});
	if (d->chrome_title == XENT_NODE_INVALID) return;
	flux_prop_text_weight(d->store, d->chrome_title, FLUX_FONT_SEMI_BOLD);
	flux_prop_text_wrap(d->store, d->chrome_title, false);
	xent_setcollapsed(d->ctx, d->chrome_title, true);
	xent_setshrink(d->ctx, d->chrome_title, 0.0f);
}

static void chrome_search_click(void *ctx) { flux_nav_open_for_search(ctx); }

static void chrome_make_search_button(FluxNavViewData *d) {
	d->chrome_search_button
	  = flux_factory_create_node(d->ctx, d->store, d->chrome_search_area, FLUX_CONTROL_NAV_VIEW_ITEM);
	if (d->chrome_search_button == XENT_NODE_INVALID) return;
	xent_setsize(d->ctx, d->chrome_search_button, (XentSize) {40.0f, FLUX_NAV_ITEM_HEIGHT});
	xent_setself(d->ctx, d->chrome_search_button, XENT_FLEX_ALIGN_CENTER);
	xent_setshrink(d->ctx, d->chrome_search_button, 0.0f);
	xent_setcollapsed(d->ctx, d->chrome_search_button, true);
	FluxNodeData *nd = flux_store_find(d->store, d->chrome_search_button);
	if (!nd) return;
	d->search_item       = (FluxNavViewItem) {d, d->chrome_search_button, NULL, "Search", FLUX_NAV_ITEM_MENU, -1};
	d->search_item.place = FLUX_NAV_PLACE_LEFT;
	d->search_item.children_host = XENT_NODE_INVALID;
	nd->component_data           = &d->search_item;
	nd->behavior.on_click        = chrome_search_click;
	nd->behavior.on_click_ctx    = d;
	nd->uia                      = &chrome_uia_search;
	xent_set_focusable(d->ctx, d->chrome_search_button, true);
	xent_sem_setrole(d->ctx, d->chrome_search_button, XENT_SEM_ROLE_BUTTON);
	xent_sem_setlabel(d->ctx, d->chrome_search_button, "Search");
	flux_uia_settext(d->store, d->chrome_search_button, FLUX_A11Y_ID, "PaneAutoSuggestButton");
}

void flux_nav_chrome_setup(FluxNavViewData *d) {
	d->chrome_column = chrome_make(d, d->pane);
	d->chrome_header = chrome_make(d, d->chrome_column);
	chrome_make_title(d);
	d->chrome_search_area = chrome_make(d, d->pane);
	d->chrome_search      = chrome_make(d, d->chrome_search_area);
	d->chrome_custom      = chrome_make(d, d->pane);
	d->chrome_footer      = chrome_make(d, d->pane);
	xent_set_focusable(d->ctx, d->pane_separator, false);
	flux_uia_part_hooks(d->store, d->pane_separator, &chrome_uia_hidden);
	chrome_make_search_button(d);
	d->chrome_custom_w = FLUX_NAV_TOP_CUSTOM_MIN;
}
