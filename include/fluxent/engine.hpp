#pragma once

// FluXent render engine

#include "types.hpp"
#include "graphics.hpp"
#include "text.hpp"
#include <xent/view.hpp>
#include <memory>
#include <unordered_map>

namespace fluxent {

class RenderEngine {
public:
    explicit RenderEngine(GraphicsPipeline* graphics);
    ~RenderEngine();
    
    RenderEngine(const RenderEngine&) = delete;
    RenderEngine& operator=(const RenderEngine&) = delete;

    void render(const xent::View& root);

    void render_frame(xent::View& root);

    void fill_rect(const Rect& rect, const Color& color);
    void fill_rounded_rect(const Rect& rect, float radius, const Color& color);
    void draw_rect(const Rect& rect, const Color& color, float stroke_width = 1.0f);
    void draw_rounded_rect(const Rect& rect, float radius, const Color& color, float stroke_width = 1.0f);

    void push_clip(const Rect& rect);
    void pop_clip();

    void push_transform(const D2D1_MATRIX_3X2_F& transform);
    void pop_transform();

    void push_translation(float x, float y);

    void clear_brush_cache();

    void set_debug_mode(bool enabled) { debug_mode_ = enabled; }
    bool is_debug_mode() const { return debug_mode_; }

    TextRenderer* get_text_renderer() const { return text_renderer_.get(); }

private:
    // Text measurement callback (no lambda).
    std::pair<float, float> measure_text_callback(const std::string& text, float font_size, float max_width);

    void draw_view_recursive(
        const xent::View& view,
        float parent_x,
        float parent_y
    );

    void draw_view_data_recursive(
        const std::shared_ptr<xent::ViewData>& data,
        float parent_x,
        float parent_y
    );

    void draw_view_background(
        const xent::ViewData& data,
        const Rect& bounds
    );

    void draw_view_border(
        const xent::ViewData& data,
        const Rect& bounds
    );

    void draw_view_text(
        const xent::ViewData& data,
        const Rect& bounds
    );

    ID2D1SolidColorBrush* get_brush(const Color& color);

    static Color convert_color(const xent::Color& c) {
        return Color(c.r, c.g, c.b, c.a);
    }

    GraphicsPipeline* graphics_;
    ID2D1DeviceContext* d2d_context_ = nullptr;
    std::unique_ptr<TextRenderer> text_renderer_;

    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> brush_cache_;

    std::vector<D2D1_MATRIX_3X2_F> transform_stack_;

    bool debug_mode_ = false;
};

} // namespace fluxent
