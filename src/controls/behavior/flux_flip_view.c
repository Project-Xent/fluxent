#include "controls/behavior/flux_flip_internal.h"
#include "a11y/flux_uia_ids.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/flux_control_uia.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_items.h"
#include "fluxent/flux_host.h"
#include "runtime/flux_anim_driver.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdlib.h>
#include <windows.h>

#define FLIP_ANIM_MS       350.0f
#define FLIP_WHEEL_GATE_MS 200
#define FLIP_BTN_W         16.0f
#define FLIP_BTN_H         38.0f
#define FLIP_BTN_MARGIN    1.0f

static FluxFlipViewData *flip_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_FLIP_VIEW || !nd->component_data) return NULL;
	return ( FluxFlipViewData * ) nd->component_data;
}

static int flip_page_count(FluxFlipViewData const *fv) {
	int n = 0;
	for (XentNodeId c = xent_node_first(fv->ctx, fv->host); c != XENT_NODE_INVALID; c = xent_node_next(fv->ctx, c)) n++;
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
		float e    = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		fv->offset = fv->anim_from + (fv->anim_to - fv->anim_from) * e;
	}
	flip_repaint(fv);
	return fv->anim;
}

static void flip_report(FluxFlipViewData *fv, int index) {
	XtkHost *host = flux_store_host(fv->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_SELECT,
		  .target         = ( XtkCtrl ) fv->root,
		  .source         = ( XtkCtrl ) fv->root,
		  .current_target = ( XtkCtrl ) fv->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
		};
		flux_host_route(host, &event);
	}
	if (fv->on_select) fv->on_select(fv->on_select_ctx, index);
}

static void flip_go(FluxFlipViewData *fv, int index, bool from_user) {
	int count = flip_page_count(fv);
	if (index < 0) index = 0;
	if (index > count - 1) index = count - 1;
	if (index < 0 || index == fv->selected) return;

	bool adjacent = index == fv->selected + 1 || index == fv->selected - 1;
	fv->selected  = index;

	float target  = ( float ) index * fv->extent;
	if (adjacent && fv->extent > 0.0f) {
		fv->anim_from  = fv->offset;
		fv->anim_to    = target;
		fv->anim_start = flux_anim_now();
		fv->anim       = true;
		flux_anim_register(fv, flip_step);
	}
	else {
		fv->offset = target;
		fv->anim   = false;
	}

	if (from_user) flip_report(fv, index);
}

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
	if (!xent_layout_rect(fv->ctx, fv->root, &root)) return 0;
	root.w        = root.w;
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
	fv->pointer_activity = flux_anim_now();
}

static void flip_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxFlipViewData *fv = ( FluxFlipViewData * ) ctx;
	fv->pressed_btn      = ( uint8_t ) flip_hit_button(fv, x, y);
}

static FluxFlipViewData *flip_of_part(FluxStore *store, XentNodeId part, int *index) {
	XentNodeId owner = flux_uia_part_owner(store, part, index);
	return owner != XENT_NODE_INVALID ? flip_data(store, owner) : NULL;
}

static bool flip_uia_go(FluxStore *store, XentNodeId part) {
	int               index = 0;
	FluxFlipViewData *fv    = flip_of_part(store, part, &index);
	if (!fv) return false;
	flip_go(fv, fv->selected + (index == 1 ? 1 : -1), true);
	return true;
}

static void flip_uia_name(FluxStore *store, XentNodeId part, char *out, size_t size) {
	int index = 0;
	( void ) flip_of_part(store, part, &index);
	snprintf(out, size, "%s", index == 1 ? "Next" : "Previous");
}

static FluxFlipViewData *flip_of_host(FluxStore *store, XentNodeId host) {
	FluxFlipViewData *fv = flip_data(store, xent_node_parent(flux_store_ctx(store), host));
	return fv && fv->host == host ? fv : NULL;
}

static XentNodeId flip_selected_page(FluxFlipViewData const *fv) {
	int i = 0;
	for (XentNodeId c = xent_node_first(fv->ctx, fv->host); c != XENT_NODE_INVALID; c = xent_node_next(fv->ctx, c), i++)
		if (i == fv->selected) return c;
	return XENT_NODE_INVALID;
}

static bool flip_uia_admits(FluxStore *store, XentNodeId host, XentNodeId child) {
	FluxFlipViewData *fv = flip_of_host(store, host);
	return !fv || child == flip_selected_page(fv);
}

static bool flip_uia_selected(FluxStore *store, XentNodeId host) { return flip_of_host(store, host) != NULL; }

static bool flip_uia_select(FluxStore *store, XentNodeId host, int op) {
	return op != FLUX_UIA_SEL_REMOVE && flip_of_host(store, host) != NULL;
}

