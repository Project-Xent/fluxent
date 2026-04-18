#include "fluxent/flux_input.h"
#include "flux_scroll_geom.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

struct FluxInput {
	XentContext    *ctx;
	FluxNodeStore  *store;
	XentNodeId      hovered;
	XentNodeId      pressed;
	XentNodeId      focused;
	FluxRect        pressed_bounds;
	/* Double/triple click detection */
	int             click_count;
	DWORD           last_click_time;
	float           last_click_x;
	float           last_click_y;
	XentNodeId      last_click_node;
	/* Device + id of the most recent event — populated by
	 * flux_input_dispatch before calling any internal helper. */
	FluxPointerType pointer_type;
	uint32_t        pointer_id;
	/* Touch-pan state (WinUI ScrollViewer direct manipulation).
	 * On touch-down, remember the nearest scrollable ancestor.  After the
	 * finger moves past the pan threshold, the inner control's press is
	 * canceled and subsequent moves drive the scroll offset directly. */
	XentNodeId      touch_pan_target;
	float           touch_start_x;
	float           touch_start_y;
	float           touch_pan_origin_sx;
	float           touch_pan_origin_sy;
	bool            touch_pan_active;
	/* Edge flag set in fi_pointer_move on the frame the finger first
	 * crosses FLUX_TOUCH_PAN_THRESHOLD.  flux_input_take_pan_promoted()
	 * reads-and-clears it; app_pointer uses that signal to hand the
	 * pointer off to DirectManipulation — we don't do it on DOWN
	 * because that would steal pure taps from buttons. */
	bool            pan_just_promoted;

	/* Scrollbar-thumb mouse drag (WinUI ScrollBar Thumb DragDelta).
	 * When the user presses on a ScrollView's overlay thumb, pointer_move
	 * adjusts the scroll offset directly instead of routing to any child. */
	XentNodeId      scroll_drag_node;
	int             scroll_drag_axis;          /* 0=none, 1=vertical, 2=horizontal */
	float           scroll_drag_pointer_start; /* py for vertical, px for horizontal */
	float           scroll_drag_scroll_start;
	float           scroll_drag_track_len;
	float           scroll_drag_thumb_len;
	float           scroll_drag_max_scroll;
	/* Which ScrollView currently receives mouse_over tracking (so we can
	 * clear it when the cursor leaves). */
	XentNodeId      scroll_hover_target;
};

/* WinUI touch pan threshold — once the finger has moved this many DIPs
 * from pointer_down, the inner press is canceled and scroll-pan begins. */
#define FLUX_TOUCH_PAN_THRESHOLD 10.0f

/* Hit-test using absolute layout coordinates.
   scroll_x/scroll_y track cumulative scroll offset from ancestor scroll
   containers.  xent_get_layout_rect returns absolute positions, so the
   screen position of a node is  rect.x - scroll_x. */
static FluxHitResult
hit_test_recursive(XentContext *ctx, XentNodeId node, float px, float py, float scroll_x, float scroll_y) {
	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);

	float ax = rect.x - scroll_x;
	float ay = rect.y - scroll_y;

	if (px < ax || px >= ax + rect.width || py < ay || py >= ay + rect.height) return (FluxHitResult) {0};

	float         child_scroll_x = scroll_x;
	float         child_scroll_y = scroll_y;
	FluxNodeData *nd             = ( FluxNodeData * ) xent_get_userdata(ctx, node);
		if (nd && xent_get_control_type(ctx, node) == XENT_CONTROL_SCROLL) {
			FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
				if (sd) {
					child_scroll_x += sd->scroll_x;
					child_scroll_y += sd->scroll_y;
				}
		}

	FluxHitResult best  = {0};
	XentNodeId    child = xent_get_first_child(ctx, node);
		while (child != XENT_NODE_INVALID) {
			FluxHitResult r = hit_test_recursive(ctx, child, px, py, child_scroll_x, child_scroll_y);
			if (r.node != XENT_NODE_INVALID) best = r;
			child = xent_get_next_sibling(ctx, child);
		}
	if (best.node != XENT_NODE_INVALID) return best;

	return (FluxHitResult) {
	  .node   = node,
	  .data   = nd,
	  .bounds = {ax, ay, rect.width, rect.height},
	  .local  = {px - ax, py - ay},
	};
}

/* ---- ScrollBar overlay interaction helpers ----------------------------- */

/* Compute the absolute (screen-space) viewport rect of `node` — its layout
 * rect adjusted by cumulative scroll offsets of ancestor SCROLL nodes.
 * Mirrors what hit_test_recursive does while descending. */
static void flux_node_abs_rect(FluxInput *input, XentNodeId node, FluxRect *out) {
	XentRect lr = {0};
	xent_get_layout_rect(input->ctx, node, &lr);
	float      ox = lr.x, oy = lr.y;
	XentNodeId p = xent_get_parent(input->ctx, node);
		while (p != XENT_NODE_INVALID) {
				if (xent_get_control_type(input->ctx, p) == XENT_CONTROL_SCROLL) {
					FluxNodeData *pnd = flux_node_store_get(input->store, p);
						if (pnd && pnd->component_data) {
							FluxScrollData *sd  = ( FluxScrollData * ) pnd->component_data;
							ox                 -= sd->scroll_x;
							oy                 -= sd->scroll_y;
						}
				}
			p = xent_get_parent(input->ctx, p);
		}
	out->x = ox;
	out->y = oy;
	out->w = lr.width;
	out->h = lr.height;
}

/* Walk ancestors of `hit_node` for a SCROLL whose overlay bar contains
 * the pointer.  Returns nearest match (deepest-first). */
