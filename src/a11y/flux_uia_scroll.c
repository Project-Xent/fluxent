#include "a11y/flux_uia_object.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/factory/flux_init_container.h"
#include "controls/flux_module.h"
#include "render/flux_scroll_geom.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_popup.h>

#include <math.h>

#define UIA_NO_SCROLL (-1.0)

typedef struct UiaScrollView {
	XentNodeId      node;
	FluxScrollData *sd;
	XentRect        rect;
	float           max_x;
	float           max_y;
} UiaScrollView;

static bool uia_scroll_view(FluxUiaContext const *info, XentNodeId node, UiaScrollView *out) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	if (h && h->scroller) node = h->scroller(info->store, node);
	FluxNodeData *nd
	  = flux_node_type(info->store, node) == FLUX_CONTROL_SCROLL ? flux_store_find(info->store, node) : NULL;
	out->node = node;
	out->sd   = nd ? ( FluxScrollData * ) nd->component_data : NULL;
	if (!out->sd || !xent_layout_rect(info->ctx, node, &out->rect)) return false;
	out->max_x = fmaxf(out->sd->content_w - out->rect.w, 0.0f);
	out->max_y = fmaxf(out->sd->content_h - out->rect.h, 0.0f);
	return true;
}

static void uia_redraw_scroll_popup(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId parent = xent_node_parent(info->ctx, node);
	while (parent != XENT_NODE_INVALID) {
		node   = parent;
		parent = xent_node_parent(info->ctx, node);
	}
	FluxNodeData const *root = flux_store_find(info->store, node);
	HWND                hwnd = root && root->popup ? flux_popup_hwnd(root->popup) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void uia_scroll_apply(FluxUiaContext const *info, UiaScrollView const *sv, float x, float y) {
	if (sv->sd->reports) flux_scroll_request(info->store, sv->node, x, y);
	else {
		sv->sd->scroll_x = x;
		sv->sd->scroll_y = y;
		flux_scroll_offset_changed(sv->sd);
	}
	uia_redraw_scroll_popup(info, sv->node);
}

static float uia_scroll_step(enum ScrollAmount amount, float view) {
	switch (amount) {
	case ScrollAmount_LargeDecrement : return -view;
	case ScrollAmount_SmallDecrement : return -FLUX_SCROLLBAR_SMALL_CHANGE;
	case ScrollAmount_LargeIncrement : return view;
	case ScrollAmount_SmallIncrement : return FLUX_SCROLLBAR_SMALL_CHANGE;
	default                          : return 0.0f;
	}
}

static FluxUiaHooks const *uia_scroll_hooks(FluxUia const *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	return h && h->scroll_info ? h : NULL;
}

static int uia_item_step(enum ScrollAmount amount) {
	switch (amount) {
	case ScrollAmount_LargeDecrement : return -5;
	case ScrollAmount_SmallDecrement : return -1;
	case ScrollAmount_LargeIncrement : return 5;
	case ScrollAmount_SmallIncrement : return 1;
	default                          : return 0;
	}
}

static double uia_items_read(FluxUia const *self, FluxUiaHooks const *h, int what) {
	double percent = 0.0, view = 100.0;
	h->scroll_info(self->info.store, self->node, &percent, &view);
	if (what & 1) return what & 2 ? view : percent;
	return what & 2 ? 100.0 : UIA_NO_SCROLL;
}

static float                     uia_clamp(float v, float max) { return v < 0.0f ? 0.0f : (v > max ? max : v); }

static HRESULT STDMETHODCALLTYPE scr_qi(IScrollProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblScroll), riid, ppv);
}

static ULONG STDMETHODCALLTYPE scr_addref(IScrollProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblScroll));
}

static ULONG STDMETHODCALLTYPE scr_release(IScrollProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblScroll));
}

