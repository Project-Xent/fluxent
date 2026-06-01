#ifndef COBJMACROS
  #define COBJMACROS
#endif
#ifndef CINTERFACE
  #define CINTERFACE
#endif

#include "compose/flux_uia.h"

#include "fluxent/controls/flux_textbox_data.h"
#include "fluxent/flux_control_type.h"
#include "fluxent/flux_controls.h"

#include <windows.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

/* UIA well-known integer ids. The C SDK headers do not export the UIA_* symbols
 * in this toolchain, so the values are inlined (matching uiautomationclient.h). */
#define FLUX_UIA_PROP_NAME                  30005
#define FLUX_UIA_PROP_CONTROL_TYPE          30003
#define FLUX_UIA_PROP_HAS_KEYBOARD_FOCUS    30008
#define FLUX_UIA_PROP_IS_KEYBOARD_FOCUSABLE 30009
#define FLUX_UIA_PROP_IS_ENABLED            30010
#define FLUX_UIA_PROP_AUTOMATION_ID         30011
#define FLUX_UIA_PROP_IS_CONTROL_ELEMENT    30016
#define FLUX_UIA_PROP_IS_CONTENT_ELEMENT    30017

#define FLUX_UIA_PATTERN_INVOKE             10000
#define FLUX_UIA_PATTERN_VALUE              10002
#define FLUX_UIA_PATTERN_RANGE_VALUE        10003
#define FLUX_UIA_PATTERN_SELECTION_ITEM     10010
#define FLUX_UIA_PATTERN_TOGGLE             10015

#define FLUX_UIA_EVENT_INVOKED              20003
#define FLUX_UIA_EVENT_FOCUS_CHANGED        20005

#define FLUX_UIA_CT_BUTTON                  50000
#define FLUX_UIA_CT_CHECKBOX                50002
#define FLUX_UIA_CT_EDIT                    50004
#define FLUX_UIA_CT_HYPERLINK               50005
#define FLUX_UIA_CT_IMAGE                   50006
#define FLUX_UIA_CT_LIST                    50008
#define FLUX_UIA_CT_MENU                    50009
#define FLUX_UIA_CT_PROGRESSBAR             50012
#define FLUX_UIA_CT_RADIO                   50013
#define FLUX_UIA_CT_SLIDER                  50015
#define FLUX_UIA_CT_SPINNER                 50016
#define FLUX_UIA_CT_TAB                     50018
#define FLUX_UIA_CT_TEXT                    50020
#define FLUX_UIA_CT_TOOLTIP                 50022
#define FLUX_UIA_CT_CUSTOM                  50025
#define FLUX_UIA_CT_GROUP                   50026
#define FLUX_UIA_CT_PANE                    50033
#define FLUX_UIA_CT_SEPARATOR               50038

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
static const GUID Flux_IID_SelectionItem = {
  0x2acad808, 0xb2d4, 0x452d, {0xa4, 0x07, 0x91, 0xff, 0x1a, 0xd1, 0x67, 0xb2}
};

/* One object exposes the three provider facets plus every pattern it may answer;
 * QI hands back the matching vtable, gated by the control's pattern support. */
typedef struct FluxUia {
	IRawElementProviderSimpleVtbl const       *lpVtblSimple;
	IRawElementProviderFragmentVtbl const     *lpVtblFragment;
	IRawElementProviderFragmentRootVtbl const *lpVtblRoot;
	IInvokeProviderVtbl const                 *lpVtblInvoke;
	IToggleProviderVtbl const                 *lpVtblToggle;
	IRangeValueProviderVtbl const             *lpVtblRange;
	IValueProviderVtbl const                  *lpVtblValue;
	ISelectionItemProviderVtbl const          *lpVtblSelection;
	LONG                                       ref;
	FluxUiaContext                             info;
	XentNodeId                                 node;
} FluxUia;

#define SELF_SIMPLE(p)    (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblSimple)))
#define SELF_FRAG(p)      (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblFragment)))
#define SELF_ROOT(p)      (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblRoot)))
#define SELF_INVOKE(p)    (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblInvoke)))
#define SELF_TOGGLE(p)    (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblToggle)))
#define SELF_RANGE(p)     (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblRange)))
#define SELF_VALUE(p)     (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblValue)))
#define SELF_SELECTION(p) (( FluxUia * ) (( char * ) (p) - offsetof(FluxUia, lpVtblSelection)))