static bool flip_uia_setinfo(FluxStore *store, XentNodeId host, int *pos, int *size, int *level) {
	( void ) level;
	FluxFlipViewData *fv = flip_of_host(store, host);
	if (!fv) return false;
	*pos  = fv->selected + 1;
	*size = flip_page_count(fv);
	return true;
}

static void flip_uia_item_name(FluxStore *store, XentNodeId host, char *out, size_t size) {
	FluxFlipViewData *fv    = flip_of_host(store, host);
	XentNodeId        page  = fv ? flip_selected_page(fv) : XENT_NODE_INVALID;
	char const       *label = page != XENT_NODE_INVALID ? xent_sem_label(fv->ctx, page) : NULL;
	snprintf(out, size, "%s", label ? label : "");
}

static FluxUiaHooks const flip_uia_button = {
  .role     = FLUX_UIA_BUTTON,
  .patterns = FLUX_UIA_P_INVOKE,
  .invoked  = true,
  .klass    = L"Button",
  .invoke   = flip_uia_go,
  .name     = flip_uia_name};
static FluxUiaHooks const flip_uia_item = {
  .role     = FLUX_UIA_LISTITEM,
  .patterns = FLUX_UIA_P_SELECTION_ITEM | FLUX_UIA_P_SCROLL_ITEM,
  .klass    = L"FlipViewItem",
  .selected = flip_uia_selected,
  .select   = flip_uia_select,
  .admits   = flip_uia_admits,
  .setinfo  = flip_uia_setinfo,
  .name     = flip_uia_item_name};

static void flip_parts_sync(FluxFlipViewData *fv, XentRect const *root) {
	FluxRect p = flip_btn_rect(fv, root, false), n = flip_btn_rect(fv, root, true);
	bool     a = fv->selected > 0, b = fv->selected < flip_page_count(fv) - 1;
	flux_uia_part_place(fv->ctx, fv->prev, a ? (XentRect) {p.x, p.y, p.w, p.h} : (XentRect) {0}, (XentPoint) {0});
	flux_uia_part_place(fv->ctx, fv->next, b ? (XentRect) {n.x, n.y, n.w, n.h} : (XentRect) {0}, (XentPoint) {0});
}

static void flip_parts_make(FluxFlipViewData *fv) {
	fv->prev = flux_uia_part(fv->ctx, fv->store, fv->root, fv->root, 0);
	fv->next = flux_uia_part(fv->ctx, fv->store, fv->root, fv->root, 1);
	flux_uia_part_hooks(fv->store, fv->prev, &flip_uia_button);
	flux_uia_part_hooks(fv->store, fv->next, &flip_uia_button);
	flux_uia_part_hooks(fv->store, fv->host, &flip_uia_item);
	if (fv->prev) xent_node_move(fv->ctx, fv->root, fv->prev, fv->host);
	if (fv->next) xent_node_move(fv->ctx, fv->root, fv->next, fv->host);
}

static void flip_click(void *ctx) {
	FluxFlipViewData *fv      = ( FluxFlipViewData * ) ctx;
	FluxNodeData     *nd      = flux_store_find(fv->store, fv->root);
	int               hit     = nd ? flip_hit_button(fv, nd->hover_local_x, nd->hover_local_y) : 0;
	int               pressed = fv->pressed_btn;
	int               target  = fv->selected + (hit == 2 ? 1 : -1);
	fv->pressed_btn           = 0;
	if (hit && hit == pressed) {
		fv->pointer_activity = flux_anim_now();
		flux_uia_note(fv->store, hit == 2 ? fv->next : fv->prev, -1, FLUX_UIA_EVT_INVOKED);
		flip_go(fv, target, true);
	}
}

static void flip_cancel(void *ctx) { (( FluxFlipViewData * ) ctx)->pressed_btn = 0; }

static bool flip_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxFlipViewData *fv   = ( FluxFlipViewData * ) ctx;
	FluxKey           prev = fv->vertical ? FLUX_KEY_UP : FLUX_KEY_LEFT;
	FluxKey           next = fv->vertical ? FLUX_KEY_DOWN : FLUX_KEY_RIGHT;
	fv->pointer_activity   = flux_anim_now();

	if (key == prev) {
		flip_go(fv, fv->selected - 1, true);
		return true;
	}
	if (key == next) {
		flip_go(fv, fv->selected + 1, true);
		return true;
	}
	if (key == FLUX_KEY_HOME) {
		flip_go(fv, 0, true);
		return true;
	}
	if (key == FLUX_KEY_END) {
		flip_go(fv, flip_page_count(fv) - 1, true);
		return true;
	}
	return false;
}

