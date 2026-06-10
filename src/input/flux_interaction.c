#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "flux_interaction.h"

#include <cwinrt/cast.h>
#include <cwinrt/factory.h>
#include <cwinrt/hstring.h>
#include <cwinrt/Windows.UI.Composition.h>
#include <cwinrt/Windows.UI.Composition.Interactions.h>
#include <cwinrt/Windows.UI.Input.h>

#include <stdlib.h>

struct FluxInteraction {
	WUC_Comp                        *comp;
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
	it->comp = comp;
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

static WUC_ExpressionAnimation *interaction_make_offset_expr(FluxInteraction *it) {
	cwinrt_hstring expr_str = NULL;
	if (FAILED(cwinrt_hstring_from(L"-tracker.Position", &expr_str))) return NULL;
	WUC_ExpressionAnimation *expr = NULL;
	HRESULT                  hr   = wuc_comp_create_expression_animation_str_p(it->comp, expr_str, &expr);
	cwinrt_hstring_free(expr_str);
	return SUCCEEDED(hr) ? expr : NULL;
}

static HRESULT
interaction_start_offset_expr(FluxInteraction *it, WUC_ExpressionAnimation *expr, WUC_Visual *target_visual) {
	WUC_CompositionAnimation *anim    = NULL;
	WUC_CompositionObject    *tracker = NULL;
	WUC_CompositionObject    *target  = NULL;
	cwinrt_hstring            key     = NULL;
	cwinrt_hstring            prop    = NULL;
	HRESULT                   hr      = cwinrt_query(expr, &CWINRT_IID_WUC_ICompositionAnimation, ( void ** ) &anim);
	if (SUCCEEDED(hr)) hr = cwinrt_query(it->tracker, &CWINRT_IID_WUC_ICompositionObject, ( void ** ) &tracker);
	if (SUCCEEDED(hr)) hr = cwinrt_hstring_from(L"tracker", &key);
	if (SUCCEEDED(hr)) {
		( void ) wuc_composition_animation_set_reference_parameter(anim, key, tracker);
		cwinrt_hstring_free(key);
		hr = cwinrt_query(target_visual, &CWINRT_IID_WUC_ICompositionObject, ( void ** ) &target);
	}
	if (SUCCEEDED(hr)) hr = cwinrt_hstring_from(L"Offset", &prop);
	if (SUCCEEDED(hr)) {
		( void ) wuc_composition_object_start_animation(target, prop, anim);
		cwinrt_hstring_free(prop);
	}

	if (target) (( IUnknown * ) target)->lpVtbl->Release(( IUnknown * ) target);
	if (tracker) (( IUnknown * ) tracker)->lpVtbl->Release(( IUnknown * ) tracker);
	if (anim) (( IUnknown * ) anim)->lpVtbl->Release(( IUnknown * ) anim);
	return hr;
}

HRESULT flux_interaction_bind_offset(FluxInteraction *it, WUC_Visual *target_visual) {
	if (!it || !it->comp || !it->tracker || !target_visual) return E_INVALIDARG;

	WUC_ExpressionAnimation *expr = interaction_make_offset_expr(it);
	if (!expr) return E_FAIL;

	HRESULT hr = interaction_start_offset_expr(it, expr, target_visual);
	(( IUnknown * ) expr)->lpVtbl->Release(( IUnknown * ) expr);
	return hr;
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

HRESULT flux_interaction_redirect_pointer_id(FluxInteraction *it, uint32_t pointer_id) {
	if (!it || !it->source) return E_INVALIDARG;

	WUIIN_IPointerPointStatics *statics = NULL;
	HRESULT                     hr      = cwinrt_factory_get_statics(
	  L"Windows.UI.Input.PointerPoint", &CWINRT_IID_WUIIN_IPointerPointStatics, ( void ** ) &statics
	);
	if (FAILED(hr)) return hr;

	WUIIN_PointerPoint *pp = NULL;
	hr                     = wuiin_pointer_point_statics_get_current_point(statics, pointer_id, &pp);
	(( IUnknown * ) statics)->lpVtbl->Release(( IUnknown * ) statics);
	if (FAILED(hr) || !pp) return FAILED(hr) ? hr : E_FAIL;

	hr = flux_interaction_redirect(it, pp);
	(( IUnknown * ) pp)->lpVtbl->Release(( IUnknown * ) pp);
	return hr;
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
