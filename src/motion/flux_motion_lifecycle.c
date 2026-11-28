#include "motion/flux_motion_internal.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_theme.h"
#include "runtime/flux_time.h"

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <vsanimation.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool track_node(struct FluxMotionSystem *sys, XentNodeId node);
static void untrack_node(struct FluxMotionSystem *sys, XentNodeId node);

typedef struct MotionSysInternals {
	struct FluxMotionSystem pub;
	XentNodeId             *tracked;
	uint32_t                tracked_count;
	uint32_t                tracked_cap;
} MotionSysInternals;

struct FluxMotionSystem *flux_motion_system_create(FluxStore *store, XentCtx *ctx) {
	MotionSysInternals *sys = ( MotionSysInternals * ) calloc(1, sizeof(*sys));
	if (!sys) return NULL;
	sys->pub.store         = store;
	sys->pub.ctx           = ctx;
	sys->pub.now           = flux_perf_seconds(flux_perf_now());
	sys->pub.anims_enabled = flux_animations_enabled();
	return &sys->pub;
}

struct FluxMotionSystem *flux_motion_ensure_system(FluxStore *store, XentCtx *ctx) {
	if (!store) return NULL;
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (sys) {
		if (ctx) sys->ctx = ctx;
		return sys;
	}
	sys = flux_motion_system_create(store, ctx);
	if (sys) flux_store_setmotion(store, sys);
	return sys;
}

double flux_motion_now(struct FluxMotionSystem *sys) {
	double now = flux_perf_seconds(flux_perf_now());
	if (sys) sys->now = now;
	return now;
}

static void drop_all_retiring(struct FluxMotionSystem *sys) {
	for (uint32_t i = 0; i < sys->retiring_count; i++) {
		XentNodeId n = sys->retiring [i].node;
		if (n != XENT_NODE_INVALID && sys->ctx && xent_node_valid(sys->ctx, n)) flux_tree_drop(sys->store, n);
	}
	free(sys->retiring);
	sys->retiring       = NULL;
	sys->retiring_count = 0;
	sys->retiring_cap   = 0;
}

void flux_motion_system_destroy(struct FluxMotionSystem *sys) {
	if (!sys) return;
	MotionSysInternals *in = ( MotionSysInternals * ) sys;
	drop_all_retiring(sys);
	for (uint32_t i = 0; i < sys->flight_count; i++) {
		free(sys->flights [i].key);
		free(sys->flights [i].coords);
		if (sys->flights [i].ghost_node != XENT_NODE_INVALID
		    && sys->ctx
		    && xent_node_valid(sys->ctx, sys->flights [i].ghost_node))
			flux_tree_drop(sys->store, sys->flights [i].ghost_node);
	}
	free(sys->flights);
	for (uint32_t i = 0; i < sys->prepared_count; i++) {
		free(sys->prepared [i].key);
		if (sys->prepared [i].ghost_node != XENT_NODE_INVALID
		    && sys->ctx
		    && xent_node_valid(sys->ctx, sys->prepared [i].ghost_node))
			flux_tree_drop(sys->store, sys->prepared [i].ghost_node);
	}
	free(sys->prepared);
	free(in->tracked);
	free(in);
}

static bool track_node(struct FluxMotionSystem *sys, XentNodeId node) {
	if (!sys || node == XENT_NODE_INVALID) return false;
	MotionSysInternals *in = ( MotionSysInternals * ) sys;
	for (uint32_t i = 0; i < in->tracked_count; i++)
		if (in->tracked [i] == node) return true;
	if (in->tracked_count == in->tracked_cap) {
		uint32_t    cap  = in->tracked_cap ? in->tracked_cap * 2u : 32u;
		XentNodeId *next = ( XentNodeId * ) realloc(in->tracked, sizeof(XentNodeId) * cap);
		if (!next) return false;
		in->tracked     = next;
		in->tracked_cap = cap;
	}
	in->tracked [in->tracked_count++] = node;
	return true;
}

bool flux_motion_track_node(FluxStore *store, XentNodeId node) {
	if (!store || node == XENT_NODE_INVALID) return false;
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, flux_store_ctx(store));
	return track_node(sys, node);
}

