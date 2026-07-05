/**
 * @file flux_selector_bar.c
 * @brief SelectorBar behavior (SelectorBar.cpp): a horizontal single-select
 * strip of self-drawing items. Selecting an item grows its accent underline
 * pill (4→16 DIP, fade in over 167 ms, cubic-bezier(0,0,0,1)); the previous
 * item's pill snaps hidden. Arrows move focus only; Enter/Space or click
 * select. One tab stop; GotFocus with no selection picks item 0.
 */
#include "controls/factory/flux_factory.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <windows.h>

#define SB_ANIM_MS 167.0f

static FluxSelectorBarData *sb_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_SELECTOR_BAR || !nd->component_data) return NULL;
	return ( FluxSelectorBarData * ) nd->component_data;
}

static bool sb_anim_step(void *ctx, unsigned long now) {
	FluxSelectorBarData *b = ( FluxSelectorBarData * ) ctx;
	if (b->anim_item < 0 || b->anim_item >= b->count) {
		b->anim_start = 0;
		return false;
	}
	float t = ( float ) (( DWORD ) now - b->anim_start) / SB_ANIM_MS;
	if (t >= 1.0f) t = 1.0f;
	b->item_data [b->anim_item]->pill_t = flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f);
	if (b->window) {
		HWND h = flux_window_hwnd(b->window);
		if (h) InvalidateRect(h, NULL, FALSE);
	}
	if (t >= 1.0f) {
		b->anim_start = 0;
		b->anim_item  = -1;
		return false;
	}
	return true;
}

static void sb_select(FluxSelectorBarData *b, int idx, bool notify) {
	if (idx < -1 || idx >= b->count || idx == b->selected) return;
	for (int i = 0; i < b->count; i++) {
		bool sel                     = (i == idx);
		b->item_data [i]->selected   = sel;
		if (!sel) b->item_data [i]->pill_t = 0.0f; /* deselect snaps hidden */
	}
	b->selected = idx;
	if (idx >= 0) {
		b->item_data [idx]->pill_t = 0.0f;
		b->anim_item               = idx;
		b->anim_start              = GetTickCount();
		flux_anim_register(b, sb_anim_step);
	}
	if (notify && idx >= 0 && b->on_select) b->on_select(b->on_select_ctx, idx);
}

static void sb_item_click(void *ctx) {
	FluxSelectorBarItemData *it = ( FluxSelectorBarItemData * ) ctx;
	if (!it->disabled) sb_select(it->bar, it->index, true);
}

static bool sb_item_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxSelectorBarItemData *it = ( FluxSelectorBarItemData * ) ctx;
	FluxSelectorBarData     *b  = it->bar;
	switch (vk) {
	case VK_LEFT :
		if (it->index > 0 && b->input) flux_input_set_focus(b->input, b->items [it->index - 1]);
		return true;
	case VK_RIGHT :
		if (it->index < b->count - 1 && b->input) flux_input_set_focus(b->input, b->items [it->index + 1]);
		return true;
	case VK_HOME :
		if (b->count > 0 && b->input) flux_input_set_focus(b->input, b->items [0]);
		return true;
	case VK_END :
		if (b->count > 0 && b->input) flux_input_set_focus(b->input, b->items [b->count - 1]);
		return true;
	case VK_RETURN :
	case VK_SPACE : sb_select(b, it->index, true); return true;
	default : return false;
	}
}

static void sb_item_destroy(void *component_data) {
	FluxSelectorBarItemData *it = ( FluxSelectorBarItemData * ) component_data;
	if (!it) return;
	flux_str_free(it->text);
	free(it);
}

static void sb_data_destroy(void *component_data) {
	FluxSelectorBarData *b = ( FluxSelectorBarData * ) component_data;
	if (!b) return;
	free(b->items);
	free(b->item_data);
	free(b);
}

