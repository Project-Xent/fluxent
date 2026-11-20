#define UIA_ScrollPatternNoScroll flux_uia_noscroll_wnd
#include "a11y/flux_uia_object.h"
#include "controls/flux_module.h"

static FluxUiaHooks const *uia_window_hooks(IWindowProvider *p, FluxUia **self) {
	*self = FLUX_UIA_SELF(p, lpVtblWindow);
	return flux_module_uia((*self)->info.store, (*self)->node);
}

static HRESULT uia_window_false(BOOL *out) {
	if (!out) return E_POINTER;
	*out = FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_qi(IWindowProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblWindow), riid, ppv);
}

static ULONG STDMETHODCALLTYPE wnd_addref(IWindowProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblWindow));
}

static ULONG STDMETHODCALLTYPE wnd_release(IWindowProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblWindow));
}

static HRESULT STDMETHODCALLTYPE wnd_set_state(IWindowProvider *p, enum WindowVisualState state) {
	( void ) p;
	return state == WindowVisualState_Normal ? S_OK : UIA_E_INVALIDOPERATION;
}

static HRESULT STDMETHODCALLTYPE wnd_close(IWindowProvider *p) {
	FluxUia            *self = NULL;
	FluxUiaHooks const *h    = uia_window_hooks(p, &self);
	if (!h || !h->close) return UIA_E_INVALIDOPERATION;
	h->close(self->info.store, self->node);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_wait_idle(IWindowProvider *p, int ms, BOOL *out) {
	( void ) p;
	( void ) ms;
	if (!out) return E_POINTER;
	*out = TRUE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_can_maximize(IWindowProvider *p, BOOL *out) {
	( void ) p;
	return uia_window_false(out);
}

static HRESULT STDMETHODCALLTYPE wnd_can_minimize(IWindowProvider *p, BOOL *out) {
	( void ) p;
	return uia_window_false(out);
}

static HRESULT STDMETHODCALLTYPE wnd_is_modal(IWindowProvider *p, BOOL *out) {
	FluxUia            *self = NULL;
	FluxUiaHooks const *h    = uia_window_hooks(p, &self);
	if (!out) return E_POINTER;
	*out = h && h->modal;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_get_state(IWindowProvider *p, enum WindowVisualState *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = WindowVisualState_Normal;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_get_interaction(IWindowProvider *p, enum WindowInteractionState *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = WindowInteractionState_ReadyForUserInteraction;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE wnd_is_topmost(IWindowProvider *p, BOOL *out) {
	FluxUia            *self = NULL;
	FluxUiaHooks const *h    = uia_window_hooks(p, &self);
	if (!out) return E_POINTER;
	*out = h && h->modal;
	return S_OK;
}

IWindowProviderVtbl const flux_uia_window_vtbl = {
  wnd_qi,
  wnd_addref,
  wnd_release,
  wnd_set_state,
  wnd_close,
  wnd_wait_idle,
  wnd_can_maximize,
  wnd_can_minimize,
  wnd_is_modal,
  wnd_get_state,
  wnd_get_interaction,
  wnd_is_topmost,
};