static void untrack_node(struct FluxMotionSystem *sys, XentNodeId node) {
	if (!sys || node == XENT_NODE_INVALID) return;
	MotionSysInternals *in = ( MotionSysInternals * ) sys;
	for (uint32_t i = 0; i < in->tracked_count; i++) {
		if (in->tracked [i] != node) continue;
		if (i + 1 < in->tracked_count)
			memmove(&in->tracked [i], &in->tracked [i + 1], sizeof(XentNodeId) * (in->tracked_count - i - 1));
		in->tracked_count--;
		return;
	}
}

static void remove_retiring_entry(struct FluxMotionSystem *sys, uint32_t i) {
	if (i + 1u < sys->retiring_count)
		memmove(
		  &sys->retiring [i], &sys->retiring [i + 1u], sizeof(FluxRetiringEntry) * (sys->retiring_count - i - 1u)
		);
	sys->retiring_count--;
}

void flux_motion_on_node_removed(FluxStore *store, XentNodeId node) {
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (!sys || node == XENT_NODE_INVALID) return;
	untrack_node(sys, node);
	flux_motion_connected_drop_node(sys, node);
	for (uint32_t i = 0; i < sys->retiring_count;) {
		bool drop = sys->retiring [i].node == node || sys->retiring [i].parent == node;
		if (drop) remove_retiring_entry(sys, i);
		else i++;
	}
}

typedef struct ThemeSbSpec {
	int   in_sb, in_tgt;
	int   out_sb, out_tgt;
	float in_y;
} ThemeSbSpec;

static void theme_sb_target(FluxThemeTransitionKind kind, bool is_exit, int *sb, int *tgt, float *def_y) {
	static ThemeSbSpec const kSpecs [] = {
	  [FLUX_THEME_TRANSITION_NONE]     = {TAS_ENTERPAGE,         TA_ENTERPAGE_TARGET,           TAS_FADEOUT,           TA_FADEOUT_HIDDEN,             0.0f },
	  [FLUX_THEME_TRANSITION_ENTRANCE] = {TAS_ENTERPAGE,         TA_ENTERPAGE_TARGET,           TAS_FADEOUT,           TA_FADEOUT_HIDDEN,             28.0f},
	  [FLUX_THEME_TRANSITION_ADD_DELETE]
	  = {TAS_ADDTOGRID,         TA_ADDTOGRID_ADDED,            TAS_DELETEFROMGRID,    TA_DELETEFROMGRID_DELETED,     28.0f},
	  [FLUX_THEME_TRANSITION_CONTENT]
	  = {TAS_TRANSITIONCONTENT, TA_TRANSITIONCONTENT_INCOMING, TAS_TRANSITIONCONTENT, TA_TRANSITIONCONTENT_OUTGOING, 28.0f},
	  [FLUX_THEME_TRANSITION_REORDER] = {TAS_REPOSITION,        TA_REPOSITION_TARGET,          TAS_FADEOUT,           TA_FADEOUT_HIDDEN,             0.0f },
	  [FLUX_THEME_TRANSITION_POPUP]   = {TAS_SHOWPOPUP,         TA_SHOWPOPUP_TARGET,           TAS_HIDEPOPUP,         TA_HIDEPOPUP_TARGET,           40.0f},
	};
	uint32_t           idx = ( uint32_t ) kind;
	ThemeSbSpec const *sp  = idx < sizeof(kSpecs) / sizeof(kSpecs [0]) ? &kSpecs [idx] : &kSpecs [1];
	*sb                    = is_exit ? sp->out_sb : sp->in_sb;
	*tgt                   = is_exit ? sp->out_tgt : sp->in_tgt;
	*def_y                 = is_exit ? 0.0f : sp->in_y;
}

