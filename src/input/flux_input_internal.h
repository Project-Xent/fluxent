#ifndef FLUX_INPUT_INTERNAL_H
#define FLUX_INPUT_INTERNAL_H

#include "fluxent/flux_input.h"
#include "store/flux_node_data.h"
#include "render/flux_scroll_geom.h"

#include <windows.h>

typedef struct FluxScrollDrag {
	XentNodeId node;
	int        axis;
	float      pointer_start;
	float      scroll_start;
	float      track_len;
	float      thumb_len;
	float      max_scroll;
} FluxScrollDrag;

typedef struct FluxTouchPan {
	XentNodeId target;
	float      start_x, start_y;
	float      origin_sx, origin_sy;
	bool       active;
	bool       just_promoted;
} FluxTouchPan;

typedef struct FluxRefreshPull {
	XentNodeId refresh;
	XentNodeId scroll;
	float      start_x, start_y;
	float      origin_sx, origin_sy;
	bool       active;
} FluxRefreshPull;

typedef enum FluxScrollPhase
{
	FLUX_SCROLL_IDLE,
	FLUX_SCROLL_THUMB_DRAG,
	FLUX_SCROLL_TOUCH_PENDING,
	FLUX_SCROLL_TOUCH_PAN,
} FluxScrollPhase;

struct FluxInput {
	XentCtx        *ctx;
	FluxStore      *store;
	XentNodeId      hovered;
	XentNodeId      pressed;
	XentNodeId      key_pressed;
	FluxKey         key_press_key;
	XentNodeId      focused;
	FluxRect        pressed_bounds;
	int             click_count;
	ULONGLONG       last_click_time;
	float           last_click_x;
	float           last_click_y;
	XentNodeId      last_click_node;
	FluxPointerType pointer_type;
	uint32_t        pointer_id;
	FluxPoint       pointer;
	bool            pointer_valid;
	FluxScrollDrag  scroll_drag;
	FluxTouchPan    touch;
	FluxRefreshPull refresh_pull;
	XentNodeId      scroll_hover_target;

	XentNodeId      modal_root;
	void            (*modal_escape)(void *ctx);
	void           *modal_escape_ctx;
};

void            input_blur_focused(FluxInput *input);
void            input_clear_node_hover(FluxNodeData *nd);
void            input_clear_hovered(FluxInput *input);

bool            input_update_scroll_drag(FluxInput *input, float px, float py);
bool            input_drive_touch_pan(FluxInput *input, float px, float py);
bool            input_press_scroll_overlay(FluxInput *input, FluxHitResult const *hit, float px, float py);
bool            input_setup_touch_pan(FluxInput *input, FluxHitResult const *hit, float px, float py);
bool            input_setup_refresh_pull(FluxInput *input, FluxHitResult const *hit, float px, float py);
bool            input_drive_refresh_pull(FluxInput *input, float px, float py);
bool            input_finish_refresh_pull(FluxInput *input);
void            input_cancel_refresh_pull(FluxInput *input);
bool            input_finish_scroll_drag(FluxInput *input);
bool            input_release_scroll_buttons(FluxInput *input);
bool            input_finish_touch_pan(FluxInput *input);
bool            input_handle_number_box_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_handle_combo_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_handle_flip_view_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_handle_calendar_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_handle_textbox_wheel(FluxInput *input, XentNodeId node, float *remaining_y);
bool            input_route_scroll_node(FluxInput *input, XentNodeId node, float *remaining_x, float *remaining_y);
void            flux_scroll_update_hover(FluxInput *input, XentNodeId hit_node, float px, float py);

void            input_node_abs_rect(FluxInput const *input, XentNodeId node, FluxRect *out);

FluxScrollPhase flux_input_scroll_phase(FluxInput const *input);

#endif
