/**
 * @file flux_display_paint.c
 * @brief Shared classic/compose MAIN/OVERLAY paint: record → replay.
 */
#include "flux_display_paint.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_internal.h"

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
paint_replay_translated(XtkBackend *backend, FluxRenderCtx const *rc, XentDl const *list, float ox, float oy) {
	D2D1_MATRIX_3X2_F saved;
	ID2D1RenderTarget_GetTransform(FLUX_RT(rc), &saved);

	D2D1_MATRIX_3X2_F translate = {1.0f, 0.0f, 0.0f, 1.0f, ox, oy};
	D2D1_MATRIX_3X2_F combined  = paint_mul(&translate, &saved);
	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &combined);

	if (backend && backend->vt && backend->vt->replay_display_list) {
		flux_xtk_setrc(backend, rc);
		backend->vt->replay_display_list(backend, list);
	}
	else {
		/* Imperative demos may host without a backend; replay still needs the DC. */
		( void ) flux_xtk_replay_rc(rc, list);
	}

	ID2D1RenderTarget_SetTransform(FLUX_RT(rc), &saved);
}

void flux_display_paint_main(
  FluxStore *store, XtkBackend *backend, FluxControlRegistry const *registry, FluxRenderCtx const *rc,
  FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (!store || !rc || !snap || !bounds || !state || !rc->d2d || !rc->brush) return;

	FluxControlRenderer const *renderer = flux_control_registry_get(registry, snap->type);
	if (renderer && renderer->record) {
		FluxRect local = {0.0f, 0.0f, bounds->w, bounds->h};
		XentDl  *list  = renderer->record(rc, snap, &local, state);
		flux_node_adoptdl(store, ( XentNodeId ) snap->id, list);
	}

	XentDl const *list = flux_node_dl(store, ( XentNodeId ) snap->id);
	if (list) paint_replay_translated(backend, rc, list, bounds->x, bounds->y);
}

void flux_display_paint_overlay(
  XtkBackend *backend, FluxControlRegistry const *registry, FluxRenderCtx const *rc, FluxRenderSnap const *snap,
  FluxRect const *bounds, FluxControlState const *state
) {
	if (!rc || !snap || !bounds || !rc->d2d || !rc->brush) return;

	FluxControlRenderer const *renderer = flux_control_registry_get(registry, snap->type);
	if (!renderer || !renderer->record_overlay) return;

	FluxRect local = {0.0f, 0.0f, bounds->w, bounds->h};
	XentDl  *list  = renderer->record_overlay(rc, snap, &local, state);
	if (list) paint_replay_translated(backend, rc, list, bounds->x, bounds->y);
	if (list) xent_dl_free(list);
}
