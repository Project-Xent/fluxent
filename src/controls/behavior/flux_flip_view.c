/**
 * @file flux_flip_view.c
 * @brief FlipView: viewport-sized pages on an ABSOLUTE host that slides
 * along the paging axis. Adjacent selection moves animate (WinUI only
 * animates ±1; longer jumps snap — FlipView_Partial::OnSelectedIndexChanged);
 * wheel flips are gated at 200 ms between distinct scrolls; nav buttons
 * fade 3 s after the last pointer activity.
 */
#include "controls/factory/flux_factory.h"
#include "fluxent/fluxent.h"
#include "runtime/flux_anim_driver.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

#define FLIP_ANIM_MS         350.0f
#define FLIP_WHEEL_GATE_MS   200
/* WinUI 3 FlipView_themeresources: nav buttons are 16x38 (38x16 vertical)
 * acrylic chips with 1px margin, rounded ControlCornerRadius. */
#define FLIP_BTN_W           16.0f
#define FLIP_BTN_H           38.0f
#define FLIP_BTN_MARGIN      1.0f

static FluxFlipViewData *flip_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_FLIP_VIEW || !nd->component_data) return NULL;
	return ( FluxFlipViewData * ) nd->component_data;
}

static int flip_page_count(FluxFlipViewData const *fv) {
	int n = 0;
	for (XentNodeId c = xent_get_first_child(fv->ctx, fv->host); c != XENT_NODE_INVALID;
	  c               = xent_get_next_sibling(fv->ctx, c))
		n++;
	return n;
}

