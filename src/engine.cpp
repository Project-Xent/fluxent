// FluXent Render Engine implementation
#include "fluxent/engine.hpp"
#include <stdexcept>
#include <functional>

namespace fluxent {

RenderEngine::RenderEngine(GraphicsPipeline* graphics)
    : graphics_(graphics) {
    if (!graphics_) {
        throw std::invalid_argument("Graphics pipeline cannot be null");
    }
    d2d_context_ = graphics_->get_d2d_context();
    text_renderer_ = std::make_unique<TextRenderer>(graphics_);
}

RenderEngine::~RenderEngine() = default;

// Text measurement callback
std::pair<float, float> RenderEngine::measure_text_callback(const std::string& text, float font_size, float max_width) {
    if (text.empty()) return {0.0f, 0.0f};
    
    std::wstring wtext(text.begin(), text.end());
    
    TextStyle style;
    style.font_size = font_size;
    
    Size measured = text_renderer_->measure_text(wtext, style, max_width > 0 ? max_width : 0.0f);
    return {measured.width, measured.height};
}

void RenderEngine::render(const xent::View& root) {
    if (!d2d_context_) return;

    draw_view_recursive(root, 0.0f, 0.0f);
}

void RenderEngine::render_frame(xent::View& root) {
    if (!graphics_) return;
    
    Size size = graphics_->get_render_target_size();
    xent::set_text_measure_func(
        std::bind(&RenderEngine::measure_text_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );
    
    auto root_data = root.data();
    if (root_data) {
        root_data->calculate_layout(size.width, size.height);
    }
    
    graphics_->begin_draw();
    graphics_->clear(Color::transparent());
    render(root);
    graphics_->end_draw();
    graphics_->present();
}

void RenderEngine::draw_view_recursive(
    const xent::View& view,
    float parent_x,
    float parent_y
) {
    auto view_data = view.data();
    if (!view_data) return;

    draw_view_data_recursive(view_data, parent_x, parent_y);
}

void RenderEngine::draw_view_data_recursive(
    const std::shared_ptr<xent::ViewData>& data,
    float parent_x,
    float parent_y
) {
    if (!data) return;
    
    float x = data->layout_x();
    float y = data->layout_y();
    float w = data->layout_width();
    float h = data->layout_height();
    float abs_x = parent_x + x;
    float abs_y = parent_y + y;
    Rect bounds(abs_x, abs_y, w, h);
    
    draw_view_background(*data, bounds);
    draw_view_border(*data, bounds);
    draw_view_text(*data, bounds);
    
    if (debug_mode_) {
        draw_rect(bounds, Color(255, 0, 255, 128), 1.0f);
    }
    
    for (const auto& child : data->children) {
        draw_view_data_recursive(child, abs_x, abs_y);
    }
}

void RenderEngine::draw_view_background(
    const xent::ViewData& data,
    const Rect& bounds
) {
    Color color = convert_color(data.background_color);
    if (color.is_transparent()) return;
    
    float radius = data.corner_radius;
    
    if (radius > 0) {
        fill_rounded_rect(bounds, radius, color);
    } else {
        fill_rect(bounds, color);
    }
}

void RenderEngine::draw_view_border(
    const xent::ViewData& data,
    const Rect& bounds
) {
    float border_width = data.border_width;
    if (border_width <= 0) return;
    
    Color color = convert_color(data.border_color);
    if (color.is_transparent()) return;
    
    float radius = data.corner_radius;
    
    if (radius > 0) {
        draw_rounded_rect(bounds, radius, color, border_width);
    } else {
        draw_rect(bounds, color, border_width);
    }
}

void RenderEngine::draw_view_text(
    const xent::ViewData& data,
    const Rect& bounds
) {
    const std::string& text = data.text_content;
    if (text.empty()) return;
    
    std::wstring wtext(text.begin(), text.end());
    
    TextStyle style;
    style.font_size = data.font_size;
    style.color = convert_color(data.text_color);
    style.alignment = TextAlignment::Center;
    style.paragraph_alignment = ParagraphAlignment::Center;
    
    text_renderer_->draw_text(wtext, bounds, style);
}

// Primitives

void RenderEngine::fill_rect(const Rect& rect, const Color& color) {
    if (!d2d_context_) return;
    
    auto* brush = get_brush(color);
    if (brush) {
        d2d_context_->FillRectangle(rect.to_d2d(), brush);
    }
}

void RenderEngine::fill_rounded_rect(const Rect& rect, float radius, const Color& color) {
    if (!d2d_context_) return;
    
    auto* brush = get_brush(color);
    if (brush) {
        D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect.to_d2d(), radius, radius);
        d2d_context_->FillRoundedRectangle(rounded, brush);
    }
}

void RenderEngine::draw_rect(const Rect& rect, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    
    auto* brush = get_brush(color);
    if (brush) {
        d2d_context_->DrawRectangle(rect.to_d2d(), brush, stroke_width);
    }
}

void RenderEngine::draw_rounded_rect(const Rect& rect, float radius, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    
    auto* brush = get_brush(color);
    if (brush) {
        D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect.to_d2d(), radius, radius);
        d2d_context_->DrawRoundedRectangle(rounded, brush, stroke_width);
    }
}

// Clipping

void RenderEngine::push_clip(const Rect& rect) {
    if (d2d_context_) {
        d2d_context_->PushAxisAlignedClip(rect.to_d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void RenderEngine::pop_clip() {
    if (d2d_context_) {
        d2d_context_->PopAxisAlignedClip();
    }
}

// Transform

void RenderEngine::push_transform(const D2D1_MATRIX_3X2_F& transform) {
    if (!d2d_context_) return;
    
    D2D1_MATRIX_3X2_F current;
    d2d_context_->GetTransform(&current);
    transform_stack_.push_back(current);
    
    D2D1_MATRIX_3X2_F combined = transform * current;
    d2d_context_->SetTransform(combined);
}

void RenderEngine::pop_transform() {
    if (!d2d_context_ || transform_stack_.empty()) return;
    
    d2d_context_->SetTransform(transform_stack_.back());
    transform_stack_.pop_back();
}

void RenderEngine::push_translation(float x, float y) {
    push_transform(D2D1::Matrix3x2F::Translation(x, y));
}

// Resources

ID2D1SolidColorBrush* RenderEngine::get_brush(const Color& color) {
    uint32_t key = (static_cast<uint32_t>(color.r) << 24) |
                   (static_cast<uint32_t>(color.g) << 16) |
                   (static_cast<uint32_t>(color.b) << 8) |
                   static_cast<uint32_t>(color.a);
    
    auto it = brush_cache_.find(key);
    if (it != brush_cache_.end()) {
        return it->second.Get();
    }
    
    ComPtr<ID2D1SolidColorBrush> brush;
    if (d2d_context_) {
        d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
        brush_cache_[key] = brush;
        return brush.Get();
    }
    
    return nullptr;
}

void RenderEngine::clear_brush_cache() {
    brush_cache_.clear();
}

} // namespace fluxent