typedef struct FluxUiaControlInfo {
	int  control_type;
	bool invoke;
	bool toggle;
	bool range;
	bool value;
	bool selection;
} FluxUiaControlInfo;

/* Control type -> UIA control type + the patterns it answers. Table-driven so the
 * provider stays free of per-control branching. */
static FluxUiaControlInfo const kFluxUiaControls [] = {
  [FLUX_CONTROL_CONTAINER]     = {FLUX_UIA_CT_GROUP,       0, 0, 0, 0, 0},
  [FLUX_CONTROL_TEXT]          = {FLUX_UIA_CT_TEXT,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_BUTTON]        = {FLUX_UIA_CT_BUTTON,      1, 0, 0, 0, 0},
  [FLUX_CONTROL_TOGGLE_BUTTON] = {FLUX_UIA_CT_BUTTON,      1, 1, 0, 0, 0},
  [FLUX_CONTROL_CHECKBOX]      = {FLUX_UIA_CT_CHECKBOX,    0, 1, 0, 0, 0},
  [FLUX_CONTROL_RADIO]         = {FLUX_UIA_CT_RADIO,       0, 0, 0, 0, 1},
  [FLUX_CONTROL_SWITCH]        = {FLUX_UIA_CT_CHECKBOX,    0, 1, 0, 0, 0},
  [FLUX_CONTROL_SLIDER]        = {FLUX_UIA_CT_SLIDER,      0, 0, 1, 0, 0},
  [FLUX_CONTROL_TEXT_INPUT]    = {FLUX_UIA_CT_EDIT,        0, 0, 0, 1, 0},
  [FLUX_CONTROL_SCROLL]        = {FLUX_UIA_CT_PANE,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_IMAGE]         = {FLUX_UIA_CT_IMAGE,       0, 0, 0, 0, 0},
  [FLUX_CONTROL_PROGRESS]      = {FLUX_UIA_CT_PROGRESSBAR, 0, 0, 1, 0, 0},
  [FLUX_CONTROL_LIST]          = {FLUX_UIA_CT_LIST,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_TAB]           = {FLUX_UIA_CT_TAB,         0, 0, 0, 0, 0},
  [FLUX_CONTROL_CARD]          = {FLUX_UIA_CT_GROUP,       0, 0, 0, 0, 0},
  [FLUX_CONTROL_DIVIDER]       = {FLUX_UIA_CT_SEPARATOR,   0, 0, 0, 0, 0},
  [FLUX_CONTROL_CANVAS]        = {FLUX_UIA_CT_PANE,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_PASSWORD_BOX]  = {FLUX_UIA_CT_EDIT,        0, 0, 0, 1, 0},
  [FLUX_CONTROL_NUMBER_BOX]    = {FLUX_UIA_CT_SPINNER,     0, 0, 1, 1, 0},
  [FLUX_CONTROL_HYPERLINK]     = {FLUX_UIA_CT_HYPERLINK,   1, 0, 0, 0, 0},
  [FLUX_CONTROL_REPEAT_BUTTON] = {FLUX_UIA_CT_BUTTON,      1, 0, 0, 0, 0},
  [FLUX_CONTROL_PROGRESS_RING] = {FLUX_UIA_CT_PROGRESSBAR, 0, 0, 1, 0, 0},
  [FLUX_CONTROL_INFO_BADGE]    = {FLUX_UIA_CT_GROUP,       0, 0, 0, 0, 0},
  [FLUX_CONTROL_TOOLTIP]       = {FLUX_UIA_CT_TOOLTIP,     0, 0, 0, 0, 0},
  [FLUX_CONTROL_FLYOUT]        = {FLUX_UIA_CT_MENU,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_MENU_FLYOUT]   = {FLUX_UIA_CT_MENU,        0, 0, 0, 0, 0},
  [FLUX_CONTROL_CUSTOM]        = {FLUX_UIA_CT_CUSTOM,      0, 0, 0, 0, 0},
};

static FluxUia           *uia_alloc(FluxUiaContext const *info, XentNodeId node);

