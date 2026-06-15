#define INITGUID
#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_graphics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <cd2d.h>
#include <cdwrite.h>

#include "compose/flux_compose.h"
#include "compose/flux_effect.h"

#define MIN_SURFACE_SIZE 1

struct FluxGraphics {
	HWND                           hwnd;
	FluxDpiInfo                    dpi;

	ID3D11Device                  *d3d_device;
	ID3D11DeviceContext           *d3d_context;
	IDXGIDevice                   *dxgi_device;
	IDXGIFactory2                 *dxgi_factory;
	IDXGISwapChain1               *swap_chain;

	ID2D1Factory1                 *d2d_factory;
	ID2D1Device                   *d2d_device;
	ID2D1DeviceContext            *d2d_context;
	ID2D1Bitmap1                  *d2d_target;

	IDWriteFactory                *dwrite_factory;

	FluxCompositor                *compositor;    /**< Thread-shared WUC compositor. */
	FluxWindowTarget              *window_target; /**< Per-HWND WUC target + root visual. */
	WUC_CompositionGraphicsDevice *gdevice;       /**< Lazily created for per-node surfaces. */

	bool                           tearing_supported;
	bool                           hwnd_mode;
	bool                           composition_mode; /**< Retained-tree path (FLUX_USE_COMPOSITION). */
	bool                           popup_presented;  /**< Swap chain hosted in a content child (popup windows). */
	bool                           window_acrylic;   /**< Host-backdrop acrylic backplate active (popup windows). */
	HRESULT                        last_hr;

	FluxRedrawCallback             redraw_cb;
	void                          *redraw_ctx;
};

static HRESULT create_device_independent(FluxGraphics *gfx) {
	D2D1_FACTORY_OPTIONS opts;
	memset(&opts, 0, sizeof(opts));

	HRESULT hr
	  = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, &opts, ( void ** ) &gfx->d2d_factory);
	if (FAILED(hr)) return hr;

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, ( void ** ) &gfx->dwrite_factory);
	return hr;
}

static HRESULT create_device_resources(FluxGraphics *gfx) {
	UINT              flags     = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3D_FEATURE_LEVEL levels [] = {
	  D3D_FEATURE_LEVEL_11_1,
	  D3D_FEATURE_LEVEL_11_0,
	  D3D_FEATURE_LEVEL_10_1,
	  D3D_FEATURE_LEVEL_10_0,
	};

	HRESULT hr = CreateDXGIFactory2(0, &IID_IDXGIFactory2, ( void ** ) &gfx->dxgi_factory);
	if (FAILED(hr)) return hr;

	IDXGIAdapter *adapter = NULL;
	hr                    = IDXGIFactory2_EnumAdapters(gfx->dxgi_factory, 0, &adapter);
	if (FAILED(hr)) return hr;

	hr = D3D11CreateDevice(
	  ( IDXGIAdapter * ) adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, levels, 4, D3D11_SDK_VERSION, &gfx->d3d_device,
	  NULL, &gfx->d3d_context
	);
	FLUX_RELEASE(adapter);
	if (FAILED(hr)) return hr;

	hr = ID3D11Device_QueryInterface(gfx->d3d_device, &IID_IDXGIDevice, ( void ** ) &gfx->dxgi_device);
	if (FAILED(hr)) return hr;

	hr = ID2D1Factory1_CreateDevice(gfx->d2d_factory, gfx->dxgi_device, &gfx->d2d_device);
	if (FAILED(hr)) return hr;

	hr = ID2D1Device_CreateDeviceContext(gfx->d2d_device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &gfx->d2d_context);
	return hr;
}

static void release_window_resources(FluxGraphics *gfx) {
	if (gfx->d2d_context) ID2D1DeviceContext_SetTarget(gfx->d2d_context, NULL);
	flux_window_target_destroy(gfx->window_target);
	gfx->window_target  = NULL;
	gfx->popup_presented = false; /* target gone; re-establish on next enable call */
	gfx->window_acrylic  = false;
	FLUX_RELEASE(gfx->d2d_target);
	FLUX_RELEASE(gfx->swap_chain);
}