static void
theme_resolve_timing(FluxThemeTransitionKind kind, bool is_exit, uint32_t idx, double *delay, double *dur, float *fy) {
	int   sb = 0, tgt = 0;
	float def_y = 0.0f;
	theme_sb_target(kind, is_exit, &sb, &tgt, &def_y);
	FluxThemeAnim fallback = {
	  .delay    = 0.0,
	  .duration = is_exit ? 0.167 : 0.30,
	  .x1       = 0.1f,
	  .y1       = 0.9f,
	  .x2       = 0.2f,
	  .y2       = 1.0f,
	  .from     = is_exit ? 1.0f : 0.0f,
	  .to       = is_exit ? 0.0f : 1.0f,
	};
	FluxThemeAnim    anim = flux_themeanim_get(sb, tgt, FLUX_THEMEANIM_OPACITY, fallback);
	FluxThemeStagger sg   = flux_themeanim_stagger(
	  sb, tgt, (FluxThemeStagger) {.valid = true, .delay_ms = 40.0, .maximum_ms = 240.0, .reduce = 0.85}
	);
	*delay = anim.delay + (is_exit ? 0.0 : flux_themeanim_stagger_delay(&sg, idx));
	*dur   = anim.duration > 0.0 ? anim.duration : (is_exit ? 0.167 : 0.30);
	*fy    = def_y;
}

static bool step_theme_enter(struct FluxMotionState *st, FluxNodeData *nd, double now, bool animate) {
	FluxThemeEnterAnim *te = &st->theme_enter;
	if (!te->active) return false;
	double el      = now - te->start_time - te->delay;
	bool   reorder = te->kind == FLUX_THEME_TRANSITION_REORDER;
	if (!animate || el >= te->duration) {
		if (te->is_exit && !reorder) nd->motion_opacity = 0.0f;
		te->active = false;
		return false;
	}
	if (el <= 0.0) {
		if (!reorder) nd->motion_opacity *= te->is_exit ? 1.0f : 0.0f;
		nd->motion_translate_x += te->from_x;
		nd->motion_translate_y += te->from_y;
		return true;
	}
	float p = flux_motion_bezier(( float ) (el / te->duration), 0.1f, 0.9f, 0.2f, 1.0f);
	if (!reorder) nd->motion_opacity *= te->is_exit ? (1.0f - p) : p;
	nd->motion_translate_x += te->from_x * (1.0f - p);
	nd->motion_translate_y += te->from_y * (1.0f - p);
	return true;
}

static void reorder_remaining_offset(FluxThemeEnterAnim const *te, double now, float *rem_x, float *rem_y) {
	*rem_x = 0.0f;
	*rem_y = 0.0f;
	if (!te->active || te->kind != FLUX_THEME_TRANSITION_REORDER) return;
	double el = now - te->start_time - te->delay;
	if (el >= te->duration) return;
	float p     = el <= 0.0 ? 0.0f : flux_motion_bezier(( float ) (el / te->duration), 0.1f, 0.9f, 0.2f, 1.0f);
	float scale = 1.0f - p;
	*rem_x      = te->from_x * scale;
	*rem_y      = te->from_y * scale;
}

static void check_reorder_delta(struct FluxMotionState *st, XentCtx *ctx, XentNodeId node, double now, bool animate) {
	if (st->enter_exit_kind != FLUX_THEME_TRANSITION_REORDER || !xent_node_valid(ctx, node)) return;
	XentRect r = {0};
	xent_layout_rect(ctx, node, &r);
	if (!st->has_prev_layout) {
		st->prev_x          = r.x;
		st->prev_y          = r.y;
		st->has_prev_layout = true;
		return;
	}
	float dx   = st->prev_x - r.x;
	float dy   = st->prev_y - r.y;
	st->prev_x = r.x;
	st->prev_y = r.y;
	if (!animate || (fabsf(dx) < 0.5f && fabsf(dy) < 0.5f)) return;

	float rem_x = 0.0f, rem_y = 0.0f;
	reorder_remaining_offset(&st->theme_enter, now, &rem_x, &rem_y);
	dx               += rem_x;
	dy               += rem_y;

	FluxThemeAnim fb  = {.duration = 0.25, .x1 = 0.1f, .y1 = 0.9f, .x2 = 0.2f, .y2 = 1.0f};
	FluxThemeAnim a   = flux_themeanim_get(TAS_REPOSITION, TA_REPOSITION_TARGET, FLUX_THEMEANIM_TRANSLATE, fb);
	st->theme_enter   = (FluxThemeEnterAnim) {
	  .active     = true,
	  .is_exit    = false,
	  .kind       = FLUX_THEME_TRANSITION_REORDER,
	  .start_time = now,
	  .delay      = a.delay,
	  .duration   = a.duration > 0.0 ? a.duration : 0.25,
	  .from_x     = dx,
	  .from_y     = dy,
	};
}

