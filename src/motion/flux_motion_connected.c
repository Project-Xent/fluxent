#include "motion/flux_motion_internal.h"
#include "controls/data/flux_scroll_data.h"
#include "render/flux_render_internal.h"
#include "runtime/flux_str.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double const kConnectedTimeoutSec = 2.0;

static float        clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static void         free_prepared_item(struct FluxMotionSystem *sys, FluxConnectedPrepared *p) {
	free(p->key);
	if (p->ghost_node != XENT_NODE_INVALID && xent_node_valid(sys->ctx, p->ghost_node)) {
		if (!flux_motion_connected_has_ghost(sys, p->ghost_node)) flux_tree_drop(sys->store, p->ghost_node);
	}
	memset(p, 0, sizeof(*p));
}

static void free_flight_item(struct FluxMotionSystem *sys, FluxConnectedFlight *f) {
	free(f->key);
	free(f->coords);
	FluxNodeData *dnd = flux_store_find(sys->store, f->dest_node);
	if (dnd) dnd->connected_hidden = false;
	for (uint32_t i = 0; i < f->coord_count; i++) {
		FluxNodeData *cnd = f->coords ? flux_store_find(sys->store, f->coords [i].node) : NULL;
		if (cnd) cnd->connected_hidden = false;
	}
	if (f->ghost_node != XENT_NODE_INVALID && xent_node_valid(sys->ctx, f->ghost_node))
		flux_tree_drop(sys->store, f->ghost_node);
	memset(f, 0, sizeof(*f));
}

bool flux_motion_connected_has_ghost(struct FluxMotionSystem *sys, XentNodeId node) {
	if (!sys || node == XENT_NODE_INVALID) return false;
	for (uint32_t i = 0; i < sys->flight_count; i++)
		if (sys->flights [i].ghost_node == node) return true;
	return false;
}

static void connected_cfg_resolve(FluxConnectedCfg const *in, FluxConnectedCfg *out) {
	*out = in ? *in : (FluxConnectedCfg) {.kind = FLUX_CONNECTED_GRAVITY, .is_shadow_enabled = true};
	if (out->duration > 0.0) return;
	out->duration = out->kind == FLUX_CONNECTED_DIRECT ? 0.20 : 0.30;
}

static void eval_gravity_dip(
  float progress, FluxConnectedCfg const *cfg, bool uniform_scale, float *dip_y, float *dip_z, float *dip_s
) {
	*dip_y = 0.0f;
	*dip_z = 0.0f;
	*dip_s = 1.0f;
	if (cfg->kind != FLUX_CONNECTED_GRAVITY) return;
	float const t_peak = 0.66f;
	float       arc    = 0.0f;
	if (progress <= t_peak) {
		float u = progress / t_peak;
		arc     = flux_motion_bezier(u, 0.35f, 0.0f, 0.55f, 1.0f);
	}
	else {
		float u = (progress - t_peak) / (1.0f - t_peak);
		arc     = 1.0f - flux_motion_bezier(u, 0.45f, 0.0f, 0.55f, 1.0f);
	}
	*dip_y = (cfg->dip_y != 0.0f ? cfg->dip_y : 80.0f) * arc;
	*dip_z = cfg->is_shadow_enabled ? 32.0f * arc : 0.0f;
	if (uniform_scale) *dip_s = 1.0f + 0.1f * arc;
}

static FluxConnectedPrepared *find_prepared(struct FluxMotionSystem *sys, char const *key, uint32_t *out_idx) {
	if (!sys || !key) return NULL;
	for (uint32_t i = 0; i < sys->prepared_count; i++) {
		if (!sys->prepared [i].key || strcmp(sys->prepared [i].key, key) != 0) continue;
		if (out_idx) *out_idx = i;
		return &sys->prepared [i];
	}
	return NULL;
}

