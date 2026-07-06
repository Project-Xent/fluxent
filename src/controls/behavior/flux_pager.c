/**
 * @file flux_pager.c
 * @brief PagerControl behavior: a single leaf hosting nav buttons (First /
 * Previous / Next / Last) around a NumberPanel (numbered buttons with
 * ellipses) or a NumberText label. Layout is shared with the renderer via
 * flux_pager_layout so clicks resolve against the drawn geometry; selection
 * recomputes the panel window and re-measures the control.
 */
#include "controls/factory/flux_factory.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"

#include <stdlib.h>
#include <windows.h>

static FluxPagerData *pager_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_PAGER || !nd->component_data) return NULL;
	return ( FluxPagerData * ) nd->component_data;
}

/* Build the element list for the current model (shared with the renderer). */
static int pager_elems(FluxPagerData const *d, FluxPagerElem *elems, float *tw) {
	bool    at_first = d->selected <= 0;
	bool    at_last  = d->selected >= d->count - 1;
	int     fs       = flux_pager_nav_state(d->first_vis, at_first);
	int     ps       = flux_pager_nav_state(d->prev_vis, at_first);
	int     ns       = flux_pager_nav_state(d->next_vis, at_last);
	int     ls       = flux_pager_nav_state(d->last_vis, at_last);
	int16_t cells [FLUX_PAGER_MAX_CELLS];
	int     cc = flux_pager_build_cells(d->count, d->selected, cells);
	return flux_pager_layout(d->display_mode, fs, ps, ns, ls, cells, cc, elems, tw);
}

static void pager_apply_size(FluxPagerData *d) {
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	float         tw = 0.0f;
	pager_elems(d, elems, &tw);
	xent_set_size(d->ctx, d->node, (XentSize) {tw, FLUX_PAGER_HEIGHT});
}

/* Element index under a node-local point, or -1. */
static int pager_hit(FluxPagerData const *d, float x, float y) {
	if (y < 0.0f || y > FLUX_PAGER_HEIGHT) return -1;
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	float         tw = 0.0f;
	int           n  = pager_elems(d, elems, &tw);
	for (int i = 0; i < n; i++)
		if (x >= elems [i].x && x < elems [i].x + elems [i].w) return i;
	return -1;
}

static void pager_select(FluxPagerData *d, int index) {
	if (index < 0) index = 0;
	if (index > d->count - 1) index = d->count - 1;
	if (index == d->selected) return;
	d->selected = index;
	pager_apply_size(d);
	if (d->on_select) d->on_select(d->on_select_ctx, index);
}

/* Act on an element (nav button or number cell), respecting nav enabled state. */
static void pager_activate(FluxPagerData *d, FluxPagerElem const *e) {
	bool at_first = d->selected <= 0;
	bool at_last  = d->selected >= d->count - 1;
	switch (e->kind) {
	case FLUX_PAGER_EL_FIRST:
		if (flux_pager_nav_state(d->first_vis, at_first) == FLUX_PAGER_NAV_ENABLED) pager_select(d, 0);
		break;
	case FLUX_PAGER_EL_PREV:
		if (flux_pager_nav_state(d->prev_vis, at_first) == FLUX_PAGER_NAV_ENABLED) pager_select(d, d->selected - 1);
		break;
	case FLUX_PAGER_EL_NEXT:
		if (flux_pager_nav_state(d->next_vis, at_last) == FLUX_PAGER_NAV_ENABLED) pager_select(d, d->selected + 1);
		break;
	case FLUX_PAGER_EL_LAST:
		if (flux_pager_nav_state(d->last_vis, at_last) == FLUX_PAGER_NAV_ENABLED) pager_select(d, d->count - 1);
		break;
	case FLUX_PAGER_EL_CELL:
		if (e->page != FLUX_PAGER_ELLIPSIS_PAGE) pager_select(d, e->page - 1);
		break;
	default: break;
	}
}

static void pager_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	d->pressed_elem  = pager_hit(d, x, y);
}

