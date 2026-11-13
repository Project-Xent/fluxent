#include "controls/behavior/flux_flip_internal.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_host.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdlib.h>
#include <windows.h>

#define PIP_MAIN  24.0f
#define PIP_CROSS 12.0f
#define PIP_NAV   24.0f

static FluxPipsPagerData *pips_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_PIPS_PAGER || !nd->component_data) return NULL;
	return ( FluxPipsPagerData * ) nd->component_data;
}

static int pips_visible(FluxPipsPagerData const *pd) {
	int v = pd->max_visible < pd->count ? pd->max_visible : pd->count;
	return v > 0 ? v : 0;
}

static bool  pips_nav_shown(FluxPipsPagerData const *pd) { return pd->nav_vis != XTK_PIPS_NAV_COLLAPSED; }

static float pips_strip_offset(FluxPipsPagerData const *pd) { return pips_nav_shown(pd) ? PIP_NAV : 0.0f; }

static void  pips_center_window(FluxPipsPagerData *pd) {
	int visible = pips_visible(pd);
	if (visible <= 0) {
		pd->window_start = 0;
		return;
	}
	int start = pd->selected - visible / 2;
	if (start > pd->count - visible) start = pd->count - visible;
	if (start < 0) start = 0;
	pd->window_start = start;
}

static void pips_apply_size(FluxPipsPagerData *pd) {
	float main  = ( float ) pips_visible(pd) * PIP_CROSS + (pips_nav_shown(pd) ? 2.0f * PIP_NAV : 0.0f);
	float cross = PIP_MAIN;
	xent_setsize(pd->ctx, pd->node, pd->vertical ? (XentSize) {cross, main} : (XentSize) {main, cross});
}

static int pips_hit(FluxPipsPagerData const *pd, float x, float y) {
	float along = pd->vertical ? y : x;
	float cross = pd->vertical ? x : y;
	if (cross < 0.0f || cross > PIP_MAIN) return -1;

	if (pips_nav_shown(pd)) {
		if (along < PIP_NAV) return -2;
		float end = pips_strip_offset(pd) + ( float ) pips_visible(pd) * PIP_CROSS;
		if (along >= end && along < end + PIP_NAV) return -3;
	}

	float in_strip = along - pips_strip_offset(pd);
	int   slot     = ( int ) floorf(in_strip / PIP_CROSS);
	if (in_strip < 0.0f || slot < 0 || slot >= pips_visible(pd)) return -1;
	return pd->window_start + slot;
}

static void pips_select(FluxPipsPagerData *pd, int index) {
	if (index < 0) index = 0;
	if (index > pd->count - 1) index = pd->count - 1;
	if (index < 0 || index == pd->selected) return;
	pd->selected = index;
	pips_center_window(pd);
	XtkHost *host = flux_store_host(pd->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_SELECT,
		  .target         = ( XtkCtrl ) pd->node,
		  .source         = ( XtkCtrl ) pd->node,
		  .current_target = ( XtkCtrl ) pd->node,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
		};
		flux_host_route(host, &event);
	}
	if (pd->on_select) pd->on_select(pd->on_select_ctx, index);
}

static void pips_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxPipsPagerData *pd  = ( FluxPipsPagerData * ) ctx;
	int                hit = pips_hit(pd, x, y);
	pd->pressed_pip        = hit >= 0 ? hit : -1;
	pd->pressed_nav        = hit == -2 ? 1 : hit == -3 ? 2 : 0;
}

static void pips_click(void *ctx) {
	FluxPipsPagerData *pd          = ( FluxPipsPagerData * ) ctx;
	FluxNodeData      *nd          = flux_store_find(pd->store, pd->node);
	int                hit         = nd ? pips_hit(pd, nd->hover_local_x, nd->hover_local_y) : -1;
	int                pressed_pip = pd->pressed_pip;
	int                pressed_nav = pd->pressed_nav;
	int                selected    = pd->selected;
	pd->pressed_pip                = -1;
	pd->pressed_nav                = 0;

	if (hit >= 0 && hit == pressed_pip) {
		pips_select(pd, hit);
		return;
	}
	if (hit == -2 && pressed_nav == 1) {
		pips_select(pd, selected - 1);
		return;
	}
	if (hit == -3 && pressed_nav == 2) pips_select(pd, selected + 1);
}

