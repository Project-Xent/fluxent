#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_container.h"
#include "store/flux_node_data.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"
#include "render/flux_icon.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/data/flux_scroll_data.h"
#include "host/flux_prop_apply.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_host.h"
#include <xtk/xtk_catalog.h>
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <windows.h>

#define SB_ANIM_MS 167.0f

static FluxSelectorBarData *sb_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
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
		bool sel                   = (i == idx);
		b->item_data [i]->selected = sel;
		if (!sel) b->item_data [i]->pill_t = 0.0f;
	}
	b->selected = idx;
	if (idx >= 0) {
		b->item_data [idx]->pill_t = 0.0f;
		b->anim_item               = idx;
		b->anim_start              = flux_anim_now();
		flux_anim_register(b, sb_anim_step);
	}
	if (!notify || idx < 0) return;
	if (b->on_select) b->on_select(b->on_select_ctx, idx);
	XtkHost *host = flux_store_host(b->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = XTK_EVENT_SELECT,
	  .target         = ( XtkCtrl ) b->root,
	  .source         = ( XtkCtrl ) b->root,
	  .current_target = ( XtkCtrl ) b->root,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = idx},
	};
	flux_host_route(host, &event);
}

static void sb_item_click(void *ctx) {
	FluxSelectorBarItemData *it = ( FluxSelectorBarItemData * ) ctx;
	if (!it->disabled) sb_select(it->bar, it->index, true);
}

static void sb_reveal(FluxSelectorBarData *b, int index) {
	FluxNodeData   *nd       = flux_store_find(b->store, b->viewport);
	FluxScrollData *scroll   = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	XentRect        viewport = {0};
	XentRect        item     = {0};
	if (!scroll
	    || !xent_layout_rect(b->ctx, b->viewport, &viewport)
	    || !xent_layout_rect(b->ctx, b->items [index], &item))
		return;
	float left  = item.x - viewport.x;
	float right = left + item.w;
	float next  = scroll->scroll_x;
	if (left < next) next = left;
	else if (right > next + viewport.w) next = right - viewport.w;
	if (next != scroll->scroll_x) flux_prop_scroll_offset(b->store, b->viewport, next, 0.0f);
}

static void sb_focus(FluxSelectorBarData *b, int index) {
	if (index < 0 || index >= b->count) return;
	sb_reveal(b, index);
	if (b->input) flux_input_setfocus(b->input, b->items [index], false);
}

static bool sb_item_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxSelectorBarItemData *it = ( FluxSelectorBarItemData * ) ctx;
	FluxSelectorBarData     *b  = it->bar;
	switch (key) {
	case FLUX_KEY_LEFT  : sb_focus(b, it->index - 1); return true;
	case FLUX_KEY_RIGHT : sb_focus(b, it->index + 1); return true;
	case FLUX_KEY_HOME  : sb_focus(b, 0); return true;
	case FLUX_KEY_END   : sb_focus(b, b->count - 1); return true;
	case FLUX_KEY_ENTER :
	case FLUX_KEY_SPACE : sb_select(b, it->index, true); return true;
	default             : return false;
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

static XentNodeId sb_viewport_create(FluxSelectorBarDesc const *info, XentNodeId root) {
	XentNodeId viewport
	  = flux_scroll_init(&(FluxContainerDesc) {.ctx = info->ctx, .store = info->store, .parent = root});
	FluxNodeData   *nd     = flux_store_find(info->store, viewport);
	FluxScrollData *scroll = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!scroll) return XENT_NODE_INVALID;
	scroll->h_vis = FLUX_SCROLL_NEVER;
	scroll->v_vis = FLUX_SCROLL_NEVER;
	xent_setflexdir(info->ctx, viewport, XENT_FLEX_ROW);
	xent_setitems(info->ctx, viewport, XENT_FLEX_ALIGN_STRETCH);
	xent_setfit(info->ctx, viewport, false, true);
	xent_setp(info->ctx, viewport, (XentInsets) {0.0f, 4.0f, 0.0f, 4.0f});
	return viewport;
}