static void remove_prepared_at(struct FluxMotionSystem *sys, uint32_t idx) {
	if (idx >= sys->prepared_count) return;
	free_prepared_item(sys, &sys->prepared [idx]);
	if (idx + 1 < sys->prepared_count)
		memmove(
		  &sys->prepared [idx], &sys->prepared [idx + 1],
		  sizeof(FluxConnectedPrepared) * (sys->prepared_count - idx - 1)
		);
	sys->prepared_count--;
}

static void remove_flight_at(struct FluxMotionSystem *sys, uint32_t idx) {
	if (idx >= sys->flight_count) return;
	free_flight_item(sys, &sys->flights [idx]);
	if (idx + 1 < sys->flight_count)
		memmove(
		  &sys->flights [idx], &sys->flights [idx + 1], sizeof(FluxConnectedFlight) * (sys->flight_count - idx - 1)
		);
	sys->flight_count--;
}

void flux_motion_connected_cancel(FluxStore *store, char const *key) {
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (!sys || !key) return;
	uint32_t idx = 0;
	if (find_prepared(sys, key, &idx)) remove_prepared_at(sys, idx);
	for (uint32_t i = 0; i < sys->flight_count;)
		if (sys->flights [i].key && strcmp(sys->flights [i].key, key) == 0) remove_flight_at(sys, i);
		else i++;
}

static bool drop_node_from_flight(FluxConnectedFlight *f, XentNodeId node) {
	if (f->ghost_node == node) f->ghost_node = XENT_NODE_INVALID;
	if (f->dest_node == node) return true;
	for (uint32_t j = 0; j < f->coord_count; j++)
		if (f->coords [j].node == node) f->coords [j].node = XENT_NODE_INVALID;
	return false;
}

void flux_motion_connected_drop_node(struct FluxMotionSystem *sys, XentNodeId node) {
	if (!sys || node == XENT_NODE_INVALID) return;
	for (uint32_t i = 0; i < sys->prepared_count; i++)
		if (sys->prepared [i].ghost_node == node) sys->prepared [i].ghost_node = XENT_NODE_INVALID;
	for (uint32_t i = 0; i < sys->flight_count;)
		if (drop_node_from_flight(&sys->flights [i], node)) remove_flight_at(sys, i);
		else i++;
}

static void connected_ancestor_offset(FluxStore *store, XentCtx *ctx, XentNodeId node, float *ox, float *oy) {
	float dx = 0.0f;
	float dy = 0.0f;
	for (XentNodeId p = xent_node_parent(ctx, node); p != XENT_NODE_INVALID; p = xent_node_parent(ctx, p)) {
		FluxNodeData const *nd = flux_store_find(store, p);
		if (!nd) continue;
		dx += nd->render_translate_x + nd->motion_translate_x;
		dy += nd->render_translate_y + nd->motion_translate_y;
		if (nd->component_type == FLUX_CONTROL_SCROLL && nd->component_data) {
			FluxScrollData const *sd  = ( FluxScrollData const * ) nd->component_data;
			dx                       -= flux_scroll_off_x(sd);
			dy                       -= flux_scroll_off_y(sd);
		}
	}
	*ox = dx;
	*oy = dy;
}

static bool connected_node_rects(FluxStore *store, XentCtx *ctx, XentNodeId node, FluxRect *raw, FluxRect *vis) {
	XentRect r = {0};
	xent_layout_rect(ctx, node, &r);
	if (r.w <= 0.0f || r.h <= 0.0f) return false;
	float ox = 0.0f;
	float oy = 0.0f;
	connected_ancestor_offset(store, ctx, node, &ox, &oy);
	if (raw) *raw = (FluxRect) {r.x, r.y, r.w, r.h};
	if (vis) *vis = (FluxRect) {r.x + ox, r.y + oy, r.w, r.h};
	return true;
}

