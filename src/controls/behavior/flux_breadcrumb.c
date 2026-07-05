#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_menu_flyout.h"
#include "fluxent/flux_window.h"
#include "fluxent/controls/flux_breadcrumb_data.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* WinUI BreadcrumbBar (BreadcrumbBar.cpp / BreadcrumbLayout.cpp). The element
 * list is [ellipsis, crumb0..crumbN-1] — element 0 is always the synthetic
 * ellipsis, arranged 0x0 unless the crumbs overflow the assigned width. The
 * last crumb is the current location: plain text, focusable, never raises
 * ItemClicked. Ellipsis invoke opens a bottom-placed light-dismiss flyout of
 * the hidden crumbs, reversed (closest-to-visible first, root last); clicking
 * a row raises ItemClicked with the data index. Crumb clicks fire on release;
 * Left/Right move focus with the WinUI ellipsis jump rules (no wrap). */

static FluxBreadcrumbBarData *bc_data(FluxNodeStore *store, XentNodeId bar) {
	FluxNodeData *nd = flux_node_store_get(store, bar);
	if (!nd || nd->component_type != FLUX_CONTROL_BREADCRUMB_BAR) return NULL;
	return ( FluxBreadcrumbBarData * ) nd->component_data;
}

static void bc_repaint(FluxBreadcrumbBarData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

/* -------------------------------------------------------------------------
 * Measure: element widths from label text + glyph advances (spec formulas:
 * crumb = 1 + text + 1 [+ chevron cell]; ellipsis = 3 + E712 + 3 + cell)
 * ---------------------------------------------------------------------- */

static float bc_text_w(FluxBreadcrumbBarData *d, char const *s, float font_size) {
	if (!s || !s [0]) return 0.0f;
	if (!d->text) return 7.0f * ( float ) strlen(s); /* headless probes: rough advance */
	FluxTextStyle ts = {0};
	ts.font_size     = font_size;
	ts.font_weight   = FLUX_FONT_REGULAR;
	return flux_text_measure(d->text, s, &ts, FLT_MAX).w;
}

static float bc_glyph_w(FluxBreadcrumbBarData *d, wchar_t const *glyph, float font_size) {
	char utf8 [8];
	flux_icon_to_utf8(glyph, utf8, sizeof(utf8));
	return bc_text_w(d, utf8, font_size);
}

static void bc_measure(FluxBreadcrumbBarData *d) {
	d->chevron_cell_w = 2.0f * FLUX_BREADCRUMB_CHEVRON_PAD
	                  + ceilf(bc_glyph_w(d, FLUX_BREADCRUMB_CHEVRON_LTR, FLUX_BREADCRUMB_CHEVRON_FONT));
	d->widths [0]     = 2.0f * FLUX_BREADCRUMB_ELLIPSIS_PAD
	                  + ceilf(bc_glyph_w(d, FLUX_BREADCRUMB_ELLIPSIS, FLUX_BREADCRUMB_ELLIPSIS_FONT))
	                  + d->chevron_cell_w;

	float natural = 0.0f;
	for (int i = 1; i <= d->count; i++) {
		float w = 2.0f * FLUX_BREADCRUMB_PAD_H + ceilf(bc_text_w(d, d->labels [i - 1], FLUX_BREADCRUMB_FONT));
		if (i < d->count) w += d->chevron_cell_w; /* the last crumb has no chevron cell */
		d->widths [i]  = w;
		natural       += w;
	}
	d->natural_w = natural;
	/* Report the natural full width (the ellipsis is excluded from the desired
	 * size, per BreadcrumbLayout::MeasureOverride); the parent clamps it. */
	xent_set_size(d->ctx, d->root, (XentSize) {natural, FLUX_BREADCRUMB_ITEM_H});
	d->widths_dirty  = false;
	d->arrange_dirty = true;
}

/* -------------------------------------------------------------------------
 * Arrange: the BreadcrumbLayout collapse algorithm against the assigned width
 * ---------------------------------------------------------------------- */

static void bc_place(FluxBreadcrumbBarData *d, int elem, float x, float w) {
	xent_set_absolute_position(d->ctx, d->nodes [elem], (XentPoint) {x, 0.0f});
	xent_set_size(d->ctx, d->nodes [elem], (XentSize) {w, FLUX_BREADCRUMB_ITEM_H});
	xent_set_focusable(d->ctx, d->nodes [elem], !d->disabled);
}

static void bc_hide(FluxBreadcrumbBarData *d, int elem) {
	xent_set_absolute_position(d->ctx, d->nodes [elem], (XentPoint) {0.0f, 0.0f});
	xent_set_size(d->ctx, d->nodes [elem], (XentSize) {0.0f, 0.0f});
	xent_set_focusable(d->ctx, d->nodes [elem], false);
}

/* BreadcrumbLayout::GetFirstBreadcrumbBarItemToArrange: the last crumb and the
 * ellipsis are unconditionally budgeted, then crumbs are re-admitted right to
 * left until one would overflow. Reaching i == 0 re-adds the ellipsis width,
 * so the result is always >= 1 whenever overflow was detected — the ellipsis
 * plus the last crumb render even when they alone overflow. */
static int bc_first_to_render(FluxBreadcrumbBarData const *d, float avail) {
	int   n     = d->count;
	float accum = d->widths [n] + d->widths [0];
	for (int i = n - 1; i >= 0; i--) {
		float next = accum + d->widths [i];
		if (next > avail) return i + 1;
		accum = next;
	}
	return 0;
}

static void bc_arrange(FluxBreadcrumbBarData *d, float avail) {
	int   n           = d->count;
	float accumulated = 0.0f;
	for (int i = 1; i <= n; i++) accumulated += d->widths [i];
	d->ellipsis_rendered = n > 0 && accumulated > avail;

	int first = 1;
	if (d->ellipsis_rendered) {
		first = bc_first_to_render(d, avail);
		if (first < 1) first = 1;
	}
	d->first_rendered = first;

	float x = 0.0f;
	if (d->ellipsis_rendered) {
		bc_place(d, 0, 0.0f, d->widths [0]);
		x = d->widths [0];
	}
	else bc_hide(d, 0); /* hidden: non-hit-testable, non-focusable */

	for (int i = 1; i <= n; i++) {
		if (i < first) {
			bc_hide(d, i);
			continue;
		}
		bc_place(d, i, x, d->widths [i]);
		x += d->widths [i];
	}
	d->arrange_dirty = false;
}

/* Fired from the engine's collect pass each rendered frame (same cadence as
 * the FlipView sync): re-measure when labels changed, re-run the collapse
 * whenever the bar's assigned width changed. */
void flux_breadcrumb_bar_sync(XentContext *ctx, XentNodeId bar, struct FluxNodeData *nd) {
	( void ) bar;
	FluxBreadcrumbBarData *d    = ( FluxBreadcrumbBarData * ) nd->component_data;
	XentRect               rect = {0};
	if (!d || !xent_get_layout_rect(ctx, d->root, &rect)) return;

	if (d->widths_dirty) bc_measure(d);
	if (!d->arrange_dirty && rect.w == d->last_avail) return;
	d->last_avail = rect.w;
	bc_arrange(d, rect.w);
	bc_repaint(d); /* newly shown/hidden crumbs land in the next layout pass */
}

/* -------------------------------------------------------------------------
 * Interaction
 * ---------------------------------------------------------------------- */

static void bc_flyout_click(void *ctx) {
	FluxBreadcrumbFlyoutSlot *slot = ( FluxBreadcrumbFlyoutSlot * ) ctx;
	FluxBreadcrumbBarData    *d    = slot->bar;
	if (d->on_item_clicked) d->on_item_clicked(d->on_item_clicked_ctx, slot->index);
}

/* Snapshot the hidden list REVERSED — the crumb closest to the visible ones
 * first (top), the root crumb last (BreadcrumbBar::OpenFlyout). Rebuilt from
 * the current hidden list on every open, never cached. */
static void bc_open_flyout(FluxBreadcrumbItem *it, FluxMenuInputKind input) {
	FluxBreadcrumbBarData *d = it->bar;
	if (!d->window || !d->ellipsis_rendered) return;
	if (!d->flyout) {
		d->flyout = flux_menu_flyout_create(d->window);
		if (!d->flyout) return;
		flux_menu_flyout_set_theme_manager(d->flyout, d->theme);
		flux_menu_flyout_set_text_renderer(d->flyout, d->text);
	}
	flux_menu_flyout_clear(d->flyout);
	int hidden = d->first_rendered - 1; /* hidden data indices: 0 .. hidden-1 */
	for (int row = 0; row < hidden; row++) {
		int index            = hidden - 1 - row;
		d->fly_slots [index] = (FluxBreadcrumbFlyoutSlot) {d, index};
		flux_menu_flyout_add_item(
		  d->flyout, &(FluxMenuItemDef) {
				  .type         = FLUX_MENU_ITEM_NORMAL,
				  .label        = d->labels [index],
				  .enabled      = true,
				  .on_click     = bc_flyout_click,
				  .on_click_ctx = &d->fly_slots [index],
				}
		);
	}
	FluxRect anchor = flux_binding_screen_anchor(d->window, d->ctx, d->store, it->node);
	flux_menu_flyout_show_for_input(d->flyout, anchor, FLUX_PLACEMENT_BOTTOM, input);
}

static void bc_invoke(FluxBreadcrumbItem *it, FluxMenuInputKind input) {
	FluxBreadcrumbBarData *d = it->bar;
	if (d->disabled) return;
	if (it->elem == 0) {
		bc_open_flyout(it, input);
		return;
	}
	if (it->elem == d->count) return; /* current crumb: plain text, never raises ItemClicked */
	if (d->on_item_clicked) d->on_item_clicked(d->on_item_clicked_ctx, it->elem - 1);
}

/* WinUI crumbs fire on Click (release). The chevron cell is a static separator
 * outside the inner button, so a release over it is a miss. */
static void bc_item_click(void *ctx) {
	FluxBreadcrumbItem    *it = ( FluxBreadcrumbItem * ) ctx;
	FluxBreadcrumbBarData *d  = it->bar;
	FluxNodeData          *nd = flux_node_store_get(d->store, it->node);
	float content_w = d->widths [it->elem] - (it->elem == d->count ? 0.0f : d->chevron_cell_w);
	if (nd && nd->hover_local_x >= 0.0f && nd->hover_local_x > content_w) return;
	bc_invoke(it, FLUX_MENU_INPUT_MOUSE);
}

/* Left target: from the first visible crumb, jump to the ellipsis when
 * rendered; from element 1 / the ellipsis, deliberately no wrap (-1) — focus
 * stays, per WinUI. */
static int bc_key_left_target(FluxBreadcrumbBarData const *d, int elem) {
	if (elem == 0) return -1;
	if (d->ellipsis_rendered && elem == d->first_rendered) return 0;
	return elem > 1 ? elem - 1 : -1;
}

/* Right = next (from the ellipsis: jump straight to the first rendered crumb);
 * Left = previous. Home/End/PageUp/PageDown are NOT handled, per WinUI. */
static bool bc_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxBreadcrumbItem    *it = ( FluxBreadcrumbItem * ) ctx;
	FluxBreadcrumbBarData *d  = it->bar;
	switch (vk) {
	case VK_RETURN :
	case VK_SPACE  : bc_invoke(it, FLUX_MENU_INPUT_KEYBOARD); return true;
	case VK_RIGHT  : {
		int next = it->elem == 0 ? d->first_rendered : it->elem + 1;
		if (next <= d->count && d->input) flux_input_set_focus(d->input, d->nodes [next]);
		return true;
	}
	case VK_LEFT : {
		int prev = bc_key_left_target(d, it->elem);
		if (prev >= 0 && d->input) flux_input_set_focus(d->input, d->nodes [prev]);
		return true;
	}
	default : return false;
	}
}

