#define INITGUID
#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "runtime/flux_anim_driver.h"
#include "fluxent/flux_gfx.h"

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

typedef struct FluxGfxDevice {
	uint32_t                       refcount;
	uint32_t                       generation;
	bool                           software;
	ID3D11Device                  *d3d_device;
	ID3D11DeviceContext           *d3d_context;
	IDXGIDevice                   *dxgi_device;
	IDXGIFactory2                 *dxgi_factory;
	ID2D1Factory1                 *d2d_factory;
	ID2D1Device                   *d2d_device;
	IDWriteFactory                *dwrite_factory;
	FluxCompositor                *compositor;
	WUC_CompositionGraphicsDevice *gdevice;
} FluxGfxDevice;

struct FluxGfx {
	HWND                hwnd;
	FluxDpi             dpi;

	FluxGfxDevice      *dev;
	uint32_t            dev_generation;
	IDXGISwapChain1    *swap_chain;
	ID2D1DeviceContext *d2d_context;
	ID2D1Bitmap1       *d2d_target;
	FluxWindowTarget   *window_target;

	bool                software;
	bool                hwnd_mode;
	bool                composition_mode;
	bool                popup_presented;
	bool                window_acrylic;
	bool                has_presented;
	UINT                surface_w;
	UINT                surface_h;
	float               plate [5];
	HRESULT             last_hr;

	FluxRedrawFn        redraw_cb;
	void               *redraw_ctx;
};

static FluxGfxDevice *g_shared_dev = NULL;

static HRESULT        create_device_independent(FluxGfxDevice *dev) {
	D2D1_FACTORY_OPTIONS opts;
	memset(&opts, 0, sizeof(opts));

	HRESULT hr
	  = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, &opts, ( void ** ) &dev->d2d_factory);
	if (FAILED(hr)) return hr;

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, ( void ** ) &dev->dwrite_factory);
	return hr;
}

static void graphics_log_failure(char const *stage, HRESULT hr) {
	wchar_t msg [128];
	_snwprintf(msg, 128, L"[fluxent] graphics init failed at %S: 0x%08lX\n", stage, ( unsigned long ) hr);
	OutputDebugStringW(msg);
}

static HRESULT graphics_create_hardware_device(FluxGfxDevice *dev, UINT flags, D3D_FEATURE_LEVEL const *levels) {
	IDXGIFactory2 *probe   = NULL;
	IDXGIAdapter  *adapter = NULL;

	HRESULT        hr      = CreateDXGIFactory2(0, &IID_IDXGIFactory2, ( void ** ) &probe);
	if (SUCCEEDED(hr)) hr = IDXGIFactory2_EnumAdapters(probe, 0, &adapter);
	if (SUCCEEDED(hr)) {
		hr = D3D11CreateDevice(
		  adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, flags, levels, 4, D3D11_SDK_VERSION, &dev->d3d_device, NULL,
		  &dev->d3d_context
		);
	}
	FLUX_RELEASE(adapter);
	FLUX_RELEASE(probe);
	return hr;
}

static HRESULT graphics_create_warp_device(FluxGfxDevice *dev, UINT flags, D3D_FEATURE_LEVEL const *levels) {
	return D3D11CreateDevice(
	  NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, levels, 4, D3D11_SDK_VERSION, &dev->d3d_device, NULL, &dev->d3d_context
	);
}

static HRESULT graphics_bind_dxgi(FluxGfxDevice *dev) {
	IDXGIAdapter *adapter = NULL;
	HRESULT       hr = ID3D11Device_QueryInterface(dev->d3d_device, &IID_IDXGIDevice, ( void ** ) &dev->dxgi_device);
	if (SUCCEEDED(hr)) hr = IDXGIDevice_GetAdapter(dev->dxgi_device, &adapter);
	if (SUCCEEDED(hr)) hr = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, ( void ** ) &dev->dxgi_factory);
	FLUX_RELEASE(adapter);
	IDXGIDevice1 *dev1 = NULL;
	if (SUCCEEDED(hr) && SUCCEEDED(IDXGIDevice_QueryInterface(dev->dxgi_device, &IID_IDXGIDevice1, ( void ** ) &dev1)))
	{
		IDXGIDevice1_SetMaximumFrameLatency(dev1, 1);
		FLUX_RELEASE(dev1);
	}
	return hr;
}

