#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_compose.h"
#include "compose/flux_interop.h"

#include <cwinrt/init.h>
#include <cwinrt/cast.h>
#include <cwinrt/bootstrap.h>

typedef enum DISPATCHERQUEUE_THREAD_TYPE
{
	DQTYPE_THREAD_DEDICATED = 1,
	DQTYPE_THREAD_CURRENT   = 2,
} DISPATCHERQUEUE_THREAD_TYPE;

typedef enum DISPATCHERQUEUE_THREAD_APARTMENTTYPE
{
	DQTAT_COM_NONE = 0,
	DQTAT_COM_ASTA = 1,
	DQTAT_COM_STA  = 2,
} DISPATCHERQUEUE_THREAD_APARTMENTTYPE;

typedef struct DispatcherQueueOptions {
	DWORD                                dwSize;
	DISPATCHERQUEUE_THREAD_TYPE          threadType;
	DISPATCHERQUEUE_THREAD_APARTMENTTYPE apartmentType;
} DispatcherQueueOptions;

typedef IUnknown *PDISPATCHERQUEUECONTROLLER;
HRESULT WINAPI CreateDispatcherQueueController(DispatcherQueueOptions options, PDISPATCHERQUEUECONTROLLER *controller);

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct FluxCompositor {
	IUnknown *dispatcher;
	WUC_Comp *comp;
	uint32_t  refcount;
	bool      winrt_owned;
};

struct FluxWindowTarget {
	FluxCompositor                          *owner;
	WUC_CompositionTarget                   *target;
	WUC_Sprite                              *root;
	WUC_Visual                              *root_visual;
	WUC_CompositionSurfaceBrush             *brush;
	WUC_Sprite                              *content;
	WUC_Visual                              *content_visual;
	WUC_CompositionSurfaceBrush             *content_brush;
	WUC_Brush                               *backdrop;
	WUC_Sprite                              *plate;
	WUC_Visual                              *plate_visual;
	WUC_CompositionRoundedRectangleGeometry *plate_geom;
};

WUC_Visual *flux_compose_as_visual(void *obj) {
	WUC_Visual *v = NULL;
	if (obj) ( void ) cwinrt_query(obj, &CWINRT_IID_WUC_IVisual, ( void ** ) &v);
	return v;
}

static FluxCompositor g_compositor;

static bool           compositor_init(FluxCompositor *c) {
	DispatcherQueueOptions dqopts;
	dqopts.dwSize        = sizeof(dqopts);
	dqopts.threadType    = DQTYPE_THREAD_CURRENT;
	dqopts.apartmentType = DQTAT_COM_ASTA;
	if (FAILED(CreateDispatcherQueueController(dqopts, ( PDISPATCHERQUEUECONTROLLER * ) &c->dispatcher))) return false;

	HRESULT hr = cwinrt_init(RO_INIT_SINGLETHREADED);
	if (hr != S_OK && hr != S_FALSE) return false;
	c->winrt_owned = false;

	if (FAILED(wuc_comp_new(&c->comp))) return false;
	return true;
}

static void compositor_teardown(FluxCompositor *c) {
	if (c->comp) (( IUnknown * ) c->comp)->lpVtbl->Release(( IUnknown * ) c->comp);
	if (c->dispatcher) c->dispatcher->lpVtbl->Release(c->dispatcher);
	c->comp       = NULL;
	c->dispatcher = NULL;
	if (c->winrt_owned) cwinrt_uninit();
	c->winrt_owned = false;
}

FluxCompositor *flux_compositor_acquire(void) {
	if (g_compositor.refcount > 0) {
		g_compositor.refcount++;
		return &g_compositor;
	}
	if (!compositor_init(&g_compositor)) {
		compositor_teardown(&g_compositor);
		return NULL;
	}
	g_compositor.refcount = 1;
	return &g_compositor;
}

void flux_compositor_release(FluxCompositor *c) {
	if (!c || c->refcount == 0) return;
	if (--c->refcount == 0) compositor_teardown(c);
}

WUC_Comp                      *flux_compositor_comp(FluxCompositor *c) { return c ? c->comp : NULL; }

WUC_CompositionGraphicsDevice *flux_compositor_dev_create(FluxCompositor *c, IUnknown *d2d_device) {
	if (!c || !c->comp || !d2d_device) return NULL;

	FluxICompositorInterop *interop = NULL;
	if (FAILED(cwinrt_query(c->comp, &FluxIID_ICompositorInterop, ( void ** ) &interop))) return NULL;

	void   *device = NULL;
	HRESULT hr     = interop->lpVtbl->CreateGraphicsDevice(interop, d2d_device, &device);
	interop->lpVtbl->Release(interop);
	if (FAILED(hr)) return NULL;

	return ( WUC_CompositionGraphicsDevice * ) device;
}

void flux_compositor_dev_destroy(WUC_CompositionGraphicsDevice *device) {
	if (device) (( IUnknown * ) device)->lpVtbl->Release(( IUnknown * ) device);
}