static void pips_cancel(void *ctx) {
	FluxPipsPagerData *pd = ( FluxPipsPagerData * ) ctx;
	pd->pressed_pip       = -1;
	pd->pressed_nav       = 0;
}

static bool pips_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxPipsPagerData *pd   = ( FluxPipsPagerData * ) ctx;
	FluxKey            prev = pd->vertical ? FLUX_KEY_UP : FLUX_KEY_LEFT;
	FluxKey            next = pd->vertical ? FLUX_KEY_DOWN : FLUX_KEY_RIGHT;
	if (key == prev) {
		pips_select(pd, pd->selected - 1);
		return true;
	}
	if (key == next) {
		pips_select(pd, pd->selected + 1);
		return true;
	}
	if (key == FLUX_KEY_HOME) {
		pips_select(pd, 0);
		return true;
	}
	if (key == FLUX_KEY_END) {
		pips_select(pd, pd->count - 1);
		return true;
	}
	return false;
}

XentNodeId flux_pips_init(FluxPipsPagerDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PIPS_PAGER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_store_find(info->store, node);
	FluxPipsPagerData *pd = nd ? ( FluxPipsPagerData * ) calloc(1, sizeof(FluxPipsPagerData)) : NULL;
	if (!nd || !pd) {
		free(pd);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	pd->ctx                          = info->ctx;
	pd->store                        = info->store;
	pd->node                         = node;
	pd->vertical                     = info->vertical;
	pd->max_visible                  = 5;
	pd->pressed_pip                  = -1;
	pd->on_select                    = info->on_select;
	pd->on_select_ctx                = info->userdata;

	nd->component_data               = pd;
	nd->destroy_component_data       = free;
	nd->behavior.on_pointer_down     = pips_pointer_down;
	nd->behavior.on_pointer_down_ctx = pd;
	nd->behavior.on_click            = pips_click;
	nd->behavior.on_click_ctx        = pd;
	nd->behavior.on_cancel           = pips_cancel;
	nd->behavior.on_cancel_ctx       = pd;
	nd->behavior.on_key              = pips_key;
	nd->behavior.on_key_ctx          = pd;

	xent_set_focusable(info->ctx, node, true);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	pips_apply_size(pd);
	return node;
}

void flux_pips_configure(FluxStore *store, XentNodeId pips, int count, int selected, int max_visible, int nav_vis) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (!pd) return;
	if (count < 0) count = 0;
	if (max_visible <= 0) max_visible = 5;
	if (selected > count - 1) selected = count - 1;
	if (selected < 0) selected = count > 0 ? 0 : -1;

	pd->count       = count;
	pd->selected    = selected;
	pd->max_visible = max_visible;
	pd->nav_vis     = ( uint8_t ) nav_vis;
	pips_center_window(pd);
	pips_apply_size(pd);
}

void flux_pips_set_vertical(FluxStore *store, XentNodeId pips, bool vertical) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (!pd || pd->vertical == vertical) return;
	pd->vertical = vertical;
	pips_apply_size(pd);
}

void flux_pips_set_count(FluxStore *store, XentNodeId pips, int count) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (pd) flux_pips_configure(store, pips, count, pd->selected, pd->max_visible, pd->nav_vis);
}

void flux_pips_set_selected(FluxStore *store, XentNodeId pips, int selected) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (pd) flux_pips_configure(store, pips, pd->count, selected, pd->max_visible, pd->nav_vis);
}

void flux_pips_set_max_visible(FluxStore *store, XentNodeId pips, int max_visible) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (pd) flux_pips_configure(store, pips, pd->count, pd->selected, max_visible, pd->nav_vis);
}

void flux_pips_set_nav_visibility(FluxStore *store, XentNodeId pips, int nav_vis) {
	FluxPipsPagerData *pd = pips_data(store, pips);
	if (pd) flux_pips_configure(store, pips, pd->count, pd->selected, pd->max_visible, nav_vis);
}
