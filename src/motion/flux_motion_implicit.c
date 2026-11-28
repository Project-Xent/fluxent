#include "motion/flux_motion_internal.h"
#include "fluxent/flux_theme.h"

#include <stdlib.h>
#include <string.h>

static float const  kImpCP1X             = 0.8f;
static float const  kImpCP1Y             = 0.0f;
static float const  kImpCP2X             = 0.2f;
static float const  kImpCP2Y             = 1.0f;

static double const kDefaultScalarDur    = 0.3;
static double const kDefaultTransformDur = 0.3;
static double const kDefaultBrushDur     = 0.15;

static void         init_default_state(struct FluxMotionState *st) {
	st->cfg.opacity_duration   = kDefaultScalarDur;
	st->cfg.transform_duration = kDefaultTransformDur;
	st->cfg.brush_duration     = kDefaultBrushDur;
	st->target_opacity         = 1.0f;
	st->target_sx              = 1.0f;
	st->target_sy              = 1.0f;
}

struct FluxMotionState *flux_motion_ensure_state(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return NULL;
	if (nd->motion) return nd->motion;
	struct FluxMotionState *st = ( struct FluxMotionState * ) calloc(1, sizeof(*st));
	if (!st) return NULL;
	init_default_state(st);
	nd->motion = st;
	flux_motion_track_node(store, node);
	return st;
}

void flux_motion_state_free(struct FluxMotionState *st) {
	if (!st) return;
	free(st->connected_key);
	free(st->coord_key);
	free(st);
}

typedef struct ImplicitSyncEnv {
	struct FluxMotionState *st;
	double                  now;
	bool                    animate;
} ImplicitSyncEnv;

static void
sync_float_prop(ImplicitSyncEnv const *env, FluxMotionTween *tw, float target, FluxMotionPropMask bit, double dur) {
	bool an = env->animate && ((env->st->cfg.enabled_mask & ( uint32_t ) bit) != 0u);
	flux_motion_tween_set(tw, target, env->now, dur, kImpCP1X, kImpCP1Y, kImpCP2X, kImpCP2Y, an);
}

static void sync_color_prop(
  ImplicitSyncEnv const *env, FluxMotionColorTween *tw, FluxColor target, FluxMotionPropMask bit, double dur
) {
	bool an = env->animate && ((env->st->cfg.enabled_mask & ( uint32_t ) bit) != 0u);
	flux_motion_color_set(tw, target, env->now, dur, an);
}

static void resolve_implicit_durations(FluxImplicitCfg const *cfg, double *op_d, double *tr_d, double *br_d) {
	*op_d = cfg->opacity_duration > 0.0 ? cfg->opacity_duration : kDefaultScalarDur;
	*tr_d = cfg->transform_duration > 0.0 ? cfg->transform_duration : kDefaultTransformDur;
	*br_d = cfg->brush_duration > 0.0 ? cfg->brush_duration : kDefaultBrushDur;
}

void flux_motion_sync_implicit(FluxStore *store, XentNodeId node, FluxNodeData *nd, bool snap_only) {
	if (!nd || !nd->motion) return;
	struct FluxMotionState  *st  = nd->motion;
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, flux_store_ctx(store));
	flux_motion_track_node(store, node);
	double          now = flux_motion_now(sys);
	bool            en  = sys ? sys->anims_enabled : flux_animations_enabled();
	ImplicitSyncEnv env = {.st = st, .now = now, .animate = !snap_only && en};
	double          op_d, tr_d, br_d;
	resolve_implicit_durations(&st->cfg, &op_d, &tr_d, &br_d);

	sync_float_prop(&env, &st->opacity, st->target_opacity, FLUX_MOTION_PROP_OPACITY, op_d);
	sync_float_prop(&env, &st->tx, st->target_tx, FLUX_MOTION_PROP_TRANSLATE_X, tr_d);
	sync_float_prop(&env, &st->ty, st->target_ty, FLUX_MOTION_PROP_TRANSLATE_Y, tr_d);
	sync_float_prop(&env, &st->sx, st->target_sx, FLUX_MOTION_PROP_SCALE_X, tr_d);
	sync_float_prop(&env, &st->sy, st->target_sy, FLUX_MOTION_PROP_SCALE_Y, tr_d);
	sync_float_prop(&env, &st->rotation, st->target_rot, FLUX_MOTION_PROP_ROTATION, op_d);
	sync_float_prop(&env, &st->corner_radius, nd->visuals.corner_radius, FLUX_MOTION_PROP_CORNER_RADIUS, br_d);
	sync_color_prop(&env, &st->background, nd->visuals.background, FLUX_MOTION_PROP_BACKGROUND, br_d);
	sync_color_prop(&env, &st->border_color, nd->visuals.border_color, FLUX_MOTION_PROP_BORDER_COLOR, br_d);
}

