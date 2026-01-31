
#pragma once

#include "types.hpp"
#include <memory>

namespace fluxent
{

class GraphicsPipeline
{
public:
  static Result<std::unique_ptr<GraphicsPipeline>> Create();
  ~GraphicsPipeline();

private:
  GraphicsPipeline();
  Result<void> Init();

public:
  GraphicsPipeline(const GraphicsPipeline &) = delete;
  GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;

  Result<void> AttachToWindow(HWND hwnd);
  Result<void> Resize(int width, int height);

  ID2D1DeviceContext *GetD2DContext() const { return d2d_context_.Get(); }
  ID2D1Factory *GetD2DFactory() const { return d2d_factory_.Get(); }

  IDWriteFactory *GetDWriteFactory() const { return dwrite_factory_.Get(); }

  void BeginDraw();
  void EndDraw();

  void Present(bool vsync = true);

  void Clear(const Color &color = Color::transparent());

  void Commit();

  // Schedules a WM_PAINT by invalidating the attached HWND.
  // Used to drive short UI animations without a dedicated timer.
  void RequestRedraw();

  // DirectComposition access (for compositor-driven animations/layers)
  IDCompositionDevice *GetDCompDevice() const { return dcomp_device_.Get(); }
  IDCompositionVisual *GetDCompRootVisual() const { return root_visual_.Get(); }
  ID2D1DeviceContext *GetD2DOverlayContext() const { return d2d_overlay_context_.Get(); }

  // Inserts a visual above the swapchain visual.
  void AddOverlayVisual(IDCompositionVisual *visual);

  ComPtr<ID2D1SolidColorBrush> CreateSolidBrush(const Color &color);

  Size GetRenderTargetSize() const;

  // Returns the HWND client size in physical pixels.
  // Useful for verifying DPI scaling (compare against
  // GetRenderTargetSize()).
  Size GetClientPixelSize() const;

  DpiInfo GetDpi() const { return dpi_; }
  void SetDpi(const DpiInfo &dpi);
  HRESULT GetLastError() const { return last_hr_; }
  void ClearLastError() { last_hr_ = S_OK; }

private:
  Result<void> CreateDeviceIndependentResources();
  Result<void> CreateDeviceResources();
  Result<void> CreateWindowSizeResources();
  void ReleaseWindowSizeResources();

  HWND hwnd_ = nullptr;
  DpiInfo dpi_;

  // D3D11
  ComPtr<ID3D11Device> d3d_device_;
  ComPtr<IDXGIDevice> dxgi_device_;
  ComPtr<IDXGIFactory2> dxgi_factory_;
  ComPtr<IDXGISwapChain1> swap_chain_;

  // D2D
  ComPtr<ID2D1Factory3> d2d_factory_;
  ComPtr<ID2D1Device2> d2d_device_;
  ComPtr<ID2D1DeviceContext2> d2d_context_;
  ComPtr<ID2D1DeviceContext2> d2d_overlay_context_;
  ComPtr<ID2D1Bitmap1> d2d_target_bitmap_;

  // DComp
  ComPtr<IDCompositionDevice> dcomp_device_;
  ComPtr<IDCompositionTarget> dcomp_target_;
  ComPtr<IDCompositionVisual> root_visual_;
  ComPtr<IDCompositionVisual> swapchain_visual_;

  // DWrite
  ComPtr<IDWriteFactory3> dwrite_factory_;

  HRESULT last_hr_ = S_OK;
};

} // namespace fluxent
