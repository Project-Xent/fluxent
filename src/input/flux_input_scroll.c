#include "flux_input_internal.h"

#include <windows.h>

#define FLUX_TOUCH_PAN_THRESHOLD 10.0f

typedef struct InputScrollHit {
	XentNodeId        node;
	FluxScrollData   *data;
	FluxRect          bounds;
	FluxScrollBarGeom geom;
} InputScrollHit;

typedef struct InputScrollAxis {
	int             axis;
	bool            has_bar;
	FluxRect const *bar;
	FluxRect const *dec_btn;
	FluxRect const *inc_btn;
	FluxRect const *track;
	FluxRect const *thumb;
	float          *scroll;
	uint8_t        *dec_pressed;
	uint8_t        *inc_pressed;
	float           pointer;
	float           page_delta;
	float           max_scroll;
} InputScrollAxis;

typedef struct InputScrollDelta {
	FluxScrollData *data;
	FluxRect const *bounds;
	float          *scroll;
	float           delta;
	uint8_t        *pressed;
} InputScrollDelta;

static double flux_now_seconds(void) {
	LARGE_INTEGER f, c;
	if (!QueryPerformanceFrequency(&f) || !QueryPerformanceCounter(&c)) return 0.0;
	return ( double ) c.QuadPart / ( double ) f.QuadPart;
}

static float input_clampf(float value, float min_value, float max_value) {
	if (value < min_value) return min_value;
	if (value > max_value) return max_value;
	return value;
}

static FluxScrollData *input_scroll_data(FluxInput *input, XentNodeId node) {
	FluxNodeData *nd = flux_node_store_get(input->store, node);
	if (!nd || !nd->component_data) return NULL;
	return ( FluxScrollData * ) nd->component_data;
}

static void input_apply_parent_scroll_offset(FluxInput *input, XentNodeId parent, float *ox, float *oy) {
	if (xent_get_control_type(input->ctx, parent) != XENT_CONTROL_SCROLL) return;

	FluxScrollData *sd = input_scroll_data(input, parent);
	if (!sd) return;

	*ox -= sd->scroll_x;
	*oy -= sd->scroll_y;
}

static void flux_node_abs_rect(FluxInput *input, XentNodeId node, FluxRect *out) {
	XentRect lr = {0};
	xent_get_layout_rect(input->ctx, node, &lr);
	float      ox = lr.x, oy = lr.y;
	XentNodeId p = xent_get_parent(input->ctx, node);

	while (p != XENT_NODE_INVALID) {
		input_apply_parent_scroll_offset(input, p, &ox, &oy);
		p = xent_get_parent(input->ctx, p);
	}

	out->x = ox;
	out->y = oy;
	out->w = lr.width;
	out->h = lr.height;
}

static void flux_scroll_clamp(FluxScrollData *sd, FluxRect const *b) {
	float max_x = sd->content_w - b->w;
	if (max_x < 0.0f) max_x = 0.0f;
	float max_y = sd->content_h - b->h;
	if (max_y < 0.0f) max_y = 0.0f;
	if (sd->scroll_x < 0.0f) sd->scroll_x = 0.0f;
	if (sd->scroll_x > max_x) sd->scroll_x = max_x;
	if (sd->scroll_y < 0.0f) sd->scroll_y = 0.0f;
	if (sd->scroll_y > max_y) sd->scroll_y = max_y;
}

static bool input_scroll_overlay_hit(FluxInput *input, XentNodeId node, float px, float py, InputScrollHit *out) {
	if (xent_get_control_type(input->ctx, node) != XENT_CONTROL_SCROLL) return false;

	FluxScrollData *sd = input_scroll_data(input, node);
	if (!sd) return false;

	FluxRect bounds;
	flux_node_abs_rect(input, node, &bounds);

	FluxScrollBarGeom   geom;
	FluxScrollGeomInput geom_input = {&bounds, sd->content_w, sd->content_h, sd->scroll_x, sd->scroll_y, 1.0f};
	flux_scroll_geom_compute(&geom, &geom_input);

	bool in_v = geom.has_v && sd->v_vis != FLUX_SCROLL_NEVER && flux_rect_contains(&geom.v_bar, px, py);
	bool in_h = geom.has_h && sd->h_vis != FLUX_SCROLL_NEVER && flux_rect_contains(&geom.h_bar, px, py);
	if (!in_v && !in_h) return false;

	*out = (InputScrollHit) {.node = node, .data = sd, .bounds = bounds, .geom = geom};
	return true;
}