bool flux_motion_connected_prepare(
  FluxStore *store, XentCtx *ctx, char const *key, XentNodeId source, FluxConnectedCfg const *cfg
) {
	if (!store || !ctx || !key || !*key || !xent_node_valid(ctx, source)) return false;
	struct FluxMotionSystem *sys = flux_motion_ensure_system(store, ctx);
	if (!sys) return false;

	FluxRect raw = {0};
	FluxRect vis = {0};
	if (!connected_node_rects(store, ctx, source, &raw, &vis)) return false;

	flux_motion_connected_cancel(store, key);
	if (sys->prepared_count == sys->prepared_cap) {
		uint32_t               cap = sys->prepared_cap ? sys->prepared_cap * 2u : 4u;
		FluxConnectedPrepared *next
		  = ( FluxConnectedPrepared * ) realloc(sys->prepared, sizeof(FluxConnectedPrepared) * cap);
		if (!next) return false;
		sys->prepared     = next;
		sys->prepared_cap = cap;
	}
	FluxNodeData    *snd = flux_store_find(store, source);
	FluxConnectedCfg resolved;
	connected_cfg_resolve(cfg, &resolved);
	double now                            = flux_motion_now(sys);
	sys->prepared [sys->prepared_count++] = (FluxConnectedPrepared) {
	  .key           = flux_str_dup(key),
	  .src_rect      = vis,
	  .src_raw_rect  = raw,
	  .ghost_node    = XENT_NODE_INVALID,
	  .cfg           = resolved,
	  .expire_time   = now + kConnectedTimeoutSec,
	  .corner_radius = snd ? snd->visuals.corner_radius : 0.0f,
	};
	return true;
}

static bool
flight_add_coords(FluxStore *store, XentCtx *ctx, FluxConnectedFlight *f, XentNodeId const *coords, uint32_t count) {
	if (!coords || count == 0u) return true;
	f->coords = ( FluxConnectedCoordItem * ) calloc(count, sizeof(FluxConnectedCoordItem));
	if (!f->coords) return false;
	uint32_t kept = 0;
	for (uint32_t i = 0; i < count; i++) {
		XentNodeId cn = coords [i];
		if (!xent_node_valid(ctx, cn)) continue;
		FluxNodeData *cnd = flux_store_find(store, cn);
		if (cnd) cnd->connected_hidden = true;
		f->coords [kept++].node = cn;
	}
	f->coord_count = kept;
	return true;
}

static void resolve_flight_coords(FluxStore *store, XentCtx *ctx, FluxConnectedFlight *f, FluxRect const *vis_dr) {
	f->coords_resolved = true;
	if (!f->coords || f->coord_count == 0u || vis_dr->w <= 0.0f || vis_dr->h <= 0.0f) return;
	float sx = f->src_rect.w / vis_dr->w;
	float sy = f->src_rect.h / vis_dr->h;
	for (uint32_t i = 0; i < f->coord_count; i++) {
		XentNodeId cn = f->coords [i].node;
		if (!xent_node_valid(ctx, cn)) continue;
		FluxRect vis_cr = {0};
		if (!connected_node_rects(store, ctx, cn, NULL, &vis_cr)) continue;
		f->coords [i].offset = (FluxRect) {
		  f->src_rect.x + (vis_cr.x - vis_dr->x) * sx,
		  f->src_rect.y + (vis_cr.y - vis_dr->y) * sy,
		  vis_cr.w * sx,
		  vis_cr.h * sy,
		};
	}
}

static bool flights_reserve(struct FluxMotionSystem *sys) {
	if (sys->flight_count < sys->flight_cap) return true;
	uint32_t             cap  = sys->flight_cap ? sys->flight_cap * 2u : 4u;
	FluxConnectedFlight *next = ( FluxConnectedFlight * ) realloc(sys->flights, sizeof(FluxConnectedFlight) * cap);
	if (!next) return false;
	sys->flights    = next;
	sys->flight_cap = cap;
	return true;
}

