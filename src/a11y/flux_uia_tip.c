#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#include "a11y/flux_uia.h"
#include "a11y/flux_uia_ids.h"

#include <windows.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

#include <stdlib.h>

#define TIP_PROP_CONTROL_TYPE          30003
#define TIP_PROP_NAME                  30005
#define TIP_PROP_IS_KEYBOARD_FOCUSABLE 30009
#define TIP_PROP_CLASS_NAME            30012
#define TIP_EVENT_OPENED               20000
#define TIP_EVENT_CLOSED               20001

static const GUID Tip_IID_IUnknown = {
  0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
static const GUID Tip_IID_REPSimple = {
  0xd6dd68d1, 0x86fd, 0x4332, {0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c}
};

struct FluxUiaTip {
	IRawElementProviderSimpleVtbl const *lpVtbl;
	LONG                                 ref;
	HWND                                 hwnd;
	BSTR                                 name;
};

#define TIP_SELF(p) (( FluxUiaTip * ) (p))

static ULONG STDMETHODCALLTYPE tip_addref(IRawElementProviderSimple *p) {
	return ( ULONG ) InterlockedIncrement(&TIP_SELF(p)->ref);
}

static ULONG STDMETHODCALLTYPE tip_release(IRawElementProviderSimple *p) {
	FluxUiaTip *self = TIP_SELF(p);
	LONG        r    = InterlockedDecrement(&self->ref);
	if (r == 0) {
		SysFreeString(self->name);
		free(self);
	}
	return ( ULONG ) r;
}

static HRESULT STDMETHODCALLTYPE tip_qi(IRawElementProviderSimple *p, REFIID riid, void **ppv) {
	if (!ppv) return E_POINTER;
	*ppv = IsEqualGUID(riid, &Tip_IID_IUnknown) || IsEqualGUID(riid, &Tip_IID_REPSimple) ? p : NULL;
	if (!*ppv) return E_NOINTERFACE;
	tip_addref(p);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE tip_get_options(IRawElementProviderSimple *p, enum ProviderOptions *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = ProviderOptions_ServerSideProvider;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE tip_get_pattern(IRawElementProviderSimple *p, PATTERNID id, IUnknown **out) {
	( void ) p;
	( void ) id;
	if (!out) return E_POINTER;
	*out = NULL;
	return S_OK;
}

static void tip_set_string(VARIANT *out, wchar_t const *s) {
	out->bstrVal = SysAllocString(s);
	out->vt      = out->bstrVal ? VT_BSTR : VT_EMPTY;
}

static HRESULT STDMETHODCALLTYPE tip_get_property(IRawElementProviderSimple *p, PROPERTYID id, VARIANT *out) {
	FluxUiaTip *self = TIP_SELF(p);
	if (!out) return E_POINTER;
	VariantInit(out);
	switch (id) {
	case TIP_PROP_CONTROL_TYPE :
		out->vt   = VT_I4;
		out->lVal = FLUX_UIA_TOOLTIP;
		break;
	case TIP_PROP_NAME       : tip_set_string(out, self->name ? self->name : L""); break;
	case TIP_PROP_CLASS_NAME : tip_set_string(out, L"ToolTip"); break;
	case TIP_PROP_IS_KEYBOARD_FOCUSABLE :
		out->vt      = VT_BOOL;
		out->boolVal = VARIANT_FALSE;
		break;
	default : break;
	}
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE tip_get_host(IRawElementProviderSimple *p, IRawElementProviderSimple **out) {
	FluxUiaTip *self = TIP_SELF(p);
	if (!out) return E_POINTER;
	*out = NULL;
	return self->hwnd ? UiaHostProviderFromHwnd(self->hwnd, out) : S_OK;
}

static IRawElementProviderSimpleVtbl const tip_vtbl
  = {tip_qi, tip_addref, tip_release, tip_get_options, tip_get_pattern, tip_get_property, tip_get_host};

FluxUiaTip *flux_uia_tip_create(void) {
	FluxUiaTip *self = ( FluxUiaTip * ) calloc(1, sizeof(*self));
	if (!self) return NULL;
	self->lpVtbl = &tip_vtbl;
	self->ref    = 1;
	return self;
}

void flux_uia_tip_release(FluxUiaTip *tip) {
	if (!tip) return;
	UiaDisconnectProvider(( IRawElementProviderSimple * ) tip);
	tip_release(( IRawElementProviderSimple * ) tip);
}

void flux_uia_tip_update(FluxUiaTip *tip, HWND hwnd, char const *text) {
	if (!tip) return;
	tip->hwnd = hwnd;
	SysFreeString(tip->name);
	tip->name = NULL;
	int wlen  = text ? MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0) : 0;
	if (wlen <= 1) return;
	tip->name = SysAllocStringLen(NULL, ( UINT ) (wlen - 1));
	if (tip->name) MultiByteToWideChar(CP_UTF8, 0, text, -1, tip->name, wlen);
}

LRESULT flux_uia_tip_object(void *ctx, HWND hwnd, WPARAM wp, LPARAM lp) {
	FluxUiaTip *tip = ( FluxUiaTip * ) ctx;
	if (!tip || lp != ( LPARAM ) UiaRootObjectId) return 0;
	tip->hwnd = hwnd;
	return UiaReturnRawElementProvider(hwnd, wp, lp, ( IRawElementProviderSimple * ) tip);
}

void flux_uia_tip_raise(FluxUiaTip *tip, bool opened) {
	if (!tip || !tip->hwnd || !UiaClientsAreListening()) return;
	UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) tip, opened ? TIP_EVENT_OPENED : TIP_EVENT_CLOSED);
}
