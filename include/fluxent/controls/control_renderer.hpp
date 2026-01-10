#pragma once

// FluXent control renderer (Fluent-style)

#include "../graphics.hpp"
#include "../text.hpp"
#include <xent/view.hpp>

namespace fluxent::controls {

// ControlState
struct ControlState {
    bool is_hovered = false;
    bool is_pressed = false;
    bool is_focused = false;
    bool is_disabled = false;
};

// ControlRenderer
class ControlRenderer {
public:
    ControlRenderer(GraphicsPipeline* graphics, TextRenderer* text);
    ~ControlRenderer() = default;
    
    void render(
        const xent::View& view,
        const Rect& bounds,
        const ControlState& state
    );

    void render_button(
        const xent::ViewData& data,
        const Rect& bounds,
        const ControlState& state
    );
    
    void render_text(
        const xent::ViewData& data,
        const Rect& bounds
    );
    
    void render_card(
        const xent::ViewData& data,
        const Rect& bounds
    );
    
    void render_divider(
        const Rect& bounds,
        bool horizontal = true
    );
    
    void render_view(
        const xent::ViewData& data,
        const Rect& bounds
    );
    
private:
    ID2D1SolidColorBrush* get_brush(const Color& color);

    // Elevation border (gradient stroke).
    void draw_elevation_border(const Rect& bounds, float corner_radius);

    // Focus rectangle.
    void draw_focus_rect(const Rect& bounds, float corner_radius);
    
    GraphicsPipeline* graphics_;
    TextRenderer* text_renderer_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

} // namespace fluxent::controls
