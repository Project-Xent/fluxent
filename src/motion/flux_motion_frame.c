#include "motion/flux_motion_internal.h"

#include <math.h>
#include <stdlib.h>

static float  clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static double frame_duration_for(FluxNavTransitionKind kind, bool outgoing, bool is_back) {
	if (kind == FLUX_NAV_TRANSITION_SUPPRESS) return 0.0;
	if (kind == FLUX_NAV_TRANSITION_DRILL_IN) return outgoing ? 0.10 : (is_back ? 0.333 : 0.783);
	if (kind == FLUX_NAV_TRANSITION_SLIDE_BOTTOM) return (outgoing || is_back) ? 0.25 : 0.60;
	return outgoing ? 0.15 : 0.45;
}

static void frame_eval_entrance(double el, bool out, bool back, float *op, float *ty) {
	if (out && !back) {
		*op = 1.0f - flux_motion_seg(el, 0.0, 0.15, 0.7f, 0.0f, 1.0f, 0.5f);
		return;
	}
	if (!out && !back) {
		*op = el >= 0.15 ? 1.0f : 0.0f;
		*ty = 140.0f * (1.0f - flux_motion_seg(el, 0.15, 0.30, 0.1f, 0.9f, 0.2f, 1.0f));
		return;
	}
	if (out && back) {
		*op = el >= 0.15 ? 0.0f : 1.0f;
		*ty = 140.0f * flux_motion_seg(el, 0.0, 0.15, 0.7f, 0.0f, 1.0f, 0.5f);
		return;
	}
	*op = el < 0.15 ? 0.0f : flux_motion_seg(el, 0.15, 0.30, 0.1f, 0.9f, 0.2f, 1.0f);
}

static void frame_eval_slide_x(double el, bool out, bool back, float dir, float *op, float *tx) {
	*op = out ? (el < 0.15 ? 1.0f : 0.0f) : (el >= 0.15 ? 1.0f : 0.0f);
	if (out && !back) {
		*tx = -dir * 150.0f * flux_motion_seg(el, 0.0, 0.15, 0.7f, 0.0f, 1.0f, 0.5f);
		return;
	}
	if (!out && !back) {
		*tx = dir * 200.0f * (1.0f - flux_motion_seg(el, 0.15, 0.30, 0.1f, 0.9f, 0.2f, 1.0f));
		return;
	}
	if (out && back) {
		*tx = dir * 200.0f * flux_motion_seg(el, 0.0, 0.15, 0.7f, 0.0f, 1.0f, 0.5f);
		return;
	}
	*tx = -dir * 150.0f * (1.0f - flux_motion_seg(el, 0.15, 0.30, 0.1f, 0.9f, 0.2f, 1.0f));
}

static void frame_eval_slide_bottom(double el, bool out, bool back, float *op, float *ty) {
	*op = out ? (el < 0.25 ? 1.0f : 0.0f) : (el >= 0.25 ? 1.0f : 0.0f);
	if (!out && !back) {
		float t = clamp01(( float ) ((el - 0.25) / 0.35));
		*ty     = 200.0f * (1.0f - flux_motion_exp_out(t, 6.0f));
	}
	else if (out && back) {
		float t = clamp01(( float ) (el / 0.60));
		*ty     = 200.0f * flux_motion_exp_in(t, 6.0f);
	}
}

static void frame_eval_drill(double el, bool out, bool back, float *op, float *sc) {
	if (out && !back) {
		*op = 1.0f - flux_motion_seg(el, 0.0, 0.10, 0.17f, 0.17f, 0.0f, 1.0f);
		*sc = flux_motion_lerpf(1.0f, 1.04f, flux_motion_seg(el, 0.0, 0.10, 0.1f, 0.9f, 0.2f, 1.0f));
		return;
	}
	if (!out && !back) {
		*op = flux_motion_seg(el, 0.0, 0.333, 0.17f, 0.17f, 0.0f, 1.0f);
		*sc = flux_motion_lerpf(0.94f, 1.0f, flux_motion_seg(el, 0.0, 0.783, 0.1f, 0.9f, 0.2f, 1.0f));
		return;
	}
	if (out && back) {
		*op = 1.0f - flux_motion_seg(el, 0.0, 0.10, 0.17f, 0.17f, 0.0f, 1.0f);
		*sc = flux_motion_lerpf(1.0f, 0.96f, flux_motion_seg(el, 0.0, 0.10, 0.1f, 0.9f, 0.2f, 1.0f));
		return;
	}
	*op = flux_motion_seg(el, 0.0, 0.333, 0.17f, 0.17f, 0.0f, 1.0f);
	*sc = flux_motion_lerpf(1.06f, 1.0f, flux_motion_seg(el, 0.0, 0.333, 0.12f, 0.0f, 0.0f, 1.0f));
}