static XentNodeId flux_find_scroll_at_pointer(
  FluxInput *input, XentNodeId hit_node, float px, float py, FluxRect *out_bounds, FluxScrollBarGeom *out_g
) {
	XentNodeId n = hit_node;
		while (n != XENT_NODE_INVALID) {
				if (xent_get_control_type(input->ctx, n) == XENT_CONTROL_SCROLL) {
					FluxNodeData *nd = flux_node_store_get(input->store, n);
						if (nd && nd->component_data) {
							FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
							FluxRect        b;
							flux_node_abs_rect(input, n, &b);
							FluxScrollBarGeom g;
							/* Always hit-test against the fully-expanded bar so the
							 * user hits what they aimed at during the expand animation. */
							flux_scroll_geom_compute(
							  &g, &b, sd->content_w, sd->content_h, sd->scroll_x, sd->scroll_y, 1.0f
							);
							bool in_v
							  = g.has_v && sd->v_vis != FLUX_SCROLL_NEVER && flux_rect_contains(&g.v_bar, px, py);
							bool in_h
							  = g.has_h && sd->h_vis != FLUX_SCROLL_NEVER && flux_rect_contains(&g.h_bar, px, py);
								if (in_v || in_h) {
									if (out_bounds) *out_bounds = b;
									if (out_g) *out_g = g;
									return n;
								}
						}
				}
			n = xent_get_parent(input->ctx, n);
		}
	return XENT_NODE_INVALID;
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

static double flux_now_seconds(void) {
	LARGE_INTEGER f, c;
	if (!QueryPerformanceFrequency(&f) || !QueryPerformanceCounter(&c)) return 0.0;
	return ( double ) c.QuadPart / ( double ) f.QuadPart;
}

/* Update mouse_over tracking on ScrollView ancestors of the hit node.
 * Only the deepest enclosing SCROLL tracks the cursor; we clear any
 * previous target we set. */
static void flux_scroll_update_hover(FluxInput *input, XentNodeId hit_node, float px, float py) {
	XentNodeId target = XENT_NODE_INVALID;
	XentNodeId n      = hit_node;
		while (n != XENT_NODE_INVALID) {
				if (xent_get_control_type(input->ctx, n) == XENT_CONTROL_SCROLL) {
					target = n;
					break;
				}
			n = xent_get_parent(input->ctx, n);
		}

		if (input->scroll_hover_target != target && input->scroll_hover_target != XENT_NODE_INVALID) {
			FluxNodeData *old = flux_node_store_get(input->store, input->scroll_hover_target);
				if (old && old->component_data) {
					FluxScrollData *sd = ( FluxScrollData * ) old->component_data;
					sd->mouse_over     = 0;
				}
		}
	input->scroll_hover_target = target;

		if (target != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, target);
				if (nd && nd->component_data) {
					FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
					FluxRect        b;
					flux_node_abs_rect(input, target, &b);
					sd->mouse_over    = 1;
					sd->mouse_local_x = px - b.x;
					sd->mouse_local_y = py - b.y;
				}
		}
}

/* ------------------------------------------------------------------------ */

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
	input->touch_pan_target    = XENT_NODE_INVALID;
	input->scroll_drag_node    = XENT_NODE_INVALID;
	input->scroll_drag_axis    = 0;
	input->scroll_hover_target = XENT_NODE_INVALID;
	return input;
}

void          flux_input_destroy(FluxInput *input) { free(input); }

FluxHitResult flux_input_hit_test(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return (FluxHitResult) {0};
	return hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
}

/* Forward declarations of the internal per-kind helpers defined further
 * below.  They keep the same body they had when they were the public
 * flux_input_pointer_* entry points — we just route through a single
 * dispatcher now. */
static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_down(FluxInput *input, XentNodeId root, float px, float py);
static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py);
static void fi_context_menu(FluxInput *input, XentNodeId root, float px, float py);
static void fi_scroll(FluxInput *input, XentNodeId root, float px, float py, float dx, float dy);

/* Abort any active press/drag/pan without synthesizing a click.  Used
 * on FLUX_POINTER_CANCEL when the OS yanks capture out from under us. */
static void fi_cancel(FluxInput *input) {
	if (!input) return;

		/* Drop scrollbar thumb drag */
		if (input->scroll_drag_axis != 0 && input->scroll_drag_node != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->scroll_drag_node);
				if (nd && nd->component_data) {
					FluxScrollData *sd     = ( FluxScrollData * ) nd->component_data;
					sd->drag_axis          = 0;
					sd->last_activity_time = flux_now_seconds();
				}
			input->scroll_drag_node = XENT_NODE_INVALID;
			input->scroll_drag_axis = 0;
		}

	/* Drop touch-pan */
	input->touch_pan_active = false;
	input->touch_pan_target = XENT_NODE_INVALID;

		/* Release pressed control without firing on_click.  Notify the node
	     * via on_cancel so controls with side effects (RepeatButton's repeat
	     * timer, etc.) can shut down cleanly.  Also clear hovered if it
	     * happens to point at the same node — touch sets hovered=pressed
	     * during a press so leaving it set would freeze the visual. */
		if (input->pressed != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
				if (nd) {
					nd->state.pressed = 0;
					if (nd->behavior.on_cancel) nd->behavior.on_cancel(nd->behavior.on_cancel_ctx);
						if (input->hovered == input->pressed) {
							nd->state.hovered = 0;
							nd->hover_local_x = -1.0f;
							nd->hover_local_y = -1.0f;
							input->hovered    = XENT_NODE_INVALID;
						}
				}
			input->pressed = XENT_NODE_INVALID;
		}
}

void flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev) {
	if (!input || !ev || root == XENT_NODE_INVALID) return;

	/* Latch device + id so internal helpers (and downstream readers
	 * like DManip) see the right values for the current event. */
	input->pointer_type = ev->device;
	input->pointer_id   = ev->pointer_id;

		switch (ev->kind) {
		case FLUX_POINTER_DOWN :
			/* Only the primary button ("left" in our bitmask, which is
			 * also what touch/pen contact reports) engages the press
			 * pipeline.  Middle- and right-button presses don't "press"
			 * controls — right-click becomes a CONTEXT_MENU event on
			 * release, middle is currently unused. */
			if (ev->changed_button == FLUX_POINTER_BUTTON_LEFT) fi_pointer_down(input, root, ev->x, ev->y);
			break;

		case FLUX_POINTER_UP :
			if (ev->changed_button == FLUX_POINTER_BUTTON_LEFT) fi_pointer_up(input, root, ev->x, ev->y);
			break;

		case FLUX_POINTER_MOVE         : fi_pointer_move(input, root, ev->x, ev->y); break;

		case FLUX_POINTER_WHEEL        : fi_scroll(input, root, ev->x, ev->y, ev->wheel_dx, ev->wheel_dy); break;

		case FLUX_POINTER_CONTEXT_MENU : fi_context_menu(input, root, ev->x, ev->y); break;

		case FLUX_POINTER_CANCEL       : fi_cancel(input); break;
		}
}

static void fi_pointer_move(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

		/* ── ScrollBar thumb drag (latched from pointer_down).
	     * Takes precedence over any other hit-test so the cursor can leave
	     * the track without losing the drag.  Mirrors WinUI Thumb.DragDelta. */
		if (input->scroll_drag_axis != 0 && input->scroll_drag_node != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->scroll_drag_node);
				if (nd && nd->component_data) {
					FluxScrollData *sd     = ( FluxScrollData * ) nd->component_data;
					float           travel = input->scroll_drag_track_len - input->scroll_drag_thumb_len;
						if (travel > 0.5f && input->scroll_drag_max_scroll > 0.0f) {
							float delta  = (input->scroll_drag_axis == 1 ? py : px) - input->scroll_drag_pointer_start;
							float pct    = delta / travel;
							float target = input->scroll_drag_scroll_start + pct * input->scroll_drag_max_scroll;
							if (target < 0.0f) target = 0.0f;
							if (target > input->scroll_drag_max_scroll) target = input->scroll_drag_max_scroll;
							if (input->scroll_drag_axis == 1) sd->scroll_y = target;
							else sd->scroll_x = target;
							sd->last_activity_time = flux_now_seconds();
						}
				}
			return; /* consumed */
		}

	FluxHitResult hit         = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
	XentNodeId    new_hovered = hit.node;

	/* Update ScrollView mouse-over tracking so the overlay scrollbar
	 * expands when the cursor nears its edge. */
	flux_scroll_update_hover(input, hit.node, px, py);

		/* ── Touch-pan promotion & drive (WinUI ScrollViewer direct
	     * manipulation).  Handled OUTSIDE the pressed-gate because once the
	     * inner control is canceled (pressed → INVALID), subsequent moves
	     * must still keep driving the scroll target. */
		if (input->pointer_type == FLUX_POINTER_TOUCH && input->touch_pan_target != XENT_NODE_INVALID) {

				/* Promotion: once the finger has traveled past the threshold,
			     * cancel the inner control's press (if any) and latch pan mode. */
				if (!input->touch_pan_active) {
					float tdx = px - input->touch_start_x;
					float tdy = py - input->touch_start_y;
						if (tdx * tdx + tdy * tdy >= FLUX_TOUCH_PAN_THRESHOLD * FLUX_TOUCH_PAN_THRESHOLD) {
								if (input->pressed != XENT_NODE_INVALID) {
									FluxNodeData *pnd = flux_node_store_get(input->store, input->pressed);
										if (pnd) {
											pnd->state.pressed = 0;
											pnd->state.hovered = 0;
											pnd->hover_local_x = -1.0f;
											pnd->hover_local_y = -1.0f;
										}
									if (input->hovered == input->pressed) input->hovered = XENT_NODE_INVALID;
									input->pressed = XENT_NODE_INVALID;
								}
							input->touch_pan_active  = true;
							input->pan_just_promoted = true;
						}
				}

				if (input->touch_pan_active) {
					FluxNodeData *snd = flux_node_store_get(input->store, input->touch_pan_target);
						if (snd && snd->component_data) {
							FluxScrollData *sd    = ( FluxScrollData * ) snd->component_data;
							XentRect        srect = {0};
							xent_get_layout_rect(input->ctx, input->touch_pan_target, &srect);
							float new_sx = input->touch_pan_origin_sx - (px - input->touch_start_x);
							float new_sy = input->touch_pan_origin_sy - (py - input->touch_start_y);
							float max_x  = sd->content_w - srect.width;
							if (max_x < 0.0f) max_x = 0.0f;
							float max_y = sd->content_h - srect.height;
							if (max_y < 0.0f) max_y = 0.0f;
							if (new_sx < 0.0f) new_sx = 0.0f;
							if (new_sx > max_x) new_sx = max_x;
							if (new_sy < 0.0f) new_sy = 0.0f;
							if (new_sy > max_y) new_sy = max_y;
							sd->scroll_x           = new_sx;
							sd->scroll_y           = new_sy;
							sd->last_activity_time = flux_now_seconds();
						}
					return;
				}
		}

	/* Touch devices do not have a hover state:
	 * WinUI's PointerEntered from touch fires only between PointerPressed
	 * and PointerReleased.  To mirror that, suppress hover when the device
	 * is TOUCH and nothing is currently pressed. */
	bool suppress_hover = (input->pointer_type == FLUX_POINTER_TOUCH) && (input->pressed == XENT_NODE_INVALID);

	if (suppress_hover) new_hovered = XENT_NODE_INVALID;

		if (new_hovered != input->hovered) {
				if (input->hovered != XENT_NODE_INVALID) {
					FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
						if (old) {
							old->state.hovered = 0;
							old->hover_local_x = -1.0f;
							old->hover_local_y = -1.0f;
						}
				}
				if (new_hovered != XENT_NODE_INVALID && hit.data) {
					hit.data->state.hovered      = 1;
					hit.data->state.pointer_type = ( uint8_t ) input->pointer_type;
				}
			input->hovered = new_hovered;
		}

		/* Update hover local coordinates on the currently hovered node */
		if (input->hovered != XENT_NODE_INVALID) {
			FluxNodeData *hnd = flux_node_store_get(input->store, input->hovered);
				if (hnd) {
					XentRect hrect = {0};
					xent_get_layout_rect(input->ctx, input->hovered, &hrect);
					hnd->hover_local_x      = px - hrect.x;
					hnd->hover_local_y      = py - hrect.y;
					hnd->state.pointer_type = ( uint8_t ) input->pointer_type;
				}
		}

		/* During a drag, forward pointer_move to the pressed node so that
	       sliders (and other drag-interactive controls) keep receiving
	       position updates even when the cursor leaves the control bounds. */
		if (input->pressed != XENT_NODE_INVALID) {
			FluxNodeData *pressed_nd = flux_node_store_get(input->store, input->pressed);
				if (pressed_nd && pressed_nd->behavior.on_pointer_move) {
					float local_x = px - input->pressed_bounds.x;
					float local_y = py - input->pressed_bounds.y;
					pressed_nd->behavior.on_pointer_move(pressed_nd->behavior.on_pointer_move_ctx, local_x, local_y);
				}
		}
}

