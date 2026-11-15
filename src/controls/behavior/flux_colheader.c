#include "controls/behavior/flux_colheader.h"
#include "controls/factory/flux_factory.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/factory/flux_init_container.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"
#include "store/flux_node_data.h"
#include "render/flux_render_snapshot.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

static FluxColItemData *item_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_COLITEM ? ( FluxColItemData * ) nd->component_data : NULL;
}

static FluxColHeaderData *header_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_COLHEADER ? ( FluxColHeaderData * ) nd->component_data : NULL;
}

static FluxColHeaderData *item_header(FluxColItemData const *d) {
	return header_data(d->store, xent_node_parent(d->ctx, d->node));
}

static void repaint(FluxColHeaderData const *h) {
	HWND hwnd = h && h->window ? flux_window_hwnd(h->window) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void route(FluxColItemData const *d, XtkEventId id, XtkEventPayload payload) {
	XtkHost *host = flux_store_host(d->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) d->node,
	  .source         = ( XtkCtrl ) d->node,
	  .current_target = ( XtkCtrl ) d->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static FluxColPart part_at(FluxColItemData const *d, float x) {
	return d->resizable && x >= d->width - FLUX_COL_SPLITTER_W ? FLUX_COL_PART_SPLITTER : FLUX_COL_PART_BUTTON;
}

static float clamp_width(FluxColItemData const *d, float w) {
	if (w < d->min_width) w = d->min_width;
	if (w > d->max_width) w = d->max_width;
	return fmaxf(w, 2.0f * FLUX_COL_SPLITTER_W + 1.0f);
}

static void apply_width(FluxColItemData *d, float w) {
	d->width = w;
	xent_setw(d->ctx, d->node, w);
}

static void user_resize(FluxColItemData *d, float w) {
	w = clamp_width(d, w);
	if (w == d->width) return;
	apply_width(d, w);
	route(d, XTK_EVENT_RESIZE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_F32, .u.f32 = w});
}

static float item_left(FluxColHeaderData const *h, XentNodeId node) {
	XentRect hr = {0}, ir = {0};
	xent_layout_rect(h->ctx, h->node, &hr);
	xent_layout_rect(h->ctx, node, &ir);
	return ir.x - hr.x;
}

static float item_width(FluxColHeaderData const *h, XentNodeId node) {
	XentRect ir = {0};
	xent_layout_rect(h->ctx, node, &ir);
	return ir.w;
}

static void set_offset(FluxColHeaderData *h, XentNodeId node, float offset, bool animate) {
	FluxColItemData *d  = item_data(h->store, node);
	FluxNodeData    *nd = flux_store_find(h->store, node);
	if (!d || !nd || (d->offset_to == offset && (animate || !d->offset_anim))) return;
	d->offset_from  = nd->render_translate_x;
	d->offset_to    = offset;
	d->offset_start = flux_anim_now();
	d->offset_anim  = animate;
	if (!animate) nd->render_translate_x = offset;
	if (animate && !h->animating) {
		h->animating = true;
		flux_anim_register(h, flux_colheader_step);
	}
}

static bool step_item(FluxColHeaderData *h, XentNodeId node, unsigned long now) {
	FluxColItemData *d  = item_data(h->store, node);
	FluxNodeData    *nd = flux_store_find(h->store, node);
	if (!d || !nd || !d->offset_anim) return false;
	float t = ( float ) (now - d->offset_start) / ( float ) FLUX_COL_REORDER_MS;
	if (t >= 1.0f) {
		nd->render_translate_x = d->offset_to;
		d->offset_anim         = false;
		return false;
	}
	float e                = flux_cubic_bezier(t, 0.8f, 0.0f, 0.2f, 1.0f);
	nd->render_translate_x = d->offset_from + (d->offset_to - d->offset_from) * e;
	return true;
}

bool flux_colheader_step(void *ctx, unsigned long now) {
	FluxColHeaderData *h      = ( FluxColHeaderData * ) ctx;
	bool               active = false;
	for (XentNodeId c = xent_node_first(h->ctx, h->node); c != XENT_NODE_INVALID; c = xent_node_next(h->ctx, c))
		active |= step_item(h, c, now);
	repaint(h);
	h->animating = active;
	return active;
}

static void set_opacity(FluxStore *store, XentNodeId node, float opacity) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd) nd->render_opacity = opacity;
}