bool flux_motion_connected_start(
  FluxStore *store, XentCtx *ctx, char const *key, XentNodeId dest, XentNodeId const *coords, uint32_t coord_count
) {
	struct FluxMotionSystem *sys  = store ? flux_motion_ensure_system(store, ctx) : NULL;
	uint32_t                 pidx = 0;
	FluxConnectedPrepared   *p
	  = (sys && ctx && key && xent_node_valid(ctx, dest)) ? find_prepared(sys, key, &pidx) : NULL;
	if (!p) return false;
	if (!sys->anims_enabled || !flights_reserve(sys)) {
		remove_prepared_at(sys, pidx);
		return false;
	}
	double              now = flux_motion_now(sys);
	FluxConnectedFlight f   = {
	  .key             = flux_str_dup(p->key),
	  .dest_node       = dest,
	  .ghost_node      = p->ghost_node,
	  .src_rect        = p->src_rect,
	  .src_raw_rect    = p->src_raw_rect,
	  .cfg             = p->cfg,
	  .start_time      = now,
	  .duration        = p->cfg.duration,
	  .linear_progress = 0.0f,
	  .progress        = 0.0f,
	  .corner_radius   = p->corner_radius,
	  .coords_resolved = false,
	};
	p->ghost_node = XENT_NODE_INVALID;
	remove_prepared_at(sys, pidx);
	FluxNodeData *dnd = flux_store_find(store, dest);
	if (dnd) dnd->connected_hidden = true;
	flight_add_coords(store, ctx, &f, coords, coord_count);
	sys->flights [sys->flight_count++] = f;
	return true;
}

bool flux_motion_tick_connected(struct FluxMotionSystem *sys, double now, bool animate) {
	if (!sys) return false;
	for (uint32_t i = 0; i < sys->prepared_count;)
		if (now >= sys->prepared [i].expire_time) remove_prepared_at(sys, i);
		else i++;
	for (uint32_t i = 0; i < sys->flight_count;) {
		FluxConnectedFlight *f  = &sys->flights [i];
		double               el = now - f->start_time;
		if (!animate || !xent_node_valid(sys->ctx, f->dest_node) || el >= f->duration) {
			remove_flight_at(sys, i);
			continue;
		}
		float lin          = clamp01(( float ) (el / f->duration));
		f->linear_progress = lin;
		if (f->cfg.kind == FLUX_CONNECTED_DIRECT) f->progress = flux_motion_bezier(lin, 0.1f, 0.9f, 0.2f, 1.0f);
		else f->progress = flux_motion_bezier(lin, 0.8f, 0.0f, 0.2f, 1.0f);
		i++;
	}
	return sys->flight_count > 0u;
}

static void
collect_coords_for_flight(FluxStore *store, FluxEngine *eng, XentCtx *ctx, FluxConnectedFlight const *f, float p) {
	for (uint32_t j = 0; j < f->coord_count; j++) {
		XentNodeId cn = f->coords [j].node;
		if (!xent_node_valid(ctx, cn)) continue;
		FluxRect raw_cr = {0};
		FluxRect vis_cr = {0};
		if (!connected_node_rects(store, ctx, cn, &raw_cr, &vis_cr)) continue;
		FluxRect const   *s  = &f->coords [j].offset;
		float             cx = flux_motion_lerpf(s->x, vis_cr.x, p);
		float             cy = flux_motion_lerpf(s->y, vis_cr.y, p);
		FluxRenderCommand xf = {
		  .scale_x     = flux_motion_lerpf(s->w / raw_cr.w, 1.0f, p),
		  .scale_y     = flux_motion_lerpf(s->h / raw_cr.h, 1.0f, p),
		  .opacity     = p,
		  .translate_x = cx - raw_cr.x,
		  .translate_y = cy - raw_cr.y,
		  .pivot_x     = raw_cr.x,
		  .pivot_y     = raw_cr.y,
		};
		flux_engine_push_transform(eng, &xf);
		flux_engine_collect_subtree(eng, ctx, cn, 0.0f, 0.0f, true);
		flux_engine_pop_transform(eng, xf.opacity < 0.9999f);
	}
}

