#include "flux_display_paint.h"
#include "render/flux_dl_replay.h"
#include "render/flux_fluent.h"
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

/* Record in local space, then place on whole device pixels so hairlines stay crisp after translate. */
static FluxRect paint_place_bounds(FluxRenderCtx const *rc, FluxRect const *bounds) {
	float sx = rc && rc->dpi.dpi_x > 0.0f ? rc->dpi.dpi_x / FLUX_DPI_BASE : 1.0f;
	float sy = rc && rc->dpi.dpi_y > 0.0f ? rc->dpi.dpi_y / FLUX_DPI_BASE : 1.0f;
	if (sx <= 0.0f) sx = 1.0f;
	if (sy <= 0.0f) sy = 1.0f;
	return flux_snap_bounds(bounds, sx, sy);
}

static void
paint_replay_translated(XtkBackend *backend, FluxRenderCtx const *rc, XentDl const *list, FluxRect const *at) {
	D2D1_MATRIX_3X2_F saved;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &saved);

	D2D1_MATRIX_3X2_F translate = {1.0f, 0.0f, 0.0f, 1.0f, at->x, at->y};
	D2D1_MATRIX_3X2_F combined  = paint_mul(&translate, &saved);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);

	if (backend && backend->vt && backend->vt->replay_display_list) {
		flux_xtk_setrc(backend, rc);
		backend->vt->replay_display_list(backend, list);
	}
	else { ( void ) flux_xtk_replay_rc(rc, list); }

	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &saved);
}

void flux_display_paint_main(
  FluxStore *store, XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (!store || !rc || !snap || !bounds || !state || !rc->d2d || !rc->brush) return;

	FluxRect                placed = paint_place_bounds(rc, bounds);
	FluxControlModule const *module = flux_module_get(snap->type);
	if (module && module->record) {
		FluxRect local = {0.0f, 0.0f, placed.w, placed.h};
		XentDl  *list  = module->record(rc, snap, &local, state);
		if (snap->rtl) xent_dl_mirror(list, placed.w);
		flux_node_adoptdl(store, ( XentNodeId ) snap->id, list);
	}

	XentDl const *list = flux_node_dl(store, ( XentNodeId ) snap->id);
	if (list) paint_replay_translated(backend, rc, list, &placed);
}

void flux_display_paint_overlay(
  XtkBackend *backend, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (!rc || !snap || !bounds || !rc->d2d || !rc->brush) return;

	FluxControlModule const *module = flux_module_get(snap->type);
	if (!module || !module->overlay) return;

	FluxRect placed = paint_place_bounds(rc, bounds);
	FluxRect local  = {0.0f, 0.0f, placed.w, placed.h};
	XentDl  *list   = module->overlay(rc, snap, &local, state);
	if (snap->rtl) xent_dl_mirror(list, placed.w);
	if (list) paint_replay_translated(backend, rc, list, &placed);
	if (list) xent_dl_free(list);
}
