#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"

#define UIA_ITEMS_NAME          30005
#define UIA_ITEMS_CONTROL_TYPE  30003
#define UIA_ITEMS_AUTOMATION_ID 30011
#define UIA_ITEMS_IS_SELECTED   30079
#define UIA_ITEMS_SELECTION     10010

static FluxUiaSub const *uia_items_sub(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h && h->sub && h->sub->count && h->sub->realized ? h->sub : NULL;
}

static IRawElementProviderSimple *uia_items_simple(FluxUia *p) {
	return p ? ( IRawElementProviderSimple * ) &p->lpVtblSimple : NULL;
}

static FluxUia *uia_items_at(FluxUia const *self, FluxUiaSub const *s, int index) {
	XentNodeId node = s->realized(self->info.store, self->node, index);
	if (node != XENT_NODE_INVALID) return flux_uia_alloc(&self->info, node);
	return flux_uia_alloc_sub(&self->info, self->node, index);
}

static int uia_items_start(FluxUia const *self, FluxUiaSub const *s, IRawElementProviderSimple *after) {
	FluxUia *a = flux_uia_unwrap(after);
	if (!a) return -1;
	if (a->node == self->node) return a->sub;
	return s->index ? s->index(self->info.store, self->node, a->node) : -1;
}

static bool uia_items_known(PROPERTYID id) {
	return id == 0
	    || id == UIA_ITEMS_NAME
	    || id == UIA_ITEMS_CONTROL_TYPE
	    || id == UIA_ITEMS_AUTOMATION_ID
	    || id == UIA_ITEMS_IS_SELECTED;
}

static void uia_items_selected(IRawElementProviderSimple *item, VARIANT *out) {
	IUnknown *unk = NULL;
	BOOL      on  = FALSE;
	if (FAILED(item->lpVtbl->GetPatternProvider(item, UIA_ITEMS_SELECTION, &unk)) || !unk) return;
	ISelectionItemProvider *sel = ( ISelectionItemProvider * ) unk;
	sel->lpVtbl->get_IsSelected(sel, &on);
	unk->lpVtbl->Release(unk);
	out->vt      = VT_BOOL;
	out->boolVal = on ? VARIANT_TRUE : VARIANT_FALSE;
}

static bool uia_items_match(IRawElementProviderSimple *item, PROPERTYID id, VARIANT *want) {
	VARIANT got;
	VariantInit(&got);
	if (id == 0) return true;
	if (id == UIA_ITEMS_IS_SELECTED) uia_items_selected(item, &got);
	else item->lpVtbl->GetPropertyValue(item, id, &got);
	bool same = got.vt != VT_EMPTY
	         && SUCCEEDED(VariantChangeType(&got, &got, 0, want->vt))
	         && VarCmp(&got, want, LOCALE_USER_DEFAULT, 0) == VARCMP_EQ;
	VariantClear(&got);
	return same;
}

static HRESULT STDMETHODCALLTYPE items_qi(IItemContainerProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblItems), riid, ppv);
}

static ULONG STDMETHODCALLTYPE items_addref(IItemContainerProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblItems));
}

static ULONG STDMETHODCALLTYPE items_release(IItemContainerProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblItems));
}

static HRESULT STDMETHODCALLTYPE items_find(
  IItemContainerProvider *p, IRawElementProviderSimple *after, PROPERTYID id, VARIANT value,
  IRawElementProviderSimple **out
) {
	FluxUia          *self = FLUX_UIA_SELF(p, lpVtblItems);
	FluxUiaSub const *s    = uia_items_sub(self);
	if (!out) return E_POINTER;
	*out = NULL;
	if (!s) return S_OK;
	if (!uia_items_known(id)) return UIA_E_INVALIDOPERATION;
	int count = s->count(self->info.store, self->node);
	for (int i = uia_items_start(self, s, after) + 1; i < count; i++) {
		IRawElementProviderSimple *item = uia_items_simple(uia_items_at(self, s, i));
		if (item && uia_items_match(item, id, &value)) {
			*out = item;
			return S_OK;
		}
		if (item) item->lpVtbl->Release(item);
	}
	return S_OK;
}

IItemContainerProviderVtbl const flux_uia_items_vtbl = {
  items_qi,
  items_addref,
  items_release,
  items_find,
};

static HRESULT STDMETHODCALLTYPE virt_qi(IVirtualizedItemProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblVirtual), riid, ppv);
}

static ULONG STDMETHODCALLTYPE virt_addref(IVirtualizedItemProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblVirtual));
}

static ULONG STDMETHODCALLTYPE virt_release(IVirtualizedItemProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblVirtual));
}

static HRESULT STDMETHODCALLTYPE virt_realize(IVirtualizedItemProvider *p) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblVirtual);
	flux_uia_dirty(&self->info);
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (self->sub >= 0 && h && h->sub && h->sub->realize) h->sub->realize(self->info.store, self->node, self->sub);
	return S_OK;
}

IVirtualizedItemProviderVtbl const flux_uia_virtual_vtbl = {
  virt_qi,
  virt_addref,
  virt_release,
  virt_realize,
};