static void end_reorder(FluxColHeaderData *h, bool committed) {
	if (h->drag != XENT_NODE_INVALID && h->reordering) {
		for (XentNodeId c = xent_node_first(h->ctx, h->node); c != XENT_NODE_INVALID; c = xent_node_next(h->ctx, c))
			set_offset(h, c, 0.0f, !committed);
		set_opacity(h->store, h->drag, 1.0f);
	}
	h->drag       = XENT_NODE_INVALID;
	h->reordering = false;
	h->target     = -1;
	repaint(h);
}

static float last_right(FluxColHeaderData const *h) {
	XentNodeId last = xent_node_last(h->ctx, h->node);
	return last == XENT_NODE_INVALID ? 0.0f : item_left(h, last) + item_width(h, last);
}

static void reflow_one(FluxColHeaderData *h, XentNodeId c, int i, int dragged, float x, float dw, bool *found) {
	float left   = item_left(h, c);
	float center = left + item_width(h, c) * 0.5f;
	float offset = 0.0f;
	if (i > dragged && x > center) {
		offset    = -dw;
		h->target = i;
	}
	else if (i < dragged && x < center) {
		offset = dw;
		if (!*found) h->target = i;
		*found = true;
	}
	set_offset(h, c, offset, true);
}

static void reflow(FluxColHeaderData *h, float x) {
	int   dragged = 0;
	float dw      = item_width(h, h->drag);
	for (XentNodeId c = xent_node_first(h->ctx, h->node); c != h->drag && c != XENT_NODE_INVALID;
	  c               = xent_node_next(h->ctx, c))
		dragged++;
	bool found = false;
	int  i     = 0;
	h->target  = -1;
	for (XentNodeId c = xent_node_first(h->ctx, h->node); c != XENT_NODE_INVALID; c = xent_node_next(h->ctx, c), i++)
		if (c != h->drag) reflow_one(h, c, i, dragged, x, dw, &found);
}

static XentNodeId scroll_owner(FluxColHeaderData const *h) {
	for (XentNodeId p = xent_node_parent(h->ctx, h->node); p != XENT_NODE_INVALID; p = xent_node_parent(h->ctx, p))
		if (flux_node_type(h->store, p) == FLUX_CONTROL_SCROLL) return p;
	return XENT_NODE_INVALID;
}

static void autoscroll(FluxColHeaderData const *h, float x) {
	XentNodeId      scroll = scroll_owner(h);
	FluxNodeData   *nd     = scroll == XENT_NODE_INVALID ? NULL : flux_store_find(h->store, scroll);
	FluxScrollData *sd     = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	XentRect        sr = {0}, hr = {0};
	if (!sd) return;
	xent_layout_rect(h->ctx, scroll, &sr);
	xent_layout_rect(h->ctx, h->node, &hr);
	float max = sd->content_w - sr.w;
	float at  = hr.x + x - sr.x - sd->scroll_x;
	float dx  = at < FLUX_COL_SCROLL_MARGIN        ? -FLUX_COL_SCROLL_STEP
	          : at > sr.w - FLUX_COL_SCROLL_MARGIN ? FLUX_COL_SCROLL_STEP
	                                               : 0.0f;
	if (max > 0.0f && dx != 0.0f)
		flux_scroll_request(h->store, scroll, fminf(fmaxf(sd->scroll_x + dx, 0.0f), max), NAN);
}

static bool begin_reorder(FluxColHeaderData *h, float x) {
	float threshold = h->touch ? FLUX_COL_DRAG_TOUCH : FLUX_COL_DRAG_MOUSE;
	if (fabsf(x - h->press_x) < threshold) return false;
	h->reordering = true;
	h->ghost_left = item_left(h, h->drag);
	h->ghost_w    = item_width(h, h->drag);
	set_opacity(h->store, h->drag, 0.0f);
	return true;
}

