#include "fluxent/flux_window.h"
#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "store/flux_node_data.h"
#include "controls/flux_module.h"
#include "bridge/flux_internal.h"

#include "controls/data/flux_slider_data.h"
#include "controls/data/flux_textbox_data.h"
#include "render/flux_slider_geom.h"
#include "fluxent/flux_control_type.h"
#include <fluxent/flux_input.h>
#include <fluxent/flux_popup.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>

#define FLUX_UIA_EVENT_FOCUS_CHANGED 20005


#define FLUX_UIA_APPEND_RUNTIME_ID   3

static const GUID Flux_IID_IUnknown = {
  0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}
};
static const GUID Flux_IID_REPSimple = {
  0xd6dd68d1, 0x86fd, 0x4332, {0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c}
};
static const GUID Flux_IID_REPFragment = {
  0xf7063da8, 0x8359, 0x439c, {0x92, 0x97, 0xbb, 0xc5, 0x29, 0x9a, 0x7d, 0x87}
};
static const GUID Flux_IID_REPFragmentRoot = {
  0x620ce2a5, 0xab8f, 0x40a9, {0x86, 0xcb, 0xde, 0x3c, 0x75, 0x59, 0x9b, 0x58}
};
static const GUID Flux_IID_Invoke = {
  0x54fcb24b, 0xe18e, 0x47a2, {0xb4, 0xd3, 0xec, 0xcb, 0xe7, 0x75, 0x99, 0xa2}
};
static const GUID Flux_IID_Toggle = {
  0x56d00bd0, 0xc4f4, 0x433c, {0xa8, 0x36, 0x1a, 0x52, 0xa5, 0x7e, 0x08, 0x92}
};
static const GUID Flux_IID_RangeValue = {
  0x36dc7aef, 0x33e6, 0x4691, {0xaf, 0xe1, 0x2b, 0xe7, 0x27, 0x4b, 0x3d, 0x33}
};
static const GUID Flux_IID_Value = {
  0xc7935180, 0x6fb3, 0x4201, {0xb1, 0x74, 0x7d, 0xf7, 0x3a, 0xdb, 0xf6, 0x4a}
};
static const GUID Flux_IID_Scroll = {
  0xb38b8077, 0x1fc3, 0x42a5, {0x8c, 0xae, 0xd4, 0x0c, 0x22, 0x15, 0x05, 0x5a}
};
static const GUID Flux_IID_ScrollItem = {
  0x2360c714, 0x4bf1, 0x4b26, {0xba, 0x65, 0x9b, 0x21, 0x31, 0x61, 0x27, 0xeb}
};
static const GUID Flux_IID_ExpandCollapse = {
  0xd847d3a5, 0xcab0, 0x4a98, {0x8c, 0x32, 0xec, 0xb4, 0x5c, 0x59, 0xad, 0x24}
};
static const GUID Flux_IID_Selection = {
  0xfb8b03af, 0x3bdf, 0x48d4, {0xbd, 0x36, 0x1a, 0x65, 0x79, 0x3b, 0xe1, 0x68}
};
static const GUID Flux_IID_Window = {
  0x987df77b, 0xdb06, 0x4d77, {0x8f, 0x8a, 0x86, 0xa9, 0xc3, 0xbb, 0x90, 0xb9}
};
static const GUID Flux_IID_SelectionItem = {
  0x2acad808, 0xb2d4, 0x452d, {0xa4, 0x07, 0x91, 0xff, 0x1a, 0xd1, 0x67, 0xb2}
};
static const GUID Flux_IID_Grid = {
  0xb17d6187, 0x0907, 0x464b, {0xa1, 0x68, 0x0e, 0xf1, 0x7a, 0x15, 0x72, 0xb1}
};
static const GUID Flux_IID_GridItem = {
  0xd02541f1, 0xfb81, 0x4d64, {0xae, 0x32, 0xf5, 0x20, 0xf8, 0xa6, 0xdb, 0xd1}
};
static const GUID Flux_IID_Table = {
  0x9c860395, 0x97b3, 0x490a, {0xb5, 0x2a, 0x85, 0x8c, 0xc2, 0x2a, 0xf1, 0x66}
};
static const GUID Flux_IID_TableItem = {
  0xb9734fa6, 0x771f, 0x4d78, {0x9c, 0x90, 0x25, 0x17, 0x99, 0x93, 0x49, 0xcd}
};
static const GUID Flux_IID_ItemContainer = {
  0xe747770b, 0x39ce, 0x4382, {0xab, 0x30, 0xd8, 0xfb, 0x3f, 0x33, 0x6f, 0x24}
};
static const GUID Flux_IID_VirtualizedItem = {
  0xcb98b665, 0x2d35, 0x4fac, {0xad, 0x35, 0xf3, 0xc6, 0x0d, 0x0c, 0x0b, 0x8b}
};
static const GUID Flux_IID_Text = {
  0x3589c92c, 0x63f3, 0x4367, {0x99, 0xbb, 0xad, 0xa6, 0x53, 0xb7, 0x7c, 0xf2}
};
static const GUID Flux_IID_DropTarget = {
  0xbae82bfd, 0x358a, 0x481c, {0x85, 0xa0, 0xd8, 0xb4, 0xd9, 0x0a, 0x5d, 0x61}
};
static const GUID Flux_IID_Text2 = {
  0x0dc5e6ed, 0x3e16, 0x4bf1, {0x8f, 0x9a, 0xa9, 0x79, 0x87, 0x8b, 0xc1, 0x95}
};


