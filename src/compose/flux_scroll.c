#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_scroll.h"

#include <cwinrt/cast.h>
#include <cwinrt/factory.h>

#include <stdlib.h>

struct FluxScrollTracker {
	WUC_Comp                        *comp;
	WUICOIN_InteractionTracker      *tracker;
	WUICOIN_VisualInteractionSource *source;
};

static WUICOIN_VisualInteractionSource *make_source(WUC_Visual *visual) {
	WUICOIN_IVisualInteractionSourceStatics *statics = NULL;
	if (FAILED(cwinrt_factory_get_statics(
		  L"Windows.UI.Composition.Interactions.VisualInteractionSource",
		  &CWINRT_IID_WUICOIN_IVisualInteractionSourceStatics, ( void ** ) &statics
		)))
		return NULL;

	WUICOIN_VisualInteractionSource *source = NULL;
	( void ) wuicoin_visual_interaction_source_statics_create(statics, visual, &source);
	(( IUnknown * ) statics)->lpVtbl->Release(( IUnknown * ) statics);
	return source;
}

FluxScrollTracker *flux_scroll_tracker_create(FluxCompositor *c, WUC_Visual *source_visual) {
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp || !source_visual) return NULL;

	FluxScrollTracker *t = ( FluxScrollTracker * ) calloc(1, sizeof(*t));
	if (!t) return NULL;
	t->comp = comp;

	if (FAILED(wuicoin_interaction_tracker_create(comp, &t->tracker))) goto fail;

	t->source = make_source(source_visual);
	if (!t->source) goto fail;

	( void ) wuicoin_visual_interaction_source_put__position_y_source_mode(
	  t->source, WUICOIN_InteractionSourceMode_EnabledWithInertia
	);
	( void ) wuicoin_visual_interaction_source_put__manipulation_redirection_mode(
	  t->source, WUICOIN_VisualInteractionSourceRedirectionMode_CapableTouchpadAndPointerWheel
	);

	WUICOIN_CompositionInteractionSourceCollection *sources = NULL;
	if (SUCCEEDED(wuicoin_interaction_tracker_get__interaction_sources(t->tracker, &sources))) {
		WUICOIN_ICompositionInteractionSource *as_src = NULL;
		if (SUCCEEDED(cwinrt_query(t->source, &CWINRT_IID_WUICOIN_ICompositionInteractionSource, ( void ** ) &as_src)))
		{
			( void ) wuicoin_composition_interaction_source_collection_add(sources, as_src);
			(( IUnknown * ) as_src)->lpVtbl->Release(( IUnknown * ) as_src);
		}
		(( IUnknown * ) sources)->lpVtbl->Release(( IUnknown * ) sources);
	}

	return t;

fail:
	flux_scroll_tracker_destroy(t);
	return NULL;
}

void flux_scroll_tracker_destroy(FluxScrollTracker *t) {
	if (!t) return;
	if (t->source) (( IUnknown * ) t->source)->lpVtbl->Release(( IUnknown * ) t->source);
	if (t->tracker) (( IUnknown * ) t->tracker)->lpVtbl->Release(( IUnknown * ) t->tracker);
	free(t);
}

void flux_scroll_tracker_set_max_y(FluxScrollTracker *t, float max_y) {
	if (!t || !t->tracker) return;
	WFN_Vector_3 max = {0.0f, max_y, 0.0f};
	( void ) wuicoin_interaction_tracker_put__max_position(t->tracker, max);
}

WUICOIN_InteractionTracker *flux_scroll_tracker_handle(FluxScrollTracker *t) { return t ? t->tracker : NULL; }

void                        flux_scroll_tracker_bind_offset(FluxScrollTracker *t, WUC_Visual *content) {
	if (!t || !t->comp || !t->tracker || !content) return;

	WUC_ExpressionAnimation *expr  = NULL;
	cwinrt_hstring           hexpr = NULL;
	if (FAILED(cwinrt_hstring_from(L"Vector3(0, -tracker.Position.Y, 0)", &hexpr))) return;
	HRESULT hr = wuc_comp_create_expression_animation_str_p(t->comp, hexpr, &expr);
	cwinrt_hstring_free(hexpr);
	if (FAILED(hr)) return;

	WUC_CompositionAnimation *anim    = NULL;
	WUC_CompositionObject    *tracker = NULL;
	WUC_CompositionObject    *target  = NULL;
	if (SUCCEEDED(cwinrt_query(expr, &CWINRT_IID_WUC_ICompositionAnimation, ( void ** ) &anim))
		&& SUCCEEDED(cwinrt_query(t->tracker, &CWINRT_IID_WUC_ICompositionObject, ( void ** ) &tracker))
		&& SUCCEEDED(cwinrt_query(content, &CWINRT_IID_WUC_ICompositionObject, ( void ** ) &target)))
	{
		cwinrt_hstring hkey = NULL;
		if (SUCCEEDED(cwinrt_hstring_from(L"tracker", &hkey))) {
			( void ) wuc_composition_animation_set_reference_parameter(anim, hkey, tracker);
			cwinrt_hstring_free(hkey);
		}
		cwinrt_hstring hprop = NULL;
		if (SUCCEEDED(cwinrt_hstring_from(L"Offset", &hprop))) {
			( void ) wuc_composition_object_start_animation(target, hprop, anim);
			cwinrt_hstring_free(hprop);
		}
	}
	if (target) (( IUnknown * ) target)->lpVtbl->Release(( IUnknown * ) target);
	if (tracker) (( IUnknown * ) tracker)->lpVtbl->Release(( IUnknown * ) tracker);
	if (anim) (( IUnknown * ) anim)->lpVtbl->Release(( IUnknown * ) anim);
	(( IUnknown * ) expr)->lpVtbl->Release(( IUnknown * ) expr);
}