static void fi_pointer_down(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

	FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);

		/* ── ScrollBar overlay intercept (mouse/pen only).
	     * Touch pointer_down on the bar area still enables touch-pan on the
	     * inner content — WinUI hides the scrollbar buttons for touch, so we
	     * skip the overlay hit-test for touch pointers. */
		if (input->pointer_type != FLUX_POINTER_TOUCH && hit.node != XENT_NODE_INVALID) {
			FluxRect          sb = {0};
			FluxScrollBarGeom g;
			XentNodeId        sn = flux_find_scroll_at_pointer(input, hit.node, px, py, &sb, &g);
				if (sn != XENT_NODE_INVALID) {
					FluxNodeData   *snd = flux_node_store_get(input->store, sn);
					FluxScrollData *sd  = ( FluxScrollData * ) snd->component_data;

						/* Vertical bar interactions */
						if (g.has_v && flux_rect_contains(&g.v_bar, px, py)) {
								if (flux_rect_contains(&g.v_up_btn, px, py)) {
									sd->scroll_y           -= FLUX_SCROLLBAR_SMALL_CHANGE;
									sd->v_up_pressed        = 1;
									sd->last_activity_time  = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
								if (flux_rect_contains(&g.v_dn_btn, px, py)) {
									sd->scroll_y           += FLUX_SCROLLBAR_SMALL_CHANGE;
									sd->v_dn_pressed        = 1;
									sd->last_activity_time  = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
								if (flux_rect_contains(&g.v_thumb, px, py)) {
									/* Start thumb drag */
									input->scroll_drag_node          = sn;
									input->scroll_drag_axis          = 1;
									input->scroll_drag_pointer_start = py;
									input->scroll_drag_scroll_start  = sd->scroll_y;
									input->scroll_drag_track_len     = g.v_track.h;
									input->scroll_drag_thumb_len     = g.v_thumb.h;
									input->scroll_drag_max_scroll    = sd->content_h - sb.h;
									if (input->scroll_drag_max_scroll < 0.0f) input->scroll_drag_max_scroll = 0.0f;
									sd->drag_axis          = 1;
									sd->last_activity_time = flux_now_seconds();
									input->pressed         = XENT_NODE_INVALID;
									return;
								}
								/* Click on track above/below thumb — page scroll */
								if (flux_rect_contains(&g.v_track, px, py)) {
									if (py < g.v_thumb.y) sd->scroll_y -= sb.h;
									else sd->scroll_y += sb.h;
									sd->last_activity_time = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
						}

						/* Horizontal bar interactions */
						if (g.has_h && flux_rect_contains(&g.h_bar, px, py)) {
								if (flux_rect_contains(&g.h_lf_btn, px, py)) {
									sd->scroll_x           -= FLUX_SCROLLBAR_SMALL_CHANGE;
									sd->h_lf_pressed        = 1;
									sd->last_activity_time  = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
								if (flux_rect_contains(&g.h_rg_btn, px, py)) {
									sd->scroll_x           += FLUX_SCROLLBAR_SMALL_CHANGE;
									sd->h_rg_pressed        = 1;
									sd->last_activity_time  = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
								if (flux_rect_contains(&g.h_thumb, px, py)) {
									input->scroll_drag_node          = sn;
									input->scroll_drag_axis          = 2;
									input->scroll_drag_pointer_start = px;
									input->scroll_drag_scroll_start  = sd->scroll_x;
									input->scroll_drag_track_len     = g.h_track.w;
									input->scroll_drag_thumb_len     = g.h_thumb.w;
									input->scroll_drag_max_scroll    = sd->content_w - sb.w;
									if (input->scroll_drag_max_scroll < 0.0f) input->scroll_drag_max_scroll = 0.0f;
									sd->drag_axis          = 2;
									sd->last_activity_time = flux_now_seconds();
									input->pressed         = XENT_NODE_INVALID;
									return;
								}
								if (flux_rect_contains(&g.h_track, px, py)) {
									if (px < g.h_thumb.x) sd->scroll_x -= sb.w;
									else sd->scroll_x += sb.w;
									sd->last_activity_time = flux_now_seconds();
									flux_scroll_clamp(sd, &sb);
									input->pressed = XENT_NODE_INVALID;
									return;
								}
						}
				}
		}

		/* NumberBox spin-button intercept: ONLY block spin area clicks.
	       Spin clicks synthesize VK_UP/VK_DOWN via on_key — no pressed, no focus.
	       X (delete) button clicks are NOT intercepted here — they pass through
	       to on_pointer_down normally so the control gets focused and tb_on_pointer_down
	       handles the clear-text logic. */
		if (hit.node != XENT_NODE_INVALID && hit.data) {
			XentControlType nb_ct = xent_get_control_type(input->ctx, hit.node);
				if (nb_ct == XENT_CONTROL_NUMBER_BOX) {
					bool spin_inline = xent_get_semantic_expanded(input->ctx, hit.node);
						if (spin_inline) {
							XentRect nb_rect = {0};
							xent_get_layout_rect(input->ctx, hit.node, &nb_rect);
							float spin_start = nb_rect.width - 76.0f;
								if (hit.local.x >= spin_start) {
									bool up = (hit.local.x < spin_start + 40.0f);
										if (hit.data->behavior.on_key) {
											hit.data->behavior.on_key(
											  hit.data->behavior.on_key_ctx,
											  up ? 0x26u /* VK_UP */ : 0x28u /* VK_DOWN */, true
											);
										}
									return; /* consumed — no pressed, no focus, no on_pointer_down */
								}
						}
				}
		}

	/* Double/triple click detection */
	DWORD now_ms   = GetTickCount();
	DWORD dbl_time = GetDoubleClickTime();
	float dx       = px - input->last_click_x;
	float dy       = py - input->last_click_y;
	float dist2    = dx * dx + dy * dy;

		if (hit.node == input->last_click_node
			&& (now_ms - input->last_click_time) < dbl_time
			&& dist2 < 25.0f) { /* ~5px tolerance */
			input->click_count++;
			if (input->click_count > 3) input->click_count = 3;
		}
		else { input->click_count = 1; }
	input->last_click_time  = now_ms;
	input->last_click_x     = px;
	input->last_click_y     = py;
	input->last_click_node  = hit.node;

	/* ── Touch-pan setup (runs regardless of whether the hit node has
	 * attached FluxNodeData).  Touches on plain containers / backgrounds
	 * where no node data exists must still be able to pan an ancestor
	 * ScrollView — otherwise the user sees "only interactive widgets
	 * respond to touch". */
	input->touch_pan_target = XENT_NODE_INVALID;
	input->touch_pan_active = false;
	input->touch_start_x    = px;
	input->touch_start_y    = py;
		if (input->pointer_type == FLUX_POINTER_TOUCH && hit.node != XENT_NODE_INVALID) {
			/* Walk from the hit node itself (inclusive) up the tree.  If we
			 * pass through a control that handles its own drag semantics
			 * (Slider / Switch / TextBox), bail out BEFORE reaching any
			 * ScrollView ancestor so horizontal drags don't hijack. */
			XentNodeId anc = hit.node;
				while (anc != XENT_NODE_INVALID) {
					XentControlType ct = xent_get_control_type(input->ctx, anc);
						if (ct == XENT_CONTROL_SLIDER
							|| ct == XENT_CONTROL_SWITCH
							|| ct == XENT_CONTROL_TEXT_INPUT
							|| ct == XENT_CONTROL_PASSWORD_BOX
							|| ct == XENT_CONTROL_NUMBER_BOX)
						{
							break; /* inner drag-handler wins; no scroll-pan */
						}
						if (ct == XENT_CONTROL_SCROLL) {
							FluxNodeData *snd = flux_node_store_get(input->store, anc);
								if (snd && snd->component_data) {
									FluxScrollData *sd         = ( FluxScrollData * ) snd->component_data;
									input->touch_pan_target    = anc;
									input->touch_pan_origin_sx = sd->scroll_x;
									input->touch_pan_origin_sy = sd->scroll_y;
								}
							break;
						}
					anc = xent_get_parent(input->ctx, anc);
				}
		}

		if (hit.node != XENT_NODE_INVALID && hit.data) {
			hit.data->state.pressed      = 1;
			hit.data->state.pointer_type = ( uint8_t ) input->pointer_type;
			input->pressed               = hit.node;
			input->pressed_bounds        = hit.bounds;

				/* On touch, WinUI shows PointerOver state for the duration of the
			     * press (PointerEntered fires at press-time).  Mirror that by
			     * setting hovered on the pressed node for touch events. */
				if (input->pointer_type == FLUX_POINTER_TOUCH) {
						if (input->hovered != XENT_NODE_INVALID && input->hovered != hit.node) {
							FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
								if (old) {
									old->state.hovered = 0;
									old->hover_local_x = -1.0f;
									old->hover_local_y = -1.0f;
								}
						}
					hit.data->state.hovered = 1;
					hit.data->hover_local_x = hit.local.x;
					hit.data->hover_local_y = hit.local.y;
					input->hovered          = hit.node;
				}

			/* Fire on_pointer_move immediately so controls like sliders can
			   respond to click-to-position (not only drag). */
			if (hit.data->behavior.on_pointer_move)
				hit.data->behavior.on_pointer_move(hit.data->behavior.on_pointer_move_ctx, hit.local.x, hit.local.y);

			if (hit.data->behavior.on_pointer_down)
				hit.data->behavior.on_pointer_down(
				  hit.data->behavior.on_pointer_down_ctx, hit.local.x, hit.local.y, input->click_count
				);
		}

		if (hit.node != input->focused) {
				if (input->focused != XENT_NODE_INVALID) {
					FluxNodeData *old = flux_node_store_get(input->store, input->focused);
						if (old) {
							old->state.focused = 0;
							if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
						}
				}
				if (hit.node != XENT_NODE_INVALID && hit.data) {
					/* Text inputs always need visual focus for caret/accent line */
					XentControlType ct = xent_get_control_type(input->ctx, hit.node);
					if (ct == XENT_CONTROL_TEXT_INPUT) hit.data->state.focused = 1;
					if (hit.data->behavior.on_focus) hit.data->behavior.on_focus(hit.data->behavior.on_focus_ctx);
				}
			input->focused = hit.node;
		}
}

static void fi_pointer_up(FluxInput *input, XentNodeId root, float px, float py) {
	if (!input || root == XENT_NODE_INVALID) return;

		/* Clear ScrollBar thumb drag / arrow-button pressed state first. */
		if (input->scroll_drag_axis != 0 && input->scroll_drag_node != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->scroll_drag_node);
				if (nd && nd->component_data) {
					FluxScrollData *sd     = ( FluxScrollData * ) nd->component_data;
					sd->drag_axis          = 0;
					sd->last_activity_time = flux_now_seconds();
				}
			input->scroll_drag_node = XENT_NODE_INVALID;
			input->scroll_drag_axis = 0;
			return;
		}
		/* Clear any transient arrow-button press flags (any SCROLL we touched). */
		if (input->scroll_hover_target != XENT_NODE_INVALID) {
			FluxNodeData *snd = flux_node_store_get(input->store, input->scroll_hover_target);
				if (snd && snd->component_data) {
					FluxScrollData *sd = ( FluxScrollData * ) snd->component_data;
						if (sd->v_up_pressed || sd->v_dn_pressed || sd->h_lf_pressed || sd->h_rg_pressed) {
							sd->v_up_pressed = sd->v_dn_pressed = 0;
							sd->h_lf_pressed = sd->h_rg_pressed = 0;
							sd->last_activity_time              = flux_now_seconds();
							return; /* click on overlay bar does not propagate */
						}
				}
		}

		/* If touch-pan was promoted, the inner press was already cleared in
	     * pointer_move.  Just drop the pan state and don't synthesize a click. */
		if (input->touch_pan_active) {
			input->touch_pan_active = false;
			input->touch_pan_target = XENT_NODE_INVALID;
			input->pressed          = XENT_NODE_INVALID;
				if (input->pointer_type == FLUX_POINTER_TOUCH && input->hovered != XENT_NODE_INVALID) {
					FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
						if (old) {
							old->state.hovered = 0;
							old->hover_local_x = -1.0f;
							old->hover_local_y = -1.0f;
						}
					input->hovered = XENT_NODE_INVALID;
				}
			return;
		}
	input->touch_pan_target = XENT_NODE_INVALID;

		if (input->pressed != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->pressed);
				if (nd) {
					nd->state.pressed = 0;

					/* PasswordBox: clear reveal on mouse release (press-to-reveal behavior) */
					if (xent_get_control_type(input->ctx, input->pressed) == XENT_CONTROL_PASSWORD_BOX)
						xent_set_semantic_checked(input->ctx, input->pressed, 0);

					FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
					if (hit.node == input->pressed && nd->behavior.on_click)
						nd->behavior.on_click(nd->behavior.on_click_ctx);
				}
			input->pressed = XENT_NODE_INVALID;
		}

		/* On touch release, drop the synthetic hover immediately.  Mouse/pen
	     * keep hover (cursor is still on the control). */
		if (input->pointer_type == FLUX_POINTER_TOUCH && input->hovered != XENT_NODE_INVALID) {
			FluxNodeData *old = flux_node_store_get(input->store, input->hovered);
				if (old) {
					old->state.hovered = 0;
					old->hover_local_x = -1.0f;
					old->hover_local_y = -1.0f;
				}
			input->hovered = XENT_NODE_INVALID;
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
	return input ? input->touch_pan_target : XENT_NODE_INVALID;
}

void flux_input_clear_touch_pan(FluxInput *input) {
	if (!input) return;
	input->touch_pan_target = XENT_NODE_INVALID;
	input->touch_pan_active = false;
}

bool flux_input_take_pan_promoted(FluxInput *input) {
	if (!input) return false;
	bool v                   = input->pan_just_promoted;
	input->pan_just_promoted = false;
	return v;
}

void flux_input_key_down(FluxInput *input, unsigned int vk) {
	if (!input || input->focused == XENT_NODE_INVALID) return;
	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_key) nd->behavior.on_key(nd->behavior.on_key_ctx, vk, true);
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

	/* Hit-test at the right-click point so we get the actual element under
	   the cursor and scroll-correct local coords.
	   (input->focused is NOT reliable: it reflects the last click, which may
	   have been on a different control.) */
	FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);

	/* Walk up the tree looking for a context-menu handler (WinUI propagates
	   ContextRequested up the element ancestry until something handles it). */
	XentNodeId    n   = hit.node;
		while (n != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, n);
				if (nd && nd->behavior.on_context_menu) {
					/* Pass local DIPs relative to the handler element.
					   If we've walked up ancestors, recompute local from the hit
					   element's visual bounds + offset difference. */
					float local_x = hit.local.x;
					float local_y = hit.local.y;
						if (n != hit.node) {
							XentRect hit_layout = {0};
							XentRect n_layout   = {0};
							xent_get_layout_rect(input->ctx, hit.node, &hit_layout);
							xent_get_layout_rect(input->ctx, n, &n_layout);
							local_x = hit.local.x + (hit_layout.x - n_layout.x);
							local_y = hit.local.y + (hit_layout.y - n_layout.y);
						}
					nd->behavior.on_context_menu(nd->behavior.on_context_menu_ctx, local_x, local_y);
					return;
				}
			n = xent_get_parent(input->ctx, n);
		}
}