static bool
input_find_scroll_at_pointer(FluxInput *input, XentNodeId hit_node, float px, float py, InputScrollHit *out) {
	XentNodeId node = hit_node;
	while (node != XENT_NODE_INVALID) {
		if (input_scroll_overlay_hit(input, node, px, py, out)) return true;
		node = xent_get_parent(input->ctx, node);
	}
	return false;
}

static bool input_scroll_drag_active(FluxInput const *input) {
	return input->scroll_drag_axis != 0 && input->scroll_drag_node != XENT_NODE_INVALID;
}

static void input_scroll_drag_reset(FluxInput *input) {
	input->scroll_drag_node = XENT_NODE_INVALID;
	input->scroll_drag_axis = 0;
}

bool input_update_scroll_drag(FluxInput *input, float px, float py) {
	if (!input_scroll_drag_active(input)) return false;

	FluxScrollData *sd = input_scroll_data(input, input->scroll_drag_node);
	if (!sd) return true;

	float travel = input->scroll_drag_track_len - input->scroll_drag_thumb_len;
	if (travel <= 0.5f || input->scroll_drag_max_scroll <= 0.0f) return true;

	float pointer = input->scroll_drag_axis == 1 ? py : px;
	float delta   = pointer - input->scroll_drag_pointer_start;
	float pct     = delta / travel;
	float target  = input->scroll_drag_scroll_start + pct * input->scroll_drag_max_scroll;
	target        = input_clampf(target, 0.0f, input->scroll_drag_max_scroll);

	if (input->scroll_drag_axis == 1) sd->scroll_y = target;
	else sd->scroll_x = target;
	sd->last_activity_time = flux_now_seconds();
	return true;
}

static void input_clear_pressed_for_pan(FluxInput *input) {
	if (input->pressed == XENT_NODE_INVALID) return;

	FluxNodeData *pnd = flux_node_store_get(input->store, input->pressed);
	if (pnd) {
		pnd->state.pressed = 0;
		input_clear_node_hover(pnd);
	}

	if (input->hovered == input->pressed) input->hovered = XENT_NODE_INVALID;
	input->pressed = XENT_NODE_INVALID;
}

static bool input_should_promote_touch_pan(FluxInput *input, float px, float py) {
	if (input->touch_pan_active) return false;

	float tdx = px - input->touch_start_x;
	float tdy = py - input->touch_start_y;
	return tdx * tdx + tdy * tdy >= FLUX_TOUCH_PAN_THRESHOLD * FLUX_TOUCH_PAN_THRESHOLD;
}

static void input_promote_touch_pan(FluxInput *input, float px, float py) {
	if (!input_should_promote_touch_pan(input, px, py)) return;

	input_clear_pressed_for_pan(input);
	input->touch_pan_active  = true;
	input->pan_just_promoted = true;
}

bool input_drive_touch_pan(FluxInput *input, float px, float py) {
	if (input->pointer_type != FLUX_POINTER_TOUCH || input->touch_pan_target == XENT_NODE_INVALID) return false;

	input_promote_touch_pan(input, px, py);
	if (!input->touch_pan_active) return false;

	FluxScrollData *sd = input_scroll_data(input, input->touch_pan_target);
	if (!sd) return true;

	XentRect srect = {0};
	xent_get_layout_rect(input->ctx, input->touch_pan_target, &srect);

	float max_x = sd->content_w - srect.width;
	if (max_x < 0.0f) max_x = 0.0f;
	float max_y = sd->content_h - srect.height;
	if (max_y < 0.0f) max_y = 0.0f;

	float new_sx           = input->touch_pan_origin_sx - (px - input->touch_start_x);
	float new_sy           = input->touch_pan_origin_sy - (py - input->touch_start_y);
	sd->scroll_x           = input_clampf(new_sx, 0.0f, max_x);
	sd->scroll_y           = input_clampf(new_sy, 0.0f, max_y);
	sd->last_activity_time = flux_now_seconds();
	return true;
}