void flux_motion_start_theme_enter(FluxStore *store, XentCtx *ctx, XentNodeId node, uint32_t sibling_index) {
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	FluxNodeData            *nd  = flux_store_find(store, node);
	if (!sys || !sys->anims_enabled || !nd || !nd->motion) return;
	FluxThemeTransitionKind kind = nd->motion->enter_exit_kind;
	if (kind == FLUX_THEME_TRANSITION_NONE || kind == FLUX_THEME_TRANSITION_REORDER) return;

	track_node(sys, node);
	double now   = flux_motion_now(sys);
	double delay = 0.0, dur = 0.0;
	float  fy = 0.0f;
	theme_resolve_timing(kind, false, sibling_index, &delay, &dur, &fy);
	nd->motion->theme_enter = (FluxThemeEnterAnim) {
	  .active        = true,
	  .is_exit       = false,
	  .kind          = kind,
	  .stagger_index = sibling_index,
	  .start_time    = now,
	  .delay         = delay,
	  .duration      = dur,
	  .from_y        = fy,
	};
	step_theme_enter(nd->motion, nd, now, true);
}

void flux_motion_set_theme_transition(FluxStore *store, XentNodeId node, FluxThemeTransitionKind kind) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	st->enter_exit_kind          = kind;
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (sys) track_node(sys, node);
}

static bool step_node_motion(struct FluxMotionSystem *sys, XentNodeId node, double now, bool animate) {
	FluxNodeData *nd = flux_store_find(sys->store, node);
	if (!nd || !nd->motion) return false;
	track_node(sys, node);
	check_reorder_delta(nd->motion, sys->ctx, node, now, animate);
	bool act  = flux_motion_step_implicit(nd->motion, nd, now, animate);
	act      |= step_theme_enter(nd->motion, nd, now, animate);
	act      |= flux_motion_step_frame(nd->motion, nd, now, animate);
	return act;
}

static bool step_tracked_nodes(struct FluxMotionSystem *sys, double now, bool animations_enabled) {
	MotionSysInternals *in     = ( MotionSysInternals * ) sys;
	bool                active = false;
	for (uint32_t i = 0; i < in->tracked_count;) {
		XentNodeId node = in->tracked [i];
		if (!xent_node_valid(sys->ctx, node)) {
			untrack_node(sys, node);
			continue;
		}
		active |= step_node_motion(sys, node, now, animations_enabled);
		i++;
	}
	return active;
}

static bool step_retiring_nodes(struct FluxMotionSystem *sys, double now, bool animations_enabled) {
	bool active = false;
	for (uint32_t i = 0; i < sys->retiring_count;) {
		FluxRetiringEntry *e       = &sys->retiring [i];
		bool               expired = !animations_enabled || now >= e->end_time || !xent_node_valid(sys->ctx, e->node);
		if (!expired) {
			step_node_motion(sys, e->node, now, animations_enabled);
			active = true;
			i++;
			continue;
		}
		XentNodeId dead = e->node;
		remove_retiring_entry(sys, i);
		if (dead != XENT_NODE_INVALID && xent_node_valid(sys->ctx, dead)) flux_tree_drop(sys->store, dead);
	}
	return active;
}

bool flux_motion_tick(FluxStore *store, XentCtx *ctx, double now_sec, bool animations_enabled) {
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	if (!sys) return false;
	sys->now            = now_sec;
	sys->anims_enabled  = animations_enabled;

	bool active         = step_tracked_nodes(sys, now_sec, animations_enabled);
	active             |= step_retiring_nodes(sys, now_sec, animations_enabled);
	active             |= flux_motion_tick_connected(sys, now_sec, animations_enabled);
	return active;
}