#define SELF_SIMPLE(p) FLUX_UIA_SELF(p, lpVtblSimple)
#define SELF_FRAG(p)   FLUX_UIA_SELF(p, lpVtblFragment)
#define SELF_ROOT(p)   FLUX_UIA_SELF(p, lpVtblRoot)
#define SELF_INVOKE(p) FLUX_UIA_SELF(p, lpVtblInvoke)
#define SELF_TOGGLE(p) FLUX_UIA_SELF(p, lpVtblToggle)
#define SELF_RANGE(p)  FLUX_UIA_SELF(p, lpVtblRange)
#define SELF_VALUE(p)  FLUX_UIA_SELF(p, lpVtblValue)

typedef struct FluxUiaControlInfo {
	int      control_type;
	unsigned patterns;
} FluxUiaControlInfo;

typedef struct UiaPatternSlot {
	unsigned    bit;
	PATTERNID   id;
	GUID const *iid;
	size_t      offset;
} UiaPatternSlot;

static UiaPatternSlot const kUiaPatterns [] = {
  {FLUX_UIA_P_INVOKE,         10000, &Flux_IID_Invoke,          offsetof(FluxUia, lpVtblInvoke)    },
  {FLUX_UIA_P_TOGGLE,         10015, &Flux_IID_Toggle,          offsetof(FluxUia, lpVtblToggle)    },
  {FLUX_UIA_P_RANGE,          10003, &Flux_IID_RangeValue,      offsetof(FluxUia, lpVtblRange)     },
  {FLUX_UIA_P_VALUE,          10002, &Flux_IID_Value,           offsetof(FluxUia, lpVtblValue)     },
  {FLUX_UIA_P_SELECTION_ITEM, 10010, &Flux_IID_SelectionItem,   offsetof(FluxUia, lpVtblSelection) },
  {FLUX_UIA_P_SCROLL,         10004, &Flux_IID_Scroll,          offsetof(FluxUia, lpVtblScroll)    },
  {FLUX_UIA_P_SCROLL_ITEM,    10017, &Flux_IID_ScrollItem,      offsetof(FluxUia, lpVtblScrollItem)},
  {FLUX_UIA_P_EXPAND,         10005, &Flux_IID_ExpandCollapse,  offsetof(FluxUia, lpVtblExpand)    },
  {FLUX_UIA_P_SELECTION,      10001, &Flux_IID_Selection,       offsetof(FluxUia, lpVtblSelectionC)},
  {FLUX_UIA_P_WINDOW,         10009, &Flux_IID_Window,          offsetof(FluxUia, lpVtblWindow)    },
  {FLUX_UIA_P_GRID,           10006, &Flux_IID_Grid,            offsetof(FluxUia, lpVtblGrid)      },
  {FLUX_UIA_P_GRID_ITEM,      10007, &Flux_IID_GridItem,        offsetof(FluxUia, lpVtblGridItem)  },
  {FLUX_UIA_P_TABLE,          10012, &Flux_IID_Table,           offsetof(FluxUia, lpVtblTable)     },
  {FLUX_UIA_P_TABLE_ITEM,     10013, &Flux_IID_TableItem,       offsetof(FluxUia, lpVtblTableItem) },
  {FLUX_UIA_P_ITEMS,          10019, &Flux_IID_ItemContainer,   offsetof(FluxUia, lpVtblItems)     },
  {FLUX_UIA_P_VIRTUAL,        10020, &Flux_IID_VirtualizedItem, offsetof(FluxUia, lpVtblVirtual)   },
  {FLUX_UIA_P_TEXT,           10014, &Flux_IID_Text,            offsetof(FluxUia, lpVtblText)      },
  {FLUX_UIA_P_TEXT2,          10024, &Flux_IID_Text2,           offsetof(FluxUia, lpVtblText)      },
  {FLUX_UIA_P_DROP,           10031, &Flux_IID_DropTarget,      offsetof(FluxUia, lpVtblDrop)      },
};

static FluxNodeData *uia_node_data(FluxUia const *self) {
	return self->info.store ? flux_store_find(self->info.store, self->node) : NULL;
}

static FluxSliderData const *uia_range_slider(FluxUia const *self) {
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || nd->component_type != FLUX_CONTROL_SLIDER || !nd->component_data) return NULL;
	FluxSliderData const *sd = ( FluxSliderData const * ) nd->component_data;
	return sd->is_range ? sd : NULL;
}

static FluxUiaSub const *uia_sub(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h ? h->sub : NULL;
}

static bool uia_sub_span(FluxUia const *self, int *first, int *last) {
	FluxUiaSub const *s = uia_sub(self);
	return s && s->span && s->span(self->info.store, self->node, first, last) && *first <= *last;
}