static void collect_one_flight(FluxStore *store, FluxEngine *eng, XentCtx *ctx, FluxConnectedFlight *f) {
	FluxRect raw_dr = {0};
	FluxRect vis_dr = {0};
	if (!connected_node_rects(store, ctx, f->dest_node, &raw_dr, &vis_dr)) return;
	if (!f->coords_resolved) resolve_flight_coords(store, ctx, f, &vis_dr);

	float p             = f->progress;
	bool  uniform_scale = fabsf(f->src_rect.w - vis_dr.w) < 1.0f && fabsf(f->src_rect.h - vis_dr.h) < 1.0f;
	float dip_y = 0.0f, dip_z = 0.0f, dip_s = 1.0f;
	eval_gravity_dip(f->linear_progress, &f->cfg, uniform_scale, &dip_y, &dip_z, &dip_s);

	float cur_x = flux_motion_lerpf(f->src_rect.x, vis_dr.x, p);
	float cur_y = flux_motion_lerpf(f->src_rect.y, vis_dr.y, p) + dip_y;
	float cur_w = flux_motion_lerpf(f->src_rect.w, vis_dr.w, p) * dip_s;
	float cur_h = flux_motion_lerpf(f->src_rect.h, vis_dr.h, p) * dip_s;

	if (dip_z > 0.5f) {
		float sh_x = cur_x - (cur_w - flux_motion_lerpf(f->src_rect.w, vis_dr.w, p)) * 0.5f;
		float sh_y = cur_y - (cur_h - flux_motion_lerpf(f->src_rect.h, vis_dr.h, p)) * 0.5f;
		flux_engine_push_shadow(eng, (FluxRect) {sh_x, sh_y, cur_w, cur_h}, dip_z, 1.0f, f->corner_radius);
	}

	if (f->ghost_node != XENT_NODE_INVALID
	    && xent_node_valid(ctx, f->ghost_node)
	    && f->src_raw_rect.w > 0.0f
	    && f->src_raw_rect.h > 0.0f
	    && p < 0.999f)
	{
		FluxRenderCommand src_xf = {
		  .scale_x     = cur_w / f->src_raw_rect.w,
		  .scale_y     = cur_h / f->src_raw_rect.h,
		  .opacity     = 1.0f - p,
		  .translate_x = cur_x - f->src_raw_rect.x,
		  .translate_y = cur_y - f->src_raw_rect.y,
		  .pivot_x     = f->src_raw_rect.x,
		  .pivot_y     = f->src_raw_rect.y,
		};
		flux_engine_push_transform(eng, &src_xf);
		flux_engine_collect_subtree(eng, ctx, f->ghost_node, 0.0f, 0.0f, true);
		flux_engine_pop_transform(eng, src_xf.opacity < 0.9999f);
	}

	float             dst_op = (f->ghost_node != XENT_NODE_INVALID) ? p : 1.0f;
	FluxRenderCommand dst_xf = {
	  .scale_x     = cur_w / raw_dr.w,
	  .scale_y     = cur_h / raw_dr.h,
	  .opacity     = dst_op,
	  .translate_x = cur_x - raw_dr.x,
	  .translate_y = cur_y - raw_dr.y,
	  .pivot_x     = raw_dr.x,
	  .pivot_y     = raw_dr.y,
	};
	flux_engine_push_transform(eng, &dst_xf);
	flux_engine_collect_subtree(eng, ctx, f->dest_node, 0.0f, 0.0f, true);
	flux_engine_pop_transform(eng, dst_xf.opacity < 0.9999f);
	collect_coords_for_flight(store, eng, ctx, f, p);
}

