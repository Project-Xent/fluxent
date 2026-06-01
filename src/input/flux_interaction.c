#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "flux_interaction.h"

#include <cwinrt/cast.h>
#include <cwinrt/Windows.UI.Composition.Interactions.h>

#include <stdlib.h>

struct FluxInteraction {
	WUICOIN_InteractionTracker      *tracker;
	WUICOIN_VisualInteractionSource *source;
	float                            last_x;
	float                            last_y;
};

static bool interaction_attach_source(FluxInteraction *it, WUC_Visual *scroll_visual) {
	if (FAILED(wuicoin_visual_interaction_source_create(scroll_visual, &it->source)) || !it->source) return false;

	wuicoin_visual_interaction_source_put__position_x_source_mode(
	  it->source, WUICOIN_InteractionSourceMode_EnabledWithInertia
	);
	wuicoin_visual_interaction_source_put__position_y_source_mode(
	  it->source, WUICOIN_InteractionSourceMode_EnabledWithInertia
	);
	wuicoin_visual_interaction_source_put__manipulation_redirection_mode(
	  it->source, WUICOIN_VisualInteractionSourceRedirectionMode_CapableTouchpadAndPointerWheel
	);

	WUICOIN_CompositionInteractionSourceCollection *sources = NULL;
	if (FAILED(wuicoin_interaction_tracker_get__interaction_sources(it->tracker, &sources)) || !sources) return false;

	WUICOIN_ICompositionInteractionSource *as_source = NULL;
	HRESULT hr = cwinrt_query(it->source, &CWINRT_IID_WUICOIN_ICompositionInteractionSource, ( void ** ) &as_source);
	if (SUCCEEDED(hr)) {
		( void ) wuicoin_composition_interaction_source_collection_add(sources, as_source);
		(( IUnknown * ) as_source)->lpVtbl->Release(( IUnknown * ) as_source);
	}
	(( IUnknown * ) sources)->lpVtbl->Release(( IUnknown * ) sources);
	return SUCCEEDED(hr);
}

FluxInteraction *flux_interaction_create(FluxCompositor *c, WUC_Visual *scroll_visual) {
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp || !scroll_visual) return NULL;

	FluxInteraction *it = ( FluxInteraction * ) calloc(1, sizeof(*it));
	if (!it) return NULL;
	if (FAILED(wuicoin_interaction_tracker_create(comp, &it->tracker)) || !it->tracker) {
		free(it);
		return NULL;
	}
	if (!interaction_attach_source(it, scroll_visual)) {
		flux_interaction_destroy(it);
		return NULL;
	}
	return it;
}

void flux_interaction_destroy(FluxInteraction *it) {
	if (!it) return;
	if (it->source) (( IUnknown * ) it->source)->lpVtbl->Release(( IUnknown * ) it->source);
	if (it->tracker) (( IUnknown * ) it->tracker)->lpVtbl->Release(( IUnknown * ) it->tracker);
	free(it);
}

void flux_interaction_set_extent(FluxInteraction *it, float max_x, float max_y) {
	if (!it || !it->tracker) return;
	WFN_Vector_3 lo = {0.0f, 0.0f, 0.0f};
	WFN_Vector_3 hi = {max_x > 0.0f ? max_x : 0.0f, max_y > 0.0f ? max_y : 0.0f, 0.0f};
	( void ) wuicoin_interaction_tracker_put__min_position(it->tracker, lo);
	( void ) wuicoin_interaction_tracker_put__max_position(it->tracker, hi);
}

HRESULT flux_interaction_redirect(FluxInteraction *it, void *pointer_point) {
	if (!it || !it->source || !pointer_point) return E_INVALIDARG;
	return wuicoin_visual_interaction_source_try_redirect_for_manipulation(
	  it->source, ( WUIIN_PointerPoint * ) pointer_point
	);
}

void flux_interaction_scroll_to(FluxInteraction *it, float x, float y) {
	if (!it || !it->tracker) return;
	WFN_Vector_3 pos = {x, y, 0.0f};
	int32_t      req = 0;
	( void ) wuicoin_interaction_tracker_try_update_position(it->tracker, pos, &req);
}

bool flux_interaction_poll(FluxInteraction *it, float *out_x, float *out_y) {
	if (!it || !it->tracker) return false;
	WFN_Vector_3 pos = {0.0f, 0.0f, 0.0f};
	if (FAILED(wuicoin_interaction_tracker_get__position(it->tracker, &pos))) return false;

	bool moved = pos.X != it->last_x || pos.Y != it->last_y;
	it->last_x = pos.X;
	it->last_y = pos.Y;
	if (out_x) *out_x = pos.X;
	if (out_y) *out_y = pos.Y;
	return moved;
}
