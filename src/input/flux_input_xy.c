#include "flux_input_internal.h"
#include "a11y/flux_uia_object.h"

#include <fluxent/flux_node_store.h>
#include <fluxent/flux_window.h>
#include <math.h>
#include <stdlib.h>

#define XY_IN_SHADOW        0.25
#define XY_IN_SHADOW_SECOND 0.02
#define XY_CONE_ANGLE       0.78539816339744830962
#define XY_PI               3.14159265358979323846
#define XY_PRIMARY_WEIGHT   15.0
#define XY_SECONDARY_WEIGHT 1.0
#define XY_MANIFOLD_WEIGHT  10000.0
#define XY_SHADOW_WEIGHT    50.0
#define XY_MAX_CANDIDATES   1024
#define XY_HIT_SCALE        0.90

typedef struct XyRect {
	double l, t, r, b;
} XyRect;

typedef struct XyPoint {
	float x, y;
} XyPoint;

typedef struct XySpan {
	double lo, hi;
} XySpan;

typedef struct XyCandidate {
	XentNodeId node;
	XyRect     bounds;
	double     score;
} XyCandidate;

typedef struct XyWalk {
	FluxUiaContext info;
	XentNodeId     current;
	XyCandidate   *items;
	int            count;
} XyWalk;

typedef struct XyQuery {
	FluxInput *input;
	FluxKey    dir;
	XyRect     bounds;
	double     max;
	bool       rtl;
} XyQuery;

static bool xy_horizontal(FluxKey dir) { return dir == FLUX_KEY_LEFT || dir == FLUX_KEY_RIGHT; }

static bool xy_rect_empty(XyRect const *r) { return r->r <= r->l || r->b <= r->t; }

static bool xy_intersect(XyRect const *a, XyRect const *b) {
	return a->l < b->r && a->t < b->b && a->r > b->l && a->b > b->t;
}

static bool xy_contains(XyRect const *c, XyRect const *x) {
	return c->l <= x->l && c->t <= x->t && c->r >= x->r && c->b >= x->b;
}

static bool xy_equal(XyRect const *a, XyRect const *b) {
	return a->l == b->l && a->t == b->t && a->r == b->r && a->b == b->b;
}

static double xy_primary(FluxKey dir, XyRect const *s, XyRect const *c) {
	bool overlap = xy_intersect(s, c);
	if (xy_equal(s, c)) return -1.0;
	if (dir == FLUX_KEY_LEFT && (c->r <= s->l || (overlap && c->l <= s->l))) return fabs(s->l - c->r);
	if (dir == FLUX_KEY_RIGHT && (c->l >= s->r || (overlap && c->r >= s->r))) return fabs(c->l - s->r);
	if (dir == FLUX_KEY_UP && (c->b <= s->t || (overlap && c->t <= s->t))) return fabs(s->t - c->b);
	if (dir == FLUX_KEY_DOWN && (c->t >= s->b || (overlap && c->b >= s->b))) return fabs(c->t - s->b);
	return -1.0;
}

static double xy_secondary(FluxKey dir, XyRect const *s, XyRect const *c) {
	if (xy_horizontal(dir)) return c->t < s->t ? fabs(s->t - c->b) : fabs(c->t - s->b);
	return c->l < s->l ? fabs(s->l - c->r) : fabs(c->l - s->r);
}

static double xy_shadow(XySpan ref, XySpan pot) {
	if (ref.lo > pot.hi || ref.hi <= pot.lo) return 0.0;
	double shadow = fabs(fmin(ref.hi, pot.hi) - fmax(ref.lo, pot.lo));
	double edge   = fmin(fabs(ref.hi - ref.lo), fabs(pot.hi - pot.lo));
	return edge != 0.0 ? fmin(shadow / edge, 1.0) : 1.0;
}

