// FluXent Control Renderer implementation
#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"

namespace fluxent::controls {

static Color to_fluxent_color(const xent::Color& c) {
    return Color(c.r, c.g, c.b, c.a);
}

ControlRenderer::ControlRenderer(GraphicsPipeline* graphics, TextRenderer* text)
    : graphics_(graphics)
    , text_renderer_(text)
{
    auto d2d = graphics_->get_d2d_context();
    d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
}

ID2D1SolidColorBrush* ControlRenderer::get_brush(const Color& color) {
    brush_->SetColor(D2D1::ColorF(
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f
    ));
    return brush_.Get();
}

void ControlRenderer::render(
    const xent::View& view,
    const Rect& bounds,
    const ControlState& state
) {
    auto data = view.data();
    if (!data) return;
    
    const auto& type = data->component_type;
    
    if (type == "Button") {
        render_button(*data, bounds, state);
    } else if (type == "Text") {
        render_text(*data, bounds);
    } else if (type == "Card") {
        render_card(*data, bounds);
    } else if (type == "Divider") {
        render_divider(bounds, true);
    } else {
        render_view(*data, bounds);
    }
}

void ControlRenderer::render_button(
    const xent::ViewData& data,
    const Rect& bounds,
    const ControlState& state
) {
    auto d2d = graphics_->get_d2d_context();
    const auto& res = theme::res();
    
    float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
    bool is_accent = data.background_color.a > 200;
    
    D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
        D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height),
        corner_radius, corner_radius
    );
    
    Color fill_color;
    Color stroke_color;
    Color text_color;
    
    if (state.is_disabled) {
        fill_color = res.ControlFillDisabled;
        stroke_color = Color::transparent();
        text_color = res.TextDisabled;
    } else if (is_accent) {
        if (state.is_pressed) {
            fill_color = res.AccentTertiary;
        } else if (state.is_hovered) {
            fill_color = res.AccentSecondary;
        } else {
            fill_color = res.AccentDefault;
        }
        stroke_color = res.ControlStrokeOnAccentDefault;
        text_color = res.TextOnAccentPrimary;
    } else {
        if (state.is_pressed) {
            fill_color = res.ControlFillTertiary;
        } else if (state.is_hovered) {
            fill_color = res.ControlFillSecondary;
        } else {
            fill_color = res.ControlFillDefault;
        }
        stroke_color = res.ControlStrokeDefault;
        text_color = res.TextPrimary;
    }
    
    Color user_bg = to_fluxent_color(data.background_color);
    if (user_bg.a > 0 && !state.is_disabled) {
        fill_color = user_bg;
        if (state.is_pressed) {
            fill_color.a = static_cast<uint8_t>(fill_color.a * 0.8f);
        } else if (state.is_hovered) {
            fill_color.a = static_cast<uint8_t>(fill_color.a * 0.9f);
        }
    }

    d2d->FillRoundedRectangle(rounded_rect, get_brush(fill_color));

    if (stroke_color.a > 0) {
        D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
            D2D1::RectF(
                bounds.x + 0.5f, bounds.y + 0.5f,
                bounds.x + bounds.width - 0.5f, bounds.y + bounds.height - 0.5f
            ),
            corner_radius, corner_radius
        );
        d2d->DrawRoundedRectangle(stroke_rect, get_brush(stroke_color), 1.0f);
    }

    if (state.is_focused && !state.is_disabled) {
        draw_focus_rect(bounds, corner_radius);
    }

    if (!data.text_content.empty() && text_renderer_) {
        Color user_text = to_fluxent_color(data.text_color);
        Color actual_text_color = state.is_disabled ? res.TextDisabled : 
                                  (user_text.a > 0 ? user_text : text_color);
        
        std::wstring wtext(data.text_content.begin(), data.text_content.end());
        TextStyle style;
        style.font_size = data.font_size > 0 ? data.font_size : 14.0f;
        style.color = actual_text_color;
        style.alignment = TextAlignment::Center;
        style.paragraph_alignment = ParagraphAlignment::Center;
        
        text_renderer_->draw_text(wtext, bounds, style);
    }
}

void ControlRenderer::render_text(
    const xent::ViewData& data,
    const Rect& bounds
) {
    if (!text_renderer_ || data.text_content.empty()) return;
    
    const auto& res = theme::res();

    Color user_color = to_fluxent_color(data.text_color);
    Color text_color = user_color.a > 0 ? user_color : res.TextPrimary;
    
    float font_size = data.font_size > 0 ? data.font_size : 14.0f;
    
    std::wstring wtext(data.text_content.begin(), data.text_content.end());
    TextStyle style;
    style.font_size = font_size;
    style.color = text_color;
    style.alignment = TextAlignment::Leading;
    style.paragraph_alignment = ParagraphAlignment::Near;
    
    text_renderer_->draw_text(wtext, bounds, style);
}

