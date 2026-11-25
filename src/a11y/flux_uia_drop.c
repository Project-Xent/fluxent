#include "a11y/flux_uia_object.h"
#include "controls/flux_module.h"

static FluxUia                  *drop_self(IDropTargetProvider *p) { return FLUX_UIA_SELF(p, lpVtblDrop); }

static HRESULT STDMETHODCALLTYPE drop_qi(IDropTargetProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(drop_self(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   drop_addref(IDropTargetProvider *p) { return flux_uia_addref(drop_self(p)); }

static ULONG STDMETHODCALLTYPE   drop_release(IDropTargetProvider *p) { return flux_uia_release(drop_self(p)); }

static HRESULT STDMETHODCALLTYPE drop_effect(IDropTargetProvider *p, BSTR *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = SysAllocString(L"");
	return *out ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE drop_effects(IDropTargetProvider *p, SAFEARRAY **out) {
	FluxUia            *self = drop_self(p);
	FluxUiaHooks const *h    = flux_module_uia(self->info.store, self->node);
	LONG                at   = 0;
	if (!out) return E_POINTER;
	*out = NULL;
	if (h && h->drop_flat) return E_NOTIMPL;
	BSTR none = SysAllocString(L"");
	*out      = none ? SafeArrayCreateVector(VT_BSTR, 0, 1) : NULL;
	if (*out) SafeArrayPutElement(*out, &at, none);
	SysFreeString(none);
	return *out ? S_OK : E_OUTOFMEMORY;
}

IDropTargetProviderVtbl const flux_uia_drop_vtbl = {
  drop_qi,
  drop_addref,
  drop_release,
  drop_effect,
  drop_effects,
};