static void graphics_client_size(FluxGraphics *gfx, UINT *w, UINT *h) {
	RECT rc;
	GetClientRect(gfx->hwnd, &rc);
	*w = ( UINT ) (rc.right - rc.left);
	*h = ( UINT ) (rc.bottom - rc.top);
	if (*w < MIN_SURFACE_SIZE) *w = MIN_SURFACE_SIZE;
	if (*h < MIN_SURFACE_SIZE) *h = MIN_SURFACE_SIZE;
}

static DXGI_SWAP_CHAIN_DESC1 graphics_swap_desc(UINT w, UINT h, DXGI_SWAP_EFFECT effect) {
	DXGI_SWAP_CHAIN_DESC1 desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width            = w;
	desc.Height           = h;
	desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount      = 2;
	desc.Scaling          = DXGI_SCALING_STRETCH;
	desc.SwapEffect       = effect;
	desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
	return desc;
}

static D2D1_BITMAP_PROPERTIES1 graphics_target_bitmap_props(FluxGraphics const *gfx) {
	D2D1_BITMAP_PROPERTIES1 bp;
	memset(&bp, 0, sizeof(bp));
	bp.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
	bp.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
	bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	bp.dpiX                  = gfx->dpi.dpi_x;
	bp.dpiY                  = gfx->dpi.dpi_y;
	return bp;
}

static HRESULT graphics_create_d2d_target(FluxGraphics *gfx) {
	IDXGISurface *surface = NULL;
	HRESULT       hr      = IDXGISwapChain1_GetBuffer(gfx->swap_chain, 0, &IID_IDXGISurface, ( void ** ) &surface);
	if (FAILED(hr)) return hr;

	D2D1_BITMAP_PROPERTIES1 bp = graphics_target_bitmap_props(gfx);
	hr = ID2D1DeviceContext_CreateBitmapFromDxgiSurface(gfx->d2d_context, surface, &bp, &gfx->d2d_target);
	FLUX_RELEASE(surface);
	if (FAILED(hr)) return hr;

	ID2D1DeviceContext_SetTarget(gfx->d2d_context, ( ID2D1Image * ) gfx->d2d_target);
	ID2D1RenderTarget_SetDpi(( ID2D1RenderTarget * ) gfx->d2d_context, gfx->dpi.dpi_x, gfx->dpi.dpi_y);
	return S_OK;
}

static HRESULT graphics_create_composition_swap_chain(FluxGraphics *gfx, UINT w, UINT h) {
	DXGI_SWAP_CHAIN_DESC1 desc = graphics_swap_desc(w, h, DXGI_SWAP_EFFECT_FLIP_DISCARD);
	return IDXGIFactory2_CreateSwapChainForComposition(
	  gfx->dxgi_factory, ( IUnknown * ) gfx->d3d_device, &desc, NULL, &gfx->swap_chain
	);
}

static HRESULT graphics_create_composition_target(FluxGraphics *gfx) {
	gfx->window_target = flux_window_target_create(gfx->compositor, gfx->hwnd);
	if (!gfx->window_target) return E_FAIL;

	/* In composition mode the root visual hosts the reconciled visual tree, so
	 * its content is the per-node surfaces — not the swap chain. Otherwise the
	 * root sprite shows the swap chain that the classic D2D renderer draws into. */
	if (!gfx->composition_mode) {
		HRESULT hr = flux_window_target_set_swapchain(gfx->window_target, ( IUnknown * ) gfx->swap_chain);
		if (FAILED(hr)) return hr;
	}

	/* DesktopWindowTarget composition space is physical pixels, 1:1 with the swap
	 * chain buffer; D2D handles DPI when rendering into that buffer. */
	UINT w, h;
	graphics_client_size(gfx, &w, &h);
	flux_window_target_set_size(gfx->window_target, ( float ) w, ( float ) h);
	return S_OK;
}

static HRESULT create_window_resources(FluxGraphics *gfx) {
	if (!gfx->hwnd || !gfx->dxgi_device) return S_OK;

	release_window_resources(gfx);

	UINT w, h;
	graphics_client_size(gfx, &w, &h);

	HRESULT hr = graphics_create_composition_swap_chain(gfx, w, h);
	if (FAILED(hr)) return hr;

	hr = graphics_create_d2d_target(gfx);
	if (FAILED(hr)) return hr;

	return graphics_create_composition_target(gfx);
}