static bool input_press_scroll_delta(FluxInput *input, InputScrollDelta const *event) {
	*event->scroll += event->delta;
	if (event->pressed) *event->pressed = 1;
	event->data->last_activity_time = flux_now_seconds();
	flux_scroll_clamp(event->data, event->bounds);
	input->pressed = XENT_NODE_INVALID;
	return true;
}

static bool input_start_scroll_drag(FluxInput *input, InputScrollHit const *hit, InputScrollAxis const *axis) {
	input->scroll_drag_node          = hit->node;
	input->scroll_drag_axis          = axis->axis;
	input->scroll_drag_pointer_start = axis->pointer;
	input->scroll_drag_scroll_start  = *axis->scroll;
	input->scroll_drag_track_len     = axis->axis == 1 ? axis->track->h : axis->track->w;
	input->scroll_drag_thumb_len     = axis->axis == 1 ? axis->thumb->h : axis->thumb->w;
	input->scroll_drag_max_scroll    = axis->max_scroll < 0.0f ? 0.0f : axis->max_scroll;
	hit->data->drag_axis             = ( uint8_t ) axis->axis;
	hit->data->last_activity_time    = flux_now_seconds();
	input->pressed                   = XENT_NODE_INVALID;
	return true;
}

static InputScrollAxis input_vertical_scroll_axis(InputScrollHit const *hit, float py) {
	return (InputScrollAxis) {
	  .axis        = 1,
	  .has_bar     = hit->geom.has_v,
	  .bar         = &hit->geom.v_bar,
	  .dec_btn     = &hit->geom.v_up_btn,
	  .inc_btn     = &hit->geom.v_dn_btn,
	  .track       = &hit->geom.v_track,
	  .thumb       = &hit->geom.v_thumb,
	  .scroll      = &hit->data->scroll_y,
	  .dec_pressed = &hit->data->v_up_pressed,
	  .inc_pressed = &hit->data->v_dn_pressed,
	  .pointer     = py,
	  .page_delta  = hit->bounds.h,
	  .max_scroll  = hit->data->content_h - hit->bounds.h,
	};
}

static InputScrollAxis input_horizontal_scroll_axis(InputScrollHit const *hit, float px) {
	return (InputScrollAxis) {
	  .axis        = 2,
	  .has_bar     = hit->geom.has_h,
	  .bar         = &hit->geom.h_bar,
	  .dec_btn     = &hit->geom.h_lf_btn,
	  .inc_btn     = &hit->geom.h_rg_btn,
	  .track       = &hit->geom.h_track,
	  .thumb       = &hit->geom.h_thumb,
	  .scroll      = &hit->data->scroll_x,
	  .dec_pressed = &hit->data->h_lf_pressed,
	  .inc_pressed = &hit->data->h_rg_pressed,
	  .pointer     = px,
	  .page_delta  = hit->bounds.w,
	  .max_scroll  = hit->data->content_w - hit->bounds.w,
	};
}

