#include "flux_input_internal.h"
#include "flux_dmanip_sync.h"
#include "bridge/flux_internal.h"
#include <fluxent/flux_nav_trace.h>

#include <fluxent/flux_node_store.h>
#include "store/flux_node_data.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

typedef struct InputHitQuery {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId node;
	FluxPoint  point;
	FluxPoint  scroll;
} InputHitQuery;

typedef struct InputScrollEvent {
	FluxInput *input;
	XentNodeId root;
	FluxPoint  point;
	FluxPoint  wheel;
} InputScrollEvent;

typedef struct HitChildList {
	XentNodeId *items;
	uint32_t    count;
	uint32_t    cap;
} HitChildList;

static FluxPoint hit_test_node_offset(InputHitQuery const *query, FluxNodeData const *nd) {
	FluxPoint offset = query->scroll;
	if (!nd) return offset;
	offset.x -= nd->render_translate_x;
	offset.y -= nd->render_translate_y;
	return offset;
}

static FluxPoint hit_test_child_scroll_offset(InputHitQuery const *query, FluxNodeData *nd, FluxPoint offset) {
	if (!nd || flux_node_type(query->store, query->node) != FLUX_CONTROL_SCROLL) return offset;

	FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
	if (!sd) return offset;

	offset.x += flux_scroll_off_x(sd);
	offset.y += flux_scroll_off_y(sd);
	return offset;
}

static bool hit_rect_contains(XentRect const *rect, FluxPoint point, FluxPoint scroll) {
	float ax = rect->x - scroll.x;
	float ay = rect->y - scroll.y;
	return point.x >= ax && point.x < ax + rect->w && point.y >= ay && point.y < ay + rect->h;
}

static bool hit_child_list_append(HitChildList *children, XentNodeId child) {
	if (children->count == children->cap) {
		uint32_t    cap   = children->cap ? children->cap * 2u : 16u;
		XentNodeId *items = ( XentNodeId * ) realloc(children->items, sizeof(*items) * cap);
		if (!items) return false;
		children->items = items;
		children->cap   = cap;
	}

	children->items [children->count++] = child;
	return true;
}

static HitChildList hit_collect_children(XentCtx *ctx, XentNodeId node) {
	HitChildList children = {0};
	XentNodeId   child    = xent_node_first(ctx, node);
	while (child != XENT_NODE_INVALID) {
		if (!hit_child_list_append(&children, child)) break;
		child = xent_node_next(ctx, child);
	}
	return children;
}

static FluxHitResult hit_test_recursive(InputHitQuery const *query);

/* A node that participates in pointer interaction (press/click/hover/focus/tooltip). */
static bool          hit_node_interactive(XentCtx *ctx, XentNodeId node, FluxNodeData const *nd) {
	if (xent_get_focusable(ctx, node)) return true;
	if (!nd) return false;
	FluxNodeBehavior const *b = &nd->behavior;
	return b->on_click || b->on_pointer_down || b->on_middle_click || b->on_hover_changed || nd->tooltip_text;
}

static FluxHitResult
hit_test_children(InputHitQuery const *query, HitChildList const *children, FluxPoint child_scroll) {
	for (uint32_t i = children->count; i > 0; i--) {
		InputHitQuery child_query = *query;
		child_query.node          = children->items [i - 1u];
		child_query.scroll        = child_scroll;
		FluxHitResult r           = hit_test_recursive(&child_query);
		if (r.node != XENT_NODE_INVALID) return r;
	}
	return (FluxHitResult) {0};
}

static FluxHitResult hit_test_recursive(InputHitQuery const *query) {
	XentRect rect = {0};
	xent_layout_rect(query->ctx, query->node, &rect);

	FluxNodeData *nd          = flux_store_find(query->store, query->node);
	FluxPoint     node_offset = hit_test_node_offset(query, nd);
	if (!hit_rect_contains(&rect, query->point, node_offset)) return (FluxHitResult) {0};

	FluxPoint     child_scroll = hit_test_child_scroll_offset(query, nd, node_offset);
	HitChildList  children     = hit_collect_children(query->ctx, query->node);
	FluxHitResult child_hit    = hit_test_children(query, &children, child_scroll);
	free(children.items);
	if (child_hit.node != XENT_NODE_INVALID) return child_hit;

	/* An empty, non-interactive node (e.g. the overlay layer with no modal) is
	 * transparent to the pointer so clicks fall through to the content beneath. */
	if (children.count == 0 && !hit_node_interactive(query->ctx, query->node, nd)) return (FluxHitResult) {0};

	float ax = rect.x - node_offset.x;
	float ay = rect.y - node_offset.y;
	return (FluxHitResult) {
	  .node   = query->node,
	  .data   = nd,
	  .bounds = {ax, ay, rect.w, rect.h},
	  .local  = {query->point.x - ax, query->point.y - ay},
	};
}

