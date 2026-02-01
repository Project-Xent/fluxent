// FluXent Graphics Pipeline implementation

#include "fluxent/config.hpp"
#include "fluxent/graphics.hpp"

#include "fluxent/config.hpp"
#include "fluxent/graphics.hpp"

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwrite.lib")
#endif

namespace fluxent
{

Result<std::unique_ptr<GraphicsPipeline>> GraphicsPipeline::Create()
{
  auto result = std::unique_ptr<GraphicsPipeline>(new GraphicsPipeline());
  auto init_res = result->Init();
  if (!init_res)
  {
    return tl::unexpected(init_res.error());
  }
  return result;
}

GraphicsPipeline::GraphicsPipeline() = default;

Result<void> GraphicsPipeline::Init()
{
  auto res = CreateDeviceIndependentResources();
  if (!res)
    return res;

  return CreateDeviceResources();
}

GraphicsPipeline::~GraphicsPipeline() = default;

Result<void> GraphicsPipeline::CreateDeviceIndependentResources()
{
  D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

  auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &options,
                              reinterpret_cast<void **>(d2d_factory_.GetAddressOf()));
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3),
                           reinterpret_cast<IUnknown **>(dwrite_factory_.GetAddressOf()));
  if (FAILED(hr))
    return tl::unexpected(hr);

  return {};
}

Result<void> GraphicsPipeline::CreateDeviceResources()
{
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

  auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creation_flags,
                              feature_levels, ARRAYSIZE(feature_levels), D3D11_SDK_VERSION,
                              d3d_device_.GetAddressOf(), nullptr, nullptr);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = d3d_device_.As(&dxgi_device_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = d2d_factory_->CreateDevice(dxgi_device_.Get(), &d2d_device_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_context_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_overlay_context_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = DCompositionCreateDevice(dxgi_device_.Get(), __uuidof(IDCompositionDevice),
                                reinterpret_cast<void **>(dcomp_device_.GetAddressOf()));
  if (FAILED(hr))
    return tl::unexpected(hr);

  return {};
}

Result<void> GraphicsPipeline::AttachToWindow(HWND hwnd)
{
  if (!hwnd)
  {
    return tl::unexpected(E_INVALIDARG);
  }

  hwnd_ = hwnd;

  UINT dpi = GetDpiForWindow(hwnd);
  dpi_.dpi_x = static_cast<float>(dpi);
  dpi_.dpi_y = static_cast<float>(dpi);

  return CreateWindowSizeResources();
}

void GraphicsPipeline::RequestRedraw()
{
  if (!hwnd_)
    return;
  InvalidateRect(hwnd_, nullptr, FALSE);
}

Result<void> GraphicsPipeline::CreateWindowSizeResources()
{
  if (!hwnd_ || !dxgi_device_)
    return {};

  ReleaseWindowSizeResources();

  ComPtr<IDXGIAdapter> adapter;
  auto hr = dxgi_device_->GetAdapter(&adapter);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = adapter->GetParent(IID_PPV_ARGS(&dxgi_factory_));
  if (FAILED(hr))
    return tl::unexpected(hr);

  RECT rc;
  GetClientRect(hwnd_, &rc);
  UINT width = std::max(static_cast<int>(fluxent::config::Render::MinSurfaceSize),
                        static_cast<int>(rc.right - rc.left));
  UINT height = std::max(static_cast<int>(fluxent::config::Render::MinSurfaceSize),
                         static_cast<int>(rc.bottom - rc.top));

  DXGI_SWAP_CHAIN_DESC1 swap_desc = {};
  swap_desc.Width = width;
  swap_desc.Height = height;
  swap_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swap_desc.Stereo = FALSE;
  swap_desc.SampleDesc.Count = fluxent::config::Graphics::SwapChainSampleCount;
  swap_desc.SampleDesc.Quality = fluxent::config::Graphics::SwapChainSampleQuality;
  swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_desc.BufferCount = fluxent::config::Graphics::SwapChainBufferCount;
  swap_desc.Scaling = DXGI_SCALING_STRETCH;
  swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  swap_desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // Required for transparency.
  swap_desc.Flags = fluxent::config::Graphics::SwapChainFlags;

  hr = dxgi_factory_->CreateSwapChainForComposition(d3d_device_.Get(), &swap_desc, nullptr,
                                                    &swap_chain_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  DXGI_RGBA bg_color = {0.0f, 0.0f, 0.0f, 0.0f};
  swap_chain_->SetBackgroundColor(&bg_color);

  ComPtr<IDXGISurface> dxgi_surface;
  hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
  if (FAILED(hr))
    return tl::unexpected(hr);

  D2D1_BITMAP_PROPERTIES1 bitmap_props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_.dpi_x,
      dpi_.dpi_y);

  hr = d2d_context_->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &bitmap_props,
                                                 &d2d_target_bitmap_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  d2d_context_->SetTarget(d2d_target_bitmap_.Get());

  // Keep the device context in sync with the window DPI so D2D coordinates are
  // in DIPs. Without this, the context may remain at the default 96 DPI and the
  // UI will appear incorrectly sized on high-DPI displays.
  d2d_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);

  if (d2d_overlay_context_)
  {
    d2d_overlay_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);
  }

  hr = dcomp_device_->CreateTargetForHwnd(hwnd_, TRUE, &dcomp_target_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = dcomp_device_->CreateVisual(&root_visual_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = dcomp_device_->CreateVisual(&swapchain_visual_);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = swapchain_visual_->SetContent(swap_chain_.Get());
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = root_visual_->AddVisual(swapchain_visual_.Get(), FALSE, nullptr);
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = dcomp_target_->SetRoot(root_visual_.Get());
  if (FAILED(hr))
    return tl::unexpected(hr);

  hr = dcomp_device_->Commit();
  if (FAILED(hr))
    return tl::unexpected(hr);

  return {};
}

void GraphicsPipeline::ReleaseWindowSizeResources()
{
  d2d_context_->SetTarget(nullptr);
  d2d_target_bitmap_.Reset();
  dcomp_target_.Reset();
  root_visual_.Reset();
  swapchain_visual_.Reset();
  swap_chain_.Reset();
}

Result<void> GraphicsPipeline::Resize(int width, int height)
{
  if (!swap_chain_ || width <= 0 || height <= 0)
    return {};

  d2d_context_->SetTarget(nullptr);
  d2d_target_bitmap_.Reset();

  HRESULT hr = swap_chain_->ResizeBuffers(fluxent::config::Graphics::ResizeBufferCount,
                                          static_cast<UINT>(width), static_cast<UINT>(height),
                                          DXGI_FORMAT_UNKNOWN,
                                          fluxent::config::Graphics::ResizeBufferFlags);

  if (FAILED(hr))
  {
    (void)CreateWindowSizeResources();
    return tl::unexpected(hr);
  }

  ComPtr<IDXGISurface> dxgi_surface;
  HRESULT hr_resize = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&dxgi_surface));
  if (FAILED(hr_resize))
    return tl::unexpected(hr_resize);

  D2D1_BITMAP_PROPERTIES1 bitmap_props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi_.dpi_x,
      dpi_.dpi_y);

  hr_resize = d2d_context_->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &bitmap_props,
                                                        &d2d_target_bitmap_);
  if (FAILED(hr_resize))
    return tl::unexpected(hr_resize);

  d2d_context_->SetTarget(d2d_target_bitmap_.Get());

  // Ensure DPI stays correct after buffer recreation.
  d2d_context_->SetDpi(dpi_.dpi_x, dpi_.dpi_y);

  return {};
}

