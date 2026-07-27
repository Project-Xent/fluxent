#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_anim_driver.h"

#include "controls/factory/flux_init_container.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_window.h"
#include "controls/behavior/flux_refresh_internal.h"
#include "controls/data/flux_scroll_data.h"
#include <xtk/xtk_catalog.h>

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

static FluxRefreshData *refresh_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_REFRESH) return NULL;
	return ( FluxRefreshData * ) nd->component_data;
}

static void refresh_repaint(FluxRefreshData *d) {
	HWND h = d->window ? flux_window_hwnd(d->window) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float refresh_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static float refresh_offset_sign(FluxPullDirection dir) {
	return (dir == FLUX_PULL_BOTTOM_TO_TOP || dir == FLUX_PULL_RIGHT_TO_LEFT) ? -1.0f : 1.0f;
}

static void refresh_apply_content_offset(FluxRefreshData *d, float offset) {
	FluxNodeData *nd  = flux_store_find(d->store, d->scroll_child);
	d->content_offset = offset;
	if (!nd) return;

	nd->render_translate_x = 0.0f;
	nd->render_translate_y = 0.0f;
	if (d->direction == FLUX_PULL_LEFT_TO_RIGHT || d->direction == FLUX_PULL_RIGHT_TO_LEFT)
		nd->render_translate_x = offset;
	else nd->render_translate_y = offset;
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
	if (d->offset_animating) {
		float elapsed = ( float ) (now - d->offset_start_tick);
		float t       = refresh_clamp01(elapsed / FLUX_REFRESH_SETTLE_MS);
		refresh_apply_content_offset(d, d->offset_from + (d->offset_to - d->offset_from) * t);
		d->offset_animating = t < 1.0f;
		animating           = d->offset_animating;
	}
	if (d->state == FLUX_REFRESH_REFRESHING) animating = true; /* perpetual spin */
	if (refresh_pop_active(d, now)) animating = true;          /* one-shot pop */
	refresh_repaint(d);
	if (!animating) d->anim_registered = false;
	return animating;
}

static void refresh_anim_start(FluxRefreshData *d) {
	d->anim_registered = true;
	flux_anim_register(d, refresh_step);
}

static void refresh_animate_content_offset(FluxRefreshData *d, float target) {
	d->offset_from       = d->content_offset;
	d->offset_to         = target;
	d->offset_start_tick = GetTickCount();
	d->offset_animating  = d->offset_from != target;
	if (d->offset_animating) refresh_anim_start(d);
	else refresh_apply_content_offset(d, target);
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

static void refresh_report(FluxRefreshData *d, XtkEventCause cause) {
	XtkHost *host = flux_store_host(d->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_REFRESH,
		  .target         = ( XtkCtrl ) d->root,
		  .source         = ( XtkCtrl ) d->root,
		  .current_target = ( XtkCtrl ) d->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = cause,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = ( int32_t ) d->direction},
		};
		flux_host_route(host, &event);
	}
	if (d->on_refresh) d->on_refresh(d->userdata, ( int ) d->direction);
}

static void refresh_begin_refresh(FluxRefreshData *d, XtkEventCause cause) {
	d->interacting_for_refresh = false;
	d->pull_tracking           = false;
	refresh_enter_state(d, FLUX_REFRESH_REFRESHING);
	refresh_animate_content_offset(d, refresh_offset_sign(d->direction) * d->visualizer_size * d->threshold_ratio);
	if (cause) refresh_report(d, cause);
}

static void refresh_complete(FluxRefreshData *d) {
	d->interaction_ratio = 0.0f;
	d->was_at_zero       = true;
	refresh_enter_state(d, FLUX_REFRESH_IDLE);
	refresh_animate_content_offset(d, 0.0f);
}

/* The armed pull's state transition for ratio r and the previous zero state. */
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

static void refresh_on_ratio(FluxRefreshData *d, float r) {
	r = refresh_clamp01(r);

	if (d->interacting_for_refresh) refresh_enter_state(d, refresh_armed_next_state(d, r, d->was_at_zero));
	else if (d->state != FLUX_REFRESH_REFRESHING)
		refresh_enter_state(d, r > 0.0f ? FLUX_REFRESH_PEEKING : FLUX_REFRESH_IDLE);

	d->interaction_ratio = r;
	d->was_at_zero       = (r == 0.0f);
	d->offset_animating  = false;
	refresh_apply_content_offset(d, refresh_offset_sign(d->direction) * d->visualizer_size * r);
	refresh_repaint(d);
}

static void refresh_on_lift(FluxRefreshData *d) {
	int state                  = d->state;
	d->interacting_for_refresh = false;
	d->pull_tracking           = false;
	if (state == FLUX_REFRESH_PENDING) refresh_begin_refresh(d, XTK_EVENT_CAUSE_USER);
	else if (state != FLUX_REFRESH_REFRESHING) refresh_complete(d);
}

/* -------------------------------------------------------------------------
 * Best-effort over-pull gesture (see file header for the limitation).
 * ---------------------------------------------------------------------- */

static void refresh_destroy(void *component_data) {
	flux_anim_unregister(component_data);
	free(component_data);
}