static void pager_click(void *ctx) {
	FluxPagerData *d  = ( FluxPagerData * ) ctx;
	FluxNodeData  *nd = ( FluxNodeData * ) xent_get_userdata(d->ctx, d->node);
	int            hit = nd ? pager_hit(d, nd->hover_local_x, nd->hover_local_y) : -1;
	if (hit >= 0 && hit == d->pressed_elem) {
		FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
		float         tw = 0.0f;
		int           n  = pager_elems(d, elems, &tw);
		if (hit < n) pager_activate(d, &elems [hit]);
	}
	d->pressed_elem = -1;
}

static void pager_cancel(void *ctx) {
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	d->pressed_elem  = -1;
}

static bool pager_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	switch (vk) {
	case VK_LEFT : pager_select(d, d->selected - 1); return true;
	case VK_RIGHT: pager_select(d, d->selected + 1); return true;
	case VK_HOME : pager_select(d, 0); return true;
	case VK_END  : pager_select(d, d->count - 1); return true;
	default      : return false;
	}
}

static void pager_destroy(void *component_data) {
	FluxPagerData *d = ( FluxPagerData * ) component_data;
	if (!d) return;
	flux_str_free(d->prefix);
	flux_str_free(d->suffix);
	free(d);
}

XentNodeId flux_create_pager(FluxPagerCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PAGER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData  *nd = flux_node_store_get(info->store, node);
	FluxPagerData *d  = nd ? ( FluxPagerData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx           = info->ctx;
	d->store         = info->store;
	d->node          = node;
	d->count         = info->count > 0 ? info->count : 1;
	d->selected      = info->selected;
	if (d->selected < 0) d->selected = 0;
	if (d->selected > d->count - 1) d->selected = d->count - 1;
	d->display_mode  = ( uint8_t ) info->display_mode;
	d->first_vis     = ( uint8_t ) info->first_button;
	d->prev_vis      = ( uint8_t ) info->prev_button;
	d->next_vis      = ( uint8_t ) info->next_button;
	d->last_vis      = ( uint8_t ) info->last_button;
	d->prefix        = info->prefix ? flux_str_dup(info->prefix) : NULL;
	d->suffix        = info->suffix ? flux_str_dup(info->suffix) : NULL;
	d->pressed_elem  = -1;
	d->on_select     = info->on_select;
	d->on_select_ctx = info->userdata;

	nd->component_data               = d;
	nd->destroy_component_data       = pager_destroy;
	nd->behavior.on_pointer_down     = pager_pointer_down;
	nd->behavior.on_pointer_down_ctx = d;
	nd->behavior.on_click            = pager_click;
	nd->behavior.on_click_ctx        = d;
	nd->behavior.on_cancel           = pager_cancel;
	nd->behavior.on_cancel_ctx       = d;
	nd->behavior.on_key              = pager_key;
	nd->behavior.on_key_ctx          = d;

	xent_set_focusable(info->ctx, node, true);
	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_CONTAINER);
	pager_apply_size(d);
	return node;
}

void flux_pager_configure(FluxNodeStore *store, XentNodeId id, int count, int selected, int display_mode) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	if (count < 1) count = 1;
	if (selected < 0) selected = 0;
	if (selected > count - 1) selected = count - 1;
	d->count        = count;
	d->selected     = selected;
	d->display_mode = ( uint8_t ) display_mode;
	pager_apply_size(d);
}

void flux_pager_set_selected(FluxNodeStore *store, XentNodeId id, int index) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	if (index < 0) index = 0;
	if (index > d->count - 1) index = d->count - 1;
	d->selected = index;
	pager_apply_size(d);
}

void flux_pager_set_button_visibility(
  FluxNodeStore *store, XentNodeId id, int first, int prev, int next, int last
) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	d->first_vis = ( uint8_t ) first;
	d->prev_vis  = ( uint8_t ) prev;
	d->next_vis  = ( uint8_t ) next;
	d->last_vis  = ( uint8_t ) last;
	pager_apply_size(d);
}