void GraphicsPipeline::SetDpi(const DpiInfo &dpi)
{
  dpi_ = dpi;
  if (d2d_context_)
  {
    d2d_context_->SetDpi(dpi.dpi_x, dpi.dpi_y);
  }
  if (d2d_overlay_context_)
  {
    d2d_overlay_context_->SetDpi(dpi.dpi_x, dpi.dpi_y);
  }
}

void GraphicsPipeline::AddOverlayVisual(IDCompositionVisual *visual)
{
  if (!root_visual_ || !swapchain_visual_ || !visual)
    return;
  // Insert the overlay above the swapchain visual.
  root_visual_->AddVisual(visual, TRUE, swapchain_visual_.Get());
}

void GraphicsPipeline::BeginDraw()
{
  if (d2d_context_)
  {
    d2d_context_->BeginDraw();
  }
}

void GraphicsPipeline::EndDraw()
{
  if (d2d_context_)
  {
    HRESULT hr = d2d_context_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
      (void)CreateDeviceResources();
      (void)CreateWindowSizeResources();
    }
    else if (FAILED(hr))
    {
      last_hr_ = hr;
    }
  }
}

void GraphicsPipeline::Present(bool vsync)
{
  if (swap_chain_)
  {
    const UINT interval = vsync ? fluxent::config::Graphics::PresentVsyncInterval
                                : fluxent::config::Graphics::PresentNoVsyncInterval;
    HRESULT hr = swap_chain_->Present(interval, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
      (void)CreateDeviceResources();
      (void)CreateWindowSizeResources();
    }
    else if (FAILED(hr))
    {
      last_hr_ = hr;
    }
  }
}

void GraphicsPipeline::Clear(const Color &color)
{
  if (d2d_context_)
  {
    d2d_context_->Clear(color.to_d2d());
  }
}

void GraphicsPipeline::Commit()
{
  if (dcomp_device_)
  {
    dcomp_device_->Commit();
  }
}

ComPtr<ID2D1SolidColorBrush> GraphicsPipeline::CreateSolidBrush(const Color &color)
{
  ComPtr<ID2D1SolidColorBrush> brush;
  if (d2d_context_)
  {
    d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
  }
  return brush;
}

Size GraphicsPipeline::GetRenderTargetSize() const
{
  if (d2d_context_)
  {
    D2D1_SIZE_F size = d2d_context_->GetSize();
    return Size(size.width, size.height);
  }
  return Size();
}

Size GraphicsPipeline::GetClientPixelSize() const
{
  if (!hwnd_)
    return Size();
  RECT rc{};
  if (!GetClientRect(hwnd_, &rc))
    return Size();
  const float w = static_cast<float>(std::max<LONG>(0, rc.right - rc.left));
  const float h = static_cast<float>(std::max<LONG>(0, rc.bottom - rc.top));
  return Size(w, h);
}

} // namespace fluxent
