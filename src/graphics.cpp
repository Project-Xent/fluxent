// FluXent Graphics Pipeline implementation
#include "fluxent/graphics.hpp"
#include <stdexcept>

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwrite.lib")
#endif

namespace fluxent {

GraphicsPipeline::GraphicsPipeline() {
    create_device_independent_resources();
    create_device_resources();
}

GraphicsPipeline::~GraphicsPipeline() = default;

void GraphicsPipeline::create_device_independent_resources() {
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    
    throw_if_failed(
        D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory3),
            &options,
            reinterpret_cast<void**>(d2d_factory_.GetAddressOf())
        ),
        "Failed to create D2D1 Factory"
    );
    
    throw_if_failed(
        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory3),
            reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf())
        ),
        "Failed to create DirectWrite Factory"
    );
}

void GraphicsPipeline::create_device_resources() {
    UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    
    throw_if_failed(
        D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creation_flags,
            feature_levels,
            ARRAYSIZE(feature_levels),
            D3D11_SDK_VERSION,
            d3d_device_.GetAddressOf(),
            nullptr,
            nullptr
        ),
        "Failed to create D3D11 Device"
    );
    
    throw_if_failed(
        d3d_device_.As(&dxgi_device_),
        "Failed to get DXGI Device"
    );
    
    throw_if_failed(
        d2d_factory_->CreateDevice(dxgi_device_.Get(), &d2d_device_),
        "Failed to create D2D Device"
    );
    
    throw_if_failed(
        d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_context_),
        "Failed to create D2D Device Context"
    );

    throw_if_failed(
        d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_overlay_context_),
        "Failed to create D2D Overlay Device Context"
    );
    
    throw_if_failed(
        DCompositionCreateDevice(
            dxgi_device_.Get(),
            __uuidof(IDCompositionDevice),
            reinterpret_cast<void**>(dcomp_device_.GetAddressOf())
        ),
        "Failed to create DirectComposition Device"
    );
}

void GraphicsPipeline::attach_to_window(HWND hwnd) {
    if (!hwnd) {
        throw std::invalid_argument("Invalid window handle");
    }
    
    hwnd_ = hwnd;

    UINT dpi = GetDpiForWindow(hwnd);
    dpi_.dpi_x = static_cast<float>(dpi);
    dpi_.dpi_y = static_cast<float>(dpi);

    create_window_size_resources();
}

void GraphicsPipeline::request_redraw() {
    if (!hwnd_) return;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void GraphicsPipeline::create_window_size_resources() {
    if (!hwnd_ || !dxgi_device_) return;

    release_window_size_resources();

    ComPtr<IDXGIAdapter> adapter;
    throw_if_failed(dxgi_device_->GetAdapter(&adapter), "Failed to get DXGI Adapter");
    throw_if_failed(
        adapter->GetParent(IID_PPV_ARGS(&dxgi_factory_)),
        "Failed to get DXGI Factory"
    );
    
    RECT rc;
    GetClientRect(hwnd_, &rc);
    UINT width = std::max(1, static_cast<int>(rc.right - rc.left));
    UINT height = std::max(1, static_cast<int>(rc.bottom - rc.top));
    
    DXGI_SWAP_CHAIN_DESC1 swap_desc = {};
    swap_desc.Width = width;
    swap_desc.Height = height;
    swap_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_desc.Stereo = FALSE;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.SampleDesc.Quality = 0;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2;
    swap_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swap_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;     // Required for transparency.
    swap_desc.Flags = 0;
    
    throw_if_failed(
        dxgi_factory_->CreateSwapChainForComposition(
            d3d_device_.Get(),
            &swap_desc,
            nullptr,
            &swap_chain_
        ),
        "Failed to create SwapChain for Composition"
    );
    
    DXGI_RGBA bg_color = { 0.0f, 0.0f, 0.0f, 0.0f };
    swap_chain_->SetBackgroundColor(&bg_color);

    ComPtr<IDXGISurface> dxgi_surface;
    throw_if_failed(
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface)),
        "Failed to get SwapChain back buffer"
    );
    
    D2D1_BITMAP_PROPERTIES1 bitmap_props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi_.dpi_x,
        dpi_.dpi_y
    );
    
    throw_if_failed(
        d2d_context_->CreateBitmapFromDxgiSurface(
            dxgi_surface.Get(),
            &bitmap_props,
            &d2d_target_bitmap_
        ),
        "Failed to create D2D bitmap from DXGI surface"
    );
    
    d2d_context_->SetTarget(d2d_target_bitmap_.Get());

    // Keep the device context in sync with the window DPI so D2D coordinates are in DIPs.
    // Without this, the context may remain at the default 96 DPI and the UI will appear
    // incorrectly sized on high-DPI displays.
    d2d_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);

    if (d2d_overlay_context_) {
        d2d_overlay_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);
    }
    
    throw_if_failed(
        dcomp_device_->CreateTargetForHwnd(hwnd_, TRUE, &dcomp_target_),
        "Failed to create DComp target"
    );
    
    throw_if_failed(
        dcomp_device_->CreateVisual(&root_visual_),
        "Failed to create DComp visual"
    );

    throw_if_failed(
        dcomp_device_->CreateVisual(&swapchain_visual_),
        "Failed to create DComp swapchain visual"
    );
    
    throw_if_failed(
        swapchain_visual_->SetContent(swap_chain_.Get()),
        "Failed to set DComp swapchain visual content"
    );

    throw_if_failed(
        root_visual_->AddVisual(swapchain_visual_.Get(), FALSE, nullptr),
        "Failed to add swapchain visual"
    );
    
    throw_if_failed(
        dcomp_target_->SetRoot(root_visual_.Get()),
        "Failed to set DComp root"
    );
    
    throw_if_failed(
        dcomp_device_->Commit(),
        "Failed to commit DComp"
    );
}

