#ifndef FLUX_INPUT_INTERNAL_H
#define FLUX_INPUT_INTERNAL_H

#include "fluxent/flux_input.h"
#include "render/flux_scroll_geom.h"

/** @brief Mouse scrollbar thumb-drag session (active when axis != 0). */
typedef struct FluxScrollDrag {
	XentNodeId node;          /**< Scroll node whose thumb is being dragged. */
	int        axis;          /**< 0 = none, 1 = vertical, 2 = horizontal. */
	float      pointer_start; /**< Pointer coord (along axis) at drag start. */
	float      scroll_start;  /**< Scroll offset at drag start. */
	float      track_len;     /**< Track length along the drag axis. */
	float      thumb_len;     /**< Thumb length along the drag axis. */
	float      max_scroll;    /**< Maximum scroll offset along the drag axis. */
} FluxScrollDrag;

/** @brief Touch content-pan session; hands off to DirectManipulation on real touch. */
typedef struct FluxTouchPan {
	XentNodeId target;               /**< Scroll node being panned, or INVALID. */
	float      start_x, start_y;     /**< Pointer position at touch-down. */
	float      origin_sx, origin_sy; /**< Scroll offsets captured at touch-down. */
	bool       active;               /**< Pan threshold crossed; pointer now pans content. */
	bool       just_promoted;        /**< Set the frame the pan promoted (suppresses the click). */
} FluxTouchPan;

/**
 * @brief Mutually-exclusive scroll interaction phases.
 *
 * Mouse thumb-drag and touch pan are partitioned by pointer type, so a single
 * pointer sequence never leaves its branch. Derived from the session fields for
 * readability and debugging — not an authoritative stored value.
 */
typedef enum FluxScrollPhase
{
	FLUX_SCROLL_IDLE,          /**< No active scroll interaction. */
	FLUX_SCROLL_THUMB_DRAG,    /**< Mouse dragging a scrollbar thumb. */
	FLUX_SCROLL_TOUCH_PENDING, /**< Touch down on a scroll node; pan not yet promoted. */
	FLUX_SCROLL_TOUCH_PAN,     /**< Touch pan active (content follows the finger). */
} FluxScrollPhase;

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
	FluxScrollDrag  scroll_drag;         /**< Mouse scrollbar thumb-drag session. */
	FluxTouchPan    touch;               /**< Touch content-pan session. */
	XentNodeId      scroll_hover_target; /**< Scroll node currently under the pointer. */

	/* Modal focus trap (ContentDialog): when modal_root is set, Tab navigation is
	 * confined to its subtree and Escape invokes modal_escape instead of blurring. */
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
bool            input_finish_scroll_drag(FluxInput *input);
bool            input_release_scroll_buttons(FluxInput *input);
bool            input_finish_touch_pan(FluxInput *input);
bool            input_handle_number_box_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_handle_flip_view_wheel(FluxInput *input, XentNodeId node, float delta_y);
bool            input_route_scroll_node(FluxInput *input, XentNodeId node, float *remaining_x, float *remaining_y);
void            flux_scroll_update_hover(FluxInput *input, XentNodeId hit_node, float px, float py);

/** @brief Current scroll interaction phase, derived from the session fields. */
FluxScrollPhase flux_input_scroll_phase(FluxInput const *input);

#endif