bool flux_motion_retire_add(
  struct FluxMotionSystem *sys, XentNodeId parent, XentNodeId node, bool below_live, double end_time
) {
	if (!sys || node == XENT_NODE_INVALID) return false;
	track_node(sys, node);
	if (sys->retiring_count == sys->retiring_cap) {
		uint32_t           cap  = sys->retiring_cap ? sys->retiring_cap * 2u : 8u;
		FluxRetiringEntry *next = ( FluxRetiringEntry * ) realloc(sys->retiring, sizeof(FluxRetiringEntry) * cap);
		if (!next) return false;
		sys->retiring     = next;
		sys->retiring_cap = cap;
	}
	XentRect pr = {0};
	if (parent != XENT_NODE_INVALID && xent_node_valid(sys->ctx, parent)) xent_layout_rect(sys->ctx, parent, &pr);
	sys->retiring [sys->retiring_count++] = (FluxRetiringEntry) {
	  .parent        = parent,
	  .node          = node,
	  .below_live    = below_live,
	  .parent_orig_x = pr.x,
	  .parent_orig_y = pr.y,
	  .end_time      = end_time,
	};
	return true;
}

void flux_motion_retire_drop_parent(struct FluxMotionSystem *sys, XentNodeId parent) {
	if (!sys || parent == XENT_NODE_INVALID) return;
	for (uint32_t i = 0; i < sys->retiring_count;) {
		if (sys->retiring [i].parent != parent) {
			i++;
			continue;
		}
		XentNodeId dead = sys->retiring [i].node;
		remove_retiring_entry(sys, i);
		if (dead != XENT_NODE_INVALID && sys->ctx && xent_node_valid(sys->ctx, dead)) flux_tree_drop(sys->store, dead);
	}
}

static void push_subtree_children(XentCtx *ctx, XentNodeId n, XentNodeId stack [64], uint32_t *top) {
	for (XentNodeId c = xent_node_first(ctx, n); c != XENT_NODE_INVALID && *top < 64u; c = xent_node_next(ctx, c))
		stack [(*top)++] = c;
}

void flux_motion_mark_subtree_retiring(FluxStore *store, XentCtx *ctx, XentNodeId root) {
	struct FluxInput *input = flux_store_input(store);
	XentNodeId        stack [64];
	uint32_t          top = 0;
	if (root != XENT_NODE_INVALID) stack [top++] = root;
	while (top > 0u) {
		XentNodeId    n  = stack [--top];
		FluxNodeData *nd = flux_store_find(store, n);
		if (nd) nd->retiring = true;
		if (input) flux_input_dropnode(input, n);
		push_subtree_children(ctx, n, stack, &top);
	}
}

bool flux_motion_try_retire(FluxStore *store, XentCtx *ctx, XentNodeId node, XentNodeId parent) {
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	if (!sys || !sys->anims_enabled || node == XENT_NODE_INVALID || parent == XENT_NODE_INVALID) return false;
	FluxNodeData *pnd = flux_store_find(store, parent);
	FluxNodeData *nd  = flux_store_find(store, node);
	if (pnd && pnd->component_type == FLUX_CONTROL_FRAME && pnd->component_data) {
		FluxFrameControlData *fd = ( FluxFrameControlData * ) pnd->component_data;
		if (fd->transition == FLUX_NAV_TRANSITION_SUPPRESS) return false;
		if (!xent_node_remove(ctx, parent, node)) return false;
		if (fd->pending_outgoing != XENT_NODE_INVALID
		    && fd->pending_outgoing != node
		    && xent_node_valid(ctx, fd->pending_outgoing))
			flux_tree_drop(store, fd->pending_outgoing);
		fd->pending_outgoing = node;
		return true;
	}
	if (!nd || !nd->motion) return false;
	FluxThemeTransitionKind kind = nd->motion->enter_exit_kind;
	if (kind == FLUX_THEME_TRANSITION_NONE || kind == FLUX_THEME_TRANSITION_REORDER) return false;
	if (!xent_node_remove(ctx, parent, node)) return false;
	double now   = flux_motion_now(sys);
	double delay = 0.0, dur = 0.0;
	float  fy = 0.0f;
	theme_resolve_timing(kind, true, 0u, &delay, &dur, &fy);
	nd->motion->theme_enter = (FluxThemeEnterAnim) {
	  .active     = true,
	  .is_exit    = true,
	  .kind       = kind,
	  .start_time = now,
	  .delay      = delay,
	  .duration   = dur,
	};
	return flux_motion_retire_add(sys, parent, node, false, now + delay + dur);
}

