#define UIA_ScrollPatternNoScroll flux_uia_noscroll_selection
#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "controls/flux_control_uia.h"
#include "controls/flux_module.h"

#include <stdlib.h>

typedef struct UiaNodeList {
	XentNodeId *items;
	uint32_t    count;
	uint32_t    cap;
} UiaNodeList;

static bool uia_item_selected(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	return h && h->selected ? h->selected(info->store, node) : xent_sem_selected(info->ctx, node);
}

static XentNodeId uia_container_of(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId p = node;
	while (p != XENT_NODE_INVALID && p != info->root) {
		p = flux_uia_parent(info, p);
		if (p != XENT_NODE_INVALID && (flux_uia_patterns(info, p) & FLUX_UIA_P_SELECTION)) return p;
	}
	return XENT_NODE_INVALID;
}

static IRawElementProviderSimple *uia_simple(FluxUiaContext const *info, XentNodeId node) {
	FluxUia *p = node == XENT_NODE_INVALID ? NULL : flux_uia_alloc(info, node);
	return p ? ( IRawElementProviderSimple * ) &p->lpVtblSimple : NULL;
}

static HRESULT uia_select_op(ISelectionItemProvider *p, int op) {
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblSelection);
	FluxUiaHooks const *h    = flux_module_uia(self->info.store, self->node);
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	if (h && h->select) return h->select(self->info.store, self->node, op) ? S_OK : UIA_E_INVALIDOPERATION;
	bool selected = uia_item_selected(&self->info, self->node);
	if (op == FLUX_UIA_SEL_REMOVE) return selected ? UIA_E_INVALIDOPERATION : S_OK;
	return selected && op == FLUX_UIA_SEL_ADD ? S_OK : flux_uia_activate(self);
}

static HRESULT STDMETHODCALLTYPE si_qi(ISelectionItemProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblSelection), riid, ppv);
}

static ULONG STDMETHODCALLTYPE si_addref(ISelectionItemProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblSelection));
}

static ULONG STDMETHODCALLTYPE si_release(ISelectionItemProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblSelection));
}

static HRESULT STDMETHODCALLTYPE si_select(ISelectionItemProvider *p) { return uia_select_op(p, FLUX_UIA_SEL_SELECT); }

static HRESULT STDMETHODCALLTYPE si_add(ISelectionItemProvider *p) { return uia_select_op(p, FLUX_UIA_SEL_ADD); }

static HRESULT STDMETHODCALLTYPE si_remove(ISelectionItemProvider *p) { return uia_select_op(p, FLUX_UIA_SEL_REMOVE); }

static HRESULT STDMETHODCALLTYPE si_is_selected(ISelectionItemProvider *p, BOOL *out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblSelection);
	if (!out) return E_POINTER;
	*out = uia_item_selected(&self->info, self->node);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE si_container(ISelectionItemProvider *p, IRawElementProviderSimple **out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblSelection);
	if (!out) return E_POINTER;
	*out = uia_simple(&self->info, uia_container_of(&self->info, self->node));
	return S_OK;
}

ISelectionItemProviderVtbl const flux_uia_selitem_vtbl = {
  si_qi,
  si_addref,
  si_release,
  si_select,
  si_add,
  si_remove,
  si_is_selected,
  si_container,
};

static bool uia_list_push(UiaNodeList *list, XentNodeId node) {
	if (list->count == list->cap) {
		uint32_t    cap  = list->cap ? list->cap * 2u : 16u;
		XentNodeId *grow = ( XentNodeId * ) realloc(list->items, sizeof(*grow) * cap);
		if (!grow) return false;
		list->items = grow;
		list->cap   = cap;
	}
	list->items [list->count++] = node;
	return true;
}

static void uia_collect(FluxUiaContext const *info, XentNodeId parent, UiaNodeList *out) {
	for (XentNodeId c = flux_uia_first(info, parent); c != XENT_NODE_INVALID; c = flux_uia_next(info, c)) {
		unsigned patterns = flux_uia_patterns(info, c);
		if ((patterns & FLUX_UIA_P_SELECTION_ITEM) && uia_item_selected(info, c) && !uia_list_push(out, c)) return;
		if (!(patterns & FLUX_UIA_P_SELECTION)) uia_collect(info, c, out);
	}
}

static HRESULT STDMETHODCALLTYPE sc_qi(ISelectionProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblSelectionC), riid, ppv);
}

static ULONG STDMETHODCALLTYPE sc_addref(ISelectionProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblSelectionC));
}

static ULONG STDMETHODCALLTYPE sc_release(ISelectionProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblSelectionC));
}

static HRESULT STDMETHODCALLTYPE sc_get_selection(ISelectionProvider *p, SAFEARRAY **out) {
	FluxUia    *self = FLUX_UIA_SELF(p, lpVtblSelectionC);
	UiaNodeList list = {0};
	if (!out) return E_POINTER;
	uia_collect(&self->info, self->node, &list);
	*out = SafeArrayCreateVector(VT_UNKNOWN, 0, list.count);
	for (LONG i = 0; *out && i < ( LONG ) list.count; i++) {
		IRawElementProviderSimple *item = uia_simple(&self->info, list.items [i]);
		if (!item) continue;
		SafeArrayPutElement(*out, &i, ( IUnknown * ) item);
		item->lpVtbl->Release(item);
	}
	free(list.items);
	return *out ? S_OK : E_OUTOFMEMORY;
}

static unsigned uia_select_mode(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h && h->select_mode ? h->select_mode(self->info.store, self->node) : 0u;
}

static HRESULT STDMETHODCALLTYPE sc_can_multiple(ISelectionProvider *p, BOOL *out) {
	if (!out) return E_POINTER;
	*out = (uia_select_mode(FLUX_UIA_SELF(p, lpVtblSelectionC)) & FLUX_UIA_SEL_MULTIPLE) != 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE sc_required(ISelectionProvider *p, BOOL *out) {
	if (!out) return E_POINTER;
	*out = (uia_select_mode(FLUX_UIA_SELF(p, lpVtblSelectionC)) & FLUX_UIA_SEL_REQUIRED) != 0;
	return S_OK;
}

ISelectionProviderVtbl const flux_uia_selection_vtbl = {
  sc_qi,
  sc_addref,
  sc_release,
  sc_get_selection,
  sc_can_multiple,
  sc_required,
};