static void fi_scroll(FluxInput *input, XentNodeId root, float px, float py, float delta_x, float delta_y) {
	if (!input || root == XENT_NODE_INVALID) return;

	/* Hit-test to find the node under the cursor */
	FluxHitResult hit = hit_test_recursive(input->ctx, root, px, py, 0.0f, 0.0f);
	if (hit.node == XENT_NODE_INVALID) return;

	float const STEP        = 48.0f;
	/* Windows WM_MOUSEHWHEEL: positive delta = tilt right = scroll right (scroll_x++).
	 * Windows WM_MOUSEWHEEL:  positive delta = wheel up  = scroll up    (scroll_y--). */
	float       remaining_x = delta_x * STEP;
	float       remaining_y = -delta_y * STEP;

	/* Walk up the tree.  Each ScrollView consumes as much of the pending
	 * delta on each axis as its content permits; any leftover bubbles up
	 * to the parent ScrollView (mirrors WinUI nested-scroll chaining). */
	XentNodeId  node        = hit.node;
		while (node != XENT_NODE_INVALID) {
			XentControlType ct = xent_get_control_type(input->ctx, node);

				/* NumberBox: scroll wheel steps value when focused (WinUI OnNumberBoxScroll) */
				if (ct == XENT_CONTROL_NUMBER_BOX) {
					FluxNodeData *nd = flux_node_store_get(input->store, node);
						if (nd && nd->state.focused && nd->behavior.on_key) {
							if (delta_y > 0) nd->behavior.on_key(nd->behavior.on_key_ctx, 0x26 /* VK_UP */, true);
							else if (delta_y < 0)
								nd->behavior.on_key(nd->behavior.on_key_ctx, 0x28 /* VK_DOWN */, true);
						}
					return;
				}

				if (ct == XENT_CONTROL_SCROLL) {
					FluxNodeData *nd = flux_node_store_get(input->store, node);
						if (nd && nd->component_data) {
							FluxScrollData *sd   = ( FluxScrollData * ) nd->component_data;
							XentRect        rect = {0};
							xent_get_layout_rect(input->ctx, node, &rect);
							float max_x = sd->content_w - rect.width;
							if (max_x < 0.0f) max_x = 0.0f;
							float max_y = sd->content_h - rect.height;
							if (max_y < 0.0f) max_y = 0.0f;

							bool moved = false;
								if (remaining_x != 0.0f && max_x > 0.0f) {
									float nx = sd->scroll_x + remaining_x;
									if (nx < 0.0f) nx = 0.0f;
									if (nx > max_x) nx = max_x;
									float consumed  = nx - sd->scroll_x;
									sd->scroll_x    = nx;
									remaining_x    -= consumed;
									if (consumed != 0.0f) moved = true;
								}
								if (remaining_y != 0.0f && max_y > 0.0f) {
									float ny = sd->scroll_y + remaining_y;
									if (ny < 0.0f) ny = 0.0f;
									if (ny > max_y) ny = max_y;
									float consumed  = ny - sd->scroll_y;
									sd->scroll_y    = ny;
									remaining_y    -= consumed;
									if (consumed != 0.0f) moved = true;
								}
							if (moved) sd->last_activity_time = flux_now_seconds();

							/* Only stop bubbling once both axes are fully consumed. */
							if (remaining_x == 0.0f && remaining_y == 0.0f) return;
						}
				}

			node = xent_get_parent(input->ctx, node);
		}
}

