#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_popup.h>

#include <math.h>

#define GEOM_DEPTH 256
#define GEOM_FAR   1.0e9f

typedef struct GeomWalk {
	float    ox;
	float    oy;
	XentRect clip;
	XentRect visual;
	XentRect layout;
	bool     shown;
} GeomWalk;

static void geom_cut(XentRect *r, XentRect c) {
	float x0 = fmaxf(r->x, c.x), y0 = fmaxf(r->y, c.y);
	float x1 = fminf(r->x + r->w, c.x + c.w), y1 = fminf(r->y + r->h, c.y + c.h);
	*r = x1 < x0 || y1 < y0 ? (XentRect) {0} : (XentRect) {x0, y0, x1 - x0, y1 - y0};
}

static bool geom_moved(FluxNodeData const *nd) {
	return nd->render_scale != 1.0f
	    || nd->render_scale_x != 1.0f
	    || nd->render_scale_y != 1.0f
	    || nd->render_opacity < 1.0f
	    || nd->motion_opacity < 1.0f
	    || nd->render_translate_x != 0.0f
	    || nd->motion_translate_x != 0.0f
	    || nd->render_translate_y != 0.0f
	    || nd->motion_translate_y != 0.0f
	    || nd->render_rotate_x != 0.0f
	    || nd->render_rotate_y != 0.0f
	    || nd->render_rotate_z != 0.0f;
}

static uint32_t geom_chain(FluxUiaContext const *info, XentNodeId node, XentNodeId *chain) {
	uint32_t n = 0;
	for (XentNodeId c = node; c != XENT_NODE_INVALID && n < GEOM_DEPTH; c = xent_node_parent(info->ctx, c))
		chain [n++] = c;
	return n;
}

static uint32_t geom_lift(FluxUiaContext const *info, XentNodeId const *chain, uint32_t n) {
	for (uint32_t i = 0; i < n; i++)
		if (flux_node_lifted(info->store, chain [i])) return i;
	return n;
}

static bool geom_top(FluxUiaContext const *info, XentNodeId top, XentRect *clip) {
	*clip = (XentRect) {-GEOM_FAR, -GEOM_FAR, 2.0f * GEOM_FAR, 2.0f * GEOM_FAR};
	if (top == info->root) {
		RECT  client = {0};
		float scale  = ( float ) GetDpiForWindow(info->hwnd) / 96.0f;
		GetClientRect(info->hwnd, &client);
		*clip = (XentRect) {0.0f, 0.0f, ( float ) client.right / scale, ( float ) client.bottom / scale};
		return true;
	}
	FluxNodeData const *nd = flux_store_find(info->store, top);
	return nd && nd->popup && flux_popup_visible(nd->popup);
}

static void geom_scroll(FluxNodeData const *nd, GeomWalk *w) {
	if (nd->component_type != FLUX_CONTROL_SCROLL || !nd->component_data) return;
	FluxScrollData const *sd  = ( FluxScrollData const * ) nd->component_data;
	w->ox                    -= flux_scroll_off_x(sd);
	w->oy                    -= flux_scroll_off_y(sd);
}

static void geom_frame(FluxNodeData const *nd, GeomWalk *w, bool clips) {
	XentRect r = w->visual;
	if (clips && nd->render_clip_subtree && geom_moved(nd)) geom_cut(&w->clip, r);
	float tx     = nd->render_translate_x + nd->motion_translate_x;
	float ty     = nd->render_translate_y + nd->motion_translate_y;
	w->ox       += tx;
	w->oy       += ty;
	w->visual.x += tx;
	w->visual.y += ty;
	r            = w->visual;
	if (clips && nd->clip_top > 0.0f)
		geom_cut(&w->clip, (XentRect) {r.x, r.y + nd->clip_top, r.w, fmaxf(r.h - nd->clip_top, 0.0f)});
}

static void geom_step(FluxUiaContext const *info, XentNodeId n, GeomWalk *w, bool clips, bool self) {
	FluxNodeData const *nd = flux_store_find(info->store, n);
	xent_layout_rect(info->ctx, n, &w->layout);
	w->visual  = (XentRect) {w->layout.x + w->ox, w->layout.y + w->oy, w->layout.w, w->layout.h};
	w->shown  &= !xent_collapsed(info->ctx, n);
	if (!nd) return;
	geom_frame(nd, w, clips);
	if (self) return;
	bool scroll = nd->component_type == FLUX_CONTROL_SCROLL;
	if (clips && (scroll || nd->clips_children)) geom_cut(&w->clip, w->visual);
	geom_scroll(nd, w);
}