static bool
sb_item_create(FluxSelectorBarData *b, int index, XentNodeId *out_node, FluxSelectorBarItemData **out_data) {
	XentNodeId node = flux_factory_create_node(b->ctx, b->store, b->viewport, FLUX_CONTROL_SELECTOR_BAR_ITEM);
	if (node == XENT_NODE_INVALID) return false;
	FluxNodeData            *nd = flux_store_find(b->store, node);
	FluxSelectorBarItemData *it = nd ? ( FluxSelectorBarItemData * ) calloc(1, sizeof(*it)) : NULL;
	if (!it) {
		flux_tree_drop(b->store, node);
		return false;
	}
	it->bar                    = b;
	it->index                  = index;
	nd->component_data         = it;
	nd->destroy_component_data = sb_item_destroy;
	nd->behavior.on_click      = sb_item_click;
	nd->behavior.on_click_ctx  = it;
	nd->behavior.on_key        = sb_item_key;
	nd->behavior.on_key_ctx    = it;
	xent_set_focusable(b->ctx, node, true);
	xent_sem_setrole(b->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_setsize(b->ctx, node, (XentSize) {NAN, NAN});
	xent_setshrink(b->ctx, node, 0.0f);
	*out_node = node;
	*out_data = it;
	return true;
}

static void sb_item_set(
  FluxSelectorBarData *b, XentNodeId node, FluxSelectorBarItemData *it, char const *text, uint32_t icon_glyph,
  bool disabled
) {
	flux_str_replace(&it->text, text);
	it->icon_glyph = icon_glyph;
	it->disabled   = disabled;
	float lead     = 12.0f + (icon_glyph ? 24.0f : 0.0f);
	xent_settext(b->ctx, node, text ? text : "");
	xent_setlinebreak(b->ctx, node, XENT_LINEBREAK_NOWRAP);
	xent_setfontsize(b->ctx, node, 14.0f);
	xent_setp(b->ctx, node, (XentInsets) {lead, 10.0f, 12.0f, 10.0f});
	xent_sem_setenabled(b->ctx, node, !disabled);
}

static void sb_items_dispose(FluxSelectorBarData *b, XentNodeId *nodes, FluxSelectorBarItemData **data, int count) {
	for (int i = 0; i < count; i++)
		if (nodes [i] != XENT_NODE_INVALID) flux_tree_drop(b->store, nodes [i]);
	free(nodes);
	free(data);
}

static bool sb_items_create(
  FluxSelectorBarData *b, XtkSelectorItem const *desc, int count, XentNodeId **out_nodes,
  FluxSelectorBarItemData ***out_data
) {
	*out_nodes = NULL;
	*out_data  = NULL;
	if (count == 0) return true;
	XentNodeId               *nodes = ( XentNodeId * ) calloc(( size_t ) count, sizeof(*nodes));
	FluxSelectorBarItemData **data  = ( FluxSelectorBarItemData ** ) calloc(( size_t ) count, sizeof(*data));
	if (!nodes || !data) {
		free(nodes);
		free(data);
		return false;
	}
	for (int i = 0; i < count; i++) {
		if (!sb_item_create(b, i, &nodes [i], &data [i])) {
			sb_items_dispose(b, nodes, data, count);
			return false;
		}
		wchar_t const *glyph = flux_icon_lookup(desc ? desc [i].icon : NULL);
		sb_item_set(
		  b, nodes [i], data [i], desc && desc [i].text ? desc [i].text : "", glyph ? ( uint32_t ) glyph [0] : 0u, false
		);
	}
	*out_nodes = nodes;
	*out_data  = data;
	return true;
}

XentNodeId flux_selector_init(FluxSelectorBarDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	int        count = info->item_count < 0 ? 0 : info->item_count;

	XentNodeId root  = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SELECTOR_BAR);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	XentNodeId viewport = sb_viewport_create(info, root);
	if (viewport == XENT_NODE_INVALID) return flux_factory_fail_node(info->ctx, info->store, root);

	FluxNodeData        *nd = flux_store_find(info->store, root);
	FluxSelectorBarData *b  = nd ? ( FluxSelectorBarData * ) calloc(1, sizeof(*b)) : NULL;
	if (!nd || !b) {
		free(b);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	b->ctx                     = info->ctx;
	b->store                   = info->store;
	b->input                   = info->input;
	b->window                  = info->window;
	b->root                    = root;
	b->viewport                = viewport;
	b->count                   = count;
	b->selected                = -1;
	b->anim_item               = -1;
	b->on_select               = info->on_select;
	b->on_select_ctx           = info->userdata;

	nd->component_data         = b;
	nd->destroy_component_data = sb_data_destroy;
	if (!sb_items_create(b, NULL, count, &b->items, &b->item_data))
		return flux_factory_fail_node(info->ctx, info->store, root);

	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);

	if (info->selected >= 0 && info->selected < count) {
		b->selected                             = info->selected;
		b->item_data [info->selected]->selected = true;
		b->item_data [info->selected]->pill_t   = 1.0f;
	}
	return root;
}

void flux_selector_set_item(
  FluxStore *store, XentNodeId bar, int i, char const *text, uint32_t icon_glyph, bool disabled
) {
	FluxSelectorBarData *b = sb_data(store, bar);
	if (!b || i < 0 || i >= b->count) return;
	sb_item_set(b, b->items [i], b->item_data [i], text, icon_glyph, disabled);
}

void flux_selector_set_selected(FluxStore *store, XentNodeId bar, int index) {
	FluxSelectorBarData *b = sb_data(store, bar);
	if (b) sb_select(b, index, false);
}

void flux_selector_set_items(FluxStore *store, XentNodeId bar, XtkSelectorItem const *items, int count, int selected) {
	FluxSelectorBarData *b = sb_data(store, bar);
	if (!b || count < 0) return;

	XentNodeId               *nodes = NULL;
	FluxSelectorBarItemData **data  = NULL;
	if (!sb_items_create(b, items, count, &nodes, &data)) return;
	sb_items_dispose(b, b->items, b->item_data, b->count);
	b->items     = nodes;
	b->item_data = data;
	b->count     = count;
	b->selected  = -1;
	b->anim_item = -1;
	if (selected >= 0 && selected < count) sb_select(b, selected, false);
}