bool flux_flip_handle_wheel(FluxStore *store, XentNodeId flip, float wheel_y) {
	FluxFlipViewData *fv = flip_data(store, flip);
	if (!fv) return false;

	unsigned long now      = flux_anim_now();
	int           sign     = wheel_y < 0.0f ? -1 : 1;
	bool          can_flip = sign != fv->wheel_sign || now - fv->last_wheel >= FLIP_WHEEL_GATE_MS;
	fv->wheel_sign         = sign;
	fv->last_wheel         = now;
	if (!can_flip) return true;

	int target = fv->selected + (wheel_y < 0.0f ? 1 : -1);
	int count  = flip_page_count(fv);
	if (target < 0 || target > count - 1) return false;
	flip_go(fv, target, true);
	return true;
}

static void flip_resize(FluxFlipViewData *fv, float extent, float cross) {
	if (extent == fv->extent && cross == fv->cross) return;
	fv->extent = extent;
	fv->cross  = cross;
	if (!fv->anim) fv->offset = ( float ) fv->selected * extent;
	else fv->anim_to = ( float ) fv->selected * extent;
}

static int flip_layout_pages(FluxFlipViewData *fv, XentRect const *root) {
	int i = 0;
	for (XentNodeId page = xent_node_first(fv->ctx, fv->host); page != XENT_NODE_INVALID;
	  page               = xent_node_next(fv->ctx, page), i++)
	{
		XentPoint at
		  = fv->vertical ? (XentPoint) {0.0f, ( float ) i * fv->extent} : (XentPoint) {( float ) i * fv->extent, 0.0f};
		xent_setpos(fv->ctx, page, at);
		xent_setsize(fv->ctx, page, (XentSize) {root->w, root->h});
	}
	return i;
}

void flux_flip_sync(XentCtx *ctx, XentNodeId flip, struct FluxNodeData *nd) {
	( void ) flip;
	FluxFlipViewData *fv   = ( FluxFlipViewData * ) nd->component_data;
	XentRect          root = {0};
	if (!fv || !xent_layout_rect(ctx, fv->root, &root) || root.w <= 0.0f || root.h <= 0.0f) return;

	float extent = fv->vertical ? root.h : root.w;
	float cross  = fv->vertical ? root.w : root.h;
	flip_resize(fv, extent, cross);
	int i = flip_layout_pages(fv, &root);
	flip_parts_sync(fv, &root);
	xent_setpos(ctx, fv->host, fv->vertical ? (XentPoint) {0.0f, -fv->offset} : (XentPoint) {-fv->offset, 0.0f});
	float total = ( float ) (i > 0 ? i : 1);
	xent_setsize(
	  ctx, fv->host, fv->vertical ? (XentSize) {root.w, total * extent} : (XentSize) {total * extent, root.h}
	);
}

XentNodeId flux_flip_init(FluxFlipViewDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_FLIP_VIEW);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_store_find(info->store, root);
	FluxFlipViewData *fv = nd ? ( FluxFlipViewData * ) calloc(1, sizeof(FluxFlipViewData)) : NULL;
	XentNodeId        host
	  = fv ? flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_CONTAINER) : XENT_NODE_INVALID;
	if (!nd || !fv || host == XENT_NODE_INVALID) {
		free(fv);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_setproto(info->ctx, root, XENT_PROTOCOL_ABSOLUTE);
	xent_setproto(info->ctx, host, XENT_PROTOCOL_ABSOLUTE);
	xent_setpos(info->ctx, host, (XentPoint) {0.0f, 0.0f});

	fv->ctx                          = info->ctx;
	fv->store                        = info->store;
	fv->window                       = info->window;
	fv->root                         = root;
	fv->host                         = host;
	fv->vertical                     = info->vertical;
	fv->on_select                    = info->on_select;
	fv->on_select_ctx                = info->userdata;

	nd->component_data               = fv;
	nd->destroy_component_data       = free;
	nd->clips_children               = true;
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
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	flip_parts_make(fv);
	return root;
}

XentNodeId flux_flip_content_node(FluxStore *store, XentNodeId flip) {
	FluxFlipViewData *fv = flip_data(store, flip);
	return fv ? fv->host : XENT_NODE_INVALID;
}

void flux_flip_select(FluxStore *store, XentNodeId flip, int index) {
	FluxFlipViewData *fv = flip_data(store, flip);
	if (fv) flip_go(fv, index, false);
}

void flux_flip_set_vertical(FluxStore *store, XentNodeId flip, bool vertical) {
	FluxFlipViewData *fv = flip_data(store, flip);
	if (fv) fv->vertical = vertical;
}
