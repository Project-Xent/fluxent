#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_anim_driver.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/controls/flux_refresh_data.h"
#include "fluxent/controls/flux_scroll_data.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI RefreshContainer + RefreshVisualizer (PullToRefresh). Two nodes mirror
 * the runtime composition:
 *   root   FLUX_CONTROL_REFRESH  — owns the runtime, clips to bounds, draws the
 *                                  visualizer glyph as an overlay pass.
 *   scroll FLUX_CONTROL_SCROLL   — the scrollable content host (children mount here).
 *
 * WinUI's InteractionTracker feeds the visualizer two signals — interactionRatio
 * (pull / size) and isInteractingForRefresh — which drive a five-state machine
 * (Idle/Peeking/Interacting/Pending/Refreshing). fluxent has no InteractionTracker
 * seam at the scroll edge, so the gesture is reconstructed best-effort from pointer
 * deltas taken while the scroller sits at its pull edge; the reliable path is the
 * programmatic RequestRefresh / set_refreshing API (mirrors the WinRT Deferral). */

#ifndef FLUX_PI
  #define FLUX_PI 3.14159265358979323846f
#endif

static FluxRefreshData *refresh_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_REFRESH) return NULL;
	return ( FluxRefreshData * ) nd->component_data;
}

static void refresh_repaint(FluxRefreshData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float refresh_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static bool  refresh_is_vertical(FluxPullDirection dir) {
	return dir == FLUX_PULL_TOP_TO_BOTTOM || dir == FLUX_PULL_BOTTOM_TO_TOP;
}

/* BottomToTop / RightToLeft pull toward the far edge, negating the axis sign. */
static bool refresh_is_far(FluxPullDirection dir) {
	return dir == FLUX_PULL_BOTTOM_TO_TOP || dir == FLUX_PULL_RIGHT_TO_LEFT;
}

/* Orientation=Auto starting angle: vertical 0, LeftToRight -pi/2, RightToLeft +pi/2. */
static float refresh_start_angle(FluxPullDirection dir) {
	if (dir == FLUX_PULL_LEFT_TO_RIGHT) return -FLUX_PI * 0.5f;
	if (dir == FLUX_PULL_RIGHT_TO_LEFT) return FLUX_PI * 0.5f;
	return 0.0f;
}

/* -------------------------------------------------------------------------
 * Continuous spin (500ms Linear loop) + Pending pop (300ms) driver.
 * ---------------------------------------------------------------------- */

static bool refresh_pop_active(FluxRefreshData const *d, unsigned long now) {
	if (d->state != FLUX_REFRESH_PENDING || d->pop_start_tick == 0) return false;
	return ( float ) (now - d->pop_start_tick) < FLUX_REFRESH_POP_MS;
}

static bool refresh_step(void *ctx, unsigned long now) {
	FluxRefreshData *d         = ( FluxRefreshData * ) ctx;
	bool             animating = false;
	if (d->state == FLUX_REFRESH_REFRESHING) animating = true;      /* perpetual spin */
	if (refresh_pop_active(d, now)) animating = true;               /* one-shot pop */
	refresh_repaint(d);
	if (!animating) d->anim_registered = false;
	return animating;
}

static void refresh_anim_start(FluxRefreshData *d) {
	d->anim_registered = true;
	flux_anim_register(d, refresh_step);
}

/* -------------------------------------------------------------------------
 * State transitions.
 * ---------------------------------------------------------------------- */

static void refresh_enter_state(FluxRefreshData *d, int state) {
	if (d->state == state) return;
	d->state = state;
	if (state == FLUX_REFRESH_PENDING) {
		d->pop_start_tick = GetTickCount();
		refresh_anim_start(d);
	}
	else if (state == FLUX_REFRESH_REFRESHING) {
		d->spin_start_tick = GetTickCount();
		refresh_anim_start(d);
	}
	refresh_repaint(d);
}

static void refresh_begin_refresh(FluxRefreshData *d, bool fire) {
	d->interacting_for_refresh = false;
	d->pull_tracking           = false;
	refresh_enter_state(d, FLUX_REFRESH_REFRESHING);
	if (fire && d->on_refresh) d->on_refresh(d->userdata, ( int ) d->direction);
}

static void refresh_complete(FluxRefreshData *d) {
	d->interaction_ratio = 0.0f;
	d->was_at_zero       = true;
	refresh_enter_state(d, FLUX_REFRESH_IDLE);
}

/* The armed pull's five-state transitions (§4.4): where the current state
 * moves for ratio r, given whether the previous ratio was zero. */
static int refresh_armed_next_state(FluxRefreshData const *d, float r, bool zero) {
	float thr = d->threshold_ratio;
	switch (d->state) {
	case FLUX_REFRESH_IDLE :
		if (zero && r > thr) return FLUX_REFRESH_PENDING;
		if (zero && r > 0.0f) return FLUX_REFRESH_INTERACTING;
		return r > 0.0f ? FLUX_REFRESH_PEEKING : d->state;
	case FLUX_REFRESH_INTERACTING :
		if (r <= 0.0f) return FLUX_REFRESH_IDLE;
		return r > thr ? FLUX_REFRESH_PENDING : d->state;
	case FLUX_REFRESH_PENDING :
		if (r <= 0.0f) return FLUX_REFRESH_IDLE;
		return r <= thr ? FLUX_REFRESH_INTERACTING : d->state;
	default : return d->state; /* Peeking / Refreshing stay */
	}
}

/* InteractionRatioChanged(r) (§4.4). */
static void refresh_on_ratio(FluxRefreshData *d, float r) {
	r = refresh_clamp01(r);

	if (d->interacting_for_refresh) {
		refresh_enter_state(d, refresh_armed_next_state(d, r, d->was_at_zero));
	}
	else if (d->state != FLUX_REFRESH_REFRESHING) {
		refresh_enter_state(d, r > 0.0f ? FLUX_REFRESH_PEEKING : FLUX_REFRESH_IDLE);
	}

	d->interaction_ratio = r;
	d->was_at_zero       = (r == 0.0f);
	refresh_repaint(d);
}

/* IsInteractingForRefreshChanged -> false (finger lifted) (§4.5). */
static void refresh_on_lift(FluxRefreshData *d) {
	int state                  = d->state;
	d->interacting_for_refresh = false;
	d->pull_tracking           = false;
	if (state == FLUX_REFRESH_PENDING) refresh_begin_refresh(d, true); /* RequestRefresh */
	else if (state != FLUX_REFRESH_REFRESHING) refresh_complete(d);
}

/* -------------------------------------------------------------------------
 * Best-effort over-pull gesture (see file header for the limitation).
 * ---------------------------------------------------------------------- */

static float refresh_pull_distance(FluxRefreshData const *d, float x, float y) {
	float coord = refresh_is_vertical(d->direction) ? y : x;
	float pull  = refresh_is_far(d->direction) ? (d->pull_origin - coord) : (coord - d->pull_origin);
	return pull < 0.0f ? 0.0f : pull;
}

/* IsWithinOffsetThreshold: the scroller sits within 1 DIP of its pull edge. */
static bool refresh_within_offset_threshold(FluxRefreshData const *d) {
	FluxNodeData *nd = flux_node_store_get(d->store, d->scroll_child);
	if (!nd || !nd->component_data) return true;
	FluxScrollData const *sd = ( FluxScrollData const * ) nd->component_data;

	XentRect rect = {0};
	xent_get_layout_rect(d->ctx, d->scroll_child, &rect);
	float const t = FLUX_REFRESH_OFFSET_THRESH;
	switch (d->direction) {
	case FLUX_PULL_TOP_TO_BOTTOM: return sd->scroll_y < t;
	case FLUX_PULL_BOTTOM_TO_TOP: return sd->scroll_y > (sd->content_h - rect.h) - t;
	case FLUX_PULL_LEFT_TO_RIGHT: return sd->scroll_x < t;
	case FLUX_PULL_RIGHT_TO_LEFT: return sd->scroll_x > (sd->content_w - rect.w) - t;
	default:                      return true;
	}
}

static void refresh_on_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxRefreshData *d = ( FluxRefreshData * ) ctx;
	if (d->state == FLUX_REFRESH_REFRESHING) return; /* don't interrupt an active refresh */
	d->interacting_for_refresh = refresh_within_offset_threshold(d); /* peeking = !armed */
	d->pull_origin             = refresh_is_vertical(d->direction) ? y : x;
	d->pull_tracking           = true;
	d->was_at_zero             = true;
	d->interaction_ratio       = 0.0f;
}