static FluxUiaControlInfo uia_control_info(FluxUia const *self) {
	FluxControlType t = flux_get_control_type(self->info.ctx, self->node);
	if (( size_t ) t >= sizeof(kFluxUiaControls) / sizeof(kFluxUiaControls [0]))
		return (FluxUiaControlInfo) {FLUX_UIA_CT_CUSTOM, 0, 0, 0, 0, 0};
	return kFluxUiaControls [t];
}

static FluxNodeData *uia_node_data(FluxUia const *self) {
	return self->info.store ? flux_node_store_get(self->info.store, self->node) : NULL;
}

/* ---- shared IUnknown + pattern dispatch ---- */

static void *uia_pattern_iface(FluxUia *self, REFIID riid) {
	FluxUiaControlInfo info = uia_control_info(self);
	if (info.invoke && IsEqualGUID(riid, &Flux_IID_Invoke)) return &self->lpVtblInvoke;
	if (info.toggle && IsEqualGUID(riid, &Flux_IID_Toggle)) return &self->lpVtblToggle;
	if (info.range && IsEqualGUID(riid, &Flux_IID_RangeValue)) return &self->lpVtblRange;
	if (info.value && IsEqualGUID(riid, &Flux_IID_Value)) return &self->lpVtblValue;
	if (info.selection && IsEqualGUID(riid, &Flux_IID_SelectionItem)) return &self->lpVtblSelection;
	return NULL;
}

static HRESULT uia_qi_impl(FluxUia *self, REFIID riid, void **ppv) {
	if (!ppv) return E_POINTER;

	*ppv = NULL;
	if (IsEqualGUID(riid, &Flux_IID_IUnknown) || IsEqualGUID(riid, &Flux_IID_REPSimple)) *ppv = &self->lpVtblSimple;
	else if (IsEqualGUID(riid, &Flux_IID_REPFragment)) *ppv = &self->lpVtblFragment;
	else if (IsEqualGUID(riid, &Flux_IID_REPFragmentRoot) && self->node == self->info.root) *ppv = &self->lpVtblRoot;
	else *ppv = uia_pattern_iface(self, riid);

	if (!*ppv) return E_NOINTERFACE;
	InterlockedIncrement(&self->ref);
	return S_OK;
}

static ULONG uia_addref_impl(FluxUia *self) { return ( ULONG ) InterlockedIncrement(&self->ref); }

static ULONG uia_release_impl(FluxUia *self) {
	LONG r = InterlockedDecrement(&self->ref);
	if (r == 0) free(self);
	return ( ULONG ) r;
}

/* ---- IRawElementProviderSimple ---- */