void GraphicsPipeline::release_window_size_resources() {
    d2d_context_->SetTarget(nullptr);
    d2d_target_bitmap_.Reset();
    dcomp_target_.Reset();
    root_visual_.Reset();
    swapchain_visual_.Reset();
    swap_chain_.Reset();
}

void GraphicsPipeline::resize(int width, int height) {
    if (!swap_chain_ || width <= 0 || height <= 0) return;

    d2d_context_->SetTarget(nullptr);
    d2d_target_bitmap_.Reset();

    HRESULT hr = swap_chain_->ResizeBuffers(
        0,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN,
        0
    );
    
    if (FAILED(hr)) {
        create_window_size_resources();
        return;
    }

    ComPtr<IDXGISurface> dxgi_surface;
    throw_if_failed(
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface)),
        "Failed to get SwapChain back buffer after resize"
    );
    
    D2D1_BITMAP_PROPERTIES1 bitmap_props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi_.dpi_x,
        dpi_.dpi_y
    );
    
    throw_if_failed(
        d2d_context_->CreateBitmapFromDxgiSurface(
            dxgi_surface.Get(),
            &bitmap_props,
            &d2d_target_bitmap_
        ),
        "Failed to create D2D bitmap after resize"
    );
    
    d2d_context_->SetTarget(d2d_target_bitmap_.Get());

    // Ensure DPI stays correct after buffer recreation.
    d2d_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);
}

void GraphicsPipeline::set_dpi(const DpiInfo& dpi) {
    dpi_ = dpi;
    if (d2d_context_) {
        d2d_context_->SetDpi(dpi.dpi_x, dpi.dpi_y);
    }
    if (d2d_overlay_context_) {
        d2d_overlay_context_->SetDpi(dpi.dpi_x, dpi.dpi_y);
    }
}

void GraphicsPipeline::add_overlay_visual(IDCompositionVisual* visual) {
    if (!root_visual_ || !swapchain_visual_ || !visual) return;
    // Insert the overlay above the swapchain visual.
    root_visual_->AddVisual(visual, TRUE, swapchain_visual_.Get());
}

void GraphicsPipeline::begin_draw() {
    if (d2d_context_) {
        d2d_context_->BeginDraw();
    }
}

void GraphicsPipeline::end_draw() {
    if (d2d_context_) {
        HRESULT hr = d2d_context_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            create_device_resources();
            create_window_size_resources();
        }
    }
}

void GraphicsPipeline::present(bool vsync) {
    if (swap_chain_) {
        HRESULT hr = swap_chain_->Present(vsync ? 1 : 0, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            create_device_resources();
            create_window_size_resources();
        }
    }
}

void GraphicsPipeline::clear(const Color& color) {
    if (d2d_context_) {
        d2d_context_->Clear(color.to_d2d());
    }
}

void GraphicsPipeline::commit() {
    if (dcomp_device_) {
        dcomp_device_->Commit();
    }
}

ComPtr<ID2D1SolidColorBrush> GraphicsPipeline::create_solid_brush(const Color& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    if (d2d_context_) {
        d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
    }
    return brush;
}

Size GraphicsPipeline::get_render_target_size() const {
    if (d2d_context_) {
        D2D1_SIZE_F size = d2d_context_->GetSize();
        return Size(size.width, size.height);
    }
    return Size();
}

Size GraphicsPipeline::get_client_pixel_size() const {
    if (!hwnd_) return Size();
    RECT rc{};
    if (!GetClientRect(hwnd_, &rc)) return Size();
    const float w = static_cast<float>(std::max<LONG>(0, rc.right - rc.left));
    const float h = static_cast<float>(std::max<LONG>(0, rc.bottom - rc.top));
    return Size(w, h);
}

} // namespace fluxent