static double xy_score(XyQuery const *q, XyRect const *c) {
	bool   h        = xy_horizontal(q->dir);
	XySpan ref      = h ? (XySpan) {q->bounds.t, q->bounds.b} : (XySpan) {q->bounds.l, q->bounds.r};
	XySpan pot      = h ? (XySpan) {c->t, c->b} : (XySpan) {c->l, c->r};
	double primary  = xy_primary(q->dir, &q->bounds, c);
	double second   = xy_secondary(q->dir, &q->bounds, c);
	double manifold = 0.0;
	if (primary < 0.0) return 0.0;
	double                 shadow = xy_shadow(ref, pot);
	FluxXyManifolds const *m      = &q->input->xy;
	if (shadow >= XY_IN_SHADOW_SECOND) {
		manifold = xy_shadow(h ? (XySpan) {m->h_lo, m->h_hi} : (XySpan) {m->v_lo, m->v_hi}, pot);
		second   = q->max;
	}
	primary = q->max - primary;
	second  = q->max - second;
	if (shadow >= XY_IN_SHADOW) {
		primary *= 2.0;
		shadow   = 1.0;
	}
	return shadow * XY_SHADOW_WEIGHT
	     + primary * XY_PRIMARY_WEIGHT
	     + second * XY_SECONDARY_WEIGHT
	     + manifold * XY_MANIFOLD_WEIGHT;
}

static int xy_side(int n, XyPoint const *poly, XyPoint at, XyPoint edge) {
	int pos = 0, neg = 0;
	for (int i = 0; i < n; i++) {
		float dot  = (poly [i].x - at.x) * edge.y - (poly [i].y - at.y) * edge.x;
		pos       += dot > 0.0f;
		neg       += dot < 0.0f;
		if ((pos && neg) || dot == 0.0f) return 0;
	}
	return pos > 0 ? 1 : -1;
}

static bool xy_outside(int na, XyPoint const *a, int nb, XyPoint const *b, int limit) {
	for (int i = 0; i < na; i++) {
		XyPoint edge = {a [(i + 1) % na].x - a [i].x, a [(i + 1) % na].y - a [i].y};
		if (xy_side(nb, b, a [i], edge) < limit) return true;
	}
	return false;
}

static bool xy_overlap(XyPoint const *a, XyPoint const *b) {
	return !xy_outside(4, a, 4, b, 0) && !xy_outside(4, b, 4, a, 0);
}

static bool xy_inside(XyPoint const *a, XyPoint const *b) { return !xy_outside(4, b, 4, a, 1); }

static void xy_ccw(XyPoint *p) {
	float ax = p [0].x - p [1].x, ay = p [0].y - p [1].y;
	float bx = p [1].x - p [2].x, by = p [1].y - p [2].y;
	if (ax * by - bx * ay <= 0.0f) return;
	XyPoint t0 = p [0], t1 = p [1];
	p [0] = p [3];
	p [3] = t0;
	p [1] = p [2];
	p [2] = t1;
}

static void xy_cone(XyQuery const *q, XyPoint *cone) {
	bool    left = q->dir == FLUX_KEY_LEFT;
	float   x    = ( float ) (left ? q->bounds.l - 1.0 : q->bounds.r + 1.0);
	double  rot  = left ? XY_PI : 0.0;
	double  d    = q->max * 2.0;
	XyPoint top = {x, ( float ) q->bounds.t}, bottom = {x, ( float ) q->bounds.b};
	XyPoint s0 = {( float ) (top.x + d * cos(rot + XY_CONE_ANGLE)), ( float ) (top.y + d * sin(rot + XY_CONE_ANGLE))};
	XyPoint s1
	  = {( float ) (bottom.x + d * cos(rot - XY_CONE_ANGLE)), ( float ) (bottom.y + d * sin(rot - XY_CONE_ANGLE))};
	cone [0] = left ? top : bottom;
	cone [1] = s0;
	cone [2] = s1;
	cone [3] = left ? bottom : top;
}

static bool xy_considered(XyQuery const *q, XyRect const *c) {
	if (xy_rect_empty(c) || xy_contains(c, &q->bounds)) return false;
	if (!xy_horizontal(q->dir)) return true;
	XyPoint cone [4], pts [4] = {
						{( float ) c->l, ( float ) c->t},
                        {( float ) c->l, ( float ) c->b},
						{( float ) c->r, ( float ) c->b},
                        {( float ) c->r, ( float ) c->t}
    };
	xy_ccw(pts);
	xy_cone(q, cone);
	return xy_overlap(cone, pts) || xy_inside(pts, cone) || xy_inside(cone, pts);
}