FluxGraphics *flux_graphics_create(void) {
	FluxGraphics *gfx = ( FluxGraphics * ) calloc(1, sizeof(*gfx));
	if (!gfx) return NULL;

	gfx->dpi.dpi_x = FLUX_DPI_BASE;
	gfx->dpi.dpi_y = FLUX_DPI_BASE;

	HRESULT hr     = create_device_independent(gfx);
	if (FAILED(hr)) {
		flux_graphics_destroy(gfx);
		return NULL;
	}

	hr = create_device_resources(gfx);
	if (FAILED(hr)) {
		flux_graphics_destroy(gfx);
		return NULL;
	}

	return gfx;
}

void flux_graphics_destroy(FluxGraphics *gfx) {
	if (!gfx) return;

	release_window_resources(gfx);

	flux_compositor_graphics_device_release(gfx->gdevice);
	gfx->gdevice = NULL;
	FLUX_RELEASE(gfx->d2d_context);
	FLUX_RELEASE(gfx->d2d_device);
	FLUX_RELEASE(gfx->d2d_factory);
	FLUX_RELEASE(gfx->dwrite_factory);
	FLUX_RELEASE(gfx->dxgi_device);
	FLUX_RELEASE(gfx->dxgi_factory);
	FLUX_RELEASE(gfx->d3d_context);
	FLUX_RELEASE(gfx->d3d_device);

	flux_compositor_release(gfx->compositor);
	gfx->compositor = NULL;

	free(gfx);
}

HRESULT flux_graphics_attach(FluxGraphics *gfx, HWND hwnd) {
	if (!gfx || !hwnd) return E_INVALIDARG;
	gfx->hwnd             = hwnd;
	gfx->hwnd_mode        = false;
	UINT dpi              = GetDpiForWindow(hwnd);
	gfx->dpi.dpi_x        = ( float ) dpi;
	gfx->dpi.dpi_y        = ( float ) dpi;

	char env [4]          = {0};
	gfx->composition_mode = GetEnvironmentVariableA("FLUX_USE_COMPOSITION", env, sizeof(env)) > 0 && env [0] == '1';

	if (!gfx->compositor) {
		gfx->compositor = flux_compositor_acquire();
		if (!gfx->compositor) return E_FAIL;
	}
	return create_window_resources(gfx);
}