static FluxHitResult input_hit_test_root(FluxInput *input, XentNodeId root, float px, float py) {
	InputHitQuery query = {
	  .ctx    = input->ctx,
	  .store  = input->store,
	  .node   = root,
	  .point  = {px,   py  },
	  .scroll = {0.0f, 0.0f},
	};
	return hit_test_recursive(&query);
}

/* A node that participates in pointer interaction: it handles presses,
 * clicks, hover, focus, or shows a tooltip. Plain content (text blocks,
 * layout containers) is transparent to the pointer. */
static bool input_node_interactive(FluxInput *input, XentNodeId node, FluxNodeData const *nd) {
	return hit_node_interactive(input->ctx, node, nd);
}

/* Re-base a hit onto ancestor `node`, shifting bounds/local by the layout-rect
 * delta (shared scroll offsets cancel out, so the delta is scroll-correct). */
static FluxHitResult input_rebase_hit(FluxInput *input, FluxHitResult const *hit, XentNodeId node, FluxNodeData *nd) {
	XentRect from = {0}, to = {0};
	xent_layout_rect(input->ctx, hit->node, &from);
	xent_layout_rect(input->ctx, node, &to);
	float dx = to.x - from.x, dy = to.y - from.y;
	return (FluxHitResult) {
	  .node   = node,
	  .data   = nd,
	  .bounds = {hit->bounds.x + dx, hit->bounds.y + dy, to.w, to.h},
	  .local  = {hit->local.x - dx, hit->local.y - dy},
	};
}

/* Pointer events land on the deepest node under the cursor, but plain
 * content must not swallow them: a click on the text inside a ListView row
 * belongs to the row. Resolve the hit to the nearest interactive ancestor. */
static FluxHitResult input_resolve_interactive(FluxInput *input, FluxHitResult const *hit) {
	for (XentNodeId node = hit->node; node != XENT_NODE_INVALID; node = xent_node_parent(input->ctx, node)) {
		FluxNodeData *nd = flux_store_find(input->store, node);
		if (!input_node_interactive(input, node, nd)) continue;
		return node == hit->node ? *hit : input_rebase_hit(input, hit, node, nd);
	}
	return (FluxHitResult) {0};
}

static InputScrollEvent
input_scroll_event_from_pointer(FluxInput *input, XentNodeId root, FluxPointerEvent const *event) {
	return (InputScrollEvent) {
	  .input = input,
	  .root  = root,
	  .point = {event->x,        event->y       },
	  .wheel = {event->wheel_dx, event->wheel_dy},
	};
}

void input_clear_node_hover(FluxNodeData *nd) {
	if (!nd) return;
	bool was          = nd->state.hovered;
	nd->state.hovered = 0;
	nd->hover_local_x = -1.0f;
	nd->hover_local_y = -1.0f;
	if (was && nd->behavior.on_hover_changed) nd->behavior.on_hover_changed(nd->behavior.on_hover_changed_ctx, false);
}

void input_clear_hovered(FluxInput *input) {
	if (input->hovered == XENT_NODE_INVALID) return;

	XentNodeId    old_id = input->hovered;
	FluxNodeData *old    = flux_store_find(input->store, old_id);
	input->hovered       = XENT_NODE_INVALID;
	input_clear_node_hover(old);
}