static XyRect xy_bounds(FluxUiaContext const *info, XentNodeId node) {
	XentRect visual = {0}, seen = {0};
	if (!flux_uia_place(info, node, -1, &visual, &seen)) return (XyRect) {0};
	return (XyRect) {seen.x, seen.y, seen.x + seen.w, seen.y + seen.h};
}

static bool xy_candidate(XentCtx *ctx, XentNodeId node) {
	XentRect r = {0};
	if (!xent_get_focusable(ctx, node) || !xent_sem_enabled(ctx, node)) return false;
	xent_layout_rect(ctx, node, &r);
	return r.w > 0.0f && r.h > 0.0f;
}

static void xy_collect(XyWalk *w, XentNodeId parent) {
	XentCtx *ctx = w->info.ctx;
	for (XentNodeId c = xent_node_first(ctx, parent); c != XENT_NODE_INVALID; c = xent_node_next(ctx, c)) {
		if (xent_collapsed(ctx, c) || w->count >= XY_MAX_CANDIDATES) continue;
		if (c != w->current && xy_candidate(ctx, c))
			w->items [w->count++] = (XyCandidate) {c, xy_bounds(&w->info, c), 0.0};
		if (xent_sem_enabled(ctx, c)) xy_collect(w, c);
	}
}

static bool xy_farther(FluxKey dir, XyRect const *candidate, XyRect const *farthest) {
	if (dir == FLUX_KEY_LEFT) return candidate->l < farthest->l;
	if (dir == FLUX_KEY_RIGHT) return candidate->r > farthest->r;
	if (dir == FLUX_KEY_UP) return candidate->t < farthest->t;
	return candidate->b > farthest->b;
}

static double xy_far(XyQuery const *q, XyCandidate const *items, int n) {
	XyRect const *m = &items [0].bounds;
	for (int i = 1; i < n; i++) {
		XyRect const *b = &items [i].bounds;
		if (xy_farther(q->dir, b, m)) m = b;
	}
	if (q->dir == FLUX_KEY_LEFT) return fabs(m->r - q->bounds.l);
	if (q->dir == FLUX_KEY_RIGHT) return fabs(q->bounds.r - m->l);
	if (q->dir == FLUX_KEY_UP) return fabs(q->bounds.b - m->t);
	return fabs(m->b - q->bounds.t);
}

static XyQuery const *xy_sort_query;

static int            xy_order(void const *pa, void const *pb) {
	XyCandidate const *a = ( XyCandidate const * ) pa, *b = ( XyCandidate const * ) pb;
	if (a->score != b->score) return a->score > b->score ? -1 : 1;
	bool   v  = !xy_horizontal(xy_sort_query->dir);
	double ka = v ? a->bounds.l : a->bounds.t, kb = v ? b->bounds.l : b->bounds.t;
	if (v && xy_sort_query->rtl) ka = -ka, kb = -kb;
	if (ka != kb) return ka < kb ? -1 : 1;
	return (a->node > b->node) - (a->node < b->node);
}

typedef struct XyHit {
	FluxInput            *input;
	FluxUiaContext const *info;
	XyCandidate const    *target;
	XyRect                area;
	bool                  any;
} XyHit;

static int xy_hit_node(XyHit *h, XentNodeId n) {
	XentCtx *ctx = h->input->ctx;
	if (!xent_sem_enabled(ctx, n) || !input_hit_interactive(h->input, n)) return -1;
	if (flux_node_type(h->info->store, n) == FLUX_CONTROL_SCROLL_BAR) return -1;
	XyRect b = xy_bounds(h->info, n);
	if (!xy_intersect(&b, &h->area)) return -1;
	h->any = true;
	if (!xy_contains(&h->target->bounds, &b)) return 1;
	return n == h->target->node ? 0 : -1;
}

static int xy_hit_walk(XyHit *h, XentNodeId n) {
	XentCtx *ctx = h->input->ctx;
	if (xent_collapsed(ctx, n)) return -1;
	for (XentNodeId c = xent_node_last(ctx, n); c != XENT_NODE_INVALID; c = xent_node_prev(ctx, c)) {
		int r = xy_hit_walk(h, c);
		if (r >= 0) return r;
	}
	return xy_hit_node(h, n);
}