int flux_uia_sub_heads(FluxUia const *self) {
	FluxUiaSub const *s = uia_sub(self);
	return s && s->heads ? s->heads(self->info.store, self->node) : 0;
}

bool flux_uia_sub_range(FluxUiaContext const *info, XentNodeId node, int *first, int *last) {
	FluxUia probe = {.info = *info, .node = node, .sub = -1};
	return uia_sub_span(&probe, first, last);
}

static FluxUiaControlInfo uia_sub_info(FluxUia const *self) {
	FluxUiaSub const  *s    = uia_sub(self);
	FluxUiaControlInfo info = {FLUX_UIA_CUSTOM, 0u};
	if (s && s->kind) info.control_type = s->kind(self->info.store, self->node, self->sub, &info.patterns);
	return info;
}

static FluxUiaControlInfo uia_control_info(FluxUia const *self) {
	if (self->sub >= 0) return uia_sub_info(self);
	if (self->node == self->info.root) return (FluxUiaControlInfo) {FLUX_UIA_WINDOW, 0};
	if (uia_range_slider(self)) return (FluxUiaControlInfo) {FLUX_UIA_GROUP, 0};
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (!h || h->role == 0) return (FluxUiaControlInfo) {FLUX_UIA_CUSTOM, 0};
	int role = h->role == FLUX_UIA_UNMAPPED ? 0 : h->role;
	return (FluxUiaControlInfo) {role, flux_uia_patterns(&self->info, self->node)};
}

unsigned flux_uia_patterns(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	if (!h) return 0u;
	return h->live ? h->patterns & h->live(info->store, node) : h->patterns;
}

static void *uia_pattern_iface(FluxUia *self, REFIID riid) {
	unsigned patterns = uia_control_info(self).patterns;
	for (size_t i = 0; i < sizeof(kUiaPatterns) / sizeof(kUiaPatterns [0]); i++)
		if ((patterns & kUiaPatterns [i].bit) && IsEqualGUID(riid, kUiaPatterns [i].iid))
			return ( char * ) self + kUiaPatterns [i].offset;
	return NULL;
}

HRESULT flux_uia_qi(FluxUia *self, REFIID riid, void **ppv) {
	if (!ppv) return E_POINTER;

	*ppv = NULL;
	if (IsEqualGUID(riid, &Flux_IID_IUnknown) || IsEqualGUID(riid, &Flux_IID_REPSimple)) *ppv = &self->lpVtblSimple;
	if (!*ppv && IsEqualGUID(riid, &Flux_IID_REPFragment)) *ppv = &self->lpVtblFragment;
	if (!*ppv && IsEqualGUID(riid, &Flux_IID_REPFragmentRoot) && self->node == self->info.root)
		*ppv = &self->lpVtblRoot;
	if (!*ppv) *ppv = uia_pattern_iface(self, riid);

	if (!*ppv) return E_NOINTERFACE;
	InterlockedIncrement(&self->ref);
	return S_OK;
}

ULONG flux_uia_addref(FluxUia *self) { return ( ULONG ) InterlockedIncrement(&self->ref); }

ULONG flux_uia_release(FluxUia *self) {
	LONG r = InterlockedDecrement(&self->ref);
	if (r == 0) free(self);
	return ( ULONG ) r;
}

