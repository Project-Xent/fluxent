#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_effect.h"
#include "compose/flux_interop.h"

#include <cwinrt/cast.h>
#include <cwinrt/factory.h>
#include <cwinrt/hstring.h>
#include <cwinrt/Windows.Foundation.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <winstring.h>

static const GUID Flux_IID_IUnknown = {
  0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
static const GUID Flux_IID_IInspectable = {
  0xaf86e2e0, 0xb12d, 0x4c6a, {0x9c, 0x5a, 0xd7, 0xaa, 0x65, 0x10, 0x1e, 0x90}
};
/* D2D1 Gaussian Blur effect CLSID; property 0 is the standard deviation. */
static const GUID Flux_CLSID_D2D1GaussianBlur = {
  0x1feb6d69, 0x2fe6, 0x4ac9, {0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5}
};

/* Primary (IInspectable-family) vtable ABI: IInspectable + IGraphicsEffect's
 * get/put_Name. The same pointer serves IUnknown/IInspectable/IGraphicsEffect/
 * IGraphicsEffectSource (the last adds no methods over IInspectable). */
typedef struct FluxEffect FluxEffect;

typedef struct FluxEffectVtbl {
	HRESULT(STDMETHODCALLTYPE *QueryInterface)(FluxEffect *, REFIID, void **);
	ULONG(STDMETHODCALLTYPE *AddRef)(FluxEffect *);
	ULONG(STDMETHODCALLTYPE *Release)(FluxEffect *);
	HRESULT(STDMETHODCALLTYPE *GetIids)(FluxEffect *, ULONG *, IID **);
	HRESULT(STDMETHODCALLTYPE *GetRuntimeClassName)(FluxEffect *, HSTRING *);
	HRESULT(STDMETHODCALLTYPE *GetTrustLevel)(FluxEffect *, TrustLevel *);
	HRESULT(STDMETHODCALLTYPE *get_Name)(FluxEffect *, HSTRING *);
	HRESULT(STDMETHODCALLTYPE *put_Name)(FluxEffect *, HSTRING);
} FluxEffectVtbl;

struct FluxEffect {
	FluxEffectVtbl const                     *lpVtbl;
	FluxIGraphicsEffectD2D1InteropVtbl const *lpVtblInterop;
	LONG                                      ref;
	float                                     blur_amount;
	cwinrt_hstring                            name;
	WUC_CompositionEffectSourceParameter     *source; /**< Implements IGraphicsEffectSource. */
};

static FluxEffect *effect_from_interop(FluxIGraphicsEffectD2D1Interop *p) {
	return ( FluxEffect * ) (( char * ) p - offsetof(FluxEffect, lpVtblInterop));
}

static HRESULT STDMETHODCALLTYPE eff_qi(FluxEffect *self, REFIID riid, void **ppv) {
	if (!ppv) return E_POINTER;
	if (IsEqualGUID(riid, &Flux_IID_IUnknown)
		|| IsEqualGUID(riid, &Flux_IID_IInspectable)
		|| IsEqualGUID(riid, &CWINRT_IID_WGREF_IGraphicsEffect)
		|| IsEqualGUID(riid, &CWINRT_IID_WGREF_IGraphicsEffectSource))
	{
		*ppv = self;
		InterlockedIncrement(&self->ref);
		return S_OK;
	}
	if (IsEqualGUID(riid, &FluxIID_IGraphicsEffectD2D1Interop)) {
		*ppv = &self->lpVtblInterop;
		InterlockedIncrement(&self->ref);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE eff_addref(FluxEffect *self) { return ( ULONG ) InterlockedIncrement(&self->ref); }

static ULONG STDMETHODCALLTYPE eff_release(FluxEffect *self) {
	LONG r = InterlockedDecrement(&self->ref);
	if (r == 0) {
		if (self->name) cwinrt_hstring_free(self->name);
		if (self->source) (( IUnknown * ) self->source)->lpVtbl->Release(( IUnknown * ) self->source);
		free(self);
	}
	return ( ULONG ) r;
}

static HRESULT STDMETHODCALLTYPE eff_get_iids(FluxEffect *self, ULONG *count, IID **iids) {
	( void ) self;
	if (count) *count = 0;
	if (iids) *iids = NULL;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE eff_get_class_name(FluxEffect *self, HSTRING *out) {
	( void ) self;
	if (!out) return E_POINTER;
	return cwinrt_hstring_from(L"FluxBlurEffect", out);
}

static HRESULT STDMETHODCALLTYPE eff_get_trust(FluxEffect *self, TrustLevel *level) {
	( void ) self;
	if (!level) return E_POINTER;
	*level = BaseTrust;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE eff_get_name(FluxEffect *self, HSTRING *out) {
	if (!out) return E_POINTER;
	return WindowsDuplicateString(self->name, out);
}

static HRESULT STDMETHODCALLTYPE eff_put_name(FluxEffect *self, HSTRING value) {
	if (self->name) cwinrt_hstring_free(self->name);
	self->name = NULL;
	return WindowsDuplicateString(value, &self->name);
}

static FluxEffectVtbl const g_effect_vtbl = {
  eff_qi,
  eff_addref,
  eff_release,
  eff_get_iids,
  eff_get_class_name,
  eff_get_trust,
  eff_get_name,
  eff_put_name,
};

static HRESULT STDMETHODCALLTYPE iop_qi(FluxIGraphicsEffectD2D1Interop *p, REFIID riid, void **ppv) {
	return eff_qi(effect_from_interop(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE iop_addref(FluxIGraphicsEffectD2D1Interop *p) {
	return eff_addref(effect_from_interop(p));
}

static ULONG STDMETHODCALLTYPE iop_release(FluxIGraphicsEffectD2D1Interop *p) {
	return eff_release(effect_from_interop(p));
}

static HRESULT STDMETHODCALLTYPE iop_get_effect_id(FluxIGraphicsEffectD2D1Interop *p, GUID *id) {
	( void ) p;
	if (!id) return E_POINTER;
	*id = Flux_CLSID_D2D1GaussianBlur;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE iop_get_named_property_mapping(
  FluxIGraphicsEffectD2D1Interop *p, LPCWSTR name, UINT *index, FluxGraphicsEffectPropertyMapping *mapping
) {
	( void ) p;
	if (!name || !index || !mapping) return E_POINTER;
	if (wcscmp(name, L"BlurAmount") == 0) {
		*index   = 0;
		*mapping = FLUX_GFX_EFFECT_PROP_DIRECT;
		return S_OK;
	}
	return E_INVALIDARG;
}

static HRESULT STDMETHODCALLTYPE iop_get_property_count(FluxIGraphicsEffectD2D1Interop *p, UINT *count) {
	( void ) p;
	if (!count) return E_POINTER;
	*count = 1;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE iop_get_property(FluxIGraphicsEffectD2D1Interop *p, UINT index, void **value) {
	if (!value) return E_POINTER;
	*value = NULL;
	if (index != 0) return E_INVALIDARG;

	WF_IPropertyValueStatics *statics = NULL;
	HRESULT                   hr      = cwinrt_factory_get_statics(
	  L"Windows.Foundation.PropertyValue", &CWINRT_IID_WF_IPropertyValueStatics, ( void ** ) &statics
	);
	if (FAILED(hr)) return hr;

	IInspectable *boxed = NULL;
	hr                  = wf_property_value_statics_create_single(statics, effect_from_interop(p)->blur_amount, &boxed);
	(( IUnknown * ) statics)->lpVtbl->Release(( IUnknown * ) statics);
	if (FAILED(hr)) return hr;

	hr = cwinrt_query(boxed, &CWINRT_IID_WF_IPropertyValue, value);
	(( IUnknown * ) boxed)->lpVtbl->Release(( IUnknown * ) boxed);
	return hr;
}

static HRESULT STDMETHODCALLTYPE iop_get_source(FluxIGraphicsEffectD2D1Interop *p, UINT index, void **source) {
	if (!source) return E_POINTER;
	*source = NULL;
	if (index != 0) return E_INVALIDARG;
	FluxEffect *self = effect_from_interop(p);
	if (!self->source) return E_FAIL;
	return cwinrt_query(self->source, &CWINRT_IID_WGREF_IGraphicsEffectSource, source);
}

static HRESULT STDMETHODCALLTYPE iop_get_source_count(FluxIGraphicsEffectD2D1Interop *p, UINT *count) {
	( void ) p;
	if (!count) return E_POINTER;
	*count = 1;
	return S_OK;
}

static FluxIGraphicsEffectD2D1InteropVtbl const g_interop_vtbl = {
  iop_qi,
  iop_addref,
  iop_release,
  iop_get_effect_id,
  iop_get_named_property_mapping,
  iop_get_property_count,
  iop_get_property,
  iop_get_source,
  iop_get_source_count,
};

static WUC_CompositionEffectSourceParameter *make_source_parameter(wchar_t const *name) {
	WUC_ICompositionEffectSourceParameterFactory *factory = NULL;
	if (FAILED(cwinrt_factory_get_statics(
		  L"Windows.UI.Composition.CompositionEffectSourceParameter",
		  &CWINRT_IID_WUC_ICompositionEffectSourceParameterFactory, ( void ** ) &factory
		)))
		return NULL;

	cwinrt_hstring hname = NULL;
	if (FAILED(cwinrt_hstring_from(name, &hname))) {
		(( IUnknown * ) factory)->lpVtbl->Release(( IUnknown * ) factory);
		return NULL;
	}

	WUC_CompositionEffectSourceParameter *param = NULL;
	( void ) wuc_composition_effect_source_parameter_factory_create(factory, hname, &param);
	cwinrt_hstring_free(hname);
	(( IUnknown * ) factory)->lpVtbl->Release(( IUnknown * ) factory);
	return param;
}

static FluxEffect *effect_create(float blur, wchar_t const *source_name) {
	WUC_CompositionEffectSourceParameter *param = make_source_parameter(source_name);
	if (!param) return NULL;

	FluxEffect *e = ( FluxEffect * ) calloc(1, sizeof(*e));
	if (!e) {
		(( IUnknown * ) param)->lpVtbl->Release(( IUnknown * ) param);
		return NULL;
	}
	e->lpVtbl        = &g_effect_vtbl;
	e->lpVtblInterop = &g_interop_vtbl;
	e->ref           = 1;
	e->blur_amount   = blur;
	e->source        = param;
	return e;
}

/* Assemble a blur effect brush over a backdrop source. `host` selects whether
 * the backdrop samples the window's own content (in-app acrylic, for inline
 * controls) or the desktop behind the window (host acrylic, for popup windows). */
/* Intermediate COM objects of a backdrop-blur brush, owned by the caller for cleanup. */
typedef struct BlurParts {
	WUC_CompositionEffectFactory *factory;
	WUC_CompositionEffectBrush   *brush;
	WUC_BackdropBrush            *backdrop;
	cwinrt_hstring                hsrc;
	WUC_Brush                    *out; /**< The finished ICompositionBrush, or NULL. */
} BlurParts;

static HRESULT blur_build(WUC_Comp *comp, FluxEffect *eff, bool host, BlurParts *p) {
	if (FAILED(wuc_comp_create_effect_factory(comp, ( WGREF_IGraphicsEffect * ) eff, &p->factory))) return E_FAIL;
	if (FAILED(wuc_composition_effect_factory_create_brush(p->factory, &p->brush))) return E_FAIL;
	HRESULT hr = host ? wuc_comp_create_host_backdrop_brush(comp, &p->backdrop)
	                  : wuc_comp_create_backdrop_brush(comp, &p->backdrop);
	if (FAILED(hr)) return hr;
	if (FAILED(cwinrt_hstring_from(L"source", &p->hsrc))) return E_FAIL;

	( void ) wuc_composition_effect_brush_set_source_parameter(p->brush, p->hsrc, ( WUC_Brush * ) p->backdrop);
	( void ) cwinrt_query(p->brush, &CWINRT_IID_WUC_ICompositionBrush, ( void ** ) &p->out);
	return S_OK;
}

static WUC_Brush *make_blur_brush(FluxCompositor *c, float blur_amount, bool host) {
	WUC_Comp *comp = flux_compositor_comp(c);
	if (!comp) return NULL;

	FluxEffect *eff = effect_create(blur_amount, L"source");
	if (!eff) return NULL;

	BlurParts p = {0};
	blur_build(comp, eff, host, &p);

	if (p.hsrc) cwinrt_hstring_free(p.hsrc);
	if (p.backdrop) (( IUnknown * ) p.backdrop)->lpVtbl->Release(( IUnknown * ) p.backdrop);
	if (p.brush) (( IUnknown * ) p.brush)->lpVtbl->Release(( IUnknown * ) p.brush);
	if (p.factory) (( IUnknown * ) p.factory)->lpVtbl->Release(( IUnknown * ) p.factory);
	eff_release(eff);
	return p.out;
}

WUC_Brush *flux_effect_make_backdrop_blur(FluxCompositor *c, float blur_amount) {
	return make_blur_brush(c, blur_amount, false);
}

WUC_Brush *flux_effect_make_host_backdrop_blur(FluxCompositor *c, float blur_amount) {
	return make_blur_brush(c, blur_amount, true);
}