static HRESULT STDMETHODCALLTYPE scr_scroll(IScrollProvider *p, enum ScrollAmount h, enum ScrollAmount v) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblScroll);
	flux_uia_dirty_node(&self->info, self->node, FLUX_UIA_DIRTY_PATTERN);
	FluxUiaHooks const *hk = uia_scroll_hooks(self);
	UiaScrollView       sv;
	if (hk && hk->scroll_by) hk->scroll_by(self->info.store, self->node, uia_item_step(v));
	if (hk) return S_OK;
	if (!uia_scroll_view(&self->info, self->node, &sv)) return UIA_E_ELEMENTNOTAVAILABLE;
	if ((h != ScrollAmount_NoAmount && sv.max_x <= 0.0f) || (v != ScrollAmount_NoAmount && sv.max_y <= 0.0f))
		return UIA_E_INVALIDOPERATION;
	float x = uia_clamp(sv.sd->scroll_x + uia_scroll_step(h, sv.rect.w), sv.max_x);
	float y = uia_clamp(sv.sd->scroll_y + uia_scroll_step(v, sv.rect.h), sv.max_y);
	uia_scroll_apply(&self->info, &sv, x, y);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE scr_set_percent(IScrollProvider *p, double h, double v) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblScroll);
	flux_uia_dirty_node(&self->info, self->node, FLUX_UIA_DIRTY_PATTERN);
	FluxUiaHooks const *hk = uia_scroll_hooks(self);
	UiaScrollView       sv;
	if (hk) return hk->scroll_set && hk->scroll_set(self->info.store, self->node, v) ? S_OK : UIA_E_INVALIDOPERATION;
	if (!uia_scroll_view(&self->info, self->node, &sv)) return UIA_E_ELEMENTNOTAVAILABLE;
	if ((h != UIA_NO_SCROLL && (h < 0.0 || h > 100.0)) || (v != UIA_NO_SCROLL && (v < 0.0 || v > 100.0)))
		return E_INVALIDARG;
	float x = h == UIA_NO_SCROLL ? sv.sd->scroll_x : ( float ) (h / 100.0) * sv.max_x;
	float y = v == UIA_NO_SCROLL ? sv.sd->scroll_y : ( float ) (v / 100.0) * sv.max_y;
	uia_scroll_apply(&self->info, &sv, x, y);
	return S_OK;
}

static double uia_view_read(UiaScrollView const *sv, int what) {
	float max  = what & 1 ? sv->max_y : sv->max_x;
	float at   = what & 1 ? sv->sd->scroll_y : sv->sd->scroll_x;
	float view = what & 1 ? sv->rect.h : sv->rect.w;
	float all  = what & 1 ? sv->sd->content_h : sv->sd->content_w;
	if (what & 2) return all > view ? 100.0 * view / all : 100.0;
	return max > 0.0f ? 100.0 * at / max : UIA_NO_SCROLL;
}

static HRESULT uia_scroll_read(IScrollProvider *p, double *out, int what) {
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblScroll);
	FluxUiaHooks const *hk   = uia_scroll_hooks(self);
	UiaScrollView       sv;
	if (!out) return E_POINTER;
	if (hk) *out = uia_items_read(self, hk, what);
	if (hk) return S_OK;
	if (!uia_scroll_view(&self->info, self->node, &sv)) return UIA_E_ELEMENTNOTAVAILABLE;
	*out = uia_view_read(&sv, what);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE scr_get_hpercent(IScrollProvider *p, double *out) {
	return uia_scroll_read(p, out, 0);
}

static HRESULT STDMETHODCALLTYPE scr_get_vpercent(IScrollProvider *p, double *out) {
	return uia_scroll_read(p, out, 1);
}

static HRESULT STDMETHODCALLTYPE scr_get_hview(IScrollProvider *p, double *out) { return uia_scroll_read(p, out, 2); }

static HRESULT STDMETHODCALLTYPE scr_get_vview(IScrollProvider *p, double *out) { return uia_scroll_read(p, out, 3); }