static void refresh_on_pointer_move(void *ctx, float x, float y) {
	FluxRefreshData *d = ( FluxRefreshData * ) ctx;
	if (!d->pull_tracking) return;
	refresh_on_ratio(d, refresh_pull_distance(d, x, y) / d->visualizer_size);
}

static void refresh_on_release(void *ctx) {
	FluxRefreshData *d = ( FluxRefreshData * ) ctx;
	if (d->pull_tracking || d->interacting_for_refresh) refresh_on_lift(d);
}

static void refresh_destroy(void *component_data) {
	flux_anim_unregister(component_data);
	free(component_data);
}

/* -------------------------------------------------------------------------
 * Construction.
 * ---------------------------------------------------------------------- */

static void refresh_setup_root(XentContext *ctx, XentNodeId root) {
	xent_set_protocol(ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_set_semantic_role(ctx, root, XENT_SEMANTIC_CONTAINER);
}

static FluxRefreshData *
refresh_make_runtime(FluxRefreshContainerCreateInfo const *info, XentNodeId root, XentNodeId scroll) {
	FluxNodeData    *root_nd = flux_node_store_get(info->store, root);
	FluxRefreshData *d       = root_nd ? ( FluxRefreshData * ) calloc(1, sizeof(*d)) : NULL;
	if (!root_nd || !d) {
		free(d);
		return NULL;
	}
	d->root            = root;
	d->scroll_child    = scroll;
	d->ctx             = info->ctx;
	d->store           = info->store;
	d->window          = info->window;
	d->direction       = info->direction;
	d->visualizer_size = info->visualizer_size > 0.0f ? info->visualizer_size : FLUX_REFRESH_PULL_DIMENSION;
	d->threshold_ratio = info->threshold_ratio > 0.0f ? info->threshold_ratio : FLUX_REFRESH_EXECUTION_RATIO;
	d->min_opacity     = FLUX_REFRESH_MIN_OPACITY;
	d->overpan_ratio   = FLUX_REFRESH_OVERPAN_RATIO;
	d->start_angle     = refresh_start_angle(info->direction);
	d->state           = FLUX_REFRESH_IDLE;
	d->was_at_zero     = true;
	d->on_refresh      = info->on_refresh;
	d->userdata        = info->userdata;

	root_nd->component_data          = d;
	root_nd->destroy_component_data  = refresh_destroy;
	root_nd->clips_children          = true; /* root inset clip: glyph reveals from behind the edge */
	root_nd->behavior.on_pointer_down    = refresh_on_pointer_down;
	root_nd->behavior.on_pointer_down_ctx = d;
	root_nd->behavior.on_pointer_move    = refresh_on_pointer_move;
	root_nd->behavior.on_pointer_move_ctx = d;
	root_nd->behavior.on_click           = refresh_on_release;
	root_nd->behavior.on_click_ctx       = d;
	root_nd->behavior.on_cancel          = refresh_on_release; /* touch-pan promotion aborts the press */
	root_nd->behavior.on_cancel_ctx      = d;
	return d;
}

XentNodeId flux_create_refresh(FluxRefreshContainerCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_REFRESH);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	XentNodeId scroll = flux_create_scroll(&(FluxContainerCreateInfo) {info->ctx, info->store, root});
	if (scroll == XENT_NODE_INVALID) return root;

	FluxRefreshData *d = refresh_make_runtime(info, root, scroll);
	if (!d) return root;

	refresh_setup_root(info->ctx, root);
	/* The scroll spine fills the container; the visualizer glyph floats over it. */
	xent_set_flex_grow(info->ctx, scroll, 1.0f);
	xent_set_flex_basis(info->ctx, scroll, 0.0f);
	return root;
}