static bool xy_occluded(FluxInput *input, FluxUiaContext const *info, XyCandidate const *c) {
	XyRect const *r = &c->bounds;
	double        w = fabs(r->r - r->l), hgt = fabs(r->b - r->t);
	double        cx = r->l + w / 2.0, cy = r->t + hgt / 2.0;
	XyHit         h = {.input = input, .info = info, .target = c};
	if (xy_rect_empty(r)) return true;
	h.area = (XyRect) {
	  cx - w / 2.0 * XY_HIT_SCALE, cy - hgt / 2.0 * XY_HIT_SCALE, cx + w / 2.0 * XY_HIT_SCALE,
	  cy + hgt / 2.0 * XY_HIT_SCALE};
	int result = xy_hit_walk(&h, info->root);
	if (result >= 0) return result == 1;
	return h.any;
}

static void xy_keep(double *lo, double *hi, double flo, double fhi, double slo, double shi) {
	*lo = fmax(fmax(flo, slo), *lo);
	*hi = fmin(fmin(fhi, shi), *hi);
	if (*hi > *lo) return;
	*lo = flo;
	*hi = fhi;
}

static void xy_manifolds(FluxXyManifolds *m, FluxKey dir, XyRect const *s, XyRect const *f) {
	if (m->v_hi < 0.0) *m = (FluxXyManifolds) {m->h_lo, m->h_hi, s->l, s->r};
	if (m->h_hi < 0.0) *m = (FluxXyManifolds) {s->t, s->b, m->v_lo, m->v_hi};
	if (xy_horizontal(dir)) {
		xy_keep(&m->h_lo, &m->h_hi, f->t, f->b, s->t, s->b);
		*m = (FluxXyManifolds) {m->h_lo, m->h_hi, f->l, f->r};
		return;
	}
	xy_keep(&m->v_lo, &m->v_hi, f->l, f->r, s->l, s->r);
	*m = (FluxXyManifolds) {f->t, f->b, m->v_lo, m->v_hi};
}

static XentNodeId xy_pick(FluxInput *input, FluxUiaContext const *info, XyQuery const *q, XyCandidate *items, int n) {
	for (int i = 0; i < n; i++)
		items [i].score = xy_considered(q, &items [i].bounds) ? xy_score(q, &items [i].bounds) : 0.0;
	xy_sort_query = q;
	qsort(items, ( size_t ) n, sizeof(*items), xy_order);
	for (int i = 0; i < n && items [i].score > 0.0; i++) {
		if (xy_occluded(input, info, &items [i])) continue;
		xy_manifolds(&input->xy, q->dir, &q->bounds, &items [i].bounds);
		return items [i].node;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId xy_tree_root(XentCtx *ctx, XentNodeId node) {
	for (XentNodeId parent = xent_node_parent(ctx, node); parent != XENT_NODE_INVALID;
	  parent               = xent_node_parent(ctx, node))
		node = parent;
	return node;
}

bool flux_input_xyfocus(FluxInput *input, XentNodeId scope, FluxKey dir) {
	if (!input || scope == XENT_NODE_INVALID || input->focused == XENT_NODE_INVALID) return false;
	HWND   hwnd = input->window ? flux_window_hwnd(input->window) : NULL;
	XyWalk w    = {
	  .info    = {.ctx = input->ctx, .root = xy_tree_root(input->ctx, scope), .hwnd = hwnd, .store = input->store},
	  .current = input->focused
    };
	w.items = ( XyCandidate * ) calloc(XY_MAX_CANDIDATES, sizeof(XyCandidate));
	if (!w.items) return false;
	xy_collect(&w, scope);
	XyRect  rb      = xy_bounds(&w.info, scope);
	XyQuery q       = {.input = input, .dir = dir, .bounds = xy_bounds(&w.info, input->focused)};
	q.rtl           = xent_resolved_dir(input->ctx, input->focused) == XENT_DIRECTION_RTL;
	XentNodeId next = XENT_NODE_INVALID;
	if (w.count > 0) {
		q.max = fmax(fmax(rb.r - rb.l, rb.b - rb.t), xy_far(&q, w.items, w.count));
		next  = xy_pick(input, &w.info, &q, w.items, w.count);
	}
	free(w.items);
	if (next == XENT_NODE_INVALID) return false;
	flux_input_setfocus(input, next, false);
	return true;
}

void input_xy_reset(FluxInput *input) { input->xy = (FluxXyManifolds) {-1.0, -1.0, -1.0, -1.0}; }
