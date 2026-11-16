#include "shell/flux_dragdrop.h"
#include "shell/flux_data_internal.h"
#include "bridge/flux_internal.h"
#include "controls/data/flux_scroll_data.h"
#include "input/flux_input_internal.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

#define DND_SCROLL_MARGIN 32.0f
#define DND_SCROLL_STEP   16.0f

typedef struct DropTarget {
	IDropTargetVtbl const *lpVtbl;
	FluxDragDrop          *owner;
} DropTarget;

struct FluxDragDrop {
	DropTarget         target;
	FluxDragDropEnv    env;
	HWND               hwnd;
	IDropTargetHelper *helper;
	FluxData          *data;
	XentNodeId         hover;
	bool               registered;
	bool               disposed;
	ULONG              refs;
};

static FluxData const  *g_delivering;

FluxData const         *flux_drag_data(void) { return g_delivering; }

static DWORD const      kEffects [] = {DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_MOVE, DROPEFFECT_LINK};

static XtkDropOperation operation_of(DWORD effect) {
	if (effect & DROPEFFECT_MOVE) return XTK_DROP_MOVE;
	if (effect & DROPEFFECT_COPY) return XTK_DROP_COPY;
	if (effect & DROPEFFECT_LINK) return XTK_DROP_LINK;
	return XTK_DROP_NONE;
}

static FluxPoint client_dips(FluxDragDrop const *dd, POINTL pt) {
	POINT p = {pt.x, pt.y};
	ScreenToClient(dd->hwnd, &p);
	float scale = flux_window_dpi(dd->env.window).dpi_x / FLUX_DPI_BASE;
	if (scale <= 0.0f) scale = 1.0f;
	return (FluxPoint) {( float ) p.x / scale, ( float ) p.y / scale};
}

static bool node_alive(FluxDragDrop const *dd, XentNodeId node) {
	return !dd->disposed && node != XENT_NODE_INVALID && xent_node_valid(flux_store_ctx(dd->env.store), node);
}

static uint8_t drop_operation(FluxDragDrop const *dd, XentNodeId node) {
	FluxNodeData const *nd = flux_store_find(dd->env.store, node);
	return nd ? nd->drop_operation : XTK_DROP_NONE;
}

static XentNodeId drop_target(FluxDragDrop const *dd, XentNodeId node) {
	XentCtx *ctx = flux_store_ctx(dd->env.store);
	for (; node != XENT_NODE_INVALID; node = xent_node_parent(ctx, node))
		if (drop_operation(dd, node) != XTK_DROP_NONE) return node;
	return XENT_NODE_INVALID;
}

static void route(FluxDragDrop const *dd, XentNodeId node, XtkEventId id, XtkEventPayload payload) {
	if (dd->disposed) return;
	XtkHost *host = flux_store_host(dd->env.store);
	if (!host || !node_alive(dd, node)) return;
	FluxData const *previous = g_delivering;
	g_delivering             = dd->data;
	flux_input_route_event(host, node, id, payload, XTK_EVENT_CAUSE_USER);
	g_delivering = previous;
}

static void set_hover(FluxDragDrop *dd, XentNodeId node) {
	if (node == dd->hover) return;
	XentNodeId old = dd->hover;
	dd->hover      = node;
	route(dd, old, XTK_EVENT_DRAG_HOVER, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = false});
	route(dd, node, XTK_EVENT_DRAG_HOVER, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = true});
}

static float scroll_step(float at, float start, float extent) {
	if (at < start + DND_SCROLL_MARGIN) return -DND_SCROLL_STEP;
	if (at > start + extent - DND_SCROLL_MARGIN) return DND_SCROLL_STEP;
	return 0.0f;
}

static void autoscroll(FluxDragDrop const *dd, XentNodeId node, FluxPoint at) {
	XentNodeId      scroll = input_scrollable_ancestor(dd->env.input, node);
	FluxNodeData   *nd     = scroll == XENT_NODE_INVALID ? NULL : flux_store_find(dd->env.store, scroll);
	FluxScrollData *sd     = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	FluxRect        r      = {0};
	if (!sd) return;
	input_node_abs_rect(dd->env.input, scroll, &r);
	float x = fminf(fmaxf(sd->scroll_x + scroll_step(at.x, r.x, r.w), 0.0f), fmaxf(sd->content_w - r.w, 0.0f));
	float y = fminf(fmaxf(sd->scroll_y + scroll_step(at.y, r.y, r.h), 0.0f), fmaxf(sd->content_h - r.h, 0.0f));
	if (x == sd->scroll_x && y == sd->scroll_y) return;
	sd->scroll_x   = x;
	sd->scroll_y   = y;
	sd->user_moved = 1;
	flux_scroll_offset_changed(sd);
}