/* ---- Focus Navigation ---- */

/* DFS collect all visible, focusable nodes */
static void collect_focusable(XentContext *ctx, XentNodeId node, XentNodeId *out, uint32_t *count, uint32_t max_count) {
	if (node == XENT_NODE_INVALID || *count >= max_count) return;

		/* Only include alive, focusable, and enabled nodes */
		if (xent_get_focusable(ctx, node) && xent_get_semantic_enabled(ctx, node)) {
			/* Check visibility: node should have non-zero layout rect */
			XentRect rect = {0};
			xent_get_layout_rect(ctx, node, &rect);
				if (rect.width > 0.0f && rect.height > 0.0f) {
					out [*count] = node;
					(*count)++;
				}
		}

	/* Recurse into children */
	XentNodeId child = xent_get_first_child(ctx, node);
		while (child != XENT_NODE_INVALID && *count < max_count) {
			collect_focusable(ctx, child, out, count, max_count);
			child = xent_get_next_sibling(ctx, child);
		}
}

/* Comparison function for sorting by tab_index (stable by insertion order) */
static XentContext *s_sort_ctx = NULL;

static int          compare_tab_index(void const *a, void const *b) {
	XentNodeId na = *( XentNodeId const * ) a;
	XentNodeId nb = *( XentNodeId const * ) b;
	int32_t    ta = xent_get_tab_index(s_sort_ctx, na);
	int32_t    tb = xent_get_tab_index(s_sort_ctx, nb);

	/* tab_index == 0 means "natural order" (treat as large value) */
	int32_t    ea = (ta == 0) ? 0x7fffffff : ta;
	int32_t    eb = (tb == 0) ? 0x7fffffff : tb;

	if (ea != eb) return (ea < eb) ? -1 : 1;
	/* Stable: preserve document order (lower id first) */
	return (na < nb) ? -1 : (na > nb) ? 1 : 0;
}