static bool step_implicit_tweens(struct FluxMotionState *st, double now, bool animate) {
	if (!animate) {
		st->opacity.active        = false;
		st->opacity.current       = st->opacity.target;
		st->tx.active             = false;
		st->tx.current            = st->tx.target;
		st->ty.active             = false;
		st->ty.current            = st->ty.target;
		st->sx.active             = false;
		st->sx.current            = st->sx.target;
		st->sy.active             = false;
		st->sy.current            = st->sy.target;
		st->rotation.active       = false;
		st->rotation.current      = st->rotation.target;
		st->corner_radius.active  = false;
		st->corner_radius.current = st->corner_radius.target;
		st->background.active     = false;
		st->background.current    = st->background.target;
		st->border_color.active   = false;
		st->border_color.current  = st->border_color.target;
		return false;
	}
	bool act  = false;
	act      |= flux_motion_tween_step(&st->opacity, now);
	act      |= flux_motion_tween_step(&st->tx, now);
	act      |= flux_motion_tween_step(&st->ty, now);
	act      |= flux_motion_tween_step(&st->sx, now);
	act      |= flux_motion_tween_step(&st->sy, now);
	act      |= flux_motion_tween_step(&st->rotation, now);
	act      |= flux_motion_tween_step(&st->corner_radius, now);
	act      |= flux_motion_color_step(&st->background, now);
	act      |= flux_motion_color_step(&st->border_color, now);
	return act;
}

bool flux_motion_step_implicit(struct FluxMotionState *st, FluxNodeData *nd, double now, bool animate) {
	bool act               = step_implicit_tweens(st, now, animate);
	nd->motion_opacity     = st->opacity.initialized ? st->opacity.current : 1.0f;
	nd->motion_translate_x = st->tx.initialized ? st->tx.current : 0.0f;
	nd->motion_translate_y = st->ty.initialized ? st->ty.current : 0.0f;
	nd->render_scale_x     = st->sx.initialized ? st->sx.current : 1.0f;
	nd->render_scale_y     = st->sy.initialized ? st->sy.current : 1.0f;
	nd->render_rotate_z    = st->rotation.initialized ? st->rotation.current : 0.0f;
	return act;
}

void flux_motion_patch_snap(FluxNodeData const *nd, FluxRenderSnap *snap) {
	struct FluxMotionState const *st = nd ? nd->motion : NULL;
	if (!st) return;
	if (st->background.active) snap->background = st->background.current;
	if (st->border_color.active) snap->border_color = st->border_color.current;
	if (st->corner_radius.active) snap->corner_radius = st->corner_radius.current;
}

void flux_motion_set_implicit(FluxStore *store, XentNodeId node, FluxImplicitCfg const *cfg) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	if (cfg) st->cfg = *cfg;
	else st->cfg.enabled_mask = 0u;
	FluxNodeData *nd = flux_store_find(store, node);
	flux_motion_sync_implicit(store, node, nd, false);
}

void flux_motion_set_opacity(FluxStore *store, XentNodeId node, float value) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	st->target_opacity = value;
	flux_motion_sync_implicit(store, node, flux_store_find(store, node), false);
}

void flux_motion_set_translation(FluxStore *store, XentNodeId node, float x, float y) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	st->target_tx = x;
	st->target_ty = y;
	flux_motion_sync_implicit(store, node, flux_store_find(store, node), false);
}

void flux_motion_set_scale(FluxStore *store, XentNodeId node, float sx, float sy) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	st->target_sx = sx;
	st->target_sy = sy;
	flux_motion_sync_implicit(store, node, flux_store_find(store, node), false);
}

void flux_motion_set_rotation(FluxStore *store, XentNodeId node, float degrees) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	st->target_rot = degrees;
	flux_motion_sync_implicit(store, node, flux_store_find(store, node), false);
}

void flux_motion_set_pivot(FluxStore *store, XentNodeId node, float px, float py) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return;
	nd->render_pivot_x = px;
	nd->render_pivot_y = py;
}