static HRESULT graphics_pick_device(FluxGfxDevice *dev) {
	UINT              flags     = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	D3D_FEATURE_LEVEL levels [] = {
	  D3D_FEATURE_LEVEL_11_1,
	  D3D_FEATURE_LEVEL_11_0,
	  D3D_FEATURE_LEVEL_10_1,
	  D3D_FEATURE_LEVEL_10_0,
	};

	HRESULT hr = E_FAIL;
	if (!dev->software) {
		hr = graphics_create_hardware_device(dev, flags, levels);
		if (FAILED(hr)) graphics_log_failure("d3d11 hardware device", hr);
	}
	if (SUCCEEDED(hr)) return hr;

	hr = graphics_create_warp_device(dev, flags, levels);
	if (FAILED(hr)) graphics_log_failure("d3d11 warp device", hr);
	else dev->software = true;
	return hr;
}

static HRESULT dev_create_resources(FluxGfxDevice *dev) {
	HRESULT hr = graphics_pick_device(dev);
	if (FAILED(hr)) return hr;

	hr = graphics_bind_dxgi(dev);
	if (FAILED(hr)) {
		graphics_log_failure("dxgi factory", hr);
		return hr;
	}

	hr = ID2D1Factory1_CreateDevice(dev->d2d_factory, dev->dxgi_device, &dev->d2d_device);
	if (FAILED(hr)) graphics_log_failure("d2d device", hr);
	return hr;
}

static void dev_release_device_resources(FluxGfxDevice *dev) {
	flux_compositor_dev_destroy(dev->gdevice);
	dev->gdevice = NULL;
	FLUX_RELEASE(dev->d2d_device);
	FLUX_RELEASE(dev->dxgi_device);
	FLUX_RELEASE(dev->dxgi_factory);
	FLUX_RELEASE(dev->d3d_context);
	FLUX_RELEASE(dev->d3d_device);
}

static void gfx_device_release(FluxGfxDevice *dev) {
	if (!dev) return;
	if (--dev->refcount > 0u) return;
	if (g_shared_dev == dev) g_shared_dev = NULL;
	dev_release_device_resources(dev);
	FLUX_RELEASE(dev->d2d_factory);
	FLUX_RELEASE(dev->dwrite_factory);
	flux_compositor_release(dev->compositor);
	dev->compositor = NULL;
	free(dev);
}

static FluxGfxDevice *gfx_device_acquire(bool software, HRESULT *out_hr) {
	if (g_shared_dev) {
		g_shared_dev->refcount++;
		if (out_hr) *out_hr = S_OK;
		return g_shared_dev;
	}
	FluxGfxDevice *dev = ( FluxGfxDevice * ) calloc(1, sizeof(*dev));
	if (!dev) {
		if (out_hr) *out_hr = E_OUTOFMEMORY;
		return NULL;
	}
	dev->refcount   = 1u;
	dev->generation = 1u;
	dev->software   = software;
	HRESULT hr      = create_device_independent(dev);
	if (SUCCEEDED(hr)) hr = dev_create_resources(dev);
	if (FAILED(hr)) {
		if (out_hr) *out_hr = hr;
		gfx_device_release(dev);
		return NULL;
	}
	g_shared_dev = dev;
	if (out_hr) *out_hr = S_OK;
	return dev;
}

static HRESULT create_surface_context(FluxGfx *gfx) {
	FLUX_RELEASE(gfx->d2d_context);
	if (!gfx->dev || !gfx->dev->d2d_device) return E_FAIL;
	return ID2D1Device_CreateDeviceContext(gfx->dev->d2d_device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &gfx->d2d_context);
}

static void release_window_resources(FluxGfx *gfx) {
	if (gfx->d2d_context) ID2D1DeviceContext_SetTarget(gfx->d2d_context, NULL);
	flux_window_target_destroy(gfx->window_target);
	gfx->window_target   = NULL;
	gfx->popup_presented = false;
	gfx->window_acrylic  = false;
	gfx->has_presented   = false;
	FLUX_RELEASE(gfx->d2d_target);
	FLUX_RELEASE(gfx->swap_chain);
}

