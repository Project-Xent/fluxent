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

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define FLUX_UIA_PROP_NAME                  30005
#define FLUX_UIA_PROP_CONTROL_TYPE          30003
#define FLUX_UIA_PROP_HAS_KEYBOARD_FOCUS    30008
#define FLUX_UIA_PROP_IS_KEYBOARD_FOCUSABLE 30009
#define FLUX_UIA_PROP_IS_ENABLED            30010
#define FLUX_UIA_PROP_AUTOMATION_ID         30011
#define FLUX_UIA_PROP_IS_CONTROL_ELEMENT    30016
#define FLUX_UIA_PROP_IS_CONTENT_ELEMENT    30017


#define FLUX_UIA_EVENT_INVOKED              20003
#define FLUX_UIA_EVENT_FOCUS_CHANGED        20005


#define FLUX_UIA_APPEND_RUNTIME_ID          3

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
  {FLUX_UIA_P_INVOKE,         10000, &Flux_IID_Invoke,         offsetof(FluxUia, lpVtblInvoke)    },
  {FLUX_UIA_P_TOGGLE,         10015, &Flux_IID_Toggle,         offsetof(FluxUia, lpVtblToggle)    },
  {FLUX_UIA_P_RANGE,          10003, &Flux_IID_RangeValue,     offsetof(FluxUia, lpVtblRange)     },
  {FLUX_UIA_P_VALUE,          10002, &Flux_IID_Value,          offsetof(FluxUia, lpVtblValue)     },
  {FLUX_UIA_P_SELECTION_ITEM, 10010, &Flux_IID_SelectionItem,  offsetof(FluxUia, lpVtblSelection) },
  {FLUX_UIA_P_SCROLL,         10004, &Flux_IID_Scroll,         offsetof(FluxUia, lpVtblScroll)    },
  {FLUX_UIA_P_SCROLL_ITEM,    10017, &Flux_IID_ScrollItem,     offsetof(FluxUia, lpVtblScrollItem)},
  {FLUX_UIA_P_EXPAND,         10005, &Flux_IID_ExpandCollapse, offsetof(FluxUia, lpVtblExpand)    },
  {FLUX_UIA_P_SELECTION,      10001, &Flux_IID_Selection,      offsetof(FluxUia, lpVtblSelectionC)},
  {FLUX_UIA_P_WINDOW,         10009, &Flux_IID_Window,         offsetof(FluxUia, lpVtblWindow)    },
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

static FluxUiaControlInfo uia_control_info(FluxUia const *self) {
	if (self->thumb >= 0) return (FluxUiaControlInfo) {FLUX_UIA_SLIDER, FLUX_UIA_P_RANGE};
	if (uia_range_slider(self)) return (FluxUiaControlInfo) {FLUX_UIA_GROUP, 0};
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (!h || h->role <= 0) return (FluxUiaControlInfo) {FLUX_UIA_CUSTOM, 0};
	return (FluxUiaControlInfo) {h->role, h->patterns};
}

unsigned flux_uia_patterns(FluxUiaContext const *info, XentNodeId node) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	return h ? h->patterns : 0u;
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
	*out = ProviderOptions_ServerSideProvider;
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

static BSTR utf8_to_bstr(char const *utf8) {
	if (!utf8) return NULL;
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (wlen <= 1) return NULL;
	BSTR bstr = SysAllocStringLen(NULL, ( UINT ) (wlen - 1));
	if (bstr) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, bstr, wlen);
	return bstr;
}

static void variant_set_bool(VARIANT *out, bool value) {
	out->vt      = VT_BOOL;
	out->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

static void variant_set_i4(VARIANT *out, int value) {
	out->vt   = VT_I4;
	out->lVal = value;
}

static bool uia_has_focus(FluxUia *self) {
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || !nd->state.focused || GetFocus() != self->info.hwnd) return false;
	FluxSliderData const *range = self->thumb >= 0 ? uia_range_slider(self) : NULL;
	return self->thumb < 0 || (range && range->active_thumb == self->thumb);
}

static int uia_automation_id(FluxUia const *self) {
	return self->thumb < 0 ? ( int ) self->node : -(( int ) self->node * 2 + self->thumb + 1);
}

static bool uia_in_control_view(FluxUia const *self) {
	FluxNodeData const *nd = flux_store_find(self->info.store, self->node);
	if (!nd || nd->component_type != FLUX_CONTROL_PROGRESS_RING) return true;
	FluxProgressRingData const *ring = ( FluxProgressRingData const * ) nd->component_data;
	return !ring || !ring->inactive;
}