static void input_set_hovered_node(FluxInput *input, XentNodeId node, float px, float py) {
	if (node == input->hovered) return;

	input_clear_hovered(input);
	if (node == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_store_find(input->store, node);
	if (!nd) return;
	XentRect rect = {0};
	xent_layout_rect(input->ctx, node, &rect);
	nd->state.hovered      = 1;
	nd->state.pointer_type = ( uint8_t ) input->pointer_type;
	nd->hover_local_x      = px - rect.x;
	nd->hover_local_y      = py - rect.y;
	input->hovered         = node;
	if (nd->behavior.on_hover_changed) nd->behavior.on_hover_changed(nd->behavior.on_hover_changed_ctx, true);
}

static void input_update_hover_from_hit(FluxInput *input, FluxHitResult const *hit, float px, float py) {
	XentNodeId hovered = hit->node;
	if (input->pointer_type == FLUX_POINTER_TOUCH && input->pressed == XENT_NODE_INVALID) hovered = XENT_NODE_INVALID;

	if (hovered != input->hovered) {
		input_set_hovered_node(input, hovered, px, py);
		return;
	}
	FluxNodeData *nd = flux_store_find(input->store, hovered);
	if (!nd) return;
	XentRect rect = {0};
	xent_layout_rect(input->ctx, hovered, &rect);
	nd->hover_local_x      = px - rect.x;
	nd->hover_local_y      = py - rect.y;
	nd->state.pointer_type = ( uint8_t ) input->pointer_type;
}

static void input_forward_pressed_move(FluxInput *input, float px, float py) {
	if (input->pressed == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_store_find(input->store, input->pressed);
	if (!nd || !nd->behavior.on_pointer_move) return;

	float local_x = px - input->pressed_bounds.x;
	float local_y = py - input->pressed_bounds.y;
	nd->behavior.on_pointer_move(nd->behavior.on_pointer_move_ctx, local_x, local_y);
}

static bool input_press_number_box_spin(FluxInput *input, FluxHitResult const *hit) {
	if (hit->node == XENT_NODE_INVALID || !hit->data) return false;
	if (flux_node_type(input->store, hit->node) != FLUX_CONTROL_NUMBER_BOX) return false;
	if (!xent_sem_expanded(input->ctx, hit->node)) return false;

	XentRect rect = {0};
	xent_layout_rect(input->ctx, hit->node, &rect);

	float spin_start = rect.w - 76.0f;
	if (hit->local.x < spin_start) return false;

	if (hit->data->behavior.on_key) {
		bool up = hit->local.x < spin_start + 40.0f;
		hit->data->behavior.on_key(hit->data->behavior.on_key_ctx, up ? FLUX_KEY_UP : FLUX_KEY_DOWN, true);
	}
	return true;
}

static void input_update_click_count(FluxInput *input, FluxHitResult const *hit, float px, float py) {
	ULONGLONG now_ms   = GetTickCount64();
	DWORD     dbl_time = GetDoubleClickTime();
	float     dx       = px - input->last_click_x;
	float     dy       = py - input->last_click_y;
	float     dist2    = dx * dx + dy * dy;
	float     dbl_cx   = ( float ) GetSystemMetrics(SM_CXDOUBLECLK);
	float     dbl_cy   = ( float ) GetSystemMetrics(SM_CYDOUBLECLK);
	float     max_dist = (dbl_cx * dbl_cx + dbl_cy * dbl_cy) * 0.25f;

	if (hit->node == input->last_click_node && now_ms - input->last_click_time < dbl_time && dist2 <= max_dist) {
		input->click_count++;
		if (input->click_count > 3) input->click_count = 3;
	}
	else { input->click_count = 1; }

	input->last_click_time = now_ms;
	input->last_click_x    = px;
	input->last_click_y    = py;
	input->last_click_node = hit->node;
}

static void input_set_touch_press_hover(FluxInput *input, FluxHitResult const *hit) {
	if (input->pointer_type != FLUX_POINTER_TOUCH) return;

	if (input->hovered != hit->node) input_clear_hovered(input);
	hit->data->state.hovered = 1;
	hit->data->hover_local_x = hit->local.x;
	hit->data->hover_local_y = hit->local.y;
	input->hovered           = hit->node;
}

static void input_press_hit(FluxInput *input, FluxHitResult const *hit) {
	if (hit->node == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, hit->node);
	if (!nd) return;

	/* Single interactive gate: a semantically-disabled node consumes the press but
	 * dispatches nothing (no down/move/click/drag). All input enters through here, so
	 * this is the only place controls need to test for disabled. */
	if (!xent_sem_enabled(input->ctx, hit->node)) return;

	nd->state.pressed      = 1;
	nd->state.pointer_type = ( uint8_t ) input->pointer_type;
	input->pressed         = hit->node;
	input->pressed_bounds  = hit->bounds;

	FluxHitResult current  = *hit;
	current.data           = nd;
	input_set_touch_press_hover(input, &current);

	if (nd->behavior.on_pointer_move)
		nd->behavior.on_pointer_move(nd->behavior.on_pointer_move_ctx, hit->local.x, hit->local.y);
	if (input->pressed != hit->node) return;
	nd = flux_store_find(input->store, hit->node);
	if (nd && nd->behavior.on_pointer_down)
		nd->behavior.on_pointer_down(nd->behavior.on_pointer_down_ctx, hit->local.x, hit->local.y, input->click_count);
}

void input_blur_focused(FluxInput *input) {
	if (input->focused == XENT_NODE_INVALID) return;

	XentNodeId    old_id = input->focused;
	FluxNodeData *old    = flux_store_find(input->store, old_id);
	input->focused       = XENT_NODE_INVALID;
	if (!old) return;

	old->state.focused        = 0;
	old->state.suppress_focus = 0;
	if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
}

static void input_focus_node(FluxInput *input, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, node);
	if (!nd) return;

	FluxControlType ct = flux_node_type(input->store, node);
	if (ct == FLUX_CONTROL_TEXT_INPUT) {
		nd->state.focused        = 1;
		nd->state.suppress_focus = 0;
	}
	input->focused = node;
	if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
}

static void input_update_focus_from_hit(FluxInput *input, XentNodeId node) {
	if (node == input->focused) return;

	input_blur_focused(input);
	input_focus_node(input, node);
}

static void input_cancel_pressed(FluxInput *input) {
	if (input->pressed == XENT_NODE_INVALID) return;

	XentNodeId    node = input->pressed;
	FluxNodeData *nd   = flux_store_find(input->store, node);
	input->pressed     = XENT_NODE_INVALID;
	if (!nd) return;
	nd->state.pressed = 0;
	if (input->hovered == node) {
		input->hovered    = XENT_NODE_INVALID;
		nd->state.hovered = 0;
		nd->hover_local_x = -1.0f;
		nd->hover_local_y = -1.0f;
	}
	if (nd->behavior.on_cancel) nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
}

static void input_activate_node(FluxInput *input, XentNodeId node, FluxNodeData *nd) {
	XtkHost *host = flux_store_host(input->store);
	if (host) {
		flux_input_route_activation(host, input->store, node, XTK_EVENT_CAUSE_USER);
		return;
	}
	if (nd->behavior.on_click) nd->behavior.on_click(nd->behavior.on_click_ctx);
}

static void
input_release_pressed_node(FluxInput *input, XentNodeId node, FluxNodeData *nd, XentNodeId root, float px, float py) {
	if (!nd) return;

	nd->state.pressed = 0;
	if (flux_node_type(input->store, node) == FLUX_CONTROL_PASSWORD_BOX) xent_sem_setchecked(input->ctx, node, 0);

	/* The release must land on the same interactive target the press did —
	 * resolved through plain content, so releasing over a row's text still
	 * counts as releasing on the row. */
	FluxHitResult hit    = input_hit_test_root(input, root, px, py);
	FluxHitResult target = input_resolve_interactive(input, &hit);
	flux_nav_trace_log(
	  "input.release", "pressed=%llu raw_hit=%llu target=%llu match=%d", ( unsigned long long ) node,
	  ( unsigned long long ) hit.node, ( unsigned long long ) target.node, target.node == node
	);
	if (target.node == node) {
		input_activate_node(input, node, nd);
		return;
	}

	if (nd->behavior.on_cancel) nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
}

static void input_release_pressed(FluxInput *input, XentNodeId root, float px, float py) {
	if (input->pressed == XENT_NODE_INVALID) return;

	XentNodeId    node = input->pressed;
	FluxNodeData *nd   = flux_store_find(input->store, node);
	input->pressed     = XENT_NODE_INVALID;
	input_release_pressed_node(input, node, nd, root, px, py);
}

static void input_clear_touch_hover(FluxInput *input) {
	if (input->pointer_type != FLUX_POINTER_TOUCH) return;
	input_clear_hovered(input);
}

static FluxPoint input_context_local_point(FluxInput *input, FluxHitResult const *hit, XentNodeId node) {
	if (node == hit->node) return hit->local;

	XentRect hit_layout  = {0};
	XentRect node_layout = {0};
	xent_layout_rect(input->ctx, hit->node, &hit_layout);
	xent_layout_rect(input->ctx, node, &node_layout);

	return (FluxPoint) {
	  .x = hit->local.x + hit_layout.x - node_layout.x,
	  .y = hit->local.y + hit_layout.y - node_layout.y,
	};
}

static bool input_dispatch_context_menu_node(FluxInput *input, FluxHitResult const *hit, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(input->store, node);
	if (!nd || !nd->behavior.on_context_menu) return false;

	FluxPoint local = input_context_local_point(input, hit, node);
	nd->behavior.on_context_menu(nd->behavior.on_context_menu_ctx, local.x, local.y);
	return true;
}

FluxInput *flux_input_create(XentCtx *ctx, FluxStore *store) {
	if (!ctx || !store) return NULL;
	FluxInput *input = ( FluxInput * ) calloc(1, sizeof(*input));
	if (!input) return NULL;
	input->ctx                  = ctx;
	input->store                = store;
	input->hovered              = XENT_NODE_INVALID;
	input->pressed              = XENT_NODE_INVALID;
	input->focused              = XENT_NODE_INVALID;
	input->click_count          = 0;
	input->last_click_node      = XENT_NODE_INVALID;
	input->touch.target         = XENT_NODE_INVALID;
	input->refresh_pull.refresh = XENT_NODE_INVALID;
	input->refresh_pull.scroll  = XENT_NODE_INVALID;
	input->scroll_drag.node     = XENT_NODE_INVALID;
	input->scroll_drag.axis     = 0;
	input->scroll_hover_target  = XENT_NODE_INVALID;
	return input;
}

void          flux_input_destroy(FluxInput *input) { free(input); }

FluxHitResult flux_input_hittest(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return (FluxHitResult) {0};
	return input_hit_test_root(input, root, px, py);
}

static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_down(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py);
static void fi_context_menu(FluxInput *input, XentNodeId root, float px, float py);
static void fi_touch_hold(FluxInput *input, XentNodeId root, float px, float py);
static void fi_scroll(InputScrollEvent const *event);

static void fi_cancel(FluxInput *input) {
	if (!input) return;

	input_finish_scroll_drag(input);
	input_cancel_refresh_pull(input);
	input->touch.active = false;
	input->touch.target = XENT_NODE_INVALID;
	input_cancel_pressed(input);
}

/* Middle-button release fires on_middle_click on the node under the pointer
 * (WinUI: TabViewItem closes on MiddleButtonReleased). No press tracking: the
 * release must simply land on a node that opted in. */
static void input_dispatch_middle_button(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev) {
	if (ev->kind != FLUX_POINTER_UP) return;
	FluxHitResult raw = input_hit_test_root(input, root, ev->x, ev->y);
	FluxHitResult hit = input_resolve_interactive(input, &raw);
	if (!hit.data || !hit.data->behavior.on_middle_click) return;
	if (!xent_sem_enabled(input->ctx, hit.node)) return;
	hit.data->behavior.on_middle_click(hit.data->behavior.on_middle_click_ctx);
}

static void input_dispatch_button(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev) {
	if (ev->changed_button == FLUX_POINTER_BUTTON_MIDDLE) {
		input_dispatch_middle_button(input, root, ev);
		return;
	}
	if (ev->changed_button != FLUX_POINTER_BUTTON_LEFT) return;
	if (ev->kind == FLUX_POINTER_DOWN) fi_pointer_down(input, root, ev->x, ev->y);
	if (ev->kind == FLUX_POINTER_UP) fi_pointer_up(input, root, ev->x, ev->y);
}

static void input_dispatch_wheel(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev) {
	InputScrollEvent scroll = input_scroll_event_from_pointer(input, root, ev);
	fi_scroll(&scroll);
}

void flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev) {
	if (!input || !ev || root == XENT_NODE_INVALID) return;
	if (!flux_nav_trace_current())
		flux_nav_trace_begin(
		  "input-api kind=%d device=%d pointer=%u x=%.1f y=%.1f", ev->kind, ev->device, ev->pointer_id, ev->x, ev->y
		);

	input->pointer_type  = ev->device;
	input->pointer_id    = ev->pointer_id;
	input->pointer       = (FluxPoint) {ev->x, ev->y};
	input->pointer_valid = ev->device == FLUX_POINTER_MOUSE;

	switch (ev->kind) {
	case FLUX_POINTER_DOWN         : input_dispatch_button(input, root, ev); break;
	case FLUX_POINTER_UP           : input_dispatch_button(input, root, ev); break;
	case FLUX_POINTER_MOVE         : fi_pointer_move(input, root, ev->x, ev->y); break;
	case FLUX_POINTER_WHEEL        : input_dispatch_wheel(input, root, ev); break;
	case FLUX_POINTER_HOLD         : fi_touch_hold(input, root, ev->x, ev->y); break;
	case FLUX_POINTER_CONTEXT_MENU : fi_context_menu(input, root, ev->x, ev->y); break;
	case FLUX_POINTER_CANCEL       : fi_cancel(input); break;
	}
}

