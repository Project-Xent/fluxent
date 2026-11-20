#define UIA_ScrollPatternNoScroll flux_uia_noscroll_expand
#include "a11y/flux_uia_object.h"
#include "controls/flux_module.h"

static FluxUiaHooks const *uia_expand_hooks(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h && h->expand && h->toggle_expand ? h : NULL;
}

static HRESULT uia_expand_to(IExpandCollapseProvider *p, int want) {
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblExpand);
	FluxUiaHooks const *h    = uia_expand_hooks(self);
	if (!h) return UIA_E_ELEMENTNOTAVAILABLE;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	int state = h->expand(self->info.store, self->node);
	if (state == ExpandCollapseState_LeafNode) return UIA_E_INVALIDOPERATION;
	if (state != want) h->toggle_expand(self->info.store, self->node);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE exp_qi(IExpandCollapseProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblExpand), riid, ppv);
}

static ULONG STDMETHODCALLTYPE exp_addref(IExpandCollapseProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblExpand));
}

static ULONG STDMETHODCALLTYPE exp_release(IExpandCollapseProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblExpand));
}

static HRESULT STDMETHODCALLTYPE exp_expand(IExpandCollapseProvider *p) {
	return uia_expand_to(p, ExpandCollapseState_Expanded);
}

static HRESULT STDMETHODCALLTYPE exp_collapse(IExpandCollapseProvider *p) {
	return uia_expand_to(p, ExpandCollapseState_Collapsed);
}

static HRESULT STDMETHODCALLTYPE exp_get_state(IExpandCollapseProvider *p, enum ExpandCollapseState *out) {
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblExpand);
	FluxUiaHooks const *h    = uia_expand_hooks(self);
	if (!out) return E_POINTER;
	*out = h ? ( enum ExpandCollapseState ) h->expand(self->info.store, self->node) : ExpandCollapseState_LeafNode;
	return S_OK;
}

IExpandCollapseProviderVtbl const flux_uia_expand_vtbl = {
  exp_qi,
  exp_addref,
  exp_release,
  exp_expand,
  exp_collapse,
  exp_get_state,
};
