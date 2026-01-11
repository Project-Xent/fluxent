#pragma once

// FluXent control renderer (Fluent-style)

#include "../graphics.hpp"
#include "../text.hpp"
#include <xent/view.hpp>

#include <chrono>
#include <unordered_map>
#include <unordered_set>

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

    void begin_frame();
    void end_frame();
    
    void render(
        const xent::ViewData& data,
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
    void update_hover_overlay();
    void ensure_hover_overlay_surface(int width, int height, const Color& color);

    ID2D1SolidColorBrush* get_brush(const Color& color);

    // Elevation border (gradient stroke). `is_accent` selects accent brush.
    void draw_elevation_border(const Rect& bounds, float corner_radius, bool is_accent);

    // Focus rectangle.
    void draw_focus_rect(const Rect& bounds, float corner_radius);

    // WinUI3 ContentPresenter.BackgroundTransition (BrushTransition Duration=0.083s).
    Color animate_button_background(const xent::ViewData* key, const Color& target);

    struct ButtonBackgroundTransition {
        bool initialized = false;
        bool active = false;
        Color current = Color::transparent();
        Color from = Color::transparent();
        Color to = Color::transparent();
        Color last_target = Color::transparent();
        std::chrono::steady_clock::time_point start;
    };
    
    GraphicsPipeline* graphics_;
    TextRenderer* text_renderer_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;

    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> control_elevation_border_brush_;
    Color control_elevation_border_secondary_ = Color::transparent();
    Color control_elevation_border_default_ = Color::transparent();
    Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> control_accent_elevation_border_brush_;
    Color control_accent_elevation_border_secondary_ = Color::transparent();
    Color control_accent_elevation_border_default_ = Color::transparent();

    // DirectComposition hover overlay (single overlay reused across buttons).
    Microsoft::WRL::ComPtr<IDCompositionVisual> hover_visual_;
    Microsoft::WRL::ComPtr<IDCompositionSurface> hover_surface_;
    Rect hover_bounds_;
    float hover_corner_radius_ = 0.0f;
    float hover_opacity_ = 0.0f;
    bool hover_visual_added_ = false;
    const xent::ViewData* frame_hovered_button_ = nullptr;

    int hover_surface_width_ = 0;
    int hover_surface_height_ = 0;
    Color hover_surface_color_ = Color::transparent();

    // WinUI3-style BackgroundTransition (BrushTransition) for button backgrounds.
    std::unordered_map<const xent::ViewData*, ButtonBackgroundTransition> button_bg_transitions_;
    std::unordered_set<const xent::ViewData*> frame_buttons_seen_;
    bool has_active_bg_transitions_ = false;
};

} // namespace fluxent::controls