void ControlRenderer::render_card(
    const xent::ViewData& data,
    const Rect& bounds
) {
    auto d2d = graphics_->get_d2d_context();
    const auto& res = theme::res();
    
    float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
    
    D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
        D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height),
        corner_radius, corner_radius
    );

    Color user_bg = to_fluxent_color(data.background_color);
    Color bg = user_bg.a > 0 ? user_bg : res.CardBackgroundDefault;
    d2d->FillRoundedRectangle(rounded_rect, get_brush(bg));

    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(
            bounds.x + 0.5f, bounds.y + 0.5f,
            bounds.x + bounds.width - 0.5f, bounds.y + bounds.height - 0.5f
        ),
        corner_radius, corner_radius
    );
    d2d->DrawRoundedRectangle(stroke_rect, get_brush(res.CardStrokeDefault), 1.0f);
}

void ControlRenderer::render_divider(const Rect& bounds, bool horizontal) {
    auto d2d = graphics_->get_d2d_context();
    const auto& res = theme::res();
    
    if (horizontal) {
        float y = bounds.y + bounds.height / 2.0f;
        d2d->DrawLine(
            D2D1::Point2F(bounds.x, y),
            D2D1::Point2F(bounds.x + bounds.width, y),
            get_brush(res.DividerStrokeDefault),
            1.0f
        );
    } else {
        float x = bounds.x + bounds.width / 2.0f;
        d2d->DrawLine(
            D2D1::Point2F(x, bounds.y),
            D2D1::Point2F(x, bounds.y + bounds.height),
            get_brush(res.DividerStrokeDefault),
            1.0f
        );
    }
}

void ControlRenderer::render_view(
    const xent::ViewData& data,
    const Rect& bounds
) {
    auto d2d = graphics_->get_d2d_context();
    
    Color user_bg = to_fluxent_color(data.background_color);

    if (user_bg.a > 0) {
        float corner_radius = data.corner_radius;
        
        if (corner_radius > 0) {
            D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
                D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height),
                corner_radius, corner_radius
            );
            d2d->FillRoundedRectangle(rounded_rect, get_brush(user_bg));
        } else {
            D2D1_RECT_F rect = D2D1::RectF(
                bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height
            );
            d2d->FillRectangle(rect, get_brush(user_bg));
        }
    }
    
    Color user_border = to_fluxent_color(data.border_color);
    if (data.border_width > 0 && user_border.a > 0) {
        float corner_radius = data.corner_radius;
        float half_border = data.border_width / 2.0f;
        
        if (corner_radius > 0) {
            D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
                D2D1::RectF(
                    bounds.x + half_border, bounds.y + half_border,
                    bounds.x + bounds.width - half_border, bounds.y + bounds.height - half_border
                ),
                corner_radius, corner_radius
            );
            d2d->DrawRoundedRectangle(stroke_rect, get_brush(user_border), data.border_width);
        } else {
            D2D1_RECT_F rect = D2D1::RectF(
                bounds.x + half_border, bounds.y + half_border,
                bounds.x + bounds.width - half_border, bounds.y + bounds.height - half_border
            );
            d2d->DrawRectangle(rect, get_brush(user_border), data.border_width);
        }
    }
    
    if (!data.text_content.empty() && text_renderer_) {
        render_text(data, bounds);
    }
}

void ControlRenderer::draw_focus_rect(const Rect& bounds, float corner_radius) {
    auto d2d = graphics_->get_d2d_context();
    const auto& res = theme::res();

    D2D1_ROUNDED_RECT outer = D2D1::RoundedRect(
        D2D1::RectF(
            bounds.x - 3.0f, bounds.y - 3.0f,
            bounds.x + bounds.width + 3.0f, bounds.y + bounds.height + 3.0f
        ),
        corner_radius + 2.0f, corner_radius + 2.0f
    );
    d2d->DrawRoundedRectangle(outer, get_brush(res.FocusStrokeOuter), 2.0f);

    D2D1_ROUNDED_RECT inner = D2D1::RoundedRect(
        D2D1::RectF(
            bounds.x - 1.0f, bounds.y - 1.0f,
            bounds.x + bounds.width + 1.0f, bounds.y + bounds.height + 1.0f
        ),
        corner_radius + 1.0f, corner_radius + 1.0f
    );
    d2d->DrawRoundedRectangle(inner, get_brush(res.FocusStrokeInner), 1.0f);
}

void ControlRenderer::draw_elevation_border(const Rect& bounds, float corner_radius) {
    // TODO: Implement gradient border (LinearGradientBrush).
    auto d2d = graphics_->get_d2d_context();
    const auto& res = theme::res();
    
    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(
            bounds.x + 0.5f, bounds.y + 0.5f,
            bounds.x + bounds.width - 0.5f, bounds.y + bounds.height - 0.5f
        ),
        corner_radius, corner_radius
    );
    d2d->DrawRoundedRectangle(stroke_rect, get_brush(res.ControlStrokeSecondary), 1.0f);
}

} // namespace fluxent::controls