static HRESULT create_hwnd_window_resources(FluxGraphics *gfx) {
	if (!gfx->hwnd || !gfx->dxgi_device) return S_OK;

	release_window_resources(gfx);

	UINT w, h;
	graphics_client_size(gfx, &w, &h);

	DXGI_SWAP_CHAIN_DESC1 desc = graphics_swap_desc(w, h, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
	HRESULT               hr   = IDXGIFactory2_CreateSwapChainForHwnd(
	  gfx->dxgi_factory, ( IUnknown * ) gfx->d3d_device, gfx->hwnd, &desc, NULL, NULL, &gfx->swap_chain
	);
	if (FAILED(hr)) return hr;

	return graphics_create_d2d_target(gfx);
}

HRESULT flux_graphics_attach_for_backdrop(FluxGraphics *gfx, HWND hwnd) {
	if (!gfx || !hwnd) return E_INVALIDARG;
	gfx->hwnd      = hwnd;
	gfx->hwnd_mode = true;
	UINT dpi       = GetDpiForWindow(hwnd);
	gfx->dpi.dpi_x = ( float ) dpi;
	gfx->dpi.dpi_y = ( float ) dpi;
	return create_hwnd_window_resources(gfx);
}

HRESULT flux_graphics_resize(FluxGraphics *gfx, int width, int height) {
	if (!gfx || !gfx->swap_chain) return E_FAIL;
	if (width < MIN_SURFACE_SIZE) width = MIN_SURFACE_SIZE;
	if (height < MIN_SURFACE_SIZE) height = MIN_SURFACE_SIZE;

	ID2D1DeviceContext_SetTarget(gfx->d2d_context, NULL);
	ID2D1RenderTarget_Flush(( ID2D1RenderTarget * ) gfx->d2d_context, NULL, NULL);
	FLUX_RELEASE(gfx->d2d_target);

	HRESULT hr
	  = IDXGISwapChain1_ResizeBuffers(gfx->swap_chain, 0, ( UINT ) width, ( UINT ) height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return hr;

	hr = graphics_create_d2d_target(gfx);
	if (FAILED(hr)) return hr;

	/* The swap chain object persists across ResizeBuffers, so its composition
	 * surface stays bound; only the root visual extent needs updating. */
	if (gfx->window_target) flux_window_target_set_size(gfx->window_target, ( float ) width, ( float ) height);
	return S_OK;
}

void flux_graphics_set_dpi(FluxGraphics *gfx, FluxDpiInfo dpi) {
	if (!gfx) return;
	gfx->dpi = dpi;
	if (gfx->d2d_context) ID2D1RenderTarget_SetDpi(( ID2D1RenderTarget * ) gfx->d2d_context, dpi.dpi_x, dpi.dpi_y);
}

FluxDpiInfo flux_graphics_get_dpi(FluxGraphics const *gfx) {
	if (!gfx) return (FluxDpiInfo) {FLUX_DPI_BASE, FLUX_DPI_BASE};
	return gfx->dpi;
}

void flux_graphics_begin_draw(FluxGraphics *gfx) {
	if (gfx && gfx->d2d_context) ID2D1RenderTarget_BeginDraw(( ID2D1RenderTarget * ) gfx->d2d_context);
}

void flux_graphics_end_draw(FluxGraphics *gfx) {
	if (!gfx || !gfx->d2d_context) return;
	HRESULT hr = ID2D1RenderTarget_EndDraw(( ID2D1RenderTarget * ) gfx->d2d_context, NULL, NULL);
	if (hr == ( HRESULT ) D2DERR_RECREATE_TARGET) ( void ) flux_graphics_handle_device_change(gfx);
	else if (FAILED(hr)) gfx->last_hr = hr;
}

void flux_graphics_present(FluxGraphics *gfx, bool vsync) {
	if (!gfx || !gfx->swap_chain) return;
	( void ) vsync;
	HRESULT hr = IDXGISwapChain_Present(( IDXGISwapChain * ) gfx->swap_chain, 0, 0);
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		( void ) flux_graphics_handle_device_change(gfx);
	else if (FAILED(hr)) gfx->last_hr = hr;
}

void flux_graphics_clear(FluxGraphics *gfx, FluxColor color) {
	if (!gfx || !gfx->d2d_context) return;
	D2D1_COLOR_F c;
	c.r = flux_color_rf(color);
	c.g = flux_color_gf(color);
	c.b = flux_color_bf(color);
	c.a = flux_color_af(color);
	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) gfx->d2d_context, &c);
}

void flux_graphics_commit(FluxGraphics *gfx) {
	/* The WUC compositor commits its batch automatically when the thread's
	 * DispatcherQueue ticks during the message pump; no explicit commit needed.
	 * Swap chain content is presented separately via flux_graphics_present(). */
	( void ) gfx;
}

FluxSize flux_graphics_get_target_size(FluxGraphics const *gfx) {
	if (!gfx || !gfx->d2d_context) return (FluxSize) {0, 0};
	D2D1_SIZE_F s = ID2D1RenderTarget_GetSize(( ID2D1RenderTarget * ) gfx->d2d_context);
	return (FluxSize) {s.width, s.height};
}