static void move_reorder(FluxColHeaderData *h, float x) {
	if (h->drag == XENT_NODE_INVALID || (!h->reordering && !begin_reorder(h, x))) return;
	float max  = fmaxf(0.0f, last_right(h) - h->ghost_w);
	h->ghost_x = fminf(fmaxf(h->ghost_left + x - h->press_x, 0.0f), max);
	autoscroll(h, x);
	reflow(h, x);
	repaint(h);
}

static void commit_reorder(FluxColHeaderData *h) {
	FluxColItemData *d      = item_data(h->store, h->drag);
	int              target = h->target;
	end_reorder(h, target >= 0);
	if (d && target >= 0)
		route(d, XTK_EVENT_REORDER, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = target});
}

static float header_x(FluxColItemData const *d, FluxColHeaderData const *h, float local_x) {
	return item_left(h, d->node) + local_x;
}

static void col_down(void *ctx, float x, float y, int clicks) {
	( void ) y;
	FluxColItemData *d = ( FluxColItemData * ) ctx;
	d->press_part      = ( uint8_t ) part_at(d, x);
	d->press_x         = x;
	d->press_width     = d->width;
	if (d->press_part == FLUX_COL_PART_SPLITTER) {
		if (clicks == 2) route(d, XTK_EVENT_FIT, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_NONE});
		return;
	}
	FluxColHeaderData *h  = item_header(d);
	FluxNodeData      *nd = flux_store_find(d->store, d->node);
	if (!h) return;
	end_reorder(h, false);
	h->drag    = d->node;
	h->touch   = nd && nd->state.pointer_type == FLUX_POINTER_TOUCH;
	h->press_x = header_x(d, h, x);
}

static void col_move(void *ctx, float x, float y) {
	( void ) y;
	FluxColItemData *d = ( FluxColItemData * ) ctx;
	if (d->press_part == FLUX_COL_PART_SPLITTER) {
		user_resize(d, d->press_width + x - d->press_x);
		return;
	}
	FluxColHeaderData *h = d->press_part == FLUX_COL_PART_BUTTON ? item_header(d) : NULL;
	if (h && h->drag == d->node) move_reorder(h, header_x(d, h, x));
}

static bool col_release(void *ctx) {
	FluxColItemData   *d    = ( FluxColItemData * ) ctx;
	FluxColPart        part = ( FluxColPart ) d->press_part;
	FluxColHeaderData *h    = item_header(d);
	d->press_part           = FLUX_COL_PART_NONE;
	if (part == FLUX_COL_PART_SPLITTER) return true;
	if (!h || !h->reordering || h->drag != d->node) {
		if (h) h->drag = XENT_NODE_INVALID;
		d->press_part = ( uint8_t ) part;
		return false;
	}
	commit_reorder(h);
	return true;
}

static void col_click(void *ctx) {
	FluxColItemData *d    = ( FluxColItemData * ) ctx;
	FluxColPart      part = d->press_part ? ( FluxColPart ) d->press_part : ( FluxColPart ) d->focus_part;
	d->press_part         = FLUX_COL_PART_NONE;
	if (part == FLUX_COL_PART_BUTTON && d->sortable)
		route(d, XTK_EVENT_INVOKE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_NONE});
}

static void col_cancel(void *ctx) {
	FluxColItemData   *d = ( FluxColItemData * ) ctx;
	FluxColHeaderData *h = item_header(d);
	d->press_part        = FLUX_COL_PART_NONE;
	if (h && h->drag == d->node) end_reorder(h, false);
}

static bool col_key(void *ctx, FluxKey key, bool down) {
	FluxColItemData   *d = ( FluxColItemData * ) ctx;
	FluxColHeaderData *h = item_header(d);
	if (!down) return false;
	if (key == FLUX_KEY_ESCAPE && h && h->reordering) {
		end_reorder(h, false);
		return true;
	}
	bool activate = key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE;
	if (activate) return d->focus_part == FLUX_COL_PART_SPLITTER || !d->sortable;
	if (d->focus_part != FLUX_COL_PART_SPLITTER || (key != FLUX_KEY_LEFT && key != FLUX_KEY_RIGHT)) return false;
	float step = flux_key_held(FLUX_KEY_CONTROL) ? 5.0f : 1.0f;
	user_resize(d, d->width + (key == FLUX_KEY_LEFT ? -step : step));
	return true;
}

