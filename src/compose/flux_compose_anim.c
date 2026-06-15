#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_compose_anim.h"

#include <cwinrt/cast.h>
#include <cwinrt/hstring.h>

#include <stdint.h>
#include <stdlib.h>

struct FluxAnimKit {
	WUC_Comp                      *c;
	WUC_CubicBezierEasingFunction *easings [FLUX_EASE_COUNT]; /**< Lazily created; [LINEAR] unused. */
};

/* WinUI-ish cubic-bezier control points. */
static WFN_Vector_2 const kEaseCP1 [FLUX_EASE_COUNT] = {
  {0.0f, 0.0f},
  {0.1f, 0.9f},
  {0.7f, 0.0f},
  {0.1f, 0.9f},
};
static WFN_Vector_2 const kEaseCP2 [FLUX_EASE_COUNT] = {
  {0.0f, 0.0f},
  {0.2f, 1.0f},
  {1.0f, 0.5f},
  {0.2f, 1.0f},
};

FluxAnimKit *flux_anim_kit_create(FluxCompositor *c) {
	if (!c) return NULL;
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxAnimKit *kit = ( FluxAnimKit * ) calloc(1, sizeof(*kit));
	if (!kit) return NULL;
	kit->c = comp;
	return kit;
}

void flux_anim_kit_destroy(FluxAnimKit *kit) {
	if (!kit) return;
	for (int i = 0; i < FLUX_EASE_COUNT; i++)
		if (kit->easings [i]) (( IUnknown * ) kit->easings [i])->lpVtbl->Release(( IUnknown * ) kit->easings [i]);
	free(kit);
}

/* Returns a CompositionEasingFunction (caller releases) for the curve, or NULL
 * for linear / on failure (NULL easing => the compositor's linear default). */
static WUC_CompositionEasingFunction *kit_easing(FluxAnimKit *kit, FluxEaseId ease) {
	if (ease <= FLUX_EASE_LINEAR || ease >= FLUX_EASE_COUNT) return NULL;
	if (!kit->easings [ease]
		&& FAILED(
		  wuc_comp_create_cubic_bezier_easing_function(kit->c, kEaseCP1 [ease], kEaseCP2 [ease], &kit->easings [ease])
		))
		return NULL;

	WUC_CompositionEasingFunction *ef = NULL;
	if (FAILED(cwinrt_query(kit->easings [ease], &CWINRT_IID_WUC_ICompositionEasingFunction, ( void ** ) &ef)))
		return NULL;
	return ef;
}

static WF_TimeSpan seconds_to_timespan(double s) {
	WF_TimeSpan ts;
	ts.Duration = ( int64_t ) (s * 10000000.0); /* 100ns units */
	return ts;
}

static void anim_set_duration(void *kf_anim, double seconds) {
	WUC_KeyFrameAnimation *kf = NULL;
	if (FAILED(cwinrt_query(kf_anim, &CWINRT_IID_WUC_IKeyFrameAnimation, ( void ** ) &kf))) return;
	( void ) wuc_key_frame_animation_put__duration(kf, seconds_to_timespan(seconds));
	(( IUnknown * ) kf)->lpVtbl->Release(( IUnknown * ) kf);
}

static HRESULT anim_start(void *target, wchar_t const *prop, void *kf_anim) {
	WUC_CompositionObject *obj = NULL;
	HRESULT                hr  = cwinrt_query(target, &CWINRT_IID_WUC_ICompositionObject, ( void ** ) &obj);
	if (FAILED(hr)) return hr;

	WUC_CompositionAnimation *anim = NULL;
	hr                             = cwinrt_query(kf_anim, &CWINRT_IID_WUC_ICompositionAnimation, ( void ** ) &anim);
	if (SUCCEEDED(hr)) {
		cwinrt_hstring hprop = NULL;
		hr                   = cwinrt_hstring_from(prop, &hprop);
		if (SUCCEEDED(hr)) {
			hr = wuc_composition_object_start_animation(obj, hprop, anim);
			cwinrt_hstring_free(hprop);
		}
		(( IUnknown * ) anim)->lpVtbl->Release(( IUnknown * ) anim);
	}
	(( IUnknown * ) obj)->lpVtbl->Release(( IUnknown * ) obj);
	return hr;
}

HRESULT
flux_anim_scalar(FluxAnimKit *kit, void *target, wchar_t const *prop, float to, double seconds, FluxEaseId ease) {
	if (!kit || !target || !prop) return E_INVALIDARG;

	WUC_ScalarKeyFrameAnimation *anim = NULL;
	HRESULT                      hr   = wuc_comp_create_scalar_key_frame_animation(kit->c, &anim);
	if (FAILED(hr)) return hr;

	WUC_CompositionEasingFunction *ef = kit_easing(kit, ease);
	hr                                = wuc_scalar_key_frame_animation_insert_key_frame_f32_f32_p(anim, 1.0f, to, ef);
	if (ef) (( IUnknown * ) ef)->lpVtbl->Release(( IUnknown * ) ef);
	if (SUCCEEDED(hr)) {
		anim_set_duration(anim, seconds);
		hr = anim_start(target, prop, anim);
	}

	(( IUnknown * ) anim)->lpVtbl->Release(( IUnknown * ) anim);
	return hr;
}

static WUI_Color to_wui_color(FluxColor c) {
	WUI_Color col;
	col.A = ( uint8_t ) (flux_color_af(c) * 255.0f + 0.5f);
	col.R = ( uint8_t ) (flux_color_rf(c) * 255.0f + 0.5f);
	col.G = ( uint8_t ) (flux_color_gf(c) * 255.0f + 0.5f);
	col.B = ( uint8_t ) (flux_color_bf(c) * 255.0f + 0.5f);
	return col;
}

HRESULT
flux_anim_color(FluxAnimKit *kit, void *target, wchar_t const *prop, FluxColor to, double seconds, FluxEaseId ease) {
	if (!kit || !target || !prop) return E_INVALIDARG;

	WUC_ColorKeyFrameAnimation *anim = NULL;
	HRESULT                     hr   = wuc_comp_create_color_key_frame_animation(kit->c, &anim);
	if (FAILED(hr)) return hr;

	WUC_CompositionEasingFunction *ef = kit_easing(kit, ease);
	hr = wuc_color_key_frame_animation_insert_key_frame_f32_color_p(anim, 1.0f, to_wui_color(to), ef);
	if (ef) (( IUnknown * ) ef)->lpVtbl->Release(( IUnknown * ) ef);
	if (SUCCEEDED(hr)) {
		anim_set_duration(anim, seconds);
		hr = anim_start(target, prop, anim);
	}

	(( IUnknown * ) anim)->lpVtbl->Release(( IUnknown * ) anim);
	return hr;
}