static XentNodeId track(FluxDragDrop *dd, POINTL pt, DWORD *effect) {
	if (dd->disposed || !dd->data) {
		*effect = DROPEFFECT_NONE;
		return XENT_NODE_INVALID;
	}
	FluxPoint     at        = client_dips(dd, pt);
	FluxHitResult hit       = flux_input_hittest(dd->env.input, dd->env.root, at.x, at.y);
	XentNodeId    target    = drop_target(dd, hit.node);
	FORMATETC     fmt       = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	IDataObject  *obj       = flux_data_object(dd->data);
	bool          supported = SUCCEEDED(IDataObject_QueryGetData(obj, &fmt));
	fmt.cfFormat            = CF_UNICODETEXT;
	supported               = supported || SUCCEEDED(IDataObject_QueryGetData(obj, &fmt));
	if (!supported) target = XENT_NODE_INVALID;
	DWORD want = target == XENT_NODE_INVALID ? DROPEFFECT_NONE : kEffects [drop_operation(dd, target)];
	*effect    = *effect & want ? want : DROPEFFECT_NONE;
	set_hover(dd, *effect ? target : XENT_NODE_INVALID);
	autoscroll(dd, hit.node, at);
	if (!dd->disposed) flux_window_redraw(dd->env.window);
	return *effect ? target : XENT_NODE_INVALID;
}

static void end_session(FluxDragDrop *dd) {
	set_hover(dd, XENT_NODE_INVALID);
	flux_data_destroy(dd->data);
	dd->data = NULL;
	if (!dd->disposed) flux_window_redraw(dd->env.window);
}

static XtkEventPayload string_payload(char const *s) {
	return (XtkEventPayload) {
	  .kind = XTK_EVENT_PAYLOAD_STRING, .u.string = {s, ( uint32_t ) strlen(s)}
    };
}

static bool deliver(FluxDragDrop *dd, XentNodeId target) {
	char *files = flux_data_files(dd->data);
	char *text  = files ? NULL : flux_data_text(dd->data);
	if (files) route(dd, target, XTK_EVENT_DROP_FILES, string_payload(files));
	else if (text) route(dd, target, XTK_EVENT_DROP_TEXT, string_payload(text));
	bool delivered = files || text;
	free(files);
	free(text);
	return delivered;
}

static FluxDragDrop             *owner_of(IDropTarget *self) { return (( DropTarget * ) self)->owner; }

static HRESULT STDMETHODCALLTYPE target_query(IDropTarget *self, REFIID riid, void **out) {
	if (!out) return E_POINTER;
	*out = NULL;
	if (!IsEqualIID(riid, &IID_IUnknown) && !IsEqualIID(riid, &IID_IDropTarget)) return E_NOINTERFACE;
	*out = self;
	IDropTarget_AddRef(self);
	return S_OK;
}

static ULONG STDMETHODCALLTYPE target_addref(IDropTarget *self) { return ++owner_of(self)->refs; }

static ULONG STDMETHODCALLTYPE target_release(IDropTarget *self) {
	FluxDragDrop *dd   = owner_of(self);
	ULONG         refs = --dd->refs;
	if (!refs) free(dd);
	return refs;
}