static bool col_tab(void *ctx, bool shift, bool entering) {
	FluxColItemData *d     = ( FluxColItemData * ) ctx;
	FluxColPart      split = d->resizable ? FLUX_COL_PART_SPLITTER : FLUX_COL_PART_BUTTON;
	if (entering) {
		d->focus_part = ( uint8_t ) (shift ? split : FLUX_COL_PART_BUTTON);
		return true;
	}
	if (!shift && d->focus_part == FLUX_COL_PART_BUTTON && d->resizable) {
		d->focus_part = FLUX_COL_PART_SPLITTER;
		return true;
	}
	if (shift && d->focus_part == FLUX_COL_PART_SPLITTER) {
		d->focus_part = FLUX_COL_PART_BUTTON;
		return true;
	}
	return false;
}

static void col_focus(void *ctx) {
	FluxColItemData *d = ( FluxColItemData * ) ctx;
	if (!d->focus_part) d->focus_part = FLUX_COL_PART_BUTTON;
}

static void col_blur(void *ctx) { (( FluxColItemData * ) ctx)->focus_part = FLUX_COL_PART_NONE; }

static void colitem_destroy(void *data) {
	FluxColItemData *d = ( FluxColItemData * ) data;
	if (!d) return;
	flux_str_free(d->text);
	free(d);
}

static void colheader_destroy(void *data) {
	flux_anim_unregister(data);
	free(data);
}

static void wire_item(FluxNodeData *nd, FluxColItemData *d) {
	nd->behavior = (FluxNodeBehavior) {
	  .on_pointer_down     = col_down,
	  .on_pointer_down_ctx = d,
	  .on_pointer_move     = col_move,
	  .on_pointer_move_ctx = d,
	  .on_release          = col_release,
	  .on_release_ctx      = d,
	  .on_click            = col_click,
	  .on_click_ctx        = d,
	  .on_cancel           = col_cancel,
	  .on_cancel_ctx       = d,
	  .on_key              = col_key,
	  .on_key_ctx          = d,
	  .on_tab              = col_tab,
	  .on_tab_ctx          = d,
	  .on_focus            = col_focus,
	  .on_focus_ctx        = d,
	  .on_blur             = col_blur,
	  .on_blur_ctx         = d,
	};
}

XentNodeId flux_colitem_init(XentCtx *ctx, FluxStore *store) {
	XentNodeId       node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_COLITEM);
	FluxNodeData    *nd   = node == XENT_NODE_INVALID ? NULL : flux_store_find(store, node);
	FluxColItemData *d    = nd ? ( FluxColItemData * ) calloc(1, sizeof(*d)) : NULL;
	if (!d) return node == XENT_NODE_INVALID ? node : flux_factory_fail_node(ctx, store, node);
	*d = (FluxColItemData) {
	  .ctx       = ctx,
	  .store     = store,
	  .node      = node,
	  .min_width = 50.0f,
	  .max_width = 800.0f,
	  .sortable  = true,
	  .resizable = true};
	nd->component_data         = d;
	nd->destroy_component_data = colitem_destroy;
	wire_item(nd, d);
	apply_width(d, 200.0f);
	xent_setshrink(ctx, node, 0.0f);
	xent_sem_setrole(ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_set_focusable(ctx, node, true);
	return node;
}

XentNodeId flux_colheader_init(XentCtx *ctx, FluxStore *store, FluxWindow *window) {
	XentNodeId         node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_COLHEADER);
	FluxNodeData      *nd   = node == XENT_NODE_INVALID ? NULL : flux_store_find(store, node);
	FluxColHeaderData *h    = nd ? ( FluxColHeaderData * ) calloc(1, sizeof(*h)) : NULL;
	if (!h) return node == XENT_NODE_INVALID ? node : flux_factory_fail_node(ctx, store, node);
	*h = (FluxColHeaderData) {
	  .ctx = ctx, .store = store, .node = node, .window = window, .drag = XENT_NODE_INVALID, .target = -1};
	nd->component_data         = h;
	nd->destroy_component_data = colheader_destroy;
	xent_setproto(ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, node, XENT_FLEX_ROW);
	xent_setitems(ctx, node, XENT_FLEX_ALIGN_STRETCH);
	xent_seth(ctx, node, FLUX_COL_HEADER_H);
	return node;
}