XentNodeId flux_refresh_content_node(FluxNodeStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	return d ? d->scroll_child : XENT_NODE_INVALID;
}

void flux_refresh_set_direction(FluxNodeStore *store, XentNodeId id, FluxPullDirection dir) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d || d->direction == dir) return;
	d->direction   = dir;
	d->start_angle = refresh_start_angle(dir);
	refresh_repaint(d);
}

/* Edge-driven completion, mirroring the WinRT Deferral: true keeps the spinner
 * spinning; false ends the refresh and retracts. Does not re-fire on_refresh. */
void flux_refresh_set_refreshing(FluxNodeStore *store, XentNodeId id, bool refreshing) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d) return;
	if (refreshing) {
		if (d->state != FLUX_REFRESH_REFRESHING) refresh_begin_refresh(d, false);
	}
	else if (d->state == FLUX_REFRESH_REFRESHING) {
		refresh_complete(d);
	}
}

/* Programmatic RequestRefresh(): start the spinner and raise on_refresh. */
void flux_refresh_request_refresh(FluxNodeStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && d->state != FLUX_REFRESH_REFRESHING) refresh_begin_refresh(d, true);
}

/* -------------------------------------------------------------------------
 * Touch over-pull seam (driven by the input layer's touch-pan path).
 *
 * A touch pan clamps the wrapped scroller at its edge; the amount it would have
 * scrolled past that edge is the over-pull that WinUI's InteractionTracker
 * would report as interactionRatio. This reconstructs it from the pre-clamp
 * offsets so real touch pulls drive the same five-state machine as the
 * programmatic path (the mouse path uses the pointer behaviors above).
 * ---------------------------------------------------------------------- */