void flux_input_tab(FluxInput *input, XentNodeId root, bool shift) {
	if (!input || root == XENT_NODE_INVALID) return;

	/* Collect focusable nodes */
	XentNodeId focusable [512];
	uint32_t   count = 0;
	collect_focusable(input->ctx, root, focusable, &count, 512);

	if (count == 0) return;

	/* Filter out negative tab_index (they are programmatic-only, skip Tab) */
	XentNodeId tab_order [512];
	uint32_t   tab_count = 0;
		for (uint32_t i = 0; i < count; i++) {
			int32_t ti = xent_get_tab_index(input->ctx, focusable [i]);
			if (ti >= 0) tab_order [tab_count++] = focusable [i];
		}
	if (tab_count == 0) return;

	/* Sort by tab_index */
	s_sort_ctx = input->ctx;
	qsort(tab_order, tab_count, sizeof(XentNodeId), compare_tab_index);
	s_sort_ctx      = NULL;

	/* Find current focused node in the sorted list */
	int current_idx = -1;
		for (uint32_t i = 0; i < tab_count; i++) {
				if (tab_order [i] == input->focused) {
					current_idx = ( int ) i;
					break;
				}
		}

	/* Calculate next index */
	int next_idx;
	if (current_idx < 0) next_idx = shift ? ( int ) tab_count - 1 : 0;
	else if (shift) next_idx = (current_idx - 1 + ( int ) tab_count) % ( int ) tab_count;
	else next_idx = (current_idx + 1) % ( int ) tab_count;

	XentNodeId next_node = tab_order [next_idx];

		/* Blur old */
		if (input->focused != XENT_NODE_INVALID) {
			FluxNodeData *old = flux_node_store_get(input->store, input->focused);
				if (old) {
					old->state.focused = 0;
					if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
				}
		}

	/* Focus new */
	input->focused   = next_node;
	FluxNodeData *nd = flux_node_store_get(input->store, next_node);
		if (nd) {
			nd->state.focused = 1;
			if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
		}
}