static void commit_frame_children(FluxStore *store, XentCtx *ctx, FluxNodeData *fnd) {
	FluxFrameControlData *fd = ( FluxFrameControlData * ) fnd->component_data;
	if (!fd) return;
	XentNodeId first     = xent_node_first(ctx, fnd->node_id);
	XentNodeId out       = fd->pending_outgoing;
	fd->pending_outgoing = XENT_NODE_INVALID;
	bool key_changed     = fd->page_key != 0u && fd->page_key != fd->last_page_key;
	bool child_swapped   = first != fd->active_child || out != XENT_NODE_INVALID || key_changed;
	if (!child_swapped) return;

	bool is_initial       = !fd->has_initial_page && out == XENT_NODE_INVALID;
	fd->has_initial_page  = true;
	fd->last_page_key     = fd->page_key;
	fd->active_child      = first;
	fd->active_transition = fd->transition;
	fd->active_back       = fd->is_back;
	if (is_initial) {
		fd->active_transition = FLUX_NAV_TRANSITION_SUPPRESS;
		if (out != XENT_NODE_INVALID && xent_node_valid(ctx, out)) flux_tree_drop(store, out);
		return;
	}
	flux_motion_start_frame_pair(store, ctx, fnd, out, first);
}

static uint32_t next_sibling_index(XentNodeId parent, XentNodeId *parents, uint32_t *counts, uint32_t *nparents) {
	for (uint32_t i = 0; i < *nparents; i++)
		if (parents [i] == parent) return counts [i]++;
	if (*nparents < 16u) {
		parents [*nparents] = parent;
		counts [*nparents]  = 1u;
		(*nparents)++;
		return 0u;
	}
	return 0u;
}

static void commit_created_node(
  struct FluxMotionSystem *sys, XentNodeId node, XentNodeId *parents, uint32_t *counts, uint32_t *nparents
) {
	FluxNodeData *nd = flux_store_find(sys->store, node);
	if (!nd || !nd->motion) return;
	track_node(sys, node);
	flux_motion_sync_implicit(sys->store, node, nd, true);
	if (nd->motion->enter_exit_kind == FLUX_THEME_TRANSITION_NONE) return;
	XentNodeId p   = xent_node_parent(sys->ctx, node);
	uint32_t   idx = next_sibling_index(p, parents, counts, nparents);
	flux_motion_start_theme_enter(sys->store, sys->ctx, node, idx);
}

void flux_motion_commit(FluxStore *store, XentCtx *ctx, XentNodeId const *created, uint32_t created_count) {
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	if (!sys) return;

	for (uint32_t i = 0; i < flux_store_nframe(store); i++) {
		XentNodeId    fn = flux_store_frame(store, i);
		FluxNodeData *nd = flux_store_find(store, fn);
		if (nd && nd->component_type == FLUX_CONTROL_FRAME) commit_frame_children(store, ctx, nd);
	}
	XentNodeId parents [16];
	uint32_t   counts [16];
	uint32_t   nparents = 0;
	for (uint32_t i = 0; i < created_count; i++) commit_created_node(sys, created [i], parents, counts, &nparents);
	flux_motion_connected_on_commit(store, ctx, created, created_count);
}

void flux_motion_collect_retiring(FluxStore *store, FluxEngine *eng, XentCtx *ctx, XentNodeId parent, bool above) {
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (!sys || sys->retiring_count == 0u) return;
	bool     want_below = !above;
	XentRect pr         = {0};
	if (parent != XENT_NODE_INVALID && xent_node_valid(ctx, parent)) xent_layout_rect(ctx, parent, &pr);

	for (uint32_t i = 0; i < sys->retiring_count; i++) {
		FluxRetiringEntry const *e = &sys->retiring [i];
		if (e->parent != parent || e->below_live != want_below) continue;
		float dx      = pr.x - e->parent_orig_x;
		float dy      = pr.y - e->parent_orig_y;
		bool  shifted = fabsf(dx) > 0.01f || fabsf(dy) > 0.01f;
		if (shifted) {
			FluxRenderCommand xf
			  = {.scale_x = 1.0f, .scale_y = 1.0f, .opacity = 1.0f, .translate_x = dx, .translate_y = dy};
			flux_engine_push_transform(eng, &xf);
		}
		flux_engine_collect_subtree(eng, ctx, e->node, 0.0f, 0.0f, false);
		if (shifted) flux_engine_pop_transform(eng, false);
	}
}