static bool input_press_scrollbar_axis(FluxInput *input, InputScrollHit const *hit, float px, float py, int axis_id) {
	InputScrollAxis axis = axis_id == 1 ? input_vertical_scroll_axis(hit, py) : input_horizontal_scroll_axis(hit, px);
	if (!axis.has_bar || !flux_rect_contains(axis.bar, px, py)) return false;
	if (flux_rect_contains(axis.dec_btn, px, py))
		return input_press_scroll_delta(
		  input,
		  &(InputScrollDelta) {hit->data, &hit->bounds, axis.scroll, -FLUX_SCROLLBAR_SMALL_CHANGE, axis.dec_pressed}
		);
	if (flux_rect_contains(axis.inc_btn, px, py))
		return input_press_scroll_delta(
		  input,
		  &(InputScrollDelta) {hit->data, &hit->bounds, axis.scroll, FLUX_SCROLLBAR_SMALL_CHANGE, axis.inc_pressed}
		);
	if (flux_rect_contains(axis.thumb, px, py)) return input_start_scroll_drag(input, hit, &axis);
	if (!flux_rect_contains(axis.track, px, py)) return false;

	float thumb_start = axis.axis == 1 ? axis.thumb->y : axis.thumb->x;
	float delta       = axis.pointer < thumb_start ? -axis.page_delta : axis.page_delta;
	return input_press_scroll_delta(input, &(InputScrollDelta) {hit->data, &hit->bounds, axis.scroll, delta, NULL});
}

bool input_press_scroll_overlay(FluxInput *input, FluxHitResult const *hit, float px, float py) {
	if (input->pointer_type == FLUX_POINTER_TOUCH || hit->node == XENT_NODE_INVALID) return false;

	InputScrollHit scroll_hit;
	if (!input_find_scroll_at_pointer(input, hit->node, px, py, &scroll_hit)) return false;

	if (input_press_scrollbar_axis(input, &scroll_hit, px, py, 1)) return true;
	return input_press_scrollbar_axis(input, &scroll_hit, px, py, 2);
}

static bool input_touch_pan_blocked_by_control(XentControlType ct) {
	return ct == XENT_CONTROL_SLIDER
	    || ct == XENT_CONTROL_SWITCH
	    || ct == XENT_CONTROL_TEXT_INPUT
	    || ct == XENT_CONTROL_PASSWORD_BOX
	    || ct == XENT_CONTROL_NUMBER_BOX;
}

static bool input_assign_touch_pan_target(FluxInput *input, XentNodeId node) {
	FluxScrollData *sd = input_scroll_data(input, node);
	if (!sd) return false;

	input->touch_pan_target    = node;
	input->touch_pan_origin_sx = sd->scroll_x;
	input->touch_pan_origin_sy = sd->scroll_y;
	return true;
}

bool input_setup_touch_pan(FluxInput *input, FluxHitResult const *hit, float px, float py) {
	input->touch_pan_target = XENT_NODE_INVALID;
	input->touch_pan_active = false;
	input->touch_start_x    = px;
	input->touch_start_y    = py;

	if (input->pointer_type != FLUX_POINTER_TOUCH || hit->node == XENT_NODE_INVALID) return false;

	XentNodeId node = hit->node;
	while (node != XENT_NODE_INVALID) {
		XentControlType ct = xent_get_control_type(input->ctx, node);
		if (input_touch_pan_blocked_by_control(ct)) return false;
		if (ct == XENT_CONTROL_SCROLL) return input_assign_touch_pan_target(input, node);
		node = xent_get_parent(input->ctx, node);
	}
	return false;
}

bool input_finish_scroll_drag(FluxInput *input) {
	if (!input_scroll_drag_active(input)) return false;

	FluxScrollData *sd = input_scroll_data(input, input->scroll_drag_node);
	if (sd) {
		sd->drag_axis          = 0;
		sd->last_activity_time = flux_now_seconds();
	}

	input_scroll_drag_reset(input);
	return true;
}

bool input_release_scroll_buttons(FluxInput *input) {
	if (input->scroll_hover_target == XENT_NODE_INVALID) return false;

	FluxScrollData *sd = input_scroll_data(input, input->scroll_hover_target);
	if (!sd) return false;
	if (!sd->v_up_pressed && !sd->v_dn_pressed && !sd->h_lf_pressed && !sd->h_rg_pressed) return false;

	sd->v_up_pressed       = 0;
	sd->v_dn_pressed       = 0;
	sd->h_lf_pressed       = 0;
	sd->h_rg_pressed       = 0;
	sd->last_activity_time = flux_now_seconds();
	return true;
}