FluxSize flux_graphics_get_client_pixel_size(FluxGraphics const *gfx) {
	if (!gfx || !gfx->hwnd) return (FluxSize) {0, 0};
	RECT rc;
	GetClientRect(gfx->hwnd, &rc);
	return (FluxSize) {( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

ID2D1DeviceContext *flux_graphics_get_d2d_context(FluxGraphics *gfx) { return gfx ? gfx->d2d_context : NULL; }

ID2D1Factory3      *flux_graphics_get_d2d_factory(FluxGraphics *gfx) {
	return gfx ? ( ID2D1Factory3 * ) gfx->d2d_factory : NULL;
}

IDWriteFactory3 *flux_graphics_get_dwrite_factory(FluxGraphics *gfx) {
	return gfx ? ( IDWriteFactory3 * ) gfx->dwrite_factory : NULL;
}

void flux_graphics_add_overlay_visual(FluxGraphics *gfx, IDCompositionVisual *visual) {
	( void ) gfx;
	( void ) visual;
}

void flux_graphics_remove_overlay_visual(FluxGraphics *gfx, IDCompositionVisual *visual) {
	( void ) gfx;
	( void ) visual;
}

FluxCompositor *flux_graphics_compositor(FluxGraphics *gfx) { return gfx ? gfx->compositor : NULL; }

bool            flux_graphics_composition_enabled(FluxGraphics const *gfx) { return gfx && gfx->composition_mode; }

WUC_Visual     *flux_graphics_composition_root(FluxGraphics *gfx) {
	return gfx ? flux_window_target_root(gfx->window_target) : NULL;
}

bool flux_graphics_enable_window_acrylic(FluxGraphics *gfx, float blur) {
	if (!gfx || !gfx->composition_mode || !gfx->window_target || !gfx->swap_chain) return false;
	if (gfx->popup_presented) return gfx->window_acrylic;

	/* The swap chain (the popup's drawn chrome + content) composes on top in a
	 * content child; presenting it is what makes the popup visible at all in
	 * composition mode, so it must not depend on the acrylic step succeeding. */
	if (FAILED(flux_window_target_present_swapchain_child(gfx->window_target, ( IUnknown * ) gfx->swap_chain)))
		return false;
	gfx->popup_presented = true;

	/* Optional: a desktop-sampling acrylic backplate behind the content child.
	 * If unavailable, the popup still renders (opaque) over an empty root. */
	WUC_Brush *backdrop  = flux_effect_make_host_backdrop_blur(gfx->compositor, blur);
	if (backdrop) {
		flux_window_target_set_backdrop_brush(gfx->window_target, backdrop);
		(( IUnknown * ) backdrop)->lpVtbl->Release(( IUnknown * ) backdrop);
		gfx->window_acrylic = true;
	}
	return gfx->window_acrylic;
}

WUC_CompositionGraphicsDevice *flux_graphics_graphics_device(FluxGraphics *gfx) {
	if (!gfx || !gfx->compositor || !gfx->d2d_device) return NULL;
	if (!gfx->gdevice)
		gfx->gdevice = flux_compositor_create_graphics_device(gfx->compositor, ( IUnknown * ) gfx->d2d_device);
	return gfx->gdevice;
}

void flux_graphics_request_redraw(FluxGraphics *gfx) {
	if (!gfx) return;
	if (gfx->redraw_cb) gfx->redraw_cb(gfx->redraw_ctx);
	else if (gfx->hwnd) InvalidateRect(gfx->hwnd, NULL, FALSE);
}

void flux_graphics_set_redraw_callback(FluxGraphics *gfx, FluxRedrawCallback cb, void *ctx) {
	if (!gfx) return;
	gfx->redraw_cb  = cb;
	gfx->redraw_ctx = ctx;
}

bool flux_graphics_is_device_current(FluxGraphics const *gfx) {
	if (!gfx || !gfx->d3d_device) return false;
	return ID3D11Device_GetDeviceRemovedReason(gfx->d3d_device) == S_OK;
}

HRESULT flux_graphics_handle_device_change(FluxGraphics *gfx) {
	if (!gfx) return E_INVALIDARG;

	release_window_resources(gfx);

	flux_compositor_graphics_device_release(gfx->gdevice);
	gfx->gdevice = NULL;
	FLUX_RELEASE(gfx->d2d_context);
	FLUX_RELEASE(gfx->d2d_device);
	FLUX_RELEASE(gfx->dxgi_device);
	FLUX_RELEASE(gfx->dxgi_factory);
	FLUX_RELEASE(gfx->d3d_context);
	FLUX_RELEASE(gfx->d3d_device);

	HRESULT hr = create_device_resources(gfx);
	if (FAILED(hr)) return hr;

	if (gfx->hwnd) hr = gfx->hwnd_mode ? create_hwnd_window_resources(gfx) : create_window_resources(gfx);

	return hr;
}