FluxWindowTarget *flux_window_target_create(FluxCompositor *c, HWND hwnd) {
	if (!c || !c->comp || !hwnd) return NULL;

	FluxWindowTarget *t = ( FluxWindowTarget * ) calloc(1, sizeof(*t));
	if (!t) return NULL;
	t->owner                               = c;

	FluxICompositorDesktopInterop *desktop = NULL;
	if (FAILED(cwinrt_query(c->comp, &FluxIID_ICompositorDesktopInterop, ( void ** ) &desktop))) goto fail;

	void   *target_unk = NULL;
	HRESULT hr         = desktop->lpVtbl->CreateDesktopWindowTarget(desktop, hwnd, FALSE, &target_unk);
	desktop->lpVtbl->Release(desktop);
	if (FAILED(hr)) goto fail;

	hr = cwinrt_query(target_unk, &CWINRT_IID_WUC_ICompositionTarget, ( void ** ) &t->target);
	(( IUnknown * ) target_unk)->lpVtbl->Release(( IUnknown * ) target_unk);
	if (FAILED(hr)) goto fail;

	if (FAILED(wuc_comp_create_sprite_visual(c->comp, &t->root))) goto fail;
	t->root_visual = flux_compose_as_visual(t->root);
	if (!t->root_visual) goto fail;
	if (FAILED(wuc_composition_target_put__root(t->target, t->root_visual))) goto fail;

	return t;

fail:
	flux_window_target_destroy(t);
	return NULL;
}

static void compose_release(void *p) {
	if (p) (( IUnknown * ) p)->lpVtbl->Release(( IUnknown * ) p);
}

void flux_window_target_destroy(FluxWindowTarget *t) {
	if (!t) return;
	compose_release(t->backdrop);
	compose_release(t->plate_geom);
	compose_release(t->plate_visual);
	compose_release(t->plate);
	compose_release(t->content_brush);
	compose_release(t->content_visual);
	compose_release(t->content);
	compose_release(t->brush);
	compose_release(t->root_visual);
	compose_release(t->root);
	compose_release(t->target);
	free(t);
}

static WUC_CompositionSurfaceBrush *swapchain_surface_brush(FluxWindowTarget *t, IUnknown *swapchain) {
	FluxICompositorInterop *interop = NULL;
	if (FAILED(cwinrt_query(t->owner->comp, &FluxIID_ICompositorInterop, ( void ** ) &interop))) return NULL;

	void   *surface = NULL;
	HRESULT hr      = interop->lpVtbl->CreateCompositionSurfaceForSwapChain(interop, swapchain, &surface);
	interop->lpVtbl->Release(interop);
	if (FAILED(hr)) return NULL;

	WUC_CompositionSurfaceBrush *brush = NULL;
	hr = wuc_comp_create_surface_brush_p_p(t->owner->comp, ( WUC_ICompositionSurface * ) surface, &brush);
	(( IUnknown * ) surface)->lpVtbl->Release(( IUnknown * ) surface);
	if (FAILED(hr)) return NULL;

	( void ) wuc_composition_surface_brush_put__stretch(brush, WUC_CompositionStretch_None);
	( void ) wuc_composition_surface_brush_put__horizontal_alignment_ratio(brush, 0.0f);
	( void ) wuc_composition_surface_brush_put__vertical_alignment_ratio(brush, 0.0f);
	return brush;
}

HRESULT flux_wintarget_setswapchain(FluxWindowTarget *t, IUnknown *swapchain) {
	if (!t || !t->owner || !t->owner->comp || !swapchain) return E_INVALIDARG;

	WUC_CompositionSurfaceBrush *brush = swapchain_surface_brush(t, swapchain);
	if (!brush) return E_FAIL;

	HRESULT hr = wuc_sprite_put__brush(t->root, ( WUC_Brush * ) brush);
	if (FAILED(hr)) {
		(( IUnknown * ) brush)->lpVtbl->Release(( IUnknown * ) brush);
		return hr;
	}

	if (t->brush) (( IUnknown * ) t->brush)->lpVtbl->Release(( IUnknown * ) t->brush);
	t->brush = brush;
	return S_OK;
}

static void swapchain_attach_content_to_root(FluxWindowTarget *t) {
	WUC_Container *root_container = NULL;
	if (FAILED(cwinrt_query(t->root, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &root_container))) return;

	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(root_container, &children))) {
		( void ) wuc_visual_collection_insert_at_top(children, t->content_visual);
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
	(( IUnknown * ) root_container)->lpVtbl->Release(( IUnknown * ) root_container);
}

static HRESULT swapchain_ensure_content(FluxWindowTarget *t) {
	HRESULT hr = wuc_comp_create_sprite_visual(t->owner->comp, &t->content);
	if (FAILED(hr)) return hr;
	t->content_visual = flux_compose_as_visual(t->content);
	if (!t->content_visual) return E_NOINTERFACE;

	WFN_Vector_2 fill = {1.0f, 1.0f};
	( void ) wuc_visual_put__relative_size_adjustment(t->content_visual, fill);
	swapchain_attach_content_to_root(t);
	return S_OK;
}