static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	if (input_update_scroll_drag(input, px, py)) return;

	FluxHitResult hit = input_hit_test_root(input, root, px, py);

	flux_scroll_update_hover(input, hit.node, px, py);
	if (input_drive_refresh_pull(input, px, py)) return;
	if (input_drive_touch_pan(input, px, py)) return;

	FluxHitResult target = input_resolve_interactive(input, &hit);
	input_update_hover_from_hit(input, &target, px, py);
	input_forward_pressed_move(input, px, py);
}

static void fi_pointer_down(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	FluxHitResult hit = input_hit_test_root(input, root, px, py);

	/* Scrollbar chrome and touch panning key off the raw (deepest) hit... */
	if (input_press_scroll_overlay(input, &hit, px, py)) return;
	if (input_press_number_box_spin(input, &hit)) return;
	input_setup_touch_pan(input, &hit, px, py);
	input_setup_refresh_pull(input, &hit, px, py);

	/* ...while press/click/focus route to the nearest interactive ancestor
	 * (a click on a row's text belongs to the row). */
	FluxHitResult target      = input_resolve_interactive(input, &hit);
	FluxNodeData *target_data = flux_store_find(input->store, target.node);
	flux_nav_trace_log(
	  "input.press", "raw_hit=%llu target=%llu type=%d x=%.1f y=%.1f", ( unsigned long long ) hit.node,
	  ( unsigned long long ) target.node, target_data ? target_data->component_type : -1, px, py
	);
	input_update_click_count(input, &target, px, py);
	input_press_hit(input, &target);
	input_update_focus_from_hit(input, target.node);
}