static bool s_get_scalar_property(FluxUia *self, FluxUiaControlInfo info, PROPERTYID id, VARIANT *out) {
	switch (id) {
	case FLUX_UIA_PROP_CONTROL_TYPE  : variant_set_i4(out, info.control_type); return true;
	case FLUX_UIA_PROP_AUTOMATION_ID : variant_set_i4(out, uia_automation_id(self)); return true;
	case FLUX_UIA_PROP_IS_ENABLED    : variant_set_bool(out, xent_sem_enabled(self->info.ctx, self->node)); return true;
	case FLUX_UIA_PROP_IS_CONTROL_ELEMENT    :
	case FLUX_UIA_PROP_IS_CONTENT_ELEMENT    : variant_set_bool(out, uia_in_control_view(self)); return true;
	case FLUX_UIA_PROP_IS_KEYBOARD_FOCUSABLE : variant_set_bool(out, info.control_type != FLUX_UIA_GROUP); return true;
	case FLUX_UIA_PROP_HAS_KEYBOARD_FOCUS    : variant_set_bool(out, uia_has_focus(self)); return true;
	default                                  : return false;
	}
}

static HRESULT STDMETHODCALLTYPE s_get_property(IRawElementProviderSimple *p, PROPERTYID id, VARIANT *out) {
	FluxUia           *self = SELF_SIMPLE(p);
	FluxUiaControlInfo info = uia_control_info(self);
	if (!out) return E_POINTER;
	VariantInit(out);

	if (s_get_scalar_property(self, info, id, out)) return S_OK;
	if (id != FLUX_UIA_PROP_NAME) return S_OK;

	char const *thumb_names [2] = {"Range start", "Range end"};
	BSTR name = utf8_to_bstr(self->thumb >= 0 ? thumb_names [self->thumb] : xent_sem_label(self->info.ctx, self->node));
	if (name) {
		out->vt      = VT_BSTR;
		out->bstrVal = name;
	}
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

static IRawElementProviderFragment *uia_wrap_thumb(FluxUia *self, int thumb) {
	FluxUia *p = flux_uia_alloc(&self->info, self->node);
	if (!p) return NULL;
	p->thumb = thumb;
	return ( IRawElementProviderFragment * ) &p->lpVtblFragment;
}

static IRawElementProviderFragment *uia_navigate_thumb(FluxUia *self, enum NavigateDirection dir) {
	switch (dir) {
	case NavigateDirection_Parent          : return uia_wrap(self, self->node);
	case NavigateDirection_NextSibling     : return self->thumb == 0 ? uia_wrap_thumb(self, 1) : NULL;
	case NavigateDirection_PreviousSibling : return self->thumb == 1 ? uia_wrap_thumb(self, 0) : NULL;
	default                                : return NULL;
	}
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
	if (self->thumb >= 0) return uia_navigate_thumb(self, dir);
	bool range = uia_range_slider(self) != NULL;
	if (range && dir == NavigateDirection_FirstChild) return uia_wrap_thumb(self, 0);
	if (range && dir == NavigateDirection_LastChild) return uia_wrap_thumb(self, 1);
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
	SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, self->thumb >= 0 ? 3 : 2);
	if (!sa) return E_OUTOFMEMORY;
	LONG idx0 = 0, idx1 = 1, idx2 = 2;
	int  append = FLUX_UIA_APPEND_RUNTIME_ID, id = ( int ) self->node, thumb = self->thumb + 1;
	SafeArrayPutElement(sa, &idx0, &append);
	SafeArrayPutElement(sa, &idx1, &id);
	if (self->thumb >= 0) SafeArrayPutElement(sa, &idx2, &thumb);
	*out = sa;
	return S_OK;
}

static void uia_thumb_rect(FluxUia const *self, XentRect *rect) {
	FluxSliderData const *sd = uia_range_slider(self);
	if (!sd) return;
	FluxSliderLine line  = flux_slider_line(rect->x, rect->y, rect->w, rect->h, sd->vertical);
	float          range = sd->max_value - sd->min_value;
	float          at    = range > 0.0f ? (sd->range_value [self->thumb] - sd->min_value) / range : 0.0f;
	float          along = flux_slider_along(&line, at);
	float          r     = FLUX_SLIDER_THUMB_OUTER;
	XentRect       node  = *rect;
	*rect                = sd->vertical ? (XentRect) {line.cross - r, along - r, r * 2.0f, r * 2.0f}
	                                    : (XentRect) {along - r, line.cross - r, r * 2.0f, r * 2.0f};
	if (xent_resolved_dir(self->info.ctx, self->node) == XENT_DIRECTION_RTL)
		rect->x = 2.0f * node.x + node.w - rect->x - rect->w;
}