void flux_colheader_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxColHeaderData *h = header_data(store, node);
	if (!h) return;
	h->ctx  = ctx;
	h->node = node;
}

void flux_colitem_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxColItemData *d = item_data(store, node);
	if (!d) return;
	d->ctx  = ctx;
	d->node = node;
}

void flux_colitem_settext(FluxStore *store, XentNodeId node, char const *text) {
	FluxColItemData *d = item_data(store, node);
	if (!d) return;
	flux_str_replace(&d->text, text);
	xent_sem_setlabel(d->ctx, node, text ? text : "");
}

void flux_colitem_setwidth(FluxStore *store, XentNodeId node, float width) {
	FluxColItemData *d = item_data(store, node);
	if (d) apply_width(d, clamp_width(d, width));
}

void flux_colitem_setmin(FluxStore *store, XentNodeId node, float width) {
	FluxColItemData *d = item_data(store, node);
	if (d) d->min_width = width;
}

void flux_colitem_setmax(FluxStore *store, XentNodeId node, float width) {
	FluxColItemData *d = item_data(store, node);
	if (d) d->max_width = width;
}

void flux_colitem_setalign(FluxStore *store, XentNodeId node, uint32_t align) {
	FluxColItemData *d = item_data(store, node);
	if (d) d->align = ( uint8_t ) align;
}

void flux_colitem_setsort(FluxStore *store, XentNodeId node, uint32_t sort) {
	FluxColItemData *d = item_data(store, node);
	if (d) d->sort = ( uint8_t ) sort;
}

void flux_colitem_setsortable(FluxStore *store, XentNodeId node, bool sortable) {
	FluxColItemData *d = item_data(store, node);
	if (d) d->sortable = sortable;
}

void flux_colitem_setresizable(FluxStore *store, XentNodeId node, bool resizable) {
	FluxColItemData *d = item_data(store, node);
	if (!d) return;
	d->resizable = resizable;
	if (!resizable && d->focus_part == FLUX_COL_PART_SPLITTER) d->focus_part = FLUX_COL_PART_BUTTON;
}

int flux_colitem_cursor(FluxStore *store, XentNodeId node) {
	FluxColItemData *d  = item_data(store, node);
	FluxNodeData    *nd = flux_store_find(store, node);
	if (!d || !nd) return FLUX_CURSOR_ARROW;
	bool over = nd->hover_local_x >= 0.0f && part_at(d, nd->hover_local_x) == FLUX_COL_PART_SPLITTER;
	return over || d->press_part == FLUX_COL_PART_SPLITTER ? FLUX_CURSOR_SIZEWE : FLUX_CURSOR_ARROW;
}

static FluxColItemSnapshot snap_item(FluxColItemData const *d) {
	return (FluxColItemSnapshot) {
	  .label      = d->text,
	  .align      = d->align,
	  .sort       = d->sort,
	  .sortable   = d->sortable,
	  .resizable  = d->resizable,
	  .press_part = d->press_part,
	  .focus_part = d->focus_part,
	};
}

void flux_snap_colitem(FluxSnapCtx const *ctx) {
	ctx->snap->u.col_item = snap_item(( FluxColItemData const * ) ctx->data);
}

void flux_snap_colheader(FluxSnapCtx const *ctx) {
	FluxColHeaderData const *h = ( FluxColHeaderData const * ) ctx->data;
	FluxColItemData const   *d = h->reordering ? item_data(h->store, h->drag) : NULL;
	if (!d) return;
	ctx->snap->u.col_header
	  = (FluxColHeaderSnapshot) {.ghost = true, .ghost_x = h->ghost_x, .ghost_w = h->ghost_w, .item = snap_item(d)};
	ctx->snap->u.col_header.item.press_part = FLUX_COL_PART_BUTTON;
}