static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	if (input_finish_scroll_drag(input)) return;
	if (input_release_scroll_buttons(input)) return;
	if (input_finish_refresh_pull(input)) return;
	if (input_finish_touch_pan(input)) return;

	input->touch.target = XENT_NODE_INVALID;
	input_release_pressed(input, root, px, py);
	input_clear_touch_hover(input);
}

static void input_drop_scroll_refs(FluxInput *input, XentNodeId node) {
	if (input->scroll_drag.node == node) input->scroll_drag = (FluxScrollDrag) {.node = XENT_NODE_INVALID};
	if (input->touch.target == node) flux_input_clearpan(input);
	if (input->refresh_pull.refresh == node || input->refresh_pull.scroll == node) input_cancel_refresh_pull(input);
}

/* The node is already dead in xent and its behavior state is being freed, so
 * references are dropped without firing blur/hover-changed. The
 * DManip viewport is released here because the input boundary owns it. */
void flux_input_dropnode(FluxInput *input, XentNodeId node) {
	if (!input || node == XENT_NODE_INVALID) return;

	flux_dmanip_vp_release(input->store, node);

	if (input->hovered == node) input->hovered = XENT_NODE_INVALID;
	if (input->pressed == node) input->pressed = XENT_NODE_INVALID;
	if (input->focused == node) input->focused = XENT_NODE_INVALID;
	if (input->last_click_node == node) input->last_click_node = XENT_NODE_INVALID;
	if (input->scroll_hover_target == node) input->scroll_hover_target = XENT_NODE_INVALID;
	input_drop_scroll_refs(input, node);
	if (input->modal_root == node) {
		input->modal_root       = XENT_NODE_INVALID;
		input->modal_escape     = NULL;
		input->modal_escape_ctx = NULL;
	}
}