static float refresh_overpull_px(FluxRefreshData const *d, float new_sx, float new_sy, float max_x, float max_y) {
	switch (d->direction) {
	case FLUX_PULL_TOP_TO_BOTTOM: return new_sy < 0.0f ? -new_sy : 0.0f;
	case FLUX_PULL_BOTTOM_TO_TOP: return new_sy > max_y ? new_sy - max_y : 0.0f;
	case FLUX_PULL_LEFT_TO_RIGHT: return new_sx < 0.0f ? -new_sx : 0.0f;
	case FLUX_PULL_RIGHT_TO_LEFT: return new_sx > max_x ? new_sx - max_x : 0.0f;
	default:                      return 0.0f;
	}
}

bool flux_refresh_touch_pull(FluxNodeStore *store, XentNodeId id, float new_sx, float new_sy, float max_x, float max_y) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d || d->state == FLUX_REFRESH_REFRESHING) return false;

	float overpull = refresh_overpull_px(d, new_sx, new_sy, max_x, max_y);
	if (overpull > 0.0f) {
		/* First over-pull arms the gesture: an over-pull only exists at the edge,
		 * which is exactly WinUI's isInteractingForRefresh condition. */
		if (!d->pull_tracking) {
			d->interacting_for_refresh = true;
			d->was_at_zero             = true;
		}
		d->pull_tracking = true;
		refresh_on_ratio(d, overpull / d->visualizer_size);
		return true;
	}
	if (d->pull_tracking) refresh_on_ratio(d, 0.0f); /* back inside the edge: retract */
	return false;
}

void flux_refresh_touch_release(FluxNodeStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && d->pull_tracking) refresh_on_lift(d);
}