static HRESULT STDMETHODCALLTYPE target_enter(
  IDropTarget *self, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect
) {
	( void ) keys;
	FluxDragDrop *dd = owner_of(self);
	if (dd->disposed) {
		*effect = DROPEFFECT_NONE;
		return S_OK;
	}
	IDataObject_AddRef(obj);
	flux_data_destroy(dd->data);
	dd->data = flux_data_adopt(obj);
	( void ) track(dd, pt, effect);
	if (dd->helper) IDropTargetHelper_DragEnter(dd->helper, dd->hwnd, obj, ( POINT * ) &pt, *effect);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE target_over(IDropTarget *self, DWORD keys, POINTL pt, DWORD *effect) {
	( void ) keys;
	FluxDragDrop *dd = owner_of(self);
	( void ) track(dd, pt, effect);
	if (dd->helper) IDropTargetHelper_DragOver(dd->helper, ( POINT * ) &pt, *effect);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE target_leave(IDropTarget *self) {
	FluxDragDrop *dd = owner_of(self);
	if (dd->helper) IDropTargetHelper_DragLeave(dd->helper);
	end_session(dd);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE target_drop(
  IDropTarget *self, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect
) {
	( void ) keys;
	FluxDragDrop *dd     = owner_of(self);
	XentNodeId    target = track(dd, pt, effect);
	if (dd->helper) IDropTargetHelper_Drop(dd->helper, obj, ( POINT * ) &pt, *effect);
	if (target != XENT_NODE_INVALID && !deliver(dd, target)) *effect = DROPEFFECT_NONE;
	end_session(dd);
	return S_OK;
}

static IDropTargetVtbl const kTargetVtbl = {
  target_query,
  target_addref,
  target_release,
  target_enter,
  target_over,
  target_leave,
  target_drop,
};

static FluxData *source_data(FluxNodeData const *nd) {
	FluxData *data = flux_data_create();
	if (!data) return NULL;
	if ((nd->drag_files && !flux_data_setfiles(data, nd->drag_files))
	    || (nd->drag_text && !flux_data_settext(data, nd->drag_text, strlen(nd->drag_text))))
	{
		flux_data_destroy(data);
		return NULL;
	}
	return data;
}

static void drag_start(void *ctx, XentNodeId source) {
	FluxDragDrop *dd  = ctx;
	FluxNodeData *nd  = node_alive(dd, source) ? flux_store_find(dd->env.store, source) : NULL;
	FluxData     *src = nd && GetKeyState(VK_LBUTTON) < 0 ? source_data(nd) : NULL;
	if (!src) return;
	IDropTarget_AddRef(( IDropTarget * ) &dd->target);
	ReleaseCapture();
	DWORD   effect = DROPEFFECT_NONE;
	HRESULT hr     = SHDoDragDrop(
	  dd->hwnd, flux_data_object(src), NULL, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect
	);
	if (!dd->disposed && (hr == DRAGDROP_S_DROP || hr == DRAGDROP_S_CANCEL)) {
		XtkEventPayload done = {
		  .kind  = XTK_EVENT_PAYLOAD_I32,
		  .u.i32 = hr == DRAGDROP_S_DROP ? ( int32_t ) operation_of(effect) : XTK_DROP_NONE};
		route(dd, source, XTK_EVENT_DRAG_COMPLETED, done);
	}
	flux_data_destroy(src);
	IDropTarget_Release(( IDropTarget * ) &dd->target);
}

FluxDragDrop *flux_dragdrop_create(FluxDragDropEnv const *env) {
	HWND          hwnd = env && env->window ? flux_window_hwnd(env->window) : NULL;
	FluxDragDrop *dd   = hwnd ? ( FluxDragDrop * ) calloc(1, sizeof(*dd)) : NULL;
	if (!dd) return NULL;
	dd->refs   = 1;
	dd->target = (DropTarget) {&kTargetVtbl, dd};
	dd->hover  = XENT_NODE_INVALID;
	dd->env    = *env;
	dd->hwnd   = hwnd;
	if (FAILED(CoCreateInstance(
		  &CLSID_DragDropHelper, NULL, CLSCTX_INPROC_SERVER, &IID_IDropTargetHelper, ( void ** ) &dd->helper
		)))
		dd->helper = NULL;
	dd->registered = SUCCEEDED(RegisterDragDrop(hwnd, ( IDropTarget * ) &dd->target));
	flux_input_ondragstart(env->input, drag_start, dd);
	return dd;
}

void flux_dragdrop_destroy(FluxDragDrop *dd) {
	if (!dd) return;
	dd->disposed = true;
	flux_input_ondragstart(dd->env.input, NULL, NULL);
	if (dd->registered) RevokeDragDrop(dd->hwnd);
	if (dd->helper) IDropTargetHelper_Release(dd->helper);
	dd->helper = NULL;
	flux_data_destroy(dd->data);
	dd->data = NULL;
	IDropTarget_Release(( IDropTarget * ) &dd->target);
}
