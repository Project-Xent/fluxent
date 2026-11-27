#include "flux_display_paint.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_cache.h"
#include "render/flux_render_internal.h"
#include "controls/flux_module.h"

#include <xent/xent_display.h>

static D2D1_MATRIX_3X2_F paint_mul(D2D1_MATRIX_3X2_F const *a, D2D1_MATRIX_3X2_F const *b) {
	D2D1_MATRIX_3X2_F m;
	m._11 = a->_11 * b->_11 + a->_12 * b->_21;
	m._12 = a->_11 * b->_12 + a->_12 * b->_22;
	m._21 = a->_21 * b->_11 + a->_22 * b->_21;
	m._22 = a->_21 * b->_12 + a->_22 * b->_22;
	m._31 = a->_31 * b->_11 + a->_32 * b->_21 + b->_31;
	m._32 = a->_31 * b->_12 + a->_32 * b->_22 + b->_32;
	return m;
}

static void
paint_replay_translated(XtkBackend *backend, FluxRenderCtx const *rc, XentDl const *list, FluxRect const *at) {
	D2D1_MATRIX_3X2_F saved;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &saved);

	D2D1_MATRIX_3X2_F translate = {._11 = 1.0f, ._22 = 1.0f, ._31 = at->x, ._32 = at->y};
	D2D1_MATRIX_3X2_F combined  = paint_mul(&translate, &saved);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);

	if (backend && backend->vt && backend->vt->replay_display_list) {
		flux_xtk_setrc(backend, rc);
		backend->vt->replay_display_list(backend, list);
	}
	else { ( void ) flux_xtk_replay_rc(rc, list); }

	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &saved);
}

static void paint_propagate_timers(FluxRenderCtx const *rc, bool local_anim, double local_redraw) {
	if (local_anim && rc->animations_active) *rc->animations_active = true;
	if (local_redraw > 0.0 && rc->next_redraw && (*rc->next_redraw <= 0.0 || local_redraw < *rc->next_redraw))
		*rc->next_redraw = local_redraw;
}

static bool
paint_can_reuse(FluxNodeData const *nd, uint64_t hash, float w, float h, bool force_dyn, FluxCacheEntry const *entry) {
	if (flux_rcache_entry_animating(entry)) return false;
	return nd
	    && !nd->dl_dynamic
	    && !force_dyn
	    && nd->display_list
	    && nd->display_list_owned
	    && nd->dl_hash == hash
	    && nd->dl_w == w
	    && nd->dl_h == h;
}

static void paint_record_main(
  FluxStore *store, FluxControlModule const *module, FluxRenderCtx const *rc, FluxRenderSnap const *snap,
  FluxRect const *bounds, FluxControlState const *state
) {
	XentNodeId            node      = ( XentNodeId ) snap->id;
	FluxNodeData         *nd        = flux_store_find(store, node);
	bool                  force_dyn = false;
	uint64_t              hash      = flux_snap_hash(snap, state, bounds->w, bounds->h, rc, &force_dyn);
	FluxCacheEntry const *entry     = rc->cache ? flux_render_cache_get(rc->cache, ( uint64_t ) node) : NULL;
	if (paint_can_reuse(nd, hash, bounds->w, bounds->h, force_dyn, entry)) return;

	FluxRect      local        = {0.0f, 0.0f, bounds->w, bounds->h};
	bool          local_anim   = false;
	double        local_redraw = 0.0;
	FluxRenderCtx rec_rc       = *rc;
	rec_rc.animations_active   = &local_anim;
	rec_rc.next_redraw         = &local_redraw;
	XentDl *list               = module->record(&rec_rc, snap, &local, state);
	if (snap->rtl) xent_dl_mirror(list, bounds->w);
	flux_node_adoptdl(store, node, list);
	nd = flux_store_find(store, node);
	if (nd) {
		nd->dl_hash    = hash;
		nd->dl_w       = bounds->w;
		nd->dl_h       = bounds->h;
		nd->dl_dynamic = (local_anim || local_redraw > 0.0 || force_dyn) ? 1u : 0u;
	}
	paint_propagate_timers(rc, local_anim, local_redraw);
}

void flux_display_paint_main(
  FluxStore *store, XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (!store || !rc || !snap || !bounds || !state || !rc->d2d || !rc->brush) return;

	FluxControlModule const *module = flux_module_get(snap->type);
	if (module && module->record) paint_record_main(store, module, rc, snap, bounds, state);

	XentDl const *list = flux_node_dl(store, ( XentNodeId ) snap->id);
	if (list) paint_replay_translated(backend, rc, list, bounds);
}

void flux_display_paint_overlay(
  XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (!rc || !snap || !bounds || !rc->d2d || !rc->brush) return;

	FluxControlModule const *module = flux_module_get(snap->type);
	if (!module || !module->overlay) return;

	FluxRect local = {0.0f, 0.0f, bounds->w, bounds->h};
	XentDl  *list  = module->overlay(rc, snap, &local, state);
	if (snap->rtl) xent_dl_mirror(list, bounds->w);
	if (list) paint_replay_translated(backend, rc, list, bounds);
	if (list) xent_dl_free(list);
}