/* -------------------------------------------------------------------------
 * Construction.
 * ---------------------------------------------------------------------- */

static void refresh_setup_root(XentCtx *ctx, XentNodeId root) {
	xent_setproto(ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_sem_setrole(ctx, root, XENT_SEM_ROLE_CONTAINER);
}

static FluxRefreshData *refresh_make_runtime(FluxRefreshContainerDesc const *info, XentNodeId root, XentNodeId scroll) {
	FluxNodeData    *root_nd = flux_store_find(info->store, root);
	FluxRefreshData *d       = root_nd ? ( FluxRefreshData * ) calloc(1, sizeof(*d)) : NULL;
	if (!root_nd || !d) {
		free(d);
		return NULL;
	}
	d->root                 = root;
	d->scroll_child         = scroll;
	d->ctx                  = info->ctx;
	d->store                = info->store;
	d->window               = info->window;
	d->direction            = info->direction;
	d->visualizer_size      = info->visualizer_size > 0.0f ? info->visualizer_size : FLUX_REFRESH_PULL_DIMENSION;
	d->threshold_ratio      = info->threshold_ratio > 0.0f ? info->threshold_ratio : FLUX_REFRESH_EXECUTION_RATIO;
	d->min_opacity          = FLUX_REFRESH_MIN_OPACITY;
	d->overpan_ratio        = FLUX_REFRESH_OVERPAN_RATIO;
	d->start_angle          = refresh_start_angle(info->direction);
	d->state                = FLUX_REFRESH_IDLE;
	d->was_at_zero          = true;
	d->on_refresh           = info->on_refresh;
	d->userdata             = info->userdata;

	root_nd->component_data = d;
	root_nd->destroy_component_data = refresh_destroy;
	root_nd->clips_children         = true; /* root inset clip: glyph reveals from behind the edge */
	return d;
}

XentNodeId flux_refresh_init(FluxRefreshContainerDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_REFRESH);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	XentNodeId scroll = flux_scroll_init(&(FluxContainerDesc) {info->ctx, info->store, root});
	if (scroll == XENT_NODE_INVALID) return root;

	FluxRefreshData *d = refresh_make_runtime(info, root, scroll);
	if (!d) return root;

	refresh_setup_root(info->ctx, root);
	/* The scroll spine fills the container; the visualizer glyph floats over it. */
	xent_setgrow(info->ctx, scroll, 1.0f);
	xent_setbasis(info->ctx, scroll, 0.0f);
	return root;
}

XentNodeId flux_refresh_content_node(FluxStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	return d ? d->scroll_child : XENT_NODE_INVALID;
}

void flux_refresh_set_direction(FluxStore *store, XentNodeId id, FluxPullDirection dir) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d || d->direction == dir) return;
	float offset = d->content_offset;
	refresh_apply_content_offset(d, 0.0f);
	d->direction   = dir;
	d->start_angle = refresh_start_angle(dir);
	refresh_apply_content_offset(d, offset);
	refresh_repaint(d);
}

void flux_refresh_set_visualizer_size(FluxStore *store, XentNodeId id, float size) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && size > 0.0f) d->visualizer_size = size;
}

void flux_refresh_set_threshold_ratio(FluxStore *store, XentNodeId id, float ratio) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && ratio > 0.0f) d->threshold_ratio = ratio;
}

/* Edge-driven completion, mirroring the WinRT Deferral: true keeps the spinner
 * spinning; false ends the refresh and retracts. Does not re-fire on_refresh. */
void flux_refresh_set_refreshing(FluxStore *store, XentNodeId id, bool refreshing) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d) return;
	if (refreshing) {
		if (d->state != FLUX_REFRESH_REFRESHING) refresh_begin_refresh(d, 0);
	}
	else if (d->state == FLUX_REFRESH_REFRESHING) { refresh_complete(d); }
}

/* Programmatic RequestRefresh(): start the spinner and raise on_refresh. */
void flux_refresh_request_refresh(FluxStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && d->state != FLUX_REFRESH_REFRESHING) refresh_begin_refresh(d, XTK_EVENT_CAUSE_PROGRAM);
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
	case FLUX_PULL_TOP_TO_BOTTOM : return new_sy < 0.0f ? -new_sy : 0.0f;
	case FLUX_PULL_BOTTOM_TO_TOP : return new_sy > max_y ? new_sy - max_y : 0.0f;
	case FLUX_PULL_LEFT_TO_RIGHT : return new_sx < 0.0f ? -new_sx : 0.0f;
	case FLUX_PULL_RIGHT_TO_LEFT : return new_sx > max_x ? new_sx - max_x : 0.0f;
	default                      : return 0.0f;
	}
}

bool flux_refresh_touch_pull(FluxStore *store, XentNodeId id, float new_sx, float new_sy, float max_x, float max_y) {
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

void flux_refresh_touch_release(FluxStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	if (d && d->pull_tracking) refresh_on_lift(d);
}

void flux_refresh_touch_cancel(FluxStore *store, XentNodeId id) {
	FluxRefreshData *d = refresh_data(store, id);
	if (!d || !d->pull_tracking) return;
	d->interacting_for_refresh = false;
	d->pull_tracking           = false;
	refresh_complete(d);
}