void flux_input_arrow(FluxInput *input, XentNodeId root, int direction) {
	if (!input || root == XENT_NODE_INVALID) return;
	if (input->focused == XENT_NODE_INVALID) return;

	/* Get parent of focused node to find siblings */
	XentNodeId parent = xent_get_parent(input->ctx, input->focused);
	if (parent == XENT_NODE_INVALID) return;

	/* Collect focusable siblings */
	XentNodeId siblings [128];
	uint32_t   sib_count = 0;
	XentNodeId child     = xent_get_first_child(input->ctx, parent);
		while (child != XENT_NODE_INVALID && sib_count < 128) {
			if (xent_get_focusable(input->ctx, child) && xent_get_semantic_enabled(input->ctx, child))
				siblings [sib_count++] = child;
			child = xent_get_next_sibling(input->ctx, child);
		}

	if (sib_count <= 1) return;

	/* Find current in siblings */
	int cur = -1;
		for (uint32_t i = 0; i < sib_count; i++) {
				if (siblings [i] == input->focused) {
					cur = ( int ) i;
					break;
				}
		}
	if (cur < 0) return;

	/* Determine direction: left/up = -1, right/down = +1 */
	int delta = 0;
	if (direction == 0x25 || direction == 0x26) delta = -1; /* VK_LEFT, VK_UP */
	if (direction == 0x27 || direction == 0x28) delta = +1; /* VK_RIGHT, VK_DOWN */
	if (delta == 0) return;

	int           next      = (cur + delta + ( int ) sib_count) % ( int ) sib_count;
	XentNodeId    next_node = siblings [next];

	/* Blur old */
	FluxNodeData *old       = flux_node_store_get(input->store, input->focused);
		if (old) {
			old->state.focused = 0;
			if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
		}

	/* Focus new */
	input->focused   = next_node;
	FluxNodeData *nd = flux_node_store_get(input->store, next_node);
		if (nd) {
			nd->state.focused = 1;
			if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
		}
}

void flux_input_activate(FluxInput *input) {
	if (!input || input->focused == XENT_NODE_INVALID) return;

	FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
	if (nd && nd->behavior.on_click) nd->behavior.on_click(nd->behavior.on_click_ctx);
}

void flux_input_escape(FluxInput *input) {
	if (!input) return;

		if (input->focused != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, input->focused);
				if (nd) {
					nd->state.focused = 0;
					if (nd->behavior.on_blur) nd->behavior.on_blur(nd->behavior.on_blur_ctx);
				}
			input->focused = XENT_NODE_INVALID;
		}
}

void flux_input_set_focus(FluxInput *input, XentNodeId node) {
	if (!input) return;

		/* Blur old */
		if (input->focused != XENT_NODE_INVALID && input->focused != node) {
			FluxNodeData *old = flux_node_store_get(input->store, input->focused);
				if (old) {
					old->state.focused = 0;
					if (old->behavior.on_blur) old->behavior.on_blur(old->behavior.on_blur_ctx);
				}
		}

	/* Focus new */
	input->focused = node;
		if (node != XENT_NODE_INVALID) {
			FluxNodeData *nd = flux_node_store_get(input->store, node);
				if (nd) {
					nd->state.focused = 1;
					if (nd->behavior.on_focus) nd->behavior.on_focus(nd->behavior.on_focus_ctx);
				}
		}
}