static void graphics_client_size(FluxGfx *gfx, UINT *w, UINT *h) {
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

static D2D1_BITMAP_PROPERTIES1 graphics_target_bitmap_props(FluxGfx const *gfx) {
	D2D1_BITMAP_PROPERTIES1 bp;
	memset(&bp, 0, sizeof(bp));
	bp.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
	bp.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
	bp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	bp.dpiX                  = gfx->dpi.dpi_x;
	bp.dpiY                  = gfx->dpi.dpi_y;
	return bp;
}

static HRESULT graphics_create_d2d_target(FluxGfx *gfx) {
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

static HRESULT graphics_create_composition_swap_chain(FluxGfx *gfx, UINT w, UINT h) {
	DXGI_SWAP_CHAIN_DESC1 desc = graphics_swap_desc(w, h, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
	return IDXGIFactory2_CreateSwapChainForComposition(
	  gfx->dev->dxgi_factory, ( IUnknown * ) gfx->dev->d3d_device, &desc, NULL, &gfx->swap_chain
	);
}

static HRESULT graphics_create_composition_target(FluxGfx *gfx) {
	gfx->window_target = flux_window_target_create(gfx->dev->compositor, gfx->hwnd);
	if (!gfx->window_target) return E_FAIL;

	if (!gfx->composition_mode) {
		HRESULT hr = flux_wintarget_setswapchain(gfx->window_target, ( IUnknown * ) gfx->swap_chain);
		if (FAILED(hr)) return hr;
	}

	UINT w, h;
	graphics_client_size(gfx, &w, &h);
	flux_wintarget_setsize(gfx->window_target, ( float ) w, ( float ) h);
	return S_OK;
}

static HRESULT create_window_resources(FluxGfx *gfx) {
	if (!gfx->hwnd || !gfx->dev || !gfx->dev->dxgi_device) return S_OK;

	release_window_resources(gfx);

	UINT w, h;
	graphics_client_size(gfx, &w, &h);
	gfx->surface_w     = w;
	gfx->surface_h     = h;
	gfx->has_presented = false;

	HRESULT hr         = graphics_create_composition_swap_chain(gfx, w, h);
	if (FAILED(hr)) return hr;

	hr = graphics_create_d2d_target(gfx);
	if (FAILED(hr)) return hr;

	return graphics_create_composition_target(gfx);
}

FluxGfx *flux_gfx_create(HRESULT *out_hr) {
	HRESULT  hr  = E_OUTOFMEMORY;
	FluxGfx *gfx = ( FluxGfx * ) calloc(1, sizeof(*gfx));
	if (!gfx) goto done;

	gfx->dpi.dpi_x = FLUX_DPI_BASE;
	gfx->dpi.dpi_y = FLUX_DPI_BASE;

	char env [4]   = {0};
	bool software  = GetEnvironmentVariableA("FLUXENT_FORCE_WARP", env, sizeof(env)) > 0 && env [0] == '1';

	gfx->dev       = gfx_device_acquire(software, &hr);
	if (gfx->dev) {
		gfx->software       = gfx->dev->software;
		gfx->dev_generation = gfx->dev->generation;
		hr                  = create_surface_context(gfx);
	}
	if (FAILED(hr)) {
		flux_gfx_destroy(gfx);
		gfx = NULL;
	}
done:
	if (out_hr) *out_hr = hr;
	return gfx;
}

bool flux_gfx_software(FluxGfx const *gfx) { return gfx && gfx->software; }

void flux_gfx_destroy(FluxGfx *gfx) {
	if (!gfx) return;

	release_window_resources(gfx);
	FLUX_RELEASE(gfx->d2d_context);
	gfx_device_release(gfx->dev);
	gfx->dev = NULL;

	free(gfx);
}

HRESULT flux_gfx_attach(FluxGfx *gfx, HWND hwnd) {
	if (!gfx || !hwnd || !gfx->dev) return E_INVALIDARG;
	gfx->hwnd             = hwnd;
	gfx->hwnd_mode        = false;
	UINT dpi              = GetDpiForWindow(hwnd);
	gfx->dpi.dpi_x        = ( float ) dpi;
	gfx->dpi.dpi_y        = ( float ) dpi;

	char env [4]          = {0};
	gfx->composition_mode = GetEnvironmentVariableA("FLUX_USE_COMPOSITION", env, sizeof(env)) > 0 && env [0] == '1';

	if (!gfx->dev->compositor) {
		gfx->dev->compositor = flux_compositor_acquire();
		if (!gfx->dev->compositor) return E_FAIL;
	}
	HRESULT hr = create_window_resources(gfx);
	if (FAILED(hr)) graphics_log_failure("swap chain/composition", hr);
	return hr;
}

static HRESULT create_hwnd_window_resources(FluxGfx *gfx) {
	if (!gfx->hwnd || !gfx->dev || !gfx->dev->dxgi_device) return S_OK;

	release_window_resources(gfx);

	UINT w, h;
	graphics_client_size(gfx, &w, &h);
	gfx->surface_w             = w;
	gfx->surface_h             = h;
	gfx->has_presented         = false;

	DXGI_SWAP_CHAIN_DESC1 desc = graphics_swap_desc(w, h, DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL);
	HRESULT               hr   = IDXGIFactory2_CreateSwapChainForHwnd(
	  gfx->dev->dxgi_factory, ( IUnknown * ) gfx->dev->d3d_device, gfx->hwnd, &desc, NULL, NULL, &gfx->swap_chain
	);
	if (FAILED(hr)) return hr;

	return graphics_create_d2d_target(gfx);
}

HRESULT flux_gfx_attachbackdrop(FluxGfx *gfx, HWND hwnd) {
	if (!gfx || !hwnd) return E_INVALIDARG;
	gfx->hwnd      = hwnd;
	gfx->hwnd_mode = true;
	UINT dpi       = GetDpiForWindow(hwnd);
	gfx->dpi.dpi_x = ( float ) dpi;
	gfx->dpi.dpi_y = ( float ) dpi;
	return create_hwnd_window_resources(gfx);
}

HRESULT flux_gfx_resize(FluxGfx *gfx, int width, int height) {
	if (!gfx || !gfx->swap_chain) return E_FAIL;
	if (width < MIN_SURFACE_SIZE) width = MIN_SURFACE_SIZE;
	if (height < MIN_SURFACE_SIZE) height = MIN_SURFACE_SIZE;

	ID2D1DeviceContext_SetTarget(gfx->d2d_context, NULL);
	ID2D1RenderTarget_Flush(( ID2D1RenderTarget * ) gfx->d2d_context, NULL, NULL);
	FLUX_RELEASE(gfx->d2d_target);

	HRESULT hr
	  = IDXGISwapChain1_ResizeBuffers(gfx->swap_chain, 0, ( UINT ) width, ( UINT ) height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return hr;
	gfx->surface_w     = ( UINT ) width;
	gfx->surface_h     = ( UINT ) height;
	gfx->has_presented = false;

	hr                 = graphics_create_d2d_target(gfx);
	if (FAILED(hr)) return hr;

	if (gfx->window_target) flux_wintarget_setsize(gfx->window_target, ( float ) width, ( float ) height);
	return S_OK;
}

void flux_gfx_setdpi(FluxGfx *gfx, FluxDpi dpi) {
	if (!gfx) return;
	gfx->dpi = dpi;
	if (gfx->d2d_context) ID2D1RenderTarget_SetDpi(( ID2D1RenderTarget * ) gfx->d2d_context, dpi.dpi_x, dpi.dpi_y);
}

FluxDpi flux_gfx_dpi(FluxGfx const *gfx) {
	if (!gfx) return (FluxDpi) {FLUX_DPI_BASE, FLUX_DPI_BASE};
	return gfx->dpi;
}

void flux_gfx_begin(FluxGfx *gfx) {
	if (gfx && gfx->d2d_context) ID2D1RenderTarget_BeginDraw(( ID2D1RenderTarget * ) gfx->d2d_context);
}

void flux_gfx_end(FluxGfx *gfx) {
	if (!gfx || !gfx->d2d_context) return;
	HRESULT hr = ID2D1RenderTarget_EndDraw(( ID2D1RenderTarget * ) gfx->d2d_context, NULL, NULL);
	if (hr == ( HRESULT ) D2DERR_RECREATE_TARGET) ( void ) flux_gfx_devicechange(gfx);
	else if (FAILED(hr)) gfx->last_hr = hr;
}

bool        flux_gfx_has_presented(FluxGfx const *gfx) { return gfx && gfx->has_presented; }

static RECT gfx_clamp_dirty(FluxGfx const *gfx, RECT const *dirty_px) {
	LONG max_w = gfx->surface_w > 0u ? ( LONG ) gfx->surface_w : 1;
	LONG max_h = gfx->surface_h > 0u ? ( LONG ) gfx->surface_h : 1;
	RECT r     = *dirty_px;
	if (r.left < 0) r.left = 0;
	if (r.top < 0) r.top = 0;
	if (r.right > max_w) r.right = max_w;
	if (r.bottom > max_h) r.bottom = max_h;
	if (r.right <= r.left || r.bottom <= r.top) r = (RECT) {0, 0, 1, 1};
	return r;
}

void flux_gfx_present_dirty(FluxGfx *gfx, bool vsync, RECT const *dirty_px) {
	if (!gfx || !gfx->swap_chain) return;
	HRESULT hr;
	if (!gfx->has_presented || !dirty_px) {
		hr = IDXGISwapChain_Present(( IDXGISwapChain * ) gfx->swap_chain, vsync ? 1u : 0u, 0);
		if (SUCCEEDED(hr)) gfx->has_presented = true;
	}
	else {
		RECT                    r      = gfx_clamp_dirty(gfx, dirty_px);
		DXGI_PRESENT_PARAMETERS params = {.DirtyRectsCount = 1u, .pDirtyRects = &r};
		hr                             = IDXGISwapChain1_Present1(gfx->swap_chain, vsync ? 1u : 0u, 0, &params);
	}
	if (vsync) flux_anim_presented();
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) ( void ) flux_gfx_devicechange(gfx);
	else if (FAILED(hr)) gfx->last_hr = hr;
}

void flux_gfx_present(FluxGfx *gfx, bool vsync) { flux_gfx_present_dirty(gfx, vsync, NULL); }

void flux_gfx_clear(FluxGfx *gfx, FluxColor color) {
	if (!gfx || !gfx->d2d_context) return;
	D2D1_COLOR_F c;
	c.r = flux_color_rf(color);
	c.g = flux_color_gf(color);
	c.b = flux_color_bf(color);
	c.a = flux_color_af(color);
	ID2D1RenderTarget_Clear(( ID2D1RenderTarget * ) gfx->d2d_context, &c);
}

void     flux_gfx_commit(FluxGfx *gfx) { ( void ) gfx; }

FluxSize flux_gfx_size(FluxGfx const *gfx) {
	if (!gfx || !gfx->d2d_context) return (FluxSize) {0, 0};
	D2D1_SIZE_F s = ID2D1RenderTarget_GetSize(( ID2D1RenderTarget * ) gfx->d2d_context);
	return (FluxSize) {s.width, s.height};
}

FluxSize flux_gfx_pixels(FluxGfx const *gfx) {
	if (!gfx || !gfx->hwnd) return (FluxSize) {0, 0};
	RECT rc;
	GetClientRect(gfx->hwnd, &rc);
	return (FluxSize) {( float ) (rc.right - rc.left), ( float ) (rc.bottom - rc.top)};
}

ID2D1DeviceContext *flux_gfx_d2d(FluxGfx *gfx) { return gfx ? gfx->d2d_context : NULL; }

ID2D1Factory3      *flux_gfx_d2dfactory(FluxGfx *gfx) {
	return gfx && gfx->dev ? ( ID2D1Factory3 * ) gfx->dev->d2d_factory : NULL;
}

IDWriteFactory3 *flux_gfx_dwrite(FluxGfx *gfx) {
	return gfx && gfx->dev ? ( IDWriteFactory3 * ) gfx->dev->dwrite_factory : NULL;
}

FluxCompositor *flux_graphics_compositor(FluxGfx *gfx) { return gfx && gfx->dev ? gfx->dev->compositor : NULL; }

bool            flux_graphics_composition_enabled(FluxGfx const *gfx) { return gfx && gfx->composition_mode; }

WUC_Visual     *flux_graphics_composition_root(FluxGfx *gfx) {
	return gfx ? flux_window_target_root(gfx->window_target) : NULL;
}

static bool gfx_install_acrylic(FluxGfx *gfx, float blur) {
	WUC_Brush *backdrop = flux_effect_host_blur(gfx->dev->compositor, blur);
	if (!backdrop) return false;
	flux_wintarget_setbackdrop(gfx->window_target, backdrop);
	if (gfx->plate [2] > 0.0f)
		flux_wintarget_setplate(
		  gfx->window_target, gfx->plate [0], gfx->plate [1], gfx->plate [2], gfx->plate [3], gfx->plate [4]
		);
	(( IUnknown * ) backdrop)->lpVtbl->Release(( IUnknown * ) backdrop);
	return true;
}

bool flux_gfx_enable_acrylic(FluxGfx *gfx, float blur) {
	if (!gfx || !gfx->dev || !gfx->composition_mode || !gfx->window_target || !gfx->swap_chain) return false;
	if (!gfx->popup_presented
	    && FAILED(flux_wintarget_presentchild(gfx->window_target, ( IUnknown * ) gfx->swap_chain)))
		return false;
	gfx->popup_presented = true;
	if (blur <= 0.0f) {
		if (gfx->window_acrylic) flux_wintarget_setbackdrop(gfx->window_target, NULL);
		gfx->window_acrylic = false;
		return false;
	}
	if (!gfx->window_acrylic) gfx->window_acrylic = gfx_install_acrylic(gfx, blur);
	return gfx->window_acrylic;
}

WUC_CompositionGraphicsDevice *flux_graphics_graphics_device(FluxGfx *gfx) {
	if (!gfx || !gfx->dev || !gfx->dev->compositor || !gfx->dev->d2d_device) return NULL;
	if (!gfx->dev->gdevice)
		gfx->dev->gdevice = flux_compositor_dev_create(gfx->dev->compositor, ( IUnknown * ) gfx->dev->d2d_device);
	return gfx->dev->gdevice;
}

uint32_t flux_gfx_generation(FluxGfx const *gfx) { return gfx ? gfx->dev_generation : 0u; }

void     flux_gfx_redraw(FluxGfx *gfx) {
	if (!gfx) return;
	if (gfx->redraw_cb) gfx->redraw_cb(gfx->redraw_ctx);
	else if (gfx->hwnd) InvalidateRect(gfx->hwnd, NULL, FALSE);
}

void flux_gfx_onredraw(FluxGfx *gfx, FluxRedrawFn cb, void *ctx) {
	if (!gfx) return;
	gfx->redraw_cb  = cb;
	gfx->redraw_ctx = ctx;
}

bool flux_gfx_current(FluxGfx const *gfx) {
	if (!gfx || !gfx->dev || !gfx->dev->d3d_device) return false;
	if (gfx->dev_generation != gfx->dev->generation) return false;
	return ID3D11Device_GetDeviceRemovedReason(gfx->dev->d3d_device) == S_OK;
}

HRESULT flux_gfx_devicechange(FluxGfx *gfx) {
	if (!gfx || !gfx->dev) return E_INVALIDARG;

	release_window_resources(gfx);
	FLUX_RELEASE(gfx->d2d_context);

	FluxGfxDevice *dev = gfx->dev;
	if (gfx->dev_generation == dev->generation) {
		dev_release_device_resources(dev);
		HRESULT hr = dev_create_resources(dev);
		if (FAILED(hr)) return hr;
		dev->generation++;
	}

	HRESULT hr = create_surface_context(gfx);
	if (FAILED(hr)) return hr;
	gfx->dev_generation = dev->generation;
	gfx->software       = dev->software;

	if (gfx->hwnd) hr = gfx->hwnd_mode ? create_hwnd_window_resources(gfx) : create_window_resources(gfx);
	return hr;
}

void flux_gfx_setopacity(FluxGfx *gfx, float opacity) {
	if (gfx) flux_wintarget_setopacity(gfx->window_target, opacity);
}

void flux_gfx_setplate(FluxGfx *gfx, float x, float y, float w, float h, float radius) {
	if (!gfx) return;
	float p [5] = {x, y, w, h, radius};
	memcpy(gfx->plate, p, sizeof(p));
	if (gfx->window_acrylic && w > 0.0f) flux_wintarget_setplate(gfx->window_target, x, y, w, h, radius);
}
