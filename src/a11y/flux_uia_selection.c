#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "controls/flux_control_uia.h"
#include "controls/flux_module.h"

#include <stdlib.h>

typedef struct UiaItem {
	XentNodeId node;
	int        sub;
} UiaItem;

typedef struct UiaNodeList {
	UiaItem *items;
	uint32_t count;
	uint32_t cap;
} UiaNodeList;

static bool uia_item_selected(FluxUiaContext const *info, XentNodeId node, int sub) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	if (sub >= 0) return h && h->sub && h->sub->selected && h->sub->selected(info->store, node, sub);
	return h && h->selected ? h->selected(info->store, node) : xent_sem_selected(info->ctx, node);
}

static bool uia_shared(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	return h && h->select_shared;
}

static bool uia_owns_selection(FluxUiaContext const *info, XentNodeId node) {
	return (flux_uia_patterns(info, node) & FLUX_UIA_P_SELECTION) && !uia_shared(info, node);
}

static XentNodeId uia_container_of(FluxUiaContext const *info, XentNodeId node, int sub) {
	if (sub >= 0 && (flux_uia_patterns(info, node) & FLUX_UIA_P_SELECTION)) return node;
	XentNodeId p = node;
	while (p != XENT_NODE_INVALID && p != info->root) {
		p = flux_uia_parent(info, p);
		if (p != XENT_NODE_INVALID && uia_owns_selection(info, p)) return p;
	}
	return XENT_NODE_INVALID;
}

static IRawElementProviderSimple *uia_simple(FluxUiaContext const *info, XentNodeId node, int sub) {
	FluxUia *p = node == XENT_NODE_INVALID ? NULL : flux_uia_alloc_sub(info, node, sub);
	return p ? ( IRawElementProviderSimple * ) &p->lpVtblSimple : NULL;
}

static HRESULT uia_sub_select(FluxUia *self, FluxUiaHooks const *h, int op) {
	FluxUiaSub const *s = h ? h->sub : NULL;
	return s && s->select && s->select(self->info.store, self->node, self->sub, op) ? S_OK : UIA_E_INVALIDOPERATION;
}

static HRESULT uia_select_op(ISelectionItemProvider *p, int op) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblSelection);
	flux_uia_dirty_node(&self->info, self->node, FLUX_UIA_DIRTY_PROPS | FLUX_UIA_DIRTY_PATTERN | FLUX_UIA_DIRTY_TREE);
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	if (self->sub >= 0) return uia_sub_select(self, h, op);
	if (h && h->select) return h->select(self->info.store, self->node, op) ? S_OK : UIA_E_INVALIDOPERATION;
	bool selected = uia_item_selected(&self->info, self->node, -1);
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
	*out = uia_item_selected(&self->info, self->node, self->sub);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE si_container(ISelectionItemProvider *p, IRawElementProviderSimple **out) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblSelection);
	if (!out) return E_POINTER;
	XentNodeId owner = uia_container_of(&self->info, self->node, self->sub);
	*out             = uia_simple(&self->info, owner, -1);
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

static bool uia_list_push(UiaNodeList *list, XentNodeId node, int sub) {
	if (list->count == list->cap) {
		uint32_t cap  = list->cap ? list->cap * 2u : 16u;
		UiaItem *grow = ( UiaItem * ) realloc(list->items, sizeof(*grow) * cap);
		if (!grow) return false;
		list->items = grow;
		list->cap   = cap;
	}
	list->items [list->count++] = (UiaItem) {node, sub};
	return true;
}

static void uia_collect_subs(FluxUiaContext const *info, XentNodeId node, UiaNodeList *out) {
	FluxUiaHooks const *h     = flux_module_uia(info->store, node);
	int                 first = 0, last = -1;
	if (!h || !h->sub || !h->sub->span || !h->sub->span(info->store, node, &first, &last)) return;
	for (int i = first; i <= last; i++)
		if (uia_item_selected(info, node, i) && !uia_list_push(out, node, i)) return;
}

static void uia_collect(FluxUiaContext const *info, XentNodeId parent, UiaNodeList *out) {
	uia_collect_subs(info, parent, out);
	for (XentNodeId c = flux_uia_first(info, parent); c != XENT_NODE_INVALID; c = flux_uia_next(info, c)) {
		unsigned            patterns = flux_uia_patterns(info, c);
		FluxUiaHooks const *h        = flux_module_uia(info->store, c);
		bool                item     = (patterns & FLUX_UIA_P_SELECTION_ITEM) || (h && h->selected);
		if (item && uia_item_selected(info, c, -1) && !uia_list_push(out, c, -1)) return;
		if (!(patterns & FLUX_UIA_P_SELECTION) || (h && h->select_shared)) uia_collect(info, c, out);
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
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblSelectionC);
	FluxUiaHooks const *h    = flux_module_uia(self->info.store, self->node);
	UiaNodeList         list = {0};
	if (!out) return E_POINTER;
	if (!h || !h->unlisted) uia_collect(&self->info, self->node, &list);
	*out = SafeArrayCreateVector(VT_UNKNOWN, 0, list.count);
	for (LONG i = 0; *out && i < ( LONG ) list.count; i++) {
		IRawElementProviderSimple *item = uia_simple(&self->info, list.items [i].node, list.items [i].sub);
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