void flux_motion_collect_connected(FluxStore *store, FluxEngine *eng, XentCtx *ctx, XentNodeId root) {
	( void ) root;
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (!sys || sys->flight_count == 0u) return;
	for (uint32_t i = 0; i < sys->flight_count; i++) collect_one_flight(store, eng, ctx, &sys->flights [i]);
}

void flux_motion_paint_shadow(FluxRenderCtx const *rc, FluxRenderCommand const *cmd) {
	if (!rc || !rc->d2d || !rc->brush || !cmd) return;
	float z = cmd->scale_x;
	float r = cmd->scale_y;
	if (z <= 0.1f) return;

	float dir_off = z * 0.4f;
	float dir_pad = z * 0.35f;
	D2D1_ROUNDED_RECT dir_rr = {
	  .rect    = {cmd->bounds.x - dir_pad, cmd->bounds.y + dir_off - dir_pad * 0.5f,
	              cmd->bounds.x + cmd->bounds.w + dir_pad, cmd->bounds.y + cmd->bounds.h + dir_off + dir_pad},
	  .radiusX = r + dir_pad * 0.5f,
	  .radiusY = r + dir_pad * 0.5f,
	};
	D2D1_COLOR_F dir_col = {0.0f, 0.0f, 0.0f, (rc->is_dark ? 0.22f : 0.14f) * cmd->opacity};
	ID2D1SolidColorBrush_SetColor(rc->brush, &dir_col);
	ID2D1RenderTarget_FillRoundedRectangle(FLUX_RT(rc), &dir_rr, ( ID2D1Brush * ) rc->brush);

	float amb_pad = z * 0.18f;
	D2D1_ROUNDED_RECT amb_rr  = {
	  .rect    = {cmd->bounds.x - amb_pad, cmd->bounds.y - amb_pad,
	              cmd->bounds.x + cmd->bounds.w + amb_pad, cmd->bounds.y + cmd->bounds.h + amb_pad + z * 0.04f},
	  .radiusX = r + amb_pad * 0.5f,
	  .radiusY = r + amb_pad * 0.5f,
	};
	D2D1_COLOR_F amb_col = {0.0f, 0.0f, 0.0f, (rc->is_dark ? 0.18f : 0.11f) * cmd->opacity};
	ID2D1SolidColorBrush_SetColor(rc->brush, &amb_col);
	ID2D1RenderTarget_FillRoundedRectangle(FLUX_RT(rc), &amb_rr, ( ID2D1Brush * ) rc->brush);
}

void flux_motion_set_connected_key(FluxStore *store, XentNodeId node, char const *key, FluxConnectedCfg const *cfg) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	free(st->connected_key);
	st->connected_key = (key && *key) ? flux_str_dup(key) : NULL;
	connected_cfg_resolve(cfg, &st->connected_cfg);
}

void flux_motion_set_connected_coord(FluxStore *store, XentNodeId node, char const *key) {
	struct FluxMotionState *st = flux_motion_ensure_state(store, node);
	if (!st) return;
	free(st->coord_key);
	st->coord_key = (key && *key) ? flux_str_dup(key) : NULL;
}

static bool collect_coord_nodes(
  FluxStore *store, XentCtx *ctx, XentNodeId const *nodes, uint32_t count, char const *key, XentNodeId *out,
  uint32_t max_out, uint32_t *out_n
) {
	uint32_t n = 0;
	for (uint32_t i = 0; i < count && n < max_out; i++) {
		FluxNodeData *nd = flux_store_find(store, nodes [i]);
		if (nd
		    && nd->motion
		    && nd->motion->coord_key
		    && strcmp(nd->motion->coord_key, key) == 0
		    && xent_node_valid(ctx, nodes [i]))
			out [n++] = nodes [i];
	}
	*out_n = n;
	return n > 0u;
}

