#ifndef FLUX_INPUT_INTERNAL_H
#define FLUX_INPUT_INTERNAL_H

#include "fluxent/flux_input.h"
#include "render/flux_scroll_geom.h"

struct FluxInput {
	XentContext    *ctx;
	FluxNodeStore  *store;
	XentNodeId      hovered;
	XentNodeId      pressed;
	XentNodeId      focused;
	FluxRect        pressed_bounds;
	int             click_count;
	ULONGLONG       last_click_time;
	float           last_click_x;
	float           last_click_y;
	XentNodeId      last_click_node;
	FluxPointerType pointer_type;
	uint32_t        pointer_id;
	XentNodeId      touch_pan_target;
	float           touch_start_x;
	float           touch_start_y;
	float           touch_pan_origin_sx;
	float           touch_pan_origin_sy;
	bool            touch_pan_active;
	bool            pan_just_promoted;
	XentNodeId      scroll_drag_node;
	int             scroll_drag_axis;
	float           scroll_drag_pointer_start;
	float           scroll_drag_scroll_start;
	float           scroll_drag_track_len;
	float           scroll_drag_thumb_len;
	float           scroll_drag_max_scroll;
	XentNodeId      scroll_hover_target;
};

void input_blur_focused(FluxInput *input);
void input_clear_node_hover(FluxNodeData *nd);
void input_clear_hovered(FluxInput *input);

bool input_update_scroll_drag(FluxInput *input, float px, float py);
bool input_drive_touch_pan(FluxInput *input, float px, float py);
bool input_press_scroll_overlay(FluxInput *input, FluxHitResult const *hit, float px, float py);
bool input_setup_touch_pan(FluxInput *input, FluxHitResult const *hit, float px, float py);
bool input_finish_scroll_drag(FluxInput *input);
bool input_release_scroll_buttons(FluxInput *input);
bool input_finish_touch_pan(FluxInput *input);
bool input_handle_number_box_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool input_route_scroll_node(FluxInput *input, XentNodeId node, float *remaining_x, float *remaining_y);
void flux_scroll_update_hover(FluxInput *input, XentNodeId hit_node, float px, float py);

#endif