static HRESULT STDMETHODCALLTYPE f_get_bounds(IRawElementProviderFragment *p, struct UiaRect *out) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	XentRect rect = {0};
	xent_layout_rect(self->info.ctx, self->node, &rect);
	if (self->thumb >= 0) uia_thumb_rect(self, &rect);
	float scale  = ( float ) GetDpiForWindow(self->info.hwnd) / 96.0f;
	POINT origin = {0, 0};
	ClientToScreen(self->info.hwnd, &origin);
	out->left   = origin.x + rect.x * scale;
	out->top    = origin.y + rect.y * scale;
	out->width  = rect.w * scale;
	out->height = rect.h * scale;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_embedded(IRawElementProviderFragment *p, SAFEARRAY **out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = NULL;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_set_focus(IRawElementProviderFragment *p) {
	( void ) p;
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

static XentNodeId              uia_deepest_at(XentCtx *ctx, XentNodeId node, float x, float y) {
	XentRect rect = {0};
	xent_layout_rect(ctx, node, &rect);
	if (x < rect.x || y < rect.y || x >= rect.x + rect.w || y >= rect.y + rect.h) return XENT_NODE_INVALID;

	for (XentNodeId c = xent_node_first(ctx, node); c != XENT_NODE_INVALID; c = xent_node_next(ctx, c)) {
		XentNodeId hit = uia_deepest_at(ctx, c, x, y);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return node;
}

static HRESULT STDMETHODCALLTYPE r_from_point(
  IRawElementProviderFragmentRoot *p, double x, double y, IRawElementProviderFragment **out
) {
	FluxUia *self = SELF_ROOT(p);
	if (!out) return E_POINTER;
	*out         = NULL;

	float scale  = ( float ) GetDpiForWindow(self->info.hwnd) / 96.0f;
	POINT origin = {0, 0};
	ClientToScreen(self->info.hwnd, &origin);
	float      lx  = ( float ) ((x - origin.x) / scale);
	float      ly  = ( float ) ((y - origin.y) / scale);
	XentNodeId hit = flux_uia_visible(&self->info, uia_deepest_at(self->info.ctx, self->info.root, lx, ly));
	*out           = uia_wrap(self, hit);
	return S_OK;
}

static XentNodeId uia_find_focused(FluxUia *self, XentNodeId node) {
	FluxNodeData *nd = self->info.store ? flux_store_find(self->info.store, node) : NULL;
	if (nd && nd->state.focused) return node;

	for (XentNodeId c = xent_node_first(self->info.ctx, node); c != XENT_NODE_INVALID;
	  c               = xent_node_next(self->info.ctx, c))
	{
		XentNodeId hit = uia_find_focused(self, c);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return XENT_NODE_INVALID;
}

static HRESULT STDMETHODCALLTYPE r_get_focus(IRawElementProviderFragmentRoot *p, IRawElementProviderFragment **out) {
	FluxUia *self = SELF_ROOT(p);
	if (!out) return E_POINTER;
	XentNodeId focused          = uia_find_focused(self, self->info.root);
	FluxUia    probe            = *self;
	probe.node                  = focused;
	probe.thumb                 = -1;
	FluxSliderData const *range = focused == XENT_NODE_INVALID ? NULL : uia_range_slider(&probe);
	*out                        = range ? uia_wrap_thumb(&probe, range->active_thumb) : uia_wrap(self, focused);
	return S_OK;
}

static IRawElementProviderFragmentRootVtbl const g_root_vtbl = {
  r_qi,
  r_addref,
  r_release,
  r_from_point,
  r_get_focus,
};

HRESULT flux_uia_activate(FluxUia *self) {
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	XtkHost *host = flux_store_host(self->info.store);
	if (!host) return UIA_E_INVALIDOPERATION;
	flux_input_route_activation(host, self->info.store, self->node, XTK_EVENT_CAUSE_ACCESSIBILITY);
	return S_OK;
}

static void uia_raise_self_event(FluxUia *self, int event_id) {
	if (UiaClientsAreListening())
		UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) &self->lpVtblSimple, event_id);
}

static HRESULT STDMETHODCALLTYPE inv_qi(IInvokeProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_INVOKE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   inv_addref(IInvokeProvider *p) { return flux_uia_addref(SELF_INVOKE(p)); }

static ULONG STDMETHODCALLTYPE   inv_release(IInvokeProvider *p) { return flux_uia_release(SELF_INVOKE(p)); }

static HRESULT STDMETHODCALLTYPE inv_invoke(IInvokeProvider *p) {
	FluxUia *self = SELF_INVOKE(p);
	HRESULT  hr   = flux_uia_activate(self);
	if (SUCCEEDED(hr)) uia_raise_self_event(self, FLUX_UIA_EVENT_INVOKED);
	return hr;
}

static IInvokeProviderVtbl const g_invoke_vtbl = {inv_qi, inv_addref, inv_release, inv_invoke};

static HRESULT STDMETHODCALLTYPE tog_qi(IToggleProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(SELF_TOGGLE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   tog_addref(IToggleProvider *p) { return flux_uia_addref(SELF_TOGGLE(p)); }

static ULONG STDMETHODCALLTYPE   tog_release(IToggleProvider *p) { return flux_uia_release(SELF_TOGGLE(p)); }

static HRESULT STDMETHODCALLTYPE tog_toggle(IToggleProvider *p) { return flux_uia_activate(SELF_TOGGLE(p)); }

static HRESULT STDMETHODCALLTYPE tog_get_state(IToggleProvider *p, enum ToggleState *out) {
	FluxUia *self = SELF_TOGGLE(p);
	if (!out) return E_POINTER;
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
	FluxControlType t = flux_node_type(self->info.store, self->node);
	return t == FLUX_CONTROL_PROGRESS || t == FLUX_CONTROL_PROGRESS_RING;
}

static HRESULT STDMETHODCALLTYPE rng_set_value(IRangeValueProvider *p, double value) {
	FluxUia *self = SELF_RANGE(p);
	if (!self->info.store || uia_range_is_readonly(self)) return UIA_E_INVALIDOPERATION;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;

	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (self->thumb >= 0)
		return h && h->set_thumb && h->set_thumb(self->info.store, self->node, self->thumb, ( float ) value)
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
	if (self->thumb >= 0 && h && h->thumb)
		h->thumb(self->info.store, self->node, self->thumb, &vals [0], &vals [1], &vals [2]);
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

static HRESULT STDMETHODCALLTYPE rng_get_large(IRangeValueProvider *p, double *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = 0.0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_small(IRangeValueProvider *p, double *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = 1.0;
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
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || !nd->component_data) return NULL;
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (h && h->text) return h->text(self->info.store, self->node);
	if (nd->component_type == FLUX_CONTROL_PASSWORD_BOX) return "";
	FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
	return tb->content;
}

static bool uia_value_is_picker(FluxUia const *self) {
	FluxControlType t = flux_node_type(self->info.store, self->node);
	return t == FLUX_CONTROL_DATE_PICKER || t == FLUX_CONTROL_TIME_PICKER;
}

static HRESULT STDMETHODCALLTYPE val_set_value(IValueProvider *p, LPCWSTR value) {
	FluxUia *self = SELF_VALUE(p);
	if (!self->info.store) return UIA_E_INVALIDOPERATION;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	if (uia_value_is_picker(self)) return UIA_E_INVALIDOPERATION;

	int   n    = WideCharToMultiByte(CP_UTF8, 0, value ? value : L"", -1, NULL, 0, NULL, NULL);
	char *utf8 = ( char * ) malloc(n > 0 ? ( size_t ) n : 1);
	if (!utf8) return E_OUTOFMEMORY;
	WideCharToMultiByte(CP_UTF8, 0, value ? value : L"", -1, utf8, n, NULL, NULL);

	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (h && h->set_text) h->set_text(self->info.store, self->node, utf8);
	free(utf8);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE val_get_value(IValueProvider *p, BSTR *out) {
	FluxUia *self = SELF_VALUE(p);
	if (!out) return E_POINTER;
	char const *content = uia_text_content(self);
	*out                = utf8_to_bstr(content ? content : "");
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE val_get_readonly(IValueProvider *p, BOOL *out) {
	FluxUia *self = SELF_VALUE(p);
	if (!out) return E_POINTER;
	*out = xent_sem_enabled(self->info.ctx, self->node) && !uia_value_is_picker(self) ? FALSE : TRUE;
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
	p->ref              = 1;
	p->info             = *info;
	p->node             = node;
	p->thumb            = -1;
	return p;
}

void *flux_uia_provider_create(FluxUiaContext const *info) {
	if (!info || !info->ctx || info->root == XENT_NODE_INVALID) return NULL;
	FluxUia *p = flux_uia_alloc(info, info->root);
	return p ? &p->lpVtblSimple : NULL;
}

void flux_uia_raisefocus(FluxUiaContext const *info, XentNodeId node) {
	if (!info || !info->ctx || node == XENT_NODE_INVALID || !UiaClientsAreListening()) return;
	FluxUia *p = flux_uia_alloc(info, flux_uia_visible(info, node));
	if (!p) return;
	UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) &p->lpVtblSimple, FLUX_UIA_EVENT_FOCUS_CHANGED);
	(( IUnknown * ) &p->lpVtblSimple)->lpVtbl->Release(( IUnknown * ) &p->lpVtblSimple);
}