static void
eval_frame_track(FluxFrameAnim const *fa, FluxNodeData *nd, double el, float *op, float *tx, float *ty, float *sc) {
	if (fa->kind == FLUX_NAV_TRANSITION_ENTRANCE) {
		frame_eval_entrance(el, fa->is_outgoing, fa->is_back, op, ty);
		return;
	}
	if (fa->kind == FLUX_NAV_TRANSITION_SLIDE_RIGHT || fa->kind == FLUX_NAV_TRANSITION_SLIDE_LEFT) {
		float dir = fa->kind == FLUX_NAV_TRANSITION_SLIDE_LEFT ? -1.0f : 1.0f;
		frame_eval_slide_x(el, fa->is_outgoing, fa->is_back, dir, op, tx);
		return;
	}
	if (fa->kind == FLUX_NAV_TRANSITION_SLIDE_BOTTOM) {
		frame_eval_slide_bottom(el, fa->is_outgoing, fa->is_back, op, ty);
		return;
	}
	if (fa->kind == FLUX_NAV_TRANSITION_DRILL_IN) {
		frame_eval_drill(el, fa->is_outgoing, fa->is_back, op, sc);
		nd->render_pivot_x = 0.5f;
		nd->render_pivot_y = 0.5f;
	}
}

bool flux_motion_step_frame(struct FluxMotionState *st, FluxNodeData *nd, double now, bool animate) {
	FluxFrameAnim *fa = &st->frame_anim;
	if (!fa->active) return false;
	double el = now - fa->start_time;
	if (!animate || el >= fa->duration) {
		if (fa->is_outgoing) nd->motion_opacity = 0.0f;
		fa->active = false;
		return false;
	}
	if (el < 0.0) el = 0.0;

	float op = 1.0f, tx = 0.0f, ty = 0.0f, sc = 1.0f;
	eval_frame_track(fa, nd, el, &op, &tx, &ty, &sc);

	nd->motion_opacity     *= op;
	nd->motion_translate_x += tx;
	nd->motion_translate_y += ty;
	nd->render_scale_x     *= sc;
	nd->render_scale_y     *= sc;
	return true;
}

static void start_one_frame_anim(
  FluxStore *store, XentCtx *ctx, XentNodeId node, FluxNavTransitionKind kind, bool outgoing, bool is_back, double now,
  double *out_dur
) {
	double dur = frame_duration_for(kind, outgoing, is_back);
	if (out_dur) *out_dur = dur;
	if (dur <= 0.0 || node == XENT_NODE_INVALID) return;
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	flux_motion_track_node(store, node);
	st->frame_anim = (FluxFrameAnim) {
	  .active      = true,
	  .is_outgoing = outgoing,
	  .is_back     = is_back,
	  .kind        = kind,
	  .start_time  = now,
	  .duration    = dur,
	};
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd) flux_motion_step_frame(st, nd, now, true);
	( void ) ctx;
}

static void drop_detached_node(FluxStore *store, XentCtx *ctx, XentNodeId node) {
	if (node != XENT_NODE_INVALID && xent_node_valid(ctx, node)) flux_tree_drop(store, node);
}

void flux_motion_start_frame_pair(
  FluxStore *store, XentCtx *ctx, FluxNodeData *frame_nd, XentNodeId outgoing, XentNodeId incoming
) {
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	if (sys && frame_nd) flux_motion_retire_drop_parent(sys, frame_nd->node_id);
	FluxFrameControlData *fd = frame_nd ? ( FluxFrameControlData * ) frame_nd->component_data : NULL;
	bool skip = !fd || !sys || !sys->anims_enabled || fd->active_transition == FLUX_NAV_TRANSITION_SUPPRESS;
	if (skip) {
		drop_detached_node(store, ctx, outgoing);
		return;
	}

	double now = flux_motion_now(sys);
	if (outgoing != XENT_NODE_INVALID) {
		double out_dur = 0.0;
		start_one_frame_anim(store, ctx, outgoing, fd->active_transition, true, fd->active_back, now, &out_dur);
		if (out_dur <= 0.0 || !flux_motion_retire_add(sys, frame_nd->node_id, outgoing, true, now + out_dur))
			drop_detached_node(store, ctx, outgoing);
	}
	if (incoming != XENT_NODE_INVALID)
		start_one_frame_anim(store, ctx, incoming, fd->active_transition, false, fd->active_back, now, NULL);
}
