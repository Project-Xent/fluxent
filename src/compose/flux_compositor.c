#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_compose.h"
#include "compose/flux_interop.h"

#include <cwinrt/init.h>
#include <cwinrt/cast.h>
#include <cwinrt/bootstrap.h>

/* The SDK's <dispatcherqueue.h> is C++-only (declares ABI:: types), so hand-declare
 * the CoreMessaging C ABI. Layout/values match the SDK header exactly. */
typedef enum DISPATCHERQUEUE_THREAD_TYPE {
	DQTYPE_THREAD_DEDICATED = 1,
	DQTYPE_THREAD_CURRENT   = 2,
} DISPATCHERQUEUE_THREAD_TYPE;
typedef enum DISPATCHERQUEUE_THREAD_APARTMENTTYPE {
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
HRESULT WINAPI    CreateDispatcherQueueController(DispatcherQueueOptions options, PDISPATCHERQUEUECONTROLLER *controller);

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct FluxCompositor {
	IUnknown *dispatcher;  /**< DispatcherQueueController for this thread. */
	WUC_Comp *comp;        /**< WUC Compositor. */
	uint32_t  refcount;
	bool      winrt_owned; /**< true if this instance called cwinrt_init. */
};

struct FluxWindowTarget {
	FluxCompositor              *owner;
	WUC_CompositionTarget       *target;
	WUC_Sprite                  *root;
	WUC_Visual                  *root_visual; /**< IVisual facet of root (sizing). */
	WUC_CompositionSurfaceBrush *brush;       /**< Swap chain surface brush (root brush, classic mode). */
	WUC_Sprite                  *content;     /**< Child hosting the swap chain over an acrylic root (popup mode). */
	WUC_Visual                  *content_visual;
	WUC_CompositionSurfaceBrush *content_brush;
	WUC_Brush                   *backdrop; /**< Root backplate material (popup acrylic). */
};

WUC_Visual *flux_compose_as_visual(void *obj) {
	WUC_Visual *v = NULL;
	if (obj) ( void ) cwinrt_query(obj, &CWINRT_IID_WUC_IVisual, ( void ** ) &v);
	return v;
}

/* Single UI thread => a process-wide singleton is the natural model. */
static FluxCompositor g_compositor;

static bool           compositor_init(FluxCompositor *c) {
	/* Windows.UI.Composition's Compositor requires the thread to be an ASTA with a
	 * DispatcherQueue. The apartment type is fixed at the moment the dispatcher
	 * controller is created, so it must come FIRST (before RoInitialize) using the
	 * CoreMessaging Win32 entry point with DQTAT_COM_ASTA. The WinRT
	 * CreateOnCurrentThread path (or a prior RoInitialize STA) leaves a plain STA
	 * and Compositor() then fails with E_ACCESSDENIED (0x80070005). This mirrors
	 * Microsoft's "Using the Visual Layer with Win32" sample. */
	DispatcherQueueOptions dqopts;
	dqopts.dwSize        = sizeof(dqopts);
	dqopts.threadType    = DQTYPE_THREAD_CURRENT;
	dqopts.apartmentType = DQTAT_COM_ASTA;
	if (FAILED(CreateDispatcherQueueController(dqopts, ( PDISPATCHERQUEUECONTROLLER * ) &c->dispatcher))) return false;

	/* The dispatcher controller has now initialized this thread's apartment.
	 * cwinrt_init just registers WinRT use on top (S_FALSE = already initialized,
	 * expected here); we did not own the apartment, so we never uninitialize it. */
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

WUC_CompositionGraphicsDevice *flux_compositor_create_graphics_device(FluxCompositor *c, IUnknown *d2d_device) {
	if (!c || !c->comp || !d2d_device) return NULL;

	FluxICompositorInterop *interop = NULL;
	if (FAILED(cwinrt_query(c->comp, &FluxIID_ICompositorInterop, ( void ** ) &interop))) return NULL;

	void   *device = NULL;
	HRESULT hr     = interop->lpVtbl->CreateGraphicsDevice(interop, d2d_device, &device);
	interop->lpVtbl->Release(interop);
	if (FAILED(hr)) return NULL;

	/* The interop returns the ICompositionGraphicsDevice default interface. */
	return ( WUC_CompositionGraphicsDevice * ) device;
}

void flux_compositor_graphics_device_release(WUC_CompositionGraphicsDevice *device) {
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

void flux_window_target_destroy(FluxWindowTarget *t) {
	if (!t) return;
	if (t->backdrop) (( IUnknown * ) t->backdrop)->lpVtbl->Release(( IUnknown * ) t->backdrop);
	if (t->content_brush) (( IUnknown * ) t->content_brush)->lpVtbl->Release(( IUnknown * ) t->content_brush);
	if (t->content_visual) (( IUnknown * ) t->content_visual)->lpVtbl->Release(( IUnknown * ) t->content_visual);
	if (t->content) (( IUnknown * ) t->content)->lpVtbl->Release(( IUnknown * ) t->content);
	if (t->brush) (( IUnknown * ) t->brush)->lpVtbl->Release(( IUnknown * ) t->brush);
	if (t->root_visual) (( IUnknown * ) t->root_visual)->lpVtbl->Release(( IUnknown * ) t->root_visual);
	if (t->root) (( IUnknown * ) t->root)->lpVtbl->Release(( IUnknown * ) t->root);
	if (t->target) (( IUnknown * ) t->target)->lpVtbl->Release(( IUnknown * ) t->target);
	free(t);
}

/* Wrap a swap chain as a composition surface brush (caller releases). */
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
	return SUCCEEDED(hr) ? brush : NULL;
}

HRESULT flux_window_target_set_swapchain(FluxWindowTarget *t, IUnknown *swapchain) {
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

HRESULT flux_window_target_present_swapchain_child(FluxWindowTarget *t, IUnknown *swapchain) {
	if (!t || !t->owner || !t->owner->comp || !swapchain) return E_INVALIDARG;

	WUC_CompositionSurfaceBrush *brush = swapchain_surface_brush(t, swapchain);
	if (!brush) return E_FAIL;

	HRESULT hr = S_OK;
	if (!t->content) {
		hr = wuc_comp_create_sprite_visual(t->owner->comp, &t->content);
		if (FAILED(hr)) goto fail;
		t->content_visual = flux_compose_as_visual(t->content);
		if (!t->content_visual) {
			hr = E_NOINTERFACE;
			goto fail;
		}

		/* Fill the root regardless of window size, so only the root is resized. */
		WFN_Vector_2 fill = {1.0f, 1.0f};
		( void ) wuc_visual_put__relative_size_adjustment(t->content_visual, fill);

		WUC_Container *root_container = NULL;
		if (SUCCEEDED(cwinrt_query(t->root, &CWINRT_IID_WUC_IContainerVisual, ( void ** ) &root_container))) {
			WUC_VisualCollection *children = NULL;
			if (SUCCEEDED(wuc_container_get__children(root_container, &children))) {
				( void ) wuc_visual_collection_insert_at_top(children, t->content_visual);
				(( IUnknown * ) children)->lpVtbl->Release(( IUnknown * ) children);
			}
			(( IUnknown * ) root_container)->lpVtbl->Release(( IUnknown * ) root_container);
		}
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

void flux_window_target_set_backdrop_brush(FluxWindowTarget *t, WUC_Brush *brush) {
	if (!t || !t->root) return;
	( void ) wuc_sprite_put__brush(t->root, brush);
	if (brush) (( IUnknown * ) brush)->lpVtbl->AddRef(( IUnknown * ) brush);
	if (t->backdrop) (( IUnknown * ) t->backdrop)->lpVtbl->Release(( IUnknown * ) t->backdrop);
	t->backdrop = brush;
}

void flux_window_target_set_size(FluxWindowTarget *t, float w, float h) {
	if (!t || !t->root_visual) return;
	WFN_Vector_2 size = {w, h};
	( void ) wuc_visual_put__size(t->root_visual, size);
}

WUC_Visual *flux_window_target_root(FluxWindowTarget *t) { return t ? ( WUC_Visual * ) t->root : NULL; }