static void flip_repaint(FluxFlipViewData *fv) {
	HWND h = fv->window ? flux_window_hwnd(fv->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

static bool flip_step(void *ctx, unsigned long now) {
	FluxFlipViewData *fv = ( FluxFlipViewData * ) ctx;
	if (!fv->anim) return false;
	float t = ( float ) (now - fv->anim_start) / FLIP_ANIM_MS;
	if (t >= 1.0f) {
		fv->offset = fv->anim_to;
		fv->anim   = false;
	}
	else {
		float e    = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); /* cubic ease-out */
		fv->offset = fv->anim_from + (fv->anim_to - fv->anim_from) * e;
	}
	flip_repaint(fv);
	return fv->anim;
}

static void flip_go(FluxFlipViewData *fv, int index, bool from_user) {
	int count = flip_page_count(fv);
	if (index < 0) index = 0;
	if (index > count - 1) index = count - 1;
	if (index < 0 || index == fv->selected) return;

	bool adjacent = index == fv->selected + 1 || index == fv->selected - 1;
	fv->selected  = index;

	float target = ( float ) index * fv->extent;
	if (adjacent && fv->extent > 0.0f) {
		fv->anim_from  = fv->offset;
		fv->anim_to    = target;
		fv->anim_start = GetTickCount();
		fv->anim       = true;
		flux_anim_register(fv, flip_step);
	}
	else {
		fv->offset = target;
		fv->anim   = false;
	}

	if (from_user && fv->on_select) fv->on_select(fv->on_select_ctx, index);
}

/* -------------------------------------------------------------------------
 * Input
 * ---------------------------------------------------------------------- */

/* Nav button rects in root-local coordinates (horizontal: flush left/right,
 * vertically centered — 20×36; vertical orientation mirrored). */
static FluxRect flip_btn_rect(FluxFlipViewData const *fv, XentRect const *root, bool next) {
	if (!fv->vertical) {
		float y = (root->h - FLIP_BTN_H) * 0.5f;
		return next ? (FluxRect) {root->w - FLIP_BTN_W - FLIP_BTN_MARGIN, y, FLIP_BTN_W, FLIP_BTN_H}
		            : (FluxRect) {FLIP_BTN_MARGIN, y, FLIP_BTN_W, FLIP_BTN_H};
	}
	float x = (root->w - FLIP_BTN_H) * 0.5f;
	return next ? (FluxRect) {x, root->h - FLIP_BTN_W - FLIP_BTN_MARGIN, FLIP_BTN_H, FLIP_BTN_W}
	            : (FluxRect) {x, FLIP_BTN_MARGIN, FLIP_BTN_H, FLIP_BTN_W};
}

static bool flip_point_in(FluxRect const *r, float x, float y) {
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static int flip_hit_button(FluxFlipViewData const *fv, float x, float y) {
	XentRect root = {0};
	if (!xent_get_layout_rect(fv->ctx, fv->root, &root)) return 0;
	root.w = root.w; /* rects are node-local already via hover coords */
	FluxRect prev = flip_btn_rect(fv, &root, false);
	FluxRect next = flip_btn_rect(fv, &root, true);
	if (flip_point_in(&prev, x, y) && fv->selected > 0) return 1;
	if (flip_point_in(&next, x, y) && fv->selected < flip_page_count(fv) - 1) return 2;
	return 0;
}

static void flip_pointer_move(void *ctx, float x, float y) {
	( void ) x;
	( void ) y;
	FluxFlipViewData *fv = ( FluxFlipViewData * ) ctx;
	fv->pointer_activity = GetTickCount();
}

static void flip_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxFlipViewData *fv = ( FluxFlipViewData * ) ctx;
	fv->pressed_btn      = ( uint8_t ) flip_hit_button(fv, x, y);
}

/* Click-on-release: act on the button the press started on, but only if the
 * release still lands on it (standard button semantics). */
static void flip_click(void *ctx) {
	FluxFlipViewData *fv = ( FluxFlipViewData * ) ctx;
	FluxNodeData     *nd = ( FluxNodeData * ) xent_get_userdata(fv->ctx, fv->root);
	int hit              = nd ? flip_hit_button(fv, nd->hover_local_x, nd->hover_local_y) : 0;
	if (hit && hit == fv->pressed_btn) {
		flip_go(fv, fv->selected + (hit == 2 ? 1 : -1), true);
		fv->pointer_activity = GetTickCount();
	}
	fv->pressed_btn = 0;
}

static void flip_cancel(void *ctx) { (( FluxFlipViewData * ) ctx)->pressed_btn = 0; }

static bool flip_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxFlipViewData *fv   = ( FluxFlipViewData * ) ctx;
	int               prev = fv->vertical ? VK_UP : VK_LEFT;
	int               next = fv->vertical ? VK_DOWN : VK_RIGHT;
	fv->pointer_activity   = GetTickCount(); /* keyboard shows the buttons too */

	if (( int ) vk == prev) { flip_go(fv, fv->selected - 1, true); return true; }
	if (( int ) vk == next) { flip_go(fv, fv->selected + 1, true); return true; }
	if (vk == VK_HOME) { flip_go(fv, 0, true); return true; }
	if (vk == VK_END) { flip_go(fv, flip_page_count(fv) - 1, true); return true; }
	return false;
}

bool flux_flip_view_handle_wheel(FluxNodeStore *store, XentNodeId flip, float wheel_y) {
	FluxFlipViewData *fv = flip_data(store, flip);
	if (!fv) return false;

	unsigned long now  = GetTickCount();
	int           sign = wheel_y < 0.0f ? -1 : 1;
	bool can_flip      = sign != fv->wheel_sign || now - fv->last_wheel >= FLIP_WHEEL_GATE_MS;
	fv->wheel_sign     = sign;
	fv->last_wheel     = now;
	if (!can_flip) return true; /* swallow, per WinUI */

	int target = fv->selected + (wheel_y < 0.0f ? 1 : -1);
	int count  = flip_page_count(fv);
	if (target < 0 || target > count - 1) return false; /* edge: chain to outer scroll */
	flip_go(fv, target, true);
	return true;
}

/* -------------------------------------------------------------------------
 * Engine sync: size pages to the viewport + apply the slide offset
 * ---------------------------------------------------------------------- */