/* -------------------------------------------------------------------------
 * Item nodes / items assignment
 * ---------------------------------------------------------------------- */

static XentNodeId bc_make_item(FluxBreadcrumbBarData *d, int elem) {
	XentNodeId node = flux_factory_create_node(d->ctx, d->store, d->root, FLUX_CONTROL_BREADCRUMB_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData       *ind = flux_node_store_get(d->store, node);
	FluxBreadcrumbItem *it  = ind ? ( FluxBreadcrumbItem * ) calloc(1, sizeof(*it)) : NULL;
	if (!ind || !it) {
		free(it);
		return flux_factory_fail_node(d->ctx, d->store, node);
	}
	it->bar                     = d;
	it->node                    = node;
	it->elem                    = elem;

	ind->component_data         = it;
	ind->destroy_component_data = free;
	ind->behavior.on_click      = bc_item_click;
	ind->behavior.on_click_ctx  = it;
	ind->behavior.on_key        = bc_item_key;
	ind->behavior.on_key_ctx    = it;

	xent_set_semantic_role(d->ctx, node, XENT_SEMANTIC_BUTTON);
	xent_set_semantic_label(d->ctx, node, elem > 0 ? d->labels [elem - 1] : "More");
	xent_set_semantic_enabled(d->ctx, node, !d->disabled);
	/* Everything starts hidden (0x0, non-focusable); the first sync arranges. */
	xent_set_absolute_position(d->ctx, node, (XentPoint) {0.0f, 0.0f});
	xent_set_size(d->ctx, node, (XentSize) {0.0f, 0.0f});
	xent_set_focusable(d->ctx, node, false);
	return node;
}

static void bc_clear_items(FluxBreadcrumbBarData *d) {
	int elems = d->nodes ? d->count + 1 : 0;
	for (int e = 0; e < elems; e++)
		if (d->nodes [e] != XENT_NODE_INVALID) flux_subtree_destroy(d->store, d->nodes [e]);
	int labels = d->labels ? d->count : 0;
	for (int i = 0; i < labels; i++) flux_str_free(d->labels [i]);
	free(( void * ) d->labels);
	free(d->nodes);
	free(d->widths);
	free(d->fly_slots);
	d->labels    = NULL;
	d->nodes     = NULL;
	d->widths    = NULL;
	d->fly_slots = NULL;
	d->count     = 0;
}

static bool bc_assign_items(FluxBreadcrumbBarData *d, char const *const *items, int count) {
	if (count < 0 || !items) count = 0;
	bc_clear_items(d);

	int elems    = count + 1; /* element 0 is the ellipsis */
	d->labels    = count ? ( char const ** ) calloc(( size_t ) count, sizeof(*d->labels)) : NULL;
	d->fly_slots = count ? ( FluxBreadcrumbFlyoutSlot * ) calloc(( size_t ) count, sizeof(*d->fly_slots)) : NULL;
	d->nodes     = ( XentNodeId * ) calloc(( size_t ) elems, sizeof(*d->nodes));
	d->widths    = ( float * ) calloc(( size_t ) elems, sizeof(*d->widths));
	if (!d->nodes || !d->widths || (count && (!d->labels || !d->fly_slots))) {
		bc_clear_items(d);
		return false;
	}
	d->count = count;
	for (int i = 0; i < count; i++) d->labels [i] = flux_str_dup(items [i]);
	for (int e = 0; e < elems; e++) d->nodes [e] = bc_make_item(d, e);

	d->ellipsis_rendered = false;
	d->first_rendered    = 1;
	d->widths_dirty      = true;
	d->arrange_dirty     = true;
	d->last_avail        = -1.0f;
	return true;
}

static void bc_destroy(void *component_data) {
	FluxBreadcrumbBarData *d = ( FluxBreadcrumbBarData * ) component_data;
	if (!d) return;
	/* Item nodes are torn down with the subtree; only owned resources here. */
	if (d->flyout) flux_menu_flyout_destroy(d->flyout);
	if (d->labels)
		for (int i = 0; i < d->count; i++) flux_str_free(d->labels [i]);
	free(( void * ) d->labels);
	free(d->nodes);
	free(d->widths);
	free(d->fly_slots);
	free(d);
}

/* -------------------------------------------------------------------------
 * Create / store-facing setters
 * ---------------------------------------------------------------------- */

XentNodeId flux_create_breadcrumb_bar(FluxBreadcrumbBarCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_BREADCRUMB_BAR);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData          *nd = flux_node_store_get(info->store, root);
	FluxBreadcrumbBarData *d  = nd ? ( FluxBreadcrumbBarData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	d->ctx                     = info->ctx;
	d->store                   = info->store;
	d->window                  = info->window;
	d->text                    = info->text;
	d->theme                   = info->theme;
	d->input                   = info->input;
	d->root                    = root;
	d->disabled                = info->disabled;
	d->first_rendered          = 1;
	d->on_item_clicked         = info->on_item_clicked;
	d->on_item_clicked_ctx     = info->userdata;

	nd->component_data         = d;
	nd->destroy_component_data = bc_destroy;

	/* The bar itself is chromeless (template is just the repeater): an ABSOLUTE
	 * row of item nodes placed by the collapse; height is the shared item
	 * height, width is the natural crumb sum (set by bc_measure). */
	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_ABSOLUTE);
	xent_set_size(info->ctx, root, (XentSize) {0.0f, FLUX_BREADCRUMB_ITEM_H});
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);

	bc_assign_items(d, info->items, info->item_count);
	return root;
}

void flux_breadcrumb_bar_set_items(FluxNodeStore *store, XentNodeId bar, char const *const *items, int count) {
	FluxBreadcrumbBarData *d = bc_data(store, bar);
	if (!d) return;
	if (d->flyout) flux_menu_flyout_dismiss(d->flyout); /* hidden list is stale */
	bc_assign_items(d, items, count);
	bc_repaint(d);
}

void flux_breadcrumb_bar_set_disabled(FluxNodeStore *store, XentNodeId bar, bool disabled) {
	FluxBreadcrumbBarData *d = bc_data(store, bar);
	if (!d || d->disabled == disabled) return;
	d->disabled = disabled;
	for (int e = 0; e <= d->count; e++)
		if (d->nodes [e] != XENT_NODE_INVALID) xent_set_semantic_enabled(d->ctx, d->nodes [e], !disabled);
	d->arrange_dirty = true; /* refresh per-element focusability */
	bc_repaint(d);
}