static GeomWalk geom_walk(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId chain [GEOM_DEPTH];
	uint32_t   n    = geom_chain(info, node, chain);
	uint32_t   lift = geom_lift(info, chain, n);
	GeomWalk   w    = {0};
	w.shown         = n > 0 && n < GEOM_DEPTH && geom_top(info, chain [n - 1u], &w.clip);
	for (uint32_t i = n; i > 0; i--) geom_step(info, chain [i - 1u], &w, i - 1u <= lift, i == 1u);
	return w;
}

static bool geom_sub(FluxUiaContext const *info, XentNodeId node, int sub, GeomWalk *w) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	XentRect            r = w->layout;
	if (!h || !h->sub || !h->sub->rect || !h->sub->rect(info->store, node, sub, &r)) return true;
	if (r.w > 0.0f && xent_resolved_dir(info->ctx, node) == XENT_DIRECTION_RTL)
		r.x = 2.0f * w->layout.x + w->layout.w - r.x - r.w;
	w->visual = (XentRect) {r.x + w->visual.x - w->layout.x, r.y + w->visual.y - w->layout.y, r.w, r.h};
	return true;
}

bool flux_uia_place(FluxUiaContext const *info, XentNodeId node, int sub, XentRect *visual, XentRect *seen) {
	GeomWalk w = geom_walk(info, node);
	if (sub >= 0) geom_sub(info, node, sub, &w);
	*visual = w.visual;
	*seen   = w.visual;
	geom_cut(seen, w.clip);
	return w.shown && (seen->w > 0.0f || seen->h > 0.0f);
}

bool flux_uia_bounds(FluxUiaContext const *info, XentNodeId node, int sub, struct UiaRect *out) {
	XentRect visual = {0}, seen = {0};
	*out = (struct UiaRect) {0};
	if (!flux_uia_place(info, node, sub, &visual, &seen)) return false;
	float scale  = 1.0f;
	POINT origin = {0, 0};
	flux_uia_origin(info, node, &origin, &scale);
	*out = (struct UiaRect) {origin.x + seen.x * scale, origin.y + seen.y * scale, seen.w * scale, seen.h * scale};
	return true;
}

static bool geom_hit(FluxUiaContext const *info, XentNodeId node, int sub, double x, double y) {
	struct UiaRect r;
	if (!flux_uia_bounds(info, node, sub, &r)) return false;
	return x >= r.left && y >= r.top && x < r.left + r.width && y < r.top + r.height;
}

static XentNodeId geom_deepest(FluxUiaContext const *info, XentNodeId node, double x, double y) {
	XentNodeId best = XENT_NODE_INVALID;
	for (XentNodeId c = flux_uia_first(info, node); c != XENT_NODE_INVALID; c = flux_uia_next(info, c)) {
		XentNodeId hit = geom_deepest(info, c, x, y);
		if (hit != XENT_NODE_INVALID) best = hit;
	}
	if (best != XENT_NODE_INVALID) return best;
	return geom_hit(info, node, -1, x, y) ? node : XENT_NODE_INVALID;
}

static int geom_sub_at(FluxUiaContext const *info, XentNodeId node, double x, double y) {
	FluxUia probe = {.info = *info, .node = node, .sub = -1};
	int     first = 0, last = -1, heads = flux_uia_sub_heads(&probe);
	for (int i = 0; i < heads; i++)
		if (geom_hit(info, node, FLUX_UIA_SUB_HEAD + i, x, y)) return FLUX_UIA_SUB_HEAD + i;
	if (!flux_uia_sub_range(info, node, &first, &last)) return -1;
	for (int i = first; i <= last; i++)
		if (geom_hit(info, node, i, x, y)) return i;
	return -1;
}

XentNodeId flux_uia_hit(FluxUiaContext const *info, double x, double y, int *sub) {
	XentNodeId hit = geom_deepest(info, info->root, x, y);
	*sub           = hit != XENT_NODE_INVALID && hit != info->root ? geom_sub_at(info, hit, x, y) : -1;
	return hit;
}