bool input_finish_touch_pan(FluxInput *input) {
	if (!input->touch_pan_active) return false;

	input->touch_pan_active = false;
	input->touch_pan_target = XENT_NODE_INVALID;
	input->pressed          = XENT_NODE_INVALID;
	if (input->pointer_type == FLUX_POINTER_TOUCH) input_clear_hovered(input);
	return true;
}

static void input_dispatch_number_box_wheel(FluxNodeData *nd, float delta_y) {
	if (!nd || !nd->state.focused || !nd->behavior.on_key) return;
	if (delta_y > 0.0f) nd->behavior.on_key(nd->behavior.on_key_ctx, VK_UP, true);
	if (delta_y < 0.0f) nd->behavior.on_key(nd->behavior.on_key_ctx, VK_DOWN, true);
}

bool input_handle_number_box_wheel(FluxInput *input, XentNodeId node, float delta_y) {
	if (xent_get_control_type(input->ctx, node) != XENT_CONTROL_NUMBER_BOX) return false;

	FluxNodeData *nd = flux_node_store_get(input->store, node);
	input_dispatch_number_box_wheel(nd, delta_y);
	return true;
}

static float input_scroll_axis_max(float content, float viewport) {
	float max_scroll = content - viewport;
	return max_scroll > 0.0f ? max_scroll : 0.0f;
}

static bool input_consume_scroll_axis(float *scroll, float *remaining, float max_scroll) {
	if (*remaining == 0.0f || max_scroll <= 0.0f) return false;

	float next      = input_clampf(*scroll + *remaining, 0.0f, max_scroll);
	float consumed  = next - *scroll;
	*scroll         = next;
	*remaining     -= consumed;
	return consumed != 0.0f;
}

bool input_route_scroll_node(FluxInput *input, XentNodeId node, float *remaining_x, float *remaining_y) {
	if (xent_get_control_type(input->ctx, node) != XENT_CONTROL_SCROLL) return false;

	FluxScrollData *sd = input_scroll_data(input, node);
	if (!sd) return false;

	XentRect rect = {0};
	xent_get_layout_rect(input->ctx, node, &rect);

	float max_x = input_scroll_axis_max(sd->content_w, rect.width);
	float max_y = input_scroll_axis_max(sd->content_h, rect.height);
	bool  moved = input_consume_scroll_axis(&sd->scroll_x, remaining_x, max_x);
	moved       = input_consume_scroll_axis(&sd->scroll_y, remaining_y, max_y) || moved;

	if (moved) sd->last_activity_time = flux_now_seconds();
	return *remaining_x == 0.0f && *remaining_y == 0.0f;
}

static XentNodeId input_nearest_scroll_node(FluxInput *input, XentNodeId node) {
	while (node != XENT_NODE_INVALID) {
		if (xent_get_control_type(input->ctx, node) == XENT_CONTROL_SCROLL) return node;
		node = xent_get_parent(input->ctx, node);
	}
	return XENT_NODE_INVALID;
}

static void input_clear_scroll_hover_node(FluxInput *input, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;

	FluxScrollData *sd = input_scroll_data(input, node);
	if (!sd) return;

	sd->mouse_over = 0;
}

static void input_set_scroll_hover_node(FluxInput *input, XentNodeId node, float px, float py) {
	FluxScrollData *sd = input_scroll_data(input, node);
	if (!sd) return;

	FluxRect bounds;
	flux_node_abs_rect(input, node, &bounds);
	sd->mouse_over    = 1;
	sd->mouse_local_x = px - bounds.x;
	sd->mouse_local_y = py - bounds.y;
}

void flux_scroll_update_hover(FluxInput *input, XentNodeId hit_node, float px, float py) {
	XentNodeId target = input_nearest_scroll_node(input, hit_node);

	if (input->scroll_hover_target != target) input_clear_scroll_hover_node(input, input->scroll_hover_target);
	input->scroll_hover_target = target;
	if (target != XENT_NODE_INVALID) input_set_scroll_hover_node(input, target, px, py);
}