XentNodeId flux_create_selector_bar(FluxSelectorBarCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	int count = info->item_count < 0 ? 0 : info->item_count;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SELECTOR_BAR);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData        *nd    = flux_node_store_get(info->store, root);
	FluxSelectorBarData *b     = nd ? ( FluxSelectorBarData * ) calloc(1, sizeof(*b)) : NULL;
	XentNodeId          *items = count ? ( XentNodeId * ) calloc(( size_t ) count, sizeof(XentNodeId)) : NULL;
	FluxSelectorBarItemData **idata
	  = count ? ( FluxSelectorBarItemData ** ) calloc(( size_t ) count, sizeof(void *)) : NULL;
	if (!nd || !b || (count && (!items || !idata))) {
		free(b); free(items); free(idata);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	b->ctx = info->ctx; b->store = info->store; b->input = info->input; b->window = info->window;
	b->root = root; b->count = count; b->selected = -1; b->anim_item = -1;
	b->items = items; b->item_data = idata;
	b->on_select = info->on_select; b->on_select_ctx = info->userdata;

	nd->component_data         = b;
	nd->destroy_component_data = sb_data_destroy;

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_ROW);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_set_padding(info->ctx, root, (XentInsets) {0.0f, 4.0f, 0.0f, 4.0f});
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);

	for (int i = 0; i < count; i++) {
		XentNodeId       node = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_SELECTOR_BAR_ITEM);
		FluxNodeData    *ind  = flux_node_store_get(info->store, node);
		FluxSelectorBarItemData *it = ind ? ( FluxSelectorBarItemData * ) calloc(1, sizeof(*it)) : NULL;
		if (!it) continue;
		it->bar   = b;
		it->index = i;
		b->items [i]     = node;
		b->item_data [i] = it;

		ind->component_data         = it;
		ind->destroy_component_data = sb_item_destroy;
		ind->behavior.on_click      = sb_item_click;
		ind->behavior.on_click_ctx  = it;
		ind->behavior.on_key        = sb_item_key;
		ind->behavior.on_key_ctx    = it;
		xent_set_focusable(info->ctx, node, true);
		xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
		xent_set_size(info->ctx, node, (XentSize) {NAN, NAN});
	}

	if (info->selected >= 0 && info->selected < count) {
		b->selected                            = info->selected;
		b->item_data [info->selected]->selected = true;
		b->item_data [info->selected]->pill_t   = 1.0f; /* initial selection: shown, no animation */
	}
	return root;
}

/* Set an item's content; icon_glyph 0 = none. Sizes the item from its text +
 * icon reservation (12,10,12,10 padding + icon column). */
void flux_selector_bar_set_item(
  FluxNodeStore *store, XentNodeId bar, int i, char const *text, uint32_t icon_glyph, bool disabled
) {
	FluxSelectorBarData *b = sb_data(store, bar);
	if (!b || i < 0 || i >= b->count) return;
	FluxSelectorBarItemData *it = b->item_data [i];
	flux_str_replace(&it->text, text);
	it->icon_glyph = icon_glyph;
	it->disabled   = disabled;

	XentContext *ctx  = b->ctx;
	float        lead = 12.0f + (icon_glyph ? (16.0f * 0.8f + 8.0f) : 0.0f);
	xent_set_text(ctx, b->items [i], text ? text : "");
	/* Single-line label (SelectorBarItem never wraps); without this the item
	 * inherits xent's CHAR_WRAP default and a tight measure pass stacks the
	 * label one character per line, ballooning the whole bar's height. */
	xent_set_text_line_break_policy(ctx, b->items [i], XENT_LINE_BREAK_NO_WRAP);
	xent_set_font_size(ctx, b->items [i], 14.0f);
	xent_set_padding(ctx, b->items [i], (XentInsets) {lead, 10.0f, 12.0f, 10.0f});
	xent_set_semantic_enabled(ctx, b->items [i], !disabled);
}

void flux_selector_bar_set_selected(FluxNodeStore *store, XentNodeId bar, int index) {
	FluxSelectorBarData *b = sb_data(store, bar);
	if (b) sb_select(b, index, false);
}