FluxPointerType flux_input_pointertype(FluxInput const *input) {
	return input ? input->pointer_type : FLUX_POINTER_MOUSE;
}

uint32_t   flux_input_pointerid(FluxInput const *input) { return input ? input->pointer_id : 0; }

XentNodeId flux_input_focus(FluxInput const *input) { return input ? input->focused : XENT_NODE_INVALID; }

XentNodeId flux_input_hover(FluxInput const *input) { return input ? input->hovered : XENT_NODE_INVALID; }

XentNodeId flux_input_pressed(FluxInput const *input) { return input ? input->pressed : XENT_NODE_INVALID; }

XentNodeId flux_input_pantarget(FluxInput const *input) { return input ? input->touch.target : XENT_NODE_INVALID; }

void       flux_input_clearpan(FluxInput *input) {
	if (!input) return;
	input->touch.target = XENT_NODE_INVALID;
	input->touch.active = false;
}

bool flux_input_takepan(FluxInput *input) {
	if (!input) return false;
	bool v                     = input->touch.just_promoted;
	input->touch.just_promoted = false;
	return v;
}

bool flux_input_keydown(FluxInput *input, FluxKey key) {
	if (!input || input->focused == XENT_NODE_INVALID) return false;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (!nd || !nd->behavior.on_key) return false;
	return nd->behavior.on_key(nd->behavior.on_key_ctx, key, true);
}