void flux_motion_connected_on_destroy(FluxStore *store, XentCtx *ctx, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || !nd->motion || !nd->motion->connected_key) return;
	flux_motion_connected_prepare(store, ctx, nd->motion->connected_key, node, &nd->motion->connected_cfg);
}

bool flux_motion_connected_try_retire(FluxStore *store, XentCtx *ctx, XentNodeId node) {
	struct FluxMotionSystem *sys = flux_store_motion(store);
	FluxNodeData            *nd  = flux_store_find(store, node);
	if (!sys || !sys->anims_enabled || !nd || !nd->motion || !nd->motion->connected_key) return false;
	FluxConnectedPrepared *p = find_prepared(sys, nd->motion->connected_key, NULL);
	if (!p || p->ghost_node != XENT_NODE_INVALID) return false;
	XentNodeId parent = xent_node_parent(ctx, node);
	if (parent == XENT_NODE_INVALID || !xent_node_remove(ctx, parent, node)) return false;
	p->ghost_node = node;
	flux_motion_mark_subtree_retiring(store, ctx, node);
	return true;
}

static bool
subtree_stack_push(XentNodeId **stack, uint32_t *top, uint32_t *cap, XentNodeId inline_buf [64], XentNodeId node) {
	if (*top == *cap) {
		uint32_t    ncap = *cap * 2u;
		XentNodeId *next = (*stack == inline_buf) ? ( XentNodeId * ) malloc(sizeof(*next) * ncap)
		                                          : ( XentNodeId * ) realloc(*stack, sizeof(*next) * ncap);
		if (!next) return false;
		if (*stack == inline_buf) memcpy(next, inline_buf, sizeof(XentNodeId) * (*top));
		*stack = next;
		*cap   = ncap;
	}
	(*stack) [(*top)++] = node;
	return true;
}

static bool subtree_push_children(
  XentCtx *ctx, XentNodeId **stack, uint32_t *top, uint32_t *cap, XentNodeId inline_buf [64], XentNodeId node
) {
	for (XentNodeId c = xent_node_first(ctx, node); c != XENT_NODE_INVALID; c = xent_node_next(ctx, c))
		if (!subtree_stack_push(stack, top, cap, inline_buf, c)) return false;
	return true;
}

void flux_motion_connected_prepare_subtree(FluxStore *store, XentCtx *ctx, XentNodeId root) {
	if (!store || !ctx || root == XENT_NODE_INVALID || !xent_node_valid(ctx, root)) return;
	XentNodeId  inline_buf [64];
	XentNodeId *stack = inline_buf;
	uint32_t    cap   = 64u;
	uint32_t    top   = 0u;
	stack [top++]     = root;
	while (top > 0u) {
		XentNodeId node = stack [--top];
		( void ) subtree_push_children(ctx, &stack, &top, &cap, inline_buf, node);
		FluxNodeData const *nd = flux_store_find(store, node);
		if (!nd || nd->retiring || !nd->motion || !nd->motion->connected_key) continue;
		flux_motion_connected_on_destroy(store, ctx, node);
		flux_motion_connected_try_retire(store, ctx, node);
	}
	if (stack != inline_buf) free(stack);
}

void flux_motion_connected_on_commit(
  FluxStore *store, XentCtx *ctx, XentNodeId const *created, uint32_t created_count
) {
	struct FluxMotionSystem *sys = flux_store_motion(store);
	if (!sys || sys->prepared_count == 0u || !created || created_count == 0u) return;
	for (uint32_t i = 0; i < created_count; i++) {
		XentNodeId    node = created [i];
		FluxNodeData *nd   = flux_store_find(store, node);
		if (!nd || !nd->motion || !nd->motion->connected_key) continue;
		XentNodeId coords [16];
		uint32_t   nc = 0;
		collect_coord_nodes(store, ctx, created, created_count, nd->motion->connected_key, coords, 16u, &nc);
		flux_motion_connected_start(store, ctx, nd->motion->connected_key, node, coords, nc);
	}
}