HRESULT flux_wintarget_presentchild(FluxWindowTarget *t, IUnknown *swapchain) {
	if (!t || !t->owner || !t->owner->comp || !swapchain) return E_INVALIDARG;

	WUC_CompositionSurfaceBrush *brush = swapchain_surface_brush(t, swapchain);
	if (!brush) return E_FAIL;

	HRESULT hr = S_OK;
	if (!t->content) {
		hr = swapchain_ensure_content(t);
		if (FAILED(hr)) goto fail;
	}

	hr = wuc_sprite_put__brush(t->content, ( WUC_Brush * ) brush);
	if (FAILED(hr)) goto fail;

	if (t->content_brush) (( IUnknown * ) t->content_brush)->lpVtbl->Release(( IUnknown * ) t->content_brush);
	t->content_brush = brush;
	return S_OK;

fail:
	(( IUnknown * ) brush)->lpVtbl->Release(( IUnknown * ) brush);
	return hr;
}

void flux_wintarget_setbackdrop(FluxWindowTarget *t, WUC_Brush *brush) {
	if (!t || !t->root) return;
	( void ) wuc_sprite_put__brush(t->root, brush);
	if (brush) (( IUnknown * ) brush)->lpVtbl->AddRef(( IUnknown * ) brush);
	if (t->backdrop) (( IUnknown * ) t->backdrop)->lpVtbl->Release(( IUnknown * ) t->backdrop);
	t->backdrop = brush;
}

static bool plate_ensure(FluxWindowTarget *t) {
	if (t->plate) return true;
	WUC_Comp *comp = t->owner->comp;
	if (FAILED(wuc_comp_create_sprite_visual(comp, &t->plate))) return false;
	t->plate_visual = flux_compose_as_visual(t->plate);
	if (!t->plate_visual || FAILED(wuc_comp_create_rounded_rectangle_geometry(comp, &t->plate_geom))) return false;
	WUC_CompositionGeometry      *geom = NULL;
	WUC_CompositionGeometricClip *gc   = NULL;
	WUC_CompositionClip          *clip = NULL;
	if (SUCCEEDED(cwinrt_query(t->plate_geom, &CWINRT_IID_WUC_ICompositionGeometry, ( void ** ) &geom))
	    && SUCCEEDED(wuc_comp_create_geometric_clip_p_p(comp, geom, &gc))
	    && SUCCEEDED(cwinrt_query(gc, &CWINRT_IID_WUC_ICompositionClip, ( void ** ) &clip)))
		( void ) wuc_visual_put__clip(t->plate_visual, clip);
	if (clip) (( IUnknown * ) clip)->lpVtbl->Release(( IUnknown * ) clip);
	if (gc) (( IUnknown * ) gc)->lpVtbl->Release(( IUnknown * ) gc);
	if (geom) (( IUnknown * ) geom)->lpVtbl->Release(( IUnknown * ) geom);

	WUC_Container *root_container = NULL;
	if (FAILED(cwinrt_query(t->root, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &root_container))) return false;
	WUC_VisualCollection *children = NULL;
	if (SUCCEEDED(wuc_container_get__children(root_container, &children))) {
		( void ) wuc_visual_collection_insert_at_bottom(children, t->plate_visual);
		(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
	}
	(( IUnknown * ) root_container)->lpVtbl->Release(( IUnknown * ) root_container);
	return true;
}

void flux_wintarget_setopacity(FluxWindowTarget *t, float opacity) {
	if (t && t->root_visual) ( void ) wuc_visual_put__opacity(t->root_visual, opacity);
}

void flux_wintarget_setplate(FluxWindowTarget *t, float x, float y, float w, float h, float radius) {
	if (!t || !t->root || !t->backdrop || !plate_ensure(t)) return;
	( void ) wuc_sprite_put__brush(t->root, NULL);
	( void ) wuc_sprite_put__brush(t->plate, t->backdrop);
	( void ) wuc_visual_put__offset(t->plate_visual, (WFN_Vector_3) {x, y, 0.0f});
	( void ) wuc_visual_put__size(t->plate_visual, (WFN_Vector_2) {w, h});
	( void ) wuc_composition_rounded_rectangle_geometry_put__size(t->plate_geom, (WFN_Vector_2) {w, h});
	( void ) wuc_composition_rounded_rectangle_geometry_put__corner_radius(
	  t->plate_geom, (WFN_Vector_2) {radius, radius}
	);
}

void flux_wintarget_setsize(FluxWindowTarget *t, float w, float h) {
	if (!t || !t->root_visual) return;
	WFN_Vector_2 size = {w, h};
	( void ) wuc_visual_put__size(t->root_visual, size);
}

WUC_Visual *flux_window_target_root(FluxWindowTarget *t) { return t ? ( WUC_Visual * ) t->root : NULL; }