static HRESULT                   uia_scrollable(IScrollProvider *p, BOOL *out, bool vertical) {
	FluxUia            *self = FLUX_UIA_SELF(p, lpVtblScroll);
	FluxUiaHooks const *hk   = uia_scroll_hooks(self);
	UiaScrollView       sv;
	double              percent = 0.0, view = 0.0;
	if (!out) return E_POINTER;
	if (hk) *out = vertical && hk->scroll_info(self->info.store, self->node, &percent, &view);
	if (hk) return S_OK;
	*out = uia_scroll_view(&self->info, self->node, &sv) && (vertical ? sv.max_y : sv.max_x) > 0.0f;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE scr_get_hscrollable(IScrollProvider *p, BOOL *out) {
	return uia_scrollable(p, out, false);
}

static HRESULT STDMETHODCALLTYPE scr_get_vscrollable(IScrollProvider *p, BOOL *out) {
	return uia_scrollable(p, out, true);
}

IScrollProviderVtbl const flux_uia_scroll_vtbl = {
  scr_qi,
  scr_addref,
  scr_release,
  scr_scroll,
  scr_set_percent,
  scr_get_hpercent,
  scr_get_vpercent,
  scr_get_hview,
  scr_get_vview,
  scr_get_hscrollable,
  scr_get_vscrollable,
};

static float uia_into_view(float at, float size, float scroll, float view, float max) {
	if (at < scroll) return uia_clamp(at, max);
	if (at + size > scroll + view) return uia_clamp(at + size - view, max);
	return scroll;
}

static HRESULT STDMETHODCALLTYPE sci_qi(IScrollItemProvider *p, REFIID riid, void **ppv) {
	return flux_uia_qi(FLUX_UIA_SELF(p, lpVtblScrollItem), riid, ppv);
}

static ULONG STDMETHODCALLTYPE sci_addref(IScrollItemProvider *p) {
	return flux_uia_addref(FLUX_UIA_SELF(p, lpVtblScrollItem));
}

static ULONG STDMETHODCALLTYPE sci_release(IScrollItemProvider *p) {
	return flux_uia_release(FLUX_UIA_SELF(p, lpVtblScrollItem));
}

static HRESULT uia_sub_scroll(FluxUia *self) {
	FluxUiaHooks const *h = flux_module_uia(self->info.store, self->node);
	if (!h || !h->sub || !h->sub->scroll) return UIA_E_INVALIDOPERATION;
	h->sub->scroll(self->info.store, self->node, self->sub);
	return S_OK;
}

void flux_uia_bring_into_view(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId    host = xent_node_parent(info->ctx, node);
	UiaScrollView sv;
	while (host != XENT_NODE_INVALID && !uia_scroll_view(info, host, &sv)) host = xent_node_parent(info->ctx, host);
	XentRect r = {0};
	if (host == XENT_NODE_INVALID || !xent_layout_rect(info->ctx, node, &r)) return;
	float x = flux_scroll_start(sv.sd, &r, &sv.rect) + sv.sd->origin_x;
	float y = r.y - sv.rect.y + sv.sd->origin_y;
	uia_scroll_apply(
	  info, &sv, uia_into_view(x, r.w, sv.sd->scroll_x, sv.rect.w, sv.max_x),
	  uia_into_view(y, r.h, sv.sd->scroll_y, sv.rect.h, sv.max_y)
	);
}

static HRESULT STDMETHODCALLTYPE sci_scroll_into_view(IScrollItemProvider *p) {
	FluxUia *self = FLUX_UIA_SELF(p, lpVtblScrollItem);
	flux_uia_dirty_node(&self->info, self->node, FLUX_UIA_DIRTY_PATTERN);
	if (self->sub >= 0) return uia_sub_scroll(self);
	flux_uia_bring_into_view(&self->info, self->node);
	return S_OK;
}

IScrollItemProviderVtbl const flux_uia_scrollitem_vtbl = {
  sci_qi,
  sci_addref,
  sci_release,
  sci_scroll_into_view,
};
