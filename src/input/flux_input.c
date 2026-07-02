#include "flux_input_internal.h"
#include "flux_dmanip_sync.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

typedef struct InputHitQuery {
	XentContext *ctx;
	XentNodeId   node;
	FluxPoint    point;
	FluxPoint    scroll;
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

static FluxPoint hit_test_child_scroll_offset(InputHitQuery const *query, FluxNodeData *nd) {
	FluxPoint scroll = query->scroll;
	if (!nd || flux_get_control_type(query->ctx, query->node) != FLUX_CONTROL_SCROLL) return scroll;

	FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
	if (!sd) return scroll;

	scroll.x += sd->scroll_x;
	scroll.y += sd->scroll_y;
	return scroll;
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

static HitChildList hit_collect_children(XentContext *ctx, XentNodeId node) {
	HitChildList children = {0};
	XentNodeId   child    = xent_get_first_child(ctx, node);
	while (child != XENT_NODE_INVALID) {
		if (!hit_child_list_append(&children, child)) break;
		child = xent_get_next_sibling(ctx, child);
	}
	return children;
}

static FluxHitResult hit_test_recursive(InputHitQuery const *query);

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
	xent_get_layout_rect(query->ctx, query->node, &rect);
	if (!hit_rect_contains(&rect, query->point, query->scroll)) return (FluxHitResult) {0};

	FluxNodeData *nd           = ( FluxNodeData * ) xent_get_userdata(query->ctx, query->node);
	FluxPoint     child_scroll = hit_test_child_scroll_offset(query, nd);
	HitChildList  children     = hit_collect_children(query->ctx, query->node);
	FluxHitResult child_hit    = hit_test_children(query, &children, child_scroll);
	free(children.items);
	if (child_hit.node != XENT_NODE_INVALID) return child_hit;

	float ax = rect.x - query->scroll.x;
	float ay = rect.y - query->scroll.y;
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
	if (xent_get_focusable(input->ctx, node)) return true;
	if (!nd) return false;
	FluxNodeBehavior const *b = &nd->behavior;
	return b->on_click || b->on_pointer_down || b->on_middle_click || b->on_hover_changed || nd->tooltip_text;
}

/* Re-base a hit onto ancestor `node`, shifting bounds/local by the layout-rect
 * delta (shared scroll offsets cancel out, so the delta is scroll-correct). */
static FluxHitResult input_rebase_hit(FluxInput *input, FluxHitResult const *hit, XentNodeId node, FluxNodeData *nd) {
	XentRect from = {0}, to = {0};
	xent_get_layout_rect(input->ctx, hit->node, &from);
	xent_get_layout_rect(input->ctx, node, &to);
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
	for (XentNodeId node = hit->node; node != XENT_NODE_INVALID; node = xent_get_parent(input->ctx, node)) {
		FluxNodeData *nd = flux_node_store_get(input->store, node);
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

	FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
	input_clear_node_hover(old);
	input->hovered = XENT_NODE_INVALID;
}

static void input_set_hovered_node(FluxInput *input, XentNodeId node, FluxNodeData *nd) {
	if (node == input->hovered) return;

	input_clear_hovered(input);
	if (node != XENT_NODE_INVALID && nd) {
		nd->state.hovered      = 1;
		nd->state.pointer_type = ( uint8_t ) input->pointer_type;
		if (nd->behavior.on_hover_changed) nd->behavior.on_hover_changed(nd->behavior.on_hover_changed_ctx, true);
	}
	input->hovered = node;
}

static void input_update_hover_local(FluxInput *input, float px, float py) {
	if (input->hovered == XENT_NODE_INVALID) return;

	FluxNodeData *hnd = flux_node_store_get(input->store, input->hovered);
	if (!hnd) return;

	XentRect hrect = {0};
	xent_get_layout_rect(input->ctx, input->hovered, &hrect);
	hnd->hover_local_x      = px - hrect.x;
	hnd->hover_local_y      = py - hrect.y;
	hnd->state.pointer_type = ( uint8_t ) input->pointer_type;
}

static void input_update_hover_from_hit(FluxInput *input, FluxHitResult const *hit, float px, float py) {
	XentNodeId hovered = hit->node;
	if (input->pointer_type == FLUX_POINTER_TOUCH && input->pressed == XENT_NODE_INVALID) hovered = XENT_NODE_INVALID;

	input_set_hovered_node(input, hovered, hit->data);
	input_update_hover_local(input, px, py);
}

static void input_forward_pressed_move(FluxInput *input, float px, float py) {
	if (input->pressed == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
	if (!nd || !nd->behavior.on_pointer_move) return;

	float local_x = px - input->pressed_bounds.x;
	float local_y = py - input->pressed_bounds.y;
	nd->behavior.on_pointer_move(nd->behavior.on_pointer_move_ctx, local_x, local_y);
}

static bool input_press_number_box_spin(FluxInput *input, FluxHitResult const *hit) {
	if (hit->node == XENT_NODE_INVALID || !hit->data) return false;
	if (flux_get_control_type(input->ctx, hit->node) != FLUX_CONTROL_NUMBER_BOX) return false;
	if (!xent_get_semantic_expanded(input->ctx, hit->node)) return false;

	XentRect rect = {0};
	xent_get_layout_rect(input->ctx, hit->node, &rect);

	float spin_start = rect.w - 76.0f;
	if (hit->local.x < spin_start) return false;

	if (hit->data->behavior.on_key) {
		bool up = hit->local.x < spin_start + 40.0f;
		hit->data->behavior.on_key(hit->data->behavior.on_key_ctx, up ? VK_UP : VK_DOWN, true);
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
	if (hit->node == XENT_NODE_INVALID || !hit->data) return;

	/* Single interactive gate: a semantically-disabled node consumes the press but
	 * dispatches nothing (no down/move/click/drag). All input enters through here, so
	 * this is the only place controls need to test for disabled. */
	if (!xent_get_semantic_enabled(input->ctx, hit->node)) return;

	hit->data->state.pressed      = 1;
	hit->data->state.pointer_type = ( uint8_t ) input->pointer_type;
	input->pressed                = hit->node;
	input->pressed_bounds         = hit->bounds;

	input_set_touch_press_hover(input, hit);

	if (hit->data->behavior.on_pointer_move)
		hit->data->behavior.on_pointer_move(hit->data->behavior.on_pointer_move_ctx, hit->local.x, hit->local.y);
	if (hit->data->behavior.on_pointer_down)
		hit->data->behavior.on_pointer_down(
		  hit->data->behavior.on_pointer_down_ctx, hit->local.x, hit->local.y, input->click_count
		);
}

void input_blur_focused(FluxInput *input) {
	if (input->focused == XENT_NODE_INVALID) return;

	FluxNodeData *old = flux_node_store_get(input->store, input->focused);
	if (!old) return;

	old->state.focused = 0;
	if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
}

static void input_focus_hit(FluxInput *input, FluxHitResult const *hit) {
	if (hit->node == XENT_NODE_INVALID || !hit->data) return;

	FluxControlType ct = flux_get_control_type(input->ctx, hit->node);
	if (ct == FLUX_CONTROL_TEXT_INPUT) hit->data->state.focused = 1;
	if (hit->data->behavior.on_focus) hit->data->behavior.on_focus(hit->data->behavior.on_focus_ctx);
}

static void input_update_focus_from_hit(FluxInput *input, FluxHitResult const *hit) {
	if (hit->node == input->focused) return;

	input_blur_focused(input);
	input_focus_hit(input, hit);
	input->focused = hit->node;
}

static void input_cancel_pressed_node(FluxInput *input, FluxNodeData *nd) {
	if (!nd) return;

	nd->state.pressed = 0;
	if (nd->behavior.on_cancel) nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
	if (input->hovered == input->pressed) {
		input_clear_node_hover(nd);
		input->hovered = XENT_NODE_INVALID;
	}
}

static void input_cancel_pressed(FluxInput *input) {
	if (input->pressed == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
	input_cancel_pressed_node(input, nd);
	input->pressed = XENT_NODE_INVALID;
}

static void input_release_pressed_node(FluxInput *input, FluxNodeData *nd, XentNodeId root, float px, float py) {
	if (!nd) return;

	nd->state.pressed = 0;
	if (flux_get_control_type(input->ctx, input->pressed) == FLUX_CONTROL_PASSWORD_BOX)
		xent_set_semantic_checked(input->ctx, input->pressed, 0);

	/* The release must land on the same interactive target the press did —
	 * resolved through plain content, so releasing over a row's text still
	 * counts as releasing on the row. */
	FluxHitResult hit    = input_hit_test_root(input, root, px, py);
	FluxHitResult target = input_resolve_interactive(input, &hit);
	if (target.node == input->pressed) {
		if (nd->behavior.on_click) nd->behavior.on_click(nd->behavior.on_click_ctx);
		return;
	}

	if (nd->behavior.on_cancel) nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
}

static void input_release_pressed(FluxInput *input, XentNodeId root, float px, float py) {
	if (input->pressed == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
	input_release_pressed_node(input, nd, root, px, py);
	input->pressed = XENT_NODE_INVALID;
}

static void input_clear_touch_hover(FluxInput *input) {
	if (input->pointer_type != FLUX_POINTER_TOUCH) return;
	input_clear_hovered(input);
}

static FluxPoint input_context_local_point(FluxInput *input, FluxHitResult const *hit, XentNodeId node) {
	if (node == hit->node) return hit->local;

	XentRect hit_layout  = {0};
	XentRect node_layout = {0};
	xent_get_layout_rect(input->ctx, hit->node, &hit_layout);
	xent_get_layout_rect(input->ctx, node, &node_layout);

	return (FluxPoint) {
	  .x = hit->local.x + hit_layout.x - node_layout.x,
	  .y = hit->local.y + hit_layout.y - node_layout.y,
	};
}

static bool input_dispatch_context_menu_node(FluxInput *input, FluxHitResult const *hit, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(input->store, node);
	if (!nd || !nd->behavior.on_context_menu) return false;

	FluxPoint local = input_context_local_point(input, hit, node);
	nd->behavior.on_context_menu(nd->behavior.on_context_menu_ctx, local.x, local.y);
	return true;
}

FluxInput *flux_input_create(XentContext *ctx, FluxNodeStore *store) {
	if (!ctx || !store) return NULL;
	FluxInput *input = ( FluxInput * ) calloc(1, sizeof(*input));
	if (!input) return NULL;
	input->ctx                 = ctx;
	input->store               = store;
	input->hovered             = XENT_NODE_INVALID;
	input->pressed             = XENT_NODE_INVALID;
	input->focused             = XENT_NODE_INVALID;
	input->click_count         = 0;
	input->last_click_node     = XENT_NODE_INVALID;
	input->touch.target        = XENT_NODE_INVALID;
	input->scroll_drag.node    = XENT_NODE_INVALID;
	input->scroll_drag.axis    = 0;
	input->scroll_hover_target = XENT_NODE_INVALID;
	return input;
}

void          flux_input_destroy(FluxInput *input) { free(input); }

FluxHitResult flux_input_hit_test(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return (FluxHitResult) {0};
	return input_hit_test_root(input, root, px, py);
}

static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_down(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py);
static void fi_context_menu(FluxInput *input, XentNodeId root, float px, float py);
static void fi_scroll(InputScrollEvent const *event);

static void fi_cancel(FluxInput *input) {
	if (!input) return;

	input_finish_scroll_drag(input);
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
	if (!xent_get_semantic_enabled(input->ctx, hit.node)) return;
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

	input->pointer_type = ev->device;
	input->pointer_id   = ev->pointer_id;

	switch (ev->kind) {
	case FLUX_POINTER_DOWN         : input_dispatch_button(input, root, ev); break;
	case FLUX_POINTER_UP           : input_dispatch_button(input, root, ev); break;
	case FLUX_POINTER_MOVE         : fi_pointer_move(input, root, ev->x, ev->y); break;
	case FLUX_POINTER_WHEEL        : input_dispatch_wheel(input, root, ev); break;
	case FLUX_POINTER_CONTEXT_MENU : fi_context_menu(input, root, ev->x, ev->y); break;
	case FLUX_POINTER_CANCEL       : fi_cancel(input); break;
	}
}

static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	if (input_update_scroll_drag(input, px, py)) return;

	FluxHitResult hit = input_hit_test_root(input, root, px, py);

	flux_scroll_update_hover(input, hit.node, px, py);
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

	/* ...while press/click/focus route to the nearest interactive ancestor
	 * (a click on a row's text belongs to the row). */
	FluxHitResult target = input_resolve_interactive(input, &hit);
	input_update_click_count(input, &target, px, py);
	input_press_hit(input, &target);
	input_update_focus_from_hit(input, &target);
}

static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	if (input_finish_scroll_drag(input)) return;
	if (input_release_scroll_buttons(input)) return;
	if (input_finish_touch_pan(input)) return;

	input->touch.target = XENT_NODE_INVALID;
	input_release_pressed(input, root, px, py);
	input_clear_touch_hover(input);
}

/* The node is already dead in xent and its callbacks/userdata are about to be
 * freed, so references are dropped without firing blur/hover-changed. The
 * DManip viewport is released here because the input boundary owns it. */
void flux_input_node_destroyed(FluxInput *input, XentNodeId node) {
	if (!input || node == XENT_NODE_INVALID) return;

	flux_dmanip_release_node_viewport(input->store, node);

	if (input->hovered == node) input->hovered = XENT_NODE_INVALID;
	if (input->pressed == node) input->pressed = XENT_NODE_INVALID;
	if (input->focused == node) input->focused = XENT_NODE_INVALID;
	if (input->last_click_node == node) input->last_click_node = XENT_NODE_INVALID;
	if (input->scroll_hover_target == node) input->scroll_hover_target = XENT_NODE_INVALID;
	if (input->scroll_drag.node == node) input->scroll_drag = (FluxScrollDrag) {.node = XENT_NODE_INVALID};
	if (input->touch.target == node) flux_input_clear_touch_pan(input);
	if (input->modal_root == node) {
		input->modal_root       = XENT_NODE_INVALID;
		input->modal_escape     = NULL;
		input->modal_escape_ctx = NULL;
	}
}

FluxPointerType flux_input_get_pointer_type(FluxInput const *input) {
	return input ? input->pointer_type : FLUX_POINTER_MOUSE;
}

uint32_t   flux_input_get_pointer_id(FluxInput const *input) { return input ? input->pointer_id : 0; }

XentNodeId flux_input_get_focused(FluxInput const *input) { return input ? input->focused : XENT_NODE_INVALID; }

XentNodeId flux_input_get_hovered(FluxInput const *input) { return input ? input->hovered : XENT_NODE_INVALID; }

XentNodeId flux_input_get_pressed(FluxInput const *input) { return input ? input->pressed : XENT_NODE_INVALID; }

XentNodeId flux_input_get_touch_pan_target(FluxInput const *input) {
	return input ? input->touch.target : XENT_NODE_INVALID;
}

void flux_input_clear_touch_pan(FluxInput *input) {
	if (!input) return;
	input->touch.target = XENT_NODE_INVALID;
	input->touch.active = false;
}

bool flux_input_take_pan_promoted(FluxInput *input) {
	if (!input) return false;
	bool v                     = input->touch.just_promoted;
	input->touch.just_promoted = false;
	return v;
}

bool flux_input_key_down(FluxInput *input, unsigned int vk) {
	if (!input || input->focused == XENT_NODE_INVALID) return false;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (!nd || !nd->behavior.on_key) return false;
	return nd->behavior.on_key(nd->behavior.on_key_ctx, vk, true);
}

void flux_input_key_up(FluxInput *input, unsigned int vk) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_key) nd->behavior.on_key(nd->behavior.on_key_ctx, vk, false);
}

void flux_input_char(FluxInput *input, wchar_t ch) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_char) nd->behavior.on_char(nd->behavior.on_char_ctx, ch);
}

int  flux_input_get_click_count(FluxInput const *input) { return input ? input->click_count : 1; }

void flux_input_ime_composition(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_ime_composition)
		nd->behavior.on_ime_composition(nd->behavior.on_ime_composition_ctx, text, length, cursor);
}

void flux_input_ime_end(FluxInput *input) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_ime_composition)
		nd->behavior.on_ime_composition(nd->behavior.on_ime_composition_ctx, NULL, 0, 0);
}

static void fi_context_menu(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	FluxHitResult hit  = input_hit_test_root(input, root, px, py);

	XentNodeId    node = hit.node;
	while (node != XENT_NODE_INVALID) {
		if (input_dispatch_context_menu_node(input, &hit, node)) return;
		node = xent_get_parent(input->ctx, node);
	}
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
		node = xent_get_parent(input->ctx, node);
	}
}