static HRESULT STDMETHODCALLTYPE s_qi(IRawElementProviderSimple *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_SIMPLE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   s_addref(IRawElementProviderSimple *p) { return flux_uia_addref(SELF_SIMPLE(p)); }

static ULONG STDMETHODCALLTYPE   s_release(IRawElementProviderSimple *p) { return flux_uia_release(SELF_SIMPLE(p)); }

static HRESULT STDMETHODCALLTYPE s_get_options(IRawElementProviderSimple *p, enum ProviderOptions *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE s_get_pattern(IRawElementProviderSimple *p, PATTERNID id, IUnknown **out) {
	FluxUia *self = SELF_SIMPLE(p);
	if (!out) return E_POINTER;

	*out              = NULL;
	unsigned patterns = uia_control_info(self).patterns;
	for (size_t i = 0; i < sizeof(kUiaPatterns) / sizeof(kUiaPatterns [0]); i++)
		if (kUiaPatterns [i].id == id && (patterns & kUiaPatterns [i].bit))
			*out = ( IUnknown * ) (( char * ) self + kUiaPatterns [i].offset);

	if (*out) InterlockedIncrement(&self->ref);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE s_get_property(IRawElementProviderSimple *p, PROPERTYID id, VARIANT *out) {
	FluxUia *self = SELF_SIMPLE(p);
	if (!out) return E_POINTER;
	VariantInit(out);
	flux_uia_property(self, uia_control_info(self).control_type, id, out);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE s_get_host(IRawElementProviderSimple *p, IRawElementProviderSimple **out) {
	FluxUia *self = SELF_SIMPLE(p);
	if (!out) return E_POINTER;
	*out = NULL;
	if (self->node == self->info.root) return UiaHostProviderFromHwnd(self->info.hwnd, out);
	return S_OK;
}

static IRawElementProviderSimpleVtbl const g_simple_vtbl = {
  s_qi,
  s_addref,
  s_release,
  s_get_options,
  s_get_pattern,
  s_get_property,
  s_get_host,
};

FluxUia *flux_uia_unwrap(IRawElementProviderSimple *p) {
	return p && p->lpVtbl == &g_simple_vtbl ? SELF_SIMPLE(p) : NULL;
}

static HRESULT STDMETHODCALLTYPE f_qi(IRawElementProviderFragment *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_FRAG(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE      f_addref(IRawElementProviderFragment *p) { return flux_uia_addref(SELF_FRAG(p)); }

static ULONG STDMETHODCALLTYPE      f_release(IRawElementProviderFragment *p) { return flux_uia_release(SELF_FRAG(p)); }

static IRawElementProviderFragment *uia_wrap(FluxUia *self, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return NULL;
	FluxUia *p = flux_uia_alloc(&self->info, node);
	return p ? ( IRawElementProviderFragment * ) &p->lpVtblFragment : NULL;
}

FluxUia *flux_uia_alloc_sub(FluxUiaContext const *info, XentNodeId node, int sub) {
	FluxUia *p = flux_uia_alloc(info, node);
	if (p) p->sub = sub;
	return p;
}

static IRawElementProviderFragment *uia_wrap_sub(FluxUia *self, int sub) {
	FluxUia *p = flux_uia_alloc_sub(&self->info, self->node, sub);
	return p ? ( IRawElementProviderFragment * ) &p->lpVtblFragment : NULL;
}

static int uia_sub_step(FluxUia *self, int step) {
	int  first = 0, last = -1, heads = flux_uia_sub_heads(self);
	bool span = uia_sub_span(self, &first, &last);
	int  at   = self->sub + step;
	if (self->sub >= FLUX_UIA_SUB_LONE) return -1;
	if (self->sub >= FLUX_UIA_SUB_HEAD) {
		if (at >= FLUX_UIA_SUB_HEAD && at < FLUX_UIA_SUB_HEAD + heads) return at;
		return step > 0 && span ? first : -1;
	}
	if (span && at >= first && at <= last) return at;
	return step < 0 && span && self->sub == first && heads > 0 ? FLUX_UIA_SUB_HEAD + heads - 1 : -1;
}

static IRawElementProviderFragment *uia_navigate_sub(FluxUia *self, enum NavigateDirection dir) {
	if (dir == NavigateDirection_Parent) return uia_wrap(self, self->node);
	if (dir != NavigateDirection_NextSibling && dir != NavigateDirection_PreviousSibling) return NULL;
	int at = uia_sub_step(self, dir == NavigateDirection_NextSibling ? 1 : -1);
	return at >= 0 ? uia_wrap_sub(self, at) : NULL;
}

static XentNodeId uia_navigate_target(FluxUia *self, enum NavigateDirection dir) {
	FluxUiaContext const *info = &self->info;
	bool                  root = self->node == info->root;
	switch (dir) {
	case NavigateDirection_Parent          : return root ? XENT_NODE_INVALID : flux_uia_parent(info, self->node);
	case NavigateDirection_NextSibling     : return root ? XENT_NODE_INVALID : flux_uia_next(info, self->node);
	case NavigateDirection_PreviousSibling : return root ? XENT_NODE_INVALID : flux_uia_prev(info, self->node);
	case NavigateDirection_FirstChild      : return flux_uia_first(info, self->node);
	case NavigateDirection_LastChild       : return flux_uia_last(info, self->node);
	}
	return XENT_NODE_INVALID;
}

static IRawElementProviderFragment *uia_navigate(FluxUia *self, enum NavigateDirection dir) {
	if (self->sub >= 0) return uia_navigate_sub(self, dir);
	int  first = 0, last = -1, heads = flux_uia_sub_heads(self);
	bool span = uia_sub_span(self, &first, &last);
	if (heads > 0 && dir == NavigateDirection_FirstChild) return uia_wrap_sub(self, FLUX_UIA_SUB_HEAD);
	if (span && dir == NavigateDirection_FirstChild) return uia_wrap_sub(self, first);
	if (span && dir == NavigateDirection_LastChild) return uia_wrap_sub(self, last);
	if (heads > 0 && dir == NavigateDirection_LastChild) return uia_wrap_sub(self, FLUX_UIA_SUB_HEAD + heads - 1);
	return uia_wrap(self, uia_navigate_target(self, dir));
}

static HRESULT STDMETHODCALLTYPE f_navigate(
  IRawElementProviderFragment *p, enum NavigateDirection dir, IRawElementProviderFragment **out
) {
	if (!out) return E_POINTER;
	*out = uia_navigate(SELF_FRAG(p), dir);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_runtime_id(IRawElementProviderFragment *p, SAFEARRAY **out) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	*out          = NULL;
	SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, self->sub >= 0 ? 3 : 2);
	if (!sa) return E_OUTOFMEMORY;
	LONG idx0 = 0, idx1 = 1, idx2 = 2;
	int  append = FLUX_UIA_APPEND_RUNTIME_ID, id = ( int ) self->node, sub = self->sub + 1;
	SafeArrayPutElement(sa, &idx0, &append);
	SafeArrayPutElement(sa, &idx1, &id);
	if (self->sub >= 0) SafeArrayPutElement(sa, &idx2, &sub);
	*out = sa;
	return S_OK;
}

static bool uia_popup_origin(FluxUiaContext const *info, XentNodeId node, POINT *origin) {
	for (XentNodeId n = node; n != XENT_NODE_INVALID; n = xent_node_parent(info->ctx, n)) {
		FluxNodeData const *nd = flux_store_find(info->store, n);
		if (!nd || !nd->popup) continue;
		FluxPoint at = flux_popup_contentorigin(nd->popup);
		*origin      = (POINT) {( LONG ) at.x, ( LONG ) at.y};
		return true;
	}
	return false;
}

void flux_uia_origin(FluxUiaContext const *info, XentNodeId node, POINT *origin, float *scale) {
	*scale  = ( float ) GetDpiForWindow(info->hwnd) / 96.0f;
	*origin = (POINT) {0, 0};
	if (!uia_popup_origin(info, node, origin)) ClientToScreen(info->hwnd, origin);
}

static HRESULT STDMETHODCALLTYPE f_get_bounds(IRawElementProviderFragment *p, struct UiaRect *out) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	flux_uia_bounds(&self->info, self->node, self->sub, out);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_embedded(IRawElementProviderFragment *p, SAFEARRAY **out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = NULL;
	return S_OK;
}

static void uia_sub_focus(FluxUia *self, FluxInput *input) {
	FluxUiaSub const *s         = uia_sub(self);
	XentNodeId        container = s && s->realized ? s->realized(self->info.store, self->node, self->sub) : 0;
	if (container && container != XENT_NODE_INVALID) {
		if (xent_get_focusable(self->info.ctx, container)) flux_input_setfocus(input, container, true);
		return;
	}
	if (!s || !s->focus || !flux_uia_focusable(self)) return;
	s->focus(self->info.store, self->node, self->sub);
	flux_input_setfocus(input, self->node, true);
}

static HRESULT STDMETHODCALLTYPE f_set_focus(IRawElementProviderFragment *p) {
	FluxUia *self = SELF_FRAG(p);
	flux_uia_dirty(&self->info);
	FluxInput *input = flux_store_input(self->info.store);
	if (!input || self->node == self->info.root) return S_OK;
	if (self->sub >= 0) uia_sub_focus(self, input);
	else if (flux_uia_focusable(self)) flux_input_setfocus(input, self->node, true);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_fragment_root(
  IRawElementProviderFragment *p, IRawElementProviderFragmentRoot **out
) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	FluxUia *r = flux_uia_alloc(&self->info, self->info.root);
	if (!r) return E_OUTOFMEMORY;
	*out = ( IRawElementProviderFragmentRoot * ) &r->lpVtblRoot;
	return S_OK;
}

static IRawElementProviderFragmentVtbl const g_fragment_vtbl = {
  f_qi,
  f_addref,
  f_release,
  f_navigate,
  f_get_runtime_id,
  f_get_bounds,
  f_get_embedded,
  f_set_focus,
  f_get_fragment_root,
};

static HRESULT STDMETHODCALLTYPE r_qi(IRawElementProviderFragmentRoot *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_ROOT(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE r_addref(IRawElementProviderFragmentRoot *p) { return flux_uia_addref(SELF_ROOT(p)); }

static ULONG STDMETHODCALLTYPE r_release(IRawElementProviderFragmentRoot *p) { return flux_uia_release(SELF_ROOT(p)); }

static HRESULT STDMETHODCALLTYPE r_from_point(
  IRawElementProviderFragmentRoot *p, double x, double y, IRawElementProviderFragment **out
) {
	FluxUia *self = SELF_ROOT(p);
	if (!out) return E_POINTER;
	int        sub   = -1;
	XentNodeId hit   = flux_uia_hit(&self->info, x, y, &sub);
	FluxUia    probe = *self;
	probe.node       = hit;
	*out             = sub >= 0 ? uia_wrap_sub(&probe, sub) : uia_wrap(self, hit);
	return S_OK;
}

static int uia_focused_in_span(FluxUia const *self, FluxUiaSub const *s, int first, int last) {
	for (int i = first; i <= last; i++)
		if (s->focused(self->info.store, self->node, i)) return i;
	return -1;
}

static int uia_focused_sub(FluxUia const *self) {
	FluxUiaSub const *s     = uia_sub(self);
	int               first = 0, last = -1, heads = flux_uia_sub_heads(self);
	if (!s || !s->focused) return -1;
	int focused = uia_focused_in_span(self, s, FLUX_UIA_SUB_HEAD, FLUX_UIA_SUB_HEAD + heads - 1);
	if (focused >= 0) return focused;
	if (!uia_sub_span(self, &first, &last)) return -1;
	return uia_focused_in_span(self, s, first, last);
}

static HRESULT STDMETHODCALLTYPE r_get_focus(IRawElementProviderFragmentRoot *p, IRawElementProviderFragment **out) {
	FluxUia *self = SELF_ROOT(p);
	if (!out) return E_POINTER;
	XentNodeId focused = flux_uia_delegate(&self->info, flux_uia_find_focused(&self->info, self->info.root));
	FluxUia    probe   = *self;
	probe.node         = focused;
	probe.sub          = -1;
	int sub            = focused == XENT_NODE_INVALID ? -1 : uia_focused_sub(&probe);
	*out               = sub >= 0 ? uia_wrap_sub(&probe, sub) : uia_wrap(self, focused);
	return S_OK;
}

static IRawElementProviderFragmentRootVtbl const g_root_vtbl = {
  r_qi,
  r_addref,
  r_release,
  r_from_point,
  r_get_focus,
};

void flux_uia_dirty(FluxUiaContext const *info) {
	FluxWindow *win = info && info->hwnd ? ( FluxWindow * ) GetWindowLongPtrW(info->hwnd, GWLP_USERDATA) : NULL;
	if (win) flux_window_redraw(win);
}

HRESULT flux_uia_activate(FluxUia *self) {
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	XtkHost *host = flux_store_host(self->info.store);
	if (!host) return UIA_E_INVALIDOPERATION;
	flux_input_route_activation(host, self->info.store, self->node, XTK_EVENT_CAUSE_ACCESSIBILITY);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE inv_qi(IInvokeProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_INVOKE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE inv_addref(IInvokeProvider *p) { return flux_uia_addref(SELF_INVOKE(p)); }

static ULONG STDMETHODCALLTYPE inv_release(IInvokeProvider *p) { return flux_uia_release(SELF_INVOKE(p)); }

static HRESULT                 uia_sub_invoke(FluxUia *self) {
	FluxUiaSub const *s = uia_sub(self);
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	return s && s->invoke && s->invoke(self->info.store, self->node, self->sub) ? S_OK : UIA_E_INVALIDOPERATION;
}

static HRESULT uia_hook_invoke(FluxUia *self, FluxUiaHooks const *h) {
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	if (h->invoked) flux_uia_note(self->info.store, self->node, -1, FLUX_UIA_EVT_INVOKED);
	return h->invoke(self->info.store, self->node) ? S_OK : UIA_E_INVALIDOPERATION;
}

static HRESULT STDMETHODCALLTYPE inv_invoke(IInvokeProvider *p) {
	FluxUia *self = SELF_INVOKE(p);
	flux_uia_dirty(&self->info);
	FluxUiaHooks const *h = self->sub < 0 ? flux_module_uia(self->info.store, self->node) : NULL;
	if (h && h->invoke) return uia_hook_invoke(self, h);
	return self->sub >= 0 ? uia_sub_invoke(self) : flux_uia_activate(self);
}

static IInvokeProviderVtbl const g_invoke_vtbl = {inv_qi, inv_addref, inv_release, inv_invoke};

static HRESULT STDMETHODCALLTYPE tog_qi(IToggleProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_TOGGLE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   tog_addref(IToggleProvider *p) { return flux_uia_addref(SELF_TOGGLE(p)); }

static ULONG STDMETHODCALLTYPE   tog_release(IToggleProvider *p) { return flux_uia_release(SELF_TOGGLE(p)); }

static HRESULT STDMETHODCALLTYPE tog_toggle(IToggleProvider *p) {
	FluxUia *self = SELF_TOGGLE(p);
	flux_uia_dirty(&self->info);
	FluxUiaHooks const *h = self->sub < 0 ? flux_module_uia(self->info.store, self->node) : NULL;
	if (!h || !h->toggle) return flux_uia_activate(self);
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	h->toggle(self->info.store, self->node);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE tog_get_state(IToggleProvider *p, enum ToggleState *out) {
	FluxUia            *self = SELF_TOGGLE(p);
	FluxUiaHooks const *h    = self->sub < 0 ? flux_module_uia(self->info.store, self->node) : NULL;
	if (!out) return E_POINTER;
	if (h && h->toggle_state) {
		*out = ( enum ToggleState ) h->toggle_state(self->info.store, self->node);
		return S_OK;
	}
	uint8_t checked = xent_sem_checked(self->info.ctx, self->node);
	*out            = checked == 2 ? ToggleState_Indeterminate : (checked ? ToggleState_On : ToggleState_Off);
	return S_OK;
}

static IToggleProviderVtbl const g_toggle_vtbl = {tog_qi, tog_addref, tog_release, tog_toggle, tog_get_state};

static HRESULT STDMETHODCALLTYPE rng_qi(IRangeValueProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_RANGE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE rng_addref(IRangeValueProvider *p) { return flux_uia_addref(SELF_RANGE(p)); }

static ULONG STDMETHODCALLTYPE rng_release(IRangeValueProvider *p) { return flux_uia_release(SELF_RANGE(p)); }

static bool                    uia_range_is_readonly(FluxUia *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (h && h->readonly) return h->readonly(self->info.store, self->node);
	FluxControlType t = flux_node_type(self->info.store, self->node);
	return t == FLUX_CONTROL_PROGRESS || t == FLUX_CONTROL_PROGRESS_RING;
}

static HRESULT                   rng_get_component(IRangeValueProvider *p, double *out, int component);

static HRESULT STDMETHODCALLTYPE rng_set_value(IRangeValueProvider *p, double value) {
	FluxUia *self = SELF_RANGE(p);
	flux_uia_dirty(&self->info);
	if (!self->info.store || uia_range_is_readonly(self)) return UIA_E_INVALIDOPERATION;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;

	double lo = 0.0, hi = 0.0;
	rng_get_component(p, &lo, 1);
	rng_get_component(p, &hi, 2);
	if (value < lo || value > hi) return E_INVALIDARG;
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (self->sub >= 0)
		return h && h->set_thumb && h->set_thumb(self->info.store, self->node, self->sub, ( float ) value)
		       ? S_OK
		       : UIA_E_INVALIDOPERATION;
	if (h && h->set_range) h->set_range(self->info.store, self->node, value);
	return S_OK;
}

static HRESULT rng_get_component(IRangeValueProvider *p, double *out, int component) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	float               vals [3] = {0.0f, 0.0f, 0.0f};
	FluxUiaHooks const *h        = flux_module_uia(self->info.store, self->node);
	double              hook [3] = {0.0, 0.0, 0.0};
	if (self->sub < 0 && h && h->range && h->range(self->info.store, self->node, &hook [0], &hook [1], &hook [2])) {
		*out = hook [component];
		return S_OK;
	}
	if (self->sub >= 0 && h && h->thumb)
		h->thumb(self->info.store, self->node, self->sub, &vals [0], &vals [1], &vals [2]);
	else xent_sem_value(self->info.ctx, self->node, &vals [0], &vals [1], &vals [2]);
	*out = vals [component];
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_value(IRangeValueProvider *p, double *out) {
	return rng_get_component(p, out, 0);
}

static HRESULT STDMETHODCALLTYPE rng_get_max(IRangeValueProvider *p, double *out) {
	return rng_get_component(p, out, 2);
}

static HRESULT STDMETHODCALLTYPE rng_get_min(IRangeValueProvider *p, double *out) {
	return rng_get_component(p, out, 1);
}

static HRESULT STDMETHODCALLTYPE rng_get_readonly(IRangeValueProvider *p, BOOL *out) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	*out = uia_range_is_readonly(self) ? TRUE : FALSE;
	return S_OK;
}

static void uia_range_steps(FluxUia *self, double *small_step, double *large_step) {
	FluxUiaHooks const *h = self->sub < 0 ? flux_module_uia(self->info.store, self->node) : NULL;
	*small_step           = 0.1;
	*large_step           = 1.0;
	if (h && h->steps) h->steps(self->info.store, self->node, small_step, large_step);
}

static HRESULT STDMETHODCALLTYPE rng_get_large(IRangeValueProvider *p, double *out) {
	double small_step = 0.0;
	if (!out) return E_POINTER;
	uia_range_steps(SELF_RANGE(p), &small_step, out);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_small(IRangeValueProvider *p, double *out) {
	double large_step = 0.0;
	if (!out) return E_POINTER;
	uia_range_steps(SELF_RANGE(p), out, &large_step);
	return S_OK;
}

static IRangeValueProviderVtbl const g_range_vtbl = {
  rng_qi,
  rng_addref,
  rng_release,
  rng_set_value,
  rng_get_value,
  rng_get_readonly,
  rng_get_max,
  rng_get_min,
  rng_get_large,
  rng_get_small,
};

static HRESULT STDMETHODCALLTYPE val_qi(IValueProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_VALUE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE val_addref(IValueProvider *p) { return flux_uia_addref(SELF_VALUE(p)); }

static ULONG STDMETHODCALLTYPE val_release(IValueProvider *p) { return flux_uia_release(SELF_VALUE(p)); }

static char const             *uia_text_content(FluxUia *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (h && h->text) return h->text(self->info.store, self->node);
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type == FLUX_CONTROL_PASSWORD_BOX) return "";
	FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
	return tb->content;
}

static bool uia_value_is_picker(FluxUia const *self) {
	FluxControlType     t = flux_node_type(self->info.store, self->node);
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return t == FLUX_CONTROL_DATE_PICKER || t == FLUX_CONTROL_TIME_PICKER || (h && h->text && !h->set_text);
}

static HRESULT uia_apply_value(FluxUia const *self, FluxUiaHooks const *h, LPCWSTR value) {
	LPCWSTR text = value ? value : L"";
	int     n    = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
	if (n <= 0) return E_INVALIDARG;
	char *utf8 = malloc(( size_t ) n);
	if (!utf8) return E_OUTOFMEMORY;
	int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, n, NULL, NULL);
	if (size > 0 && h && h->set_text) h->set_text(self->info.store, self->node, utf8);
	free(utf8);
	return size > 0 ? S_OK : E_INVALIDARG;
}

static HRESULT STDMETHODCALLTYPE val_set_value(IValueProvider *p, LPCWSTR value) {
	FluxUia *self = SELF_VALUE(p);
	flux_uia_dirty(&self->info);
	if (!self->info.store) return UIA_E_INVALIDOPERATION;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	if (uia_value_is_picker(self)) return UIA_E_INVALIDOPERATION;
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (h && h->readonly && h->readonly(self->info.store, self->node)) return UIA_E_INVALIDOPERATION;
	return uia_apply_value(self, h, value);
}

static HRESULT STDMETHODCALLTYPE val_get_value(IValueProvider *p, BSTR *out) {
	FluxUia *self = SELF_VALUE(p);
	if (!out) return E_POINTER;
	char const *content = uia_text_content(self);
	*out                = flux_uia_bstr(content ? content : "");
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE val_get_readonly(IValueProvider *p, BOOL *out) {
	FluxUia            *self = SELF_VALUE(p);
	FluxUiaHooks const *h    = flux_module_uia(self->info.store, self->node);
	if (!out) return E_POINTER;
	if (h && h->readonly) *out = h->readonly(self->info.store, self->node) ? TRUE : FALSE;
	else *out = xent_sem_enabled(self->info.ctx, self->node) && !uia_value_is_picker(self) ? FALSE : TRUE;
	return S_OK;
}

static IValueProviderVtbl const g_value_vtbl = {
  val_qi,
  val_addref,
  val_release,
  val_set_value,
  val_get_value,
  val_get_readonly,
};

FluxUia *flux_uia_alloc(FluxUiaContext const *info, XentNodeId node) {
	FluxUia *p = ( FluxUia * ) calloc(1, sizeof(*p));
	if (!p) return NULL;
	p->lpVtblSimple     = &g_simple_vtbl;
	p->lpVtblFragment   = &g_fragment_vtbl;
	p->lpVtblRoot       = &g_root_vtbl;
	p->lpVtblInvoke     = &g_invoke_vtbl;
	p->lpVtblToggle     = &g_toggle_vtbl;
	p->lpVtblRange      = &g_range_vtbl;
	p->lpVtblValue      = &g_value_vtbl;
	p->lpVtblSelection  = &flux_uia_selitem_vtbl;
	p->lpVtblSelectionC = &flux_uia_selection_vtbl;
	p->lpVtblWindow     = &flux_uia_window_vtbl;
	p->lpVtblScroll     = &flux_uia_scroll_vtbl;
	p->lpVtblScrollItem = &flux_uia_scrollitem_vtbl;
	p->lpVtblExpand     = &flux_uia_expand_vtbl;
	p->lpVtblGrid       = &flux_uia_grid_vtbl;
	p->lpVtblGridItem   = &flux_uia_griditem_vtbl;
	p->lpVtblTable      = &flux_uia_table_vtbl;
	p->lpVtblTableItem  = &flux_uia_tableitem_vtbl;
	p->lpVtblItems      = &flux_uia_items_vtbl;
	p->lpVtblVirtual    = &flux_uia_virtual_vtbl;
	p->lpVtblText       = &flux_uia_text_vtbl;
	p->lpVtblDrop       = &flux_uia_drop_vtbl;
	p->ref              = 1;
	p->info             = *info;
	p->node             = node;
	p->sub              = -1;
	return p;
}

void *flux_uia_provider_create(FluxUiaContext const *info) {
	if (!info || !info->ctx || info->root == XENT_NODE_INVALID) return NULL;
	FluxUia *p = flux_uia_alloc(info, info->root);
	return p ? &p->lpVtblSimple : NULL;
}

int flux_uia_focussub(FluxUiaContext const *info, XentNodeId node) {
	if (!info || !info->ctx || node == XENT_NODE_INVALID) return -1;
	FluxUia probe = {.info = *info, .node = flux_uia_visible(info, node), .sub = -1};
	return probe.node == node ? uia_focused_sub(&probe) : -1;
}

XentNodeId flux_uia_delegate(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h  = node != XENT_NODE_INVALID ? flux_module_uia(info->store, node) : NULL;
	XentNodeId          to = h && h->delegate ? h->delegate(info->store, node) : XENT_NODE_INVALID;
	return to != XENT_NODE_INVALID ? to : node;
}

void flux_uia_raisefocus(FluxUiaContext const *info, XentNodeId node, int sub) {
	if (!info || !info->ctx || node == XENT_NODE_INVALID || !UiaClientsAreListening()) return;
	if (flux_uia_delegate(info, node) != node) sub = -1;
	node          = flux_uia_delegate(info, node);
	XentNodeId at = flux_uia_visible(info, node);
	FluxUia   *p  = sub >= 0 && at == node ? flux_uia_alloc_sub(info, node, sub) : flux_uia_alloc(info, at);
	if (!p) return;
	UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) &p->lpVtblSimple, FLUX_UIA_EVENT_FOCUS_CHANGED);
	(( IUnknown * ) &p->lpVtblSimple)->lpVtbl->Release(( IUnknown * ) &p->lpVtblSimple);
}