static HRESULT STDMETHODCALLTYPE s_qi(IRawElementProviderSimple *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_SIMPLE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   s_addref(IRawElementProviderSimple *p) { return uia_addref_impl(SELF_SIMPLE(p)); }

static ULONG STDMETHODCALLTYPE   s_release(IRawElementProviderSimple *p) { return uia_release_impl(SELF_SIMPLE(p)); }

static HRESULT STDMETHODCALLTYPE s_get_options(IRawElementProviderSimple *p, enum ProviderOptions *out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = ProviderOptions_ServerSideProvider;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE s_get_pattern(IRawElementProviderSimple *p, PATTERNID id, IUnknown **out) {
	FluxUia           *self = SELF_SIMPLE(p);
	FluxUiaControlInfo info = uia_control_info(self);
	if (!out) return E_POINTER;

	*out = NULL;
	if (id == FLUX_UIA_PATTERN_INVOKE && info.invoke) *out = ( IUnknown * ) &self->lpVtblInvoke;
	else if (id == FLUX_UIA_PATTERN_TOGGLE && info.toggle) *out = ( IUnknown * ) &self->lpVtblToggle;
	else if (id == FLUX_UIA_PATTERN_RANGE_VALUE && info.range) *out = ( IUnknown * ) &self->lpVtblRange;
	else if (id == FLUX_UIA_PATTERN_VALUE && info.value) *out = ( IUnknown * ) &self->lpVtblValue;
	else if (id == FLUX_UIA_PATTERN_SELECTION_ITEM && info.selection) *out = ( IUnknown * ) &self->lpVtblSelection;

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
	return nd && nd->state.focused && GetFocus() == self->info.hwnd;
}

static HRESULT STDMETHODCALLTYPE s_get_property(IRawElementProviderSimple *p, PROPERTYID id, VARIANT *out) {
	FluxUia           *self = SELF_SIMPLE(p);
	FluxUiaControlInfo info = uia_control_info(self);
	if (!out) return E_POINTER;
	VariantInit(out);

	switch (id) {
	case FLUX_UIA_PROP_NAME          : break;
	case FLUX_UIA_PROP_CONTROL_TYPE  : variant_set_i4(out, info.control_type); return S_OK;
	case FLUX_UIA_PROP_AUTOMATION_ID : variant_set_i4(out, ( int ) self->node); return S_OK;
	case FLUX_UIA_PROP_IS_ENABLED :
		variant_set_bool(out, xent_get_semantic_enabled(self->info.ctx, self->node));
		return S_OK;
	case FLUX_UIA_PROP_IS_CONTROL_ELEMENT : variant_set_bool(out, true); return S_OK;
	case FLUX_UIA_PROP_IS_CONTENT_ELEMENT : variant_set_bool(out, true); return S_OK;
	case FLUX_UIA_PROP_IS_KEYBOARD_FOCUSABLE :
		variant_set_bool(out, info.control_type != FLUX_UIA_CT_GROUP);
		return S_OK;
	case FLUX_UIA_PROP_HAS_KEYBOARD_FOCUS : variant_set_bool(out, uia_has_focus(self)); return S_OK;
	default                               : return S_OK;
	}

	BSTR name = utf8_to_bstr(xent_get_semantic_label(self->info.ctx, self->node));
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

/* ---- IRawElementProviderFragment ---- */

static HRESULT STDMETHODCALLTYPE f_qi(IRawElementProviderFragment *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_FRAG(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE      f_addref(IRawElementProviderFragment *p) { return uia_addref_impl(SELF_FRAG(p)); }

static ULONG STDMETHODCALLTYPE      f_release(IRawElementProviderFragment *p) { return uia_release_impl(SELF_FRAG(p)); }

static IRawElementProviderFragment *uia_wrap(FluxUia *self, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return NULL;
	FluxUia *p = uia_alloc(&self->info, node);
	return p ? ( IRawElementProviderFragment * ) &p->lpVtblFragment : NULL;
}

static XentNodeId uia_navigate_target(FluxUia *self, enum NavigateDirection dir) {
	XentContext *ctx = self->info.ctx;
	switch (dir) {
	case NavigateDirection_Parent :
		return self->node == self->info.root ? XENT_NODE_INVALID : xent_get_parent(ctx, self->node);
	case NavigateDirection_NextSibling :
		return self->node == self->info.root ? XENT_NODE_INVALID : xent_get_next_sibling(ctx, self->node);
	case NavigateDirection_PreviousSibling :
		return self->node == self->info.root ? XENT_NODE_INVALID : xent_get_prev_sibling(ctx, self->node);
	case NavigateDirection_FirstChild : return xent_get_first_child(ctx, self->node);
	case NavigateDirection_LastChild  : break;
	}

	XentNodeId target = XENT_NODE_INVALID;
	for (XentNodeId c = xent_get_first_child(ctx, self->node); c != XENT_NODE_INVALID;
	  c               = xent_get_next_sibling(ctx, c))
		target = c;
	return target;
}

static HRESULT STDMETHODCALLTYPE f_navigate(
  IRawElementProviderFragment *p, enum NavigateDirection dir, IRawElementProviderFragment **out
) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	*out = uia_wrap(self, uia_navigate_target(self, dir));
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_runtime_id(IRawElementProviderFragment *p, SAFEARRAY **out) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	*out          = NULL;
	SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, 2);
	if (!sa) return E_OUTOFMEMORY;
	LONG idx0 = 0, idx1 = 1;
	int  append = FLUX_UIA_APPEND_RUNTIME_ID, id = ( int ) self->node;
	SafeArrayPutElement(sa, &idx0, &append);
	SafeArrayPutElement(sa, &idx1, &id);
	*out = sa;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE f_get_bounds(IRawElementProviderFragment *p, struct UiaRect *out) {
	FluxUia *self = SELF_FRAG(p);
	if (!out) return E_POINTER;
	XentRect rect = {0};
	xent_get_layout_rect(self->info.ctx, self->node, &rect);
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
	FluxUia *r = uia_alloc(&self->info, self->info.root);
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

/* ---- IRawElementProviderFragmentRoot ---- */

static HRESULT STDMETHODCALLTYPE r_qi(IRawElementProviderFragmentRoot *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_ROOT(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE r_addref(IRawElementProviderFragmentRoot *p) { return uia_addref_impl(SELF_ROOT(p)); }

static ULONG STDMETHODCALLTYPE r_release(IRawElementProviderFragmentRoot *p) { return uia_release_impl(SELF_ROOT(p)); }

static XentNodeId              uia_deepest_at(XentContext *ctx, XentNodeId node, float x, float y) {
	XentRect rect = {0};
	xent_get_layout_rect(ctx, node, &rect);
	if (x < rect.x || y < rect.y || x >= rect.x + rect.w || y >= rect.y + rect.h) return XENT_NODE_INVALID;

	for (XentNodeId c = xent_get_first_child(ctx, node); c != XENT_NODE_INVALID; c = xent_get_next_sibling(ctx, c)) {
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
	XentNodeId hit = uia_deepest_at(self->info.ctx, self->info.root, lx, ly);
	*out           = uia_wrap(self, hit);
	return S_OK;
}

static XentNodeId uia_find_focused(FluxUia *self, XentNodeId node) {
	FluxNodeData *nd = self->info.store ? flux_node_store_get(self->info.store, node) : NULL;
	if (nd && nd->state.focused) return node;

	for (XentNodeId c = xent_get_first_child(self->info.ctx, node); c != XENT_NODE_INVALID;
	  c               = xent_get_next_sibling(self->info.ctx, c))
	{
		XentNodeId hit = uia_find_focused(self, c);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return XENT_NODE_INVALID;
}

static HRESULT STDMETHODCALLTYPE r_get_focus(IRawElementProviderFragmentRoot *p, IRawElementProviderFragment **out) {
	FluxUia *self = SELF_ROOT(p);
	if (!out) return E_POINTER;
	*out = uia_wrap(self, uia_find_focused(self, self->info.root));
	return S_OK;
}

static IRawElementProviderFragmentRootVtbl const g_root_vtbl = {
  r_qi,
  r_addref,
  r_release,
  r_from_point,
  r_get_focus,
};

/* ---- shared action helpers ---- */

static HRESULT uia_invoke_behavior(FluxUia *self) {
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || !nd->behavior.on_click) return UIA_E_INVALIDOPERATION;
	if (!xent_get_semantic_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;
	nd->behavior.on_click(nd->behavior.on_click_ctx);
	return S_OK;
}

static void uia_raise_self_event(FluxUia *self, int event_id) {
	if (UiaClientsAreListening())
		UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) &self->lpVtblSimple, event_id);
}

/* ---- IInvokeProvider ---- */

static HRESULT STDMETHODCALLTYPE inv_qi(IInvokeProvider *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_INVOKE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   inv_addref(IInvokeProvider *p) { return uia_addref_impl(SELF_INVOKE(p)); }

static ULONG STDMETHODCALLTYPE   inv_release(IInvokeProvider *p) { return uia_release_impl(SELF_INVOKE(p)); }

static HRESULT STDMETHODCALLTYPE inv_invoke(IInvokeProvider *p) {
	FluxUia *self = SELF_INVOKE(p);
	HRESULT  hr   = uia_invoke_behavior(self);
	if (SUCCEEDED(hr)) uia_raise_self_event(self, FLUX_UIA_EVENT_INVOKED);
	return hr;
}

static IInvokeProviderVtbl const g_invoke_vtbl = {inv_qi, inv_addref, inv_release, inv_invoke};

/* ---- IToggleProvider ---- */

static HRESULT STDMETHODCALLTYPE tog_qi(IToggleProvider *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_TOGGLE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   tog_addref(IToggleProvider *p) { return uia_addref_impl(SELF_TOGGLE(p)); }

static ULONG STDMETHODCALLTYPE   tog_release(IToggleProvider *p) { return uia_release_impl(SELF_TOGGLE(p)); }

static HRESULT STDMETHODCALLTYPE tog_toggle(IToggleProvider *p) { return uia_invoke_behavior(SELF_TOGGLE(p)); }

static HRESULT STDMETHODCALLTYPE tog_get_state(IToggleProvider *p, enum ToggleState *out) {
	FluxUia *self = SELF_TOGGLE(p);
	if (!out) return E_POINTER;
	uint8_t checked = xent_get_semantic_checked(self->info.ctx, self->node);
	*out            = checked == 2 ? ToggleState_Indeterminate : (checked ? ToggleState_On : ToggleState_Off);
	return S_OK;
}

static IToggleProviderVtbl const g_toggle_vtbl = {tog_qi, tog_addref, tog_release, tog_toggle, tog_get_state};

/* ---- IRangeValueProvider ---- */

static HRESULT STDMETHODCALLTYPE rng_qi(IRangeValueProvider *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_RANGE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE rng_addref(IRangeValueProvider *p) { return uia_addref_impl(SELF_RANGE(p)); }

static ULONG STDMETHODCALLTYPE rng_release(IRangeValueProvider *p) { return uia_release_impl(SELF_RANGE(p)); }

static bool                    uia_range_is_readonly(FluxUia *self) {
	FluxControlType t = flux_get_control_type(self->info.ctx, self->node);
	return t == FLUX_CONTROL_PROGRESS || t == FLUX_CONTROL_PROGRESS_RING;
}

static HRESULT STDMETHODCALLTYPE rng_set_value(IRangeValueProvider *p, double value) {
	FluxUia        *self = SELF_RANGE(p);
	FluxControlType t    = flux_get_control_type(self->info.ctx, self->node);
	if (!self->info.store || uia_range_is_readonly(self)) return UIA_E_INVALIDOPERATION;
	if (!xent_get_semantic_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;

	if (t == FLUX_CONTROL_NUMBER_BOX) flux_numberbox_set_value(self->info.store, self->node, value);
	else flux_slider_set_value(self->info.store, self->node, ( float ) value);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_value(IRangeValueProvider *p, double *out) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	float v = 0.0f, lo = 0.0f, hi = 0.0f;
	xent_get_semantic_value(self->info.ctx, self->node, &v, &lo, &hi);
	*out = v;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_readonly(IRangeValueProvider *p, BOOL *out) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	*out = uia_range_is_readonly(self) ? TRUE : FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_max(IRangeValueProvider *p, double *out) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	float v = 0.0f, lo = 0.0f, hi = 0.0f;
	xent_get_semantic_value(self->info.ctx, self->node, &v, &lo, &hi);
	*out = hi;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE rng_get_min(IRangeValueProvider *p, double *out) {
	FluxUia *self = SELF_RANGE(p);
	if (!out) return E_POINTER;
	float v = 0.0f, lo = 0.0f, hi = 0.0f;
	xent_get_semantic_value(self->info.ctx, self->node, &v, &lo, &hi);
	*out = lo;
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

/* ---- IValueProvider ---- */

static HRESULT STDMETHODCALLTYPE val_qi(IValueProvider *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_VALUE(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE val_addref(IValueProvider *p) { return uia_addref_impl(SELF_VALUE(p)); }

static ULONG STDMETHODCALLTYPE val_release(IValueProvider *p) { return uia_release_impl(SELF_VALUE(p)); }

static char const             *uia_text_content(FluxUia *self) {
	FluxNodeData *nd = uia_node_data(self);
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type == FLUX_CONTROL_PASSWORD_BOX) return ""; /* never expose secret text */
	FluxTextBoxData const *tb = ( FluxTextBoxData const * ) nd->component_data;
	return tb->content;
}

static HRESULT STDMETHODCALLTYPE val_set_value(IValueProvider *p, LPCWSTR value) {
	FluxUia *self = SELF_VALUE(p);
	if (!self->info.store) return UIA_E_INVALIDOPERATION;
	if (!xent_get_semantic_enabled(self->info.ctx, self->node)) return UIA_E_ELEMENTNOTENABLED;

	int   n    = WideCharToMultiByte(CP_UTF8, 0, value ? value : L"", -1, NULL, 0, NULL, NULL);
	char *utf8 = ( char * ) malloc(n > 0 ? ( size_t ) n : 1);
	if (!utf8) return E_OUTOFMEMORY;
	WideCharToMultiByte(CP_UTF8, 0, value ? value : L"", -1, utf8, n, NULL, NULL);

	if (flux_get_control_type(self->info.ctx, self->node) == FLUX_CONTROL_PASSWORD_BOX)
		flux_password_set_content(self->info.store, self->node, utf8);
	else flux_textbox_set_content(self->info.store, self->node, utf8);
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
	*out = xent_get_semantic_enabled(self->info.ctx, self->node) ? FALSE : TRUE;
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

/* ---- ISelectionItemProvider ---- */

static HRESULT STDMETHODCALLTYPE sel_qi(ISelectionItemProvider *p, REFIID riid, void **ppv) {
	return uia_qi_impl(SELF_SELECTION(p), riid, ppv);
}

static ULONG STDMETHODCALLTYPE   sel_addref(ISelectionItemProvider *p) { return uia_addref_impl(SELF_SELECTION(p)); }

static ULONG STDMETHODCALLTYPE   sel_release(ISelectionItemProvider *p) { return uia_release_impl(SELF_SELECTION(p)); }

static HRESULT STDMETHODCALLTYPE sel_select(ISelectionItemProvider *p) {
	return uia_invoke_behavior(SELF_SELECTION(p));
}

static HRESULT STDMETHODCALLTYPE sel_add(ISelectionItemProvider *p) { return uia_invoke_behavior(SELF_SELECTION(p)); }

static HRESULT STDMETHODCALLTYPE sel_remove(ISelectionItemProvider *p) {
	( void ) p;
	return UIA_E_INVALIDOPERATION; /* radio groups cannot clear the active item */
}

static HRESULT STDMETHODCALLTYPE sel_is_selected(ISelectionItemProvider *p, BOOL *out) {
	FluxUia *self = SELF_SELECTION(p);
	if (!out) return E_POINTER;
	*out = xent_get_semantic_selected(self->info.ctx, self->node) ? TRUE : FALSE;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE sel_container(ISelectionItemProvider *p, IRawElementProviderSimple **out) {
	( void ) p;
	if (!out) return E_POINTER;
	*out = NULL;
	return S_OK;
}

static ISelectionItemProviderVtbl const g_selection_vtbl = {
  sel_qi,
  sel_addref,
  sel_release,
  sel_select,
  sel_add,
  sel_remove,
  sel_is_selected,
  sel_container,
};

/* ---- lifetime + entry points ---- */

static FluxUia *uia_alloc(FluxUiaContext const *info, XentNodeId node) {
	FluxUia *p = ( FluxUia * ) calloc(1, sizeof(*p));
	if (!p) return NULL;
	p->lpVtblSimple    = &g_simple_vtbl;
	p->lpVtblFragment  = &g_fragment_vtbl;
	p->lpVtblRoot      = &g_root_vtbl;
	p->lpVtblInvoke    = &g_invoke_vtbl;
	p->lpVtblToggle    = &g_toggle_vtbl;
	p->lpVtblRange     = &g_range_vtbl;
	p->lpVtblValue     = &g_value_vtbl;
	p->lpVtblSelection = &g_selection_vtbl;
	p->ref             = 1;
	p->info            = *info;
	p->node            = node;
	return p;
}

void *flux_uia_provider_create(FluxUiaContext const *info) {
	if (!info || !info->ctx || info->root == XENT_NODE_INVALID) return NULL;
	FluxUia *p = uia_alloc(info, info->root);
	return p ? &p->lpVtblSimple : NULL;
}

void flux_uia_raise_focus_changed(FluxUiaContext const *info, XentNodeId node) {
	if (!info || !info->ctx || node == XENT_NODE_INVALID || !UiaClientsAreListening()) return;
	FluxUia *p = uia_alloc(info, node);
	if (!p) return;
	UiaRaiseAutomationEvent(( IRawElementProviderSimple * ) &p->lpVtblSimple, FLUX_UIA_EVENT_FOCUS_CHANGED);
	(( IUnknown * ) &p->lpVtblSimple)->lpVtbl->Release(( IUnknown * ) &p->lpVtblSimple);
}