void flux_flip_view_sync(XentContext *ctx, XentNodeId flip, struct FluxNodeData *nd) {
	( void ) flip;
	FluxFlipViewData *fv   = ( FluxFlipViewData * ) nd->component_data;
	XentRect          root = {0};
	if (!fv || !xent_get_layout_rect(ctx, fv->root, &root) || root.w <= 0.0f || root.h <= 0.0f) return;

	float extent = fv->vertical ? root.h : root.w;
	float cross  = fv->vertical ? root.w : root.h;
	if (extent != fv->extent || cross != fv->cross) {
		fv->extent = extent;
		fv->cross  = cross;
		if (!fv->anim) fv->offset = ( float ) fv->selected * extent; /* keep the page put on resize */
		else fv->anim_to = ( float ) fv->selected * extent;
	}

	int i = 0;
	for (XentNodeId page = xent_get_first_child(ctx, fv->host); page != XENT_NODE_INVALID;
	  page               = xent_get_next_sibling(ctx, page), i++)
	{
		XentPoint at = fv->vertical ? (XentPoint) {0.0f, ( float ) i * extent}
		                            : (XentPoint) {( float ) i * extent, 0.0f};
		xent_set_absolute_position(ctx, page, at);
		xent_set_size(ctx, page, (XentSize) {root.w, root.h});
	}

	xent_set_absolute_position(
	  ctx, fv->host, fv->vertical ? (XentPoint) {0.0f, -fv->offset} : (XentPoint) {-fv->offset, 0.0f}
	);
	float total = ( float ) (i > 0 ? i : 1);
	xent_set_size(
	  ctx, fv->host, fv->vertical ? (XentSize) {root.w, total * extent} : (XentSize) {total * extent, root.h}
	);
}

/* -------------------------------------------------------------------------
 * Create / API
 * ---------------------------------------------------------------------- */

XentNodeId flux_create_flip_view(FluxFlipViewCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_FLIP_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, root);
	FluxFlipViewData *fv = nd ? ( FluxFlipViewData * ) calloc(1, sizeof(FluxFlipViewData)) : NULL;
	XentNodeId        host
	  = fv ? flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_CONTAINER) : XENT_NODE_INVALID;
	if (!nd || !fv || host == XENT_NODE_INVALID) {
		free(fv);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_ABSOLUTE);
	xent_set_protocol(info->ctx, host, XENT_PROTOCOL_ABSOLUTE);
	xent_set_absolute_position(info->ctx, host, (XentPoint) {0.0f, 0.0f});

	fv->ctx           = info->ctx;
	fv->store         = info->store;
	fv->window        = info->window;
	fv->root          = root;
	fv->host          = host;
	fv->vertical      = info->vertical;
	fv->on_select     = info->on_select;
	fv->on_select_ctx = info->userdata;

	nd->component_data          = fv;
	nd->destroy_component_data  = free;
	nd->clips_children          = true; /* only the current page shows */
	nd->behavior.on_pointer_move     = flip_pointer_move;
	nd->behavior.on_pointer_move_ctx = fv;
	nd->behavior.on_pointer_down     = flip_pointer_down;
	nd->behavior.on_pointer_down_ctx = fv;
	nd->behavior.on_click            = flip_click;
	nd->behavior.on_click_ctx        = fv;
	nd->behavior.on_cancel           = flip_cancel;
	nd->behavior.on_cancel_ctx       = fv;
	nd->behavior.on_key              = flip_key;
	nd->behavior.on_key_ctx          = fv;

	xent_set_focusable(info->ctx, root, true);
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);
	return root;
}

XentNodeId flux_flip_view_content_node(FluxNodeStore *store, XentNodeId flip) {
	FluxFlipViewData *fv = flip_data(store, flip);
	return fv ? fv->host : XENT_NODE_INVALID;
}

void flux_flip_view_select(FluxNodeStore *store, XentNodeId flip, int index) {
	FluxFlipViewData *fv = flip_data(store, flip);
	if (fv) flip_go(fv, index, false);
}