void flux_input_keyup(FluxInput *input, FluxKey key) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (nd && nd->behavior.on_key) nd->behavior.on_key(nd->behavior.on_key_ctx, key, false);
}

void flux_input_char(FluxInput *input, wchar_t ch) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (nd && nd->behavior.on_char) nd->behavior.on_char(nd->behavior.on_char_ctx, ch);
}

int  flux_input_clicks(FluxInput const *input) { return input ? input->click_count : 1; }

void flux_input_ime(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (nd && nd->behavior.on_ime_composition)
		nd->behavior.on_ime_composition(nd->behavior.on_ime_composition_ctx, text, length, cursor);
}

void flux_input_imeend(FluxInput *input) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_store_find(input->store, input->focused);
	if (nd && nd->behavior.on_ime_composition)
		nd->behavior.on_ime_composition(nd->behavior.on_ime_composition_ctx, NULL, 0, 0);
}

static void fi_context_menu(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	FluxHitResult hit  = input_hit_test_root(input, root, px, py);

	XentNodeId    node = hit.node;
	while (node != XENT_NODE_INVALID) {
		if (input_dispatch_context_menu_node(input, &hit, node)) return;
		node = xent_node_parent(input->ctx, node);
	}
}

static bool input_has_context_menu(FluxInput *input, XentNodeId root, float px, float py) {
	FluxHitResult hit = input_hit_test_root(input, root, px, py);
	for (XentNodeId node = hit.node; node != XENT_NODE_INVALID; node = xent_node_parent(input->ctx, node)) {
		FluxNodeData *nd = flux_store_find(input->store, node);
		if (nd && nd->behavior.on_context_menu) return true;
	}
	return false;
}

static void fi_touch_hold(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;
	if (!input_has_context_menu(input, root, px, py)) return;
	input_cancel_pressed(input);
	fi_context_menu(input, root, px, py);
}

static void fi_scroll(InputScrollEvent const *event) {
	if (!event || !event->input || event->root == XENT_NODE_INVALID) return;

	FluxInput    *input = event->input;
	FluxHitResult hit   = input_hit_test_root(input, event->root, event->point.x, event->point.y);
	if (hit.node == XENT_NODE_INVALID) return;

	float      remaining_x = event->wheel.x * FLUX_SCROLLBAR_SMALL_CHANGE;
	float      remaining_y = -event->wheel.y * FLUX_SCROLLBAR_SMALL_CHANGE;
	XentNodeId node        = hit.node;

	while (node != XENT_NODE_INVALID) {
		if (input_handle_number_box_wheel(input, node, event->wheel.y)) return;
		if (input_handle_flip_view_wheel(input, node, event->wheel.y)) return;
		if (input_route_scroll_node(input, node, &remaining_x, &remaining_y)) return;
		node = xent_node_parent(input->ctx, node);
	}
}
