#pragma once

// FluXent graphics pipeline (D3D11/D2D1/DComp)

#include "types.hpp"

namespace fluxent {

class GraphicsPipeline {
public:
    GraphicsPipeline();
    ~GraphicsPipeline();
    
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    void attach_to_window(HWND hwnd);
    void resize(int width, int height);

    ID2D1DeviceContext* get_d2d_context() const { return d2d_context_.Get(); }

    IDWriteFactory* get_dwrite_factory() const { return dwrite_factory_.Get(); }

    void begin_draw();
    void end_draw();

    void present(bool vsync = true);

    void clear(const Color& color = Color::transparent());

    void commit();

    // Schedules a WM_PAINT by invalidating the attached HWND.
    // Used to drive short UI animations without a dedicated timer.
    void request_redraw();

    // DirectComposition access (for compositor-driven animations/layers)
    IDCompositionDevice* get_dcomp_device() const { return dcomp_device_.Get(); }
    IDCompositionVisual* get_dcomp_root_visual() const { return root_visual_.Get(); }
    ID2D1DeviceContext* get_d2d_overlay_context() const { return d2d_overlay_context_.Get(); }

    // Inserts a visual above the swapchain visual.
    void add_overlay_visual(IDCompositionVisual* visual);

    ComPtr<ID2D1SolidColorBrush> create_solid_brush(const Color& color);

    Size get_render_target_size() const;

    // Returns the HWND client size in physical pixels.
    // Useful for verifying DPI scaling (compare against get_render_target_size()).
    Size get_client_pixel_size() const;

    DpiInfo get_dpi() const { return dpi_; }
    void set_dpi(const DpiInfo& dpi);

private:
    void create_device_independent_resources();
    void create_device_resources();
    void create_window_size_resources();
    void release_window_size_resources();
    
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
};

} // namespace fluxent
