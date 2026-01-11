// FluXent Control Renderer implementation
#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

// DirectComposition (IDCompositionVisual3, IDCompositionAnimation)
#include <dcomp.h>

namespace fluxent::controls {

static constexpr double kButtonBrushTransitionSeconds = 0.083; // Matches CommonStyles Button BrushTransition.
static constexpr float kControlContentThemeFontSize = 14.0f; // WinUI3 ControlContentThemeFontSize

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

void ControlRenderer::begin_frame() {
    frame_hovered_button_ = nullptr;
    hover_corner_radius_ = 0.0f;
    has_active_bg_transitions_ = false;
    frame_buttons_seen_.clear();
}

void ControlRenderer::end_frame() {
    update_hover_overlay();

    // Clean up transition entries for buttons that no longer exist.
    if (frame_buttons_seen_.empty()) {
        button_bg_transitions_.clear();
    } else {
        for (auto it = button_bg_transitions_.begin(); it != button_bg_transitions_.end();) {
            if (frame_buttons_seen_.find(it->first) == frame_buttons_seen_.end()) {
                it = button_bg_transitions_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Drive short BrushTransition animations without a dedicated timer.
    if (has_active_bg_transitions_ && graphics_) {
        graphics_->request_redraw();
    }
}

static bool color_equal_rgba(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

static float srgb_to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb(float c) {
    if (c <= 0.0031308f) return 12.92f * c;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

static Color lerp_color_srgb(const Color& a, const Color& b, float t) {
    t = clamp01(t);

    const float ar = a.r / 255.0f;
    const float ag = a.g / 255.0f;
    const float ab = a.b / 255.0f;
    const float aa = a.a / 255.0f;

    const float br = b.r / 255.0f;
    const float bg = b.g / 255.0f;
    const float bb = b.b / 255.0f;
    const float ba = b.a / 255.0f;

    const float lr = srgb_to_linear(ar) + (srgb_to_linear(br) - srgb_to_linear(ar)) * t;
    const float lg = srgb_to_linear(ag) + (srgb_to_linear(bg) - srgb_to_linear(ag)) * t;
    const float lb = srgb_to_linear(ab) + (srgb_to_linear(bb) - srgb_to_linear(ab)) * t;
    const float la = aa + (ba - aa) * t;

    const float sr = clamp01(linear_to_srgb(lr));
    const float sg = clamp01(linear_to_srgb(lg));
    const float sb = clamp01(linear_to_srgb(lb));
    const float sa = clamp01(la);

    return Color(
        static_cast<uint8_t>(std::lround(sr * 255.0f)),
        static_cast<uint8_t>(std::lround(sg * 255.0f)),
        static_cast<uint8_t>(std::lround(sb * 255.0f)),
        static_cast<uint8_t>(std::lround(sa * 255.0f))
    );
}

Color ControlRenderer::animate_button_background(const xent::ViewData* key, const Color& target) {
    auto& s = button_bg_transitions_[key];
    const auto now = std::chrono::steady_clock::now();

    if (!s.initialized) {
        s.initialized = true;
        s.active = false;
        s.current = target;
        s.from = target;
        s.to = target;
        s.last_target = target;
        s.start = now;
        return target;
    }

    // First, advance any in-flight animation.
    if (s.active) {
        const float elapsed = std::chrono::duration<float>(now - s.start).count();
        const float t = clamp01(elapsed / static_cast<float>(kButtonBrushTransitionSeconds));
        s.current = lerp_color_srgb(s.from, s.to, t);
        if (t >= 1.0f) {
            s.active = false;
            s.current = s.to;
        }
    }

    // Start a new transition if the target changed.
    if (!color_equal_rgba(target, s.last_target)) {
        s.from = s.current;
        s.to = target;
        s.last_target = target;
        s.start = now;
        s.active = true;
    }

    if (s.active) {
        has_active_bg_transitions_ = true;
    }
    return s.current;
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
    const xent::ViewData& data,
    const Rect& bounds,
    const ControlState& state
) {
    const auto& type = data.component_type;
    
    if (type == "Button") {
        frame_buttons_seen_.insert(&data);
        // WinUI3 DefaultButtonStyle uses Background brush transitions rather than a separate
        // overlay visual. We keep the DComp hover overlay infrastructure for future use, but
        // disable it here to avoid double-applying pointer-over visuals.
        render_button(data, bounds, state);
    } else if (type == "Text") {
        render_text(data, bounds);
    } else if (type == "Card") {
        render_card(data, bounds);
    } else if (type == "Divider") {
        render_divider(bounds, true);
    } else {
        render_view(data, bounds);
    }
}

void ControlRenderer::ensure_hover_overlay_surface(int width, int height, const Color& color) {
    if (!graphics_) return;

    width = std::max(1, width);
    height = std::max(1, height);

    auto* dcomp = graphics_->get_dcomp_device();
    auto* d2d = graphics_->get_d2d_overlay_context();
    if (!dcomp || !d2d) return;

    bool need_recreate = !hover_surface_
        || hover_surface_width_ != width
        || hover_surface_height_ != height;

    if (need_recreate) {
        hover_surface_.Reset();
        throw_if_failed(
            dcomp->CreateSurface(
                width,
                height,
                DXGI_FORMAT_B8G8R8A8_UNORM,
                DXGI_ALPHA_MODE_PREMULTIPLIED,
                &hover_surface_
            ),
            "Failed to create DComp hover surface"
        );

        hover_surface_width_ = width;
        hover_surface_height_ = height;
        hover_surface_color_ = Color::transparent();
    }

    if (!need_recreate &&
        hover_surface_color_.r == color.r &&
        hover_surface_color_.g == color.g &&
        hover_surface_color_.b == color.b &&
        hover_surface_color_.a == color.a) {
        return;
    }

    RECT update = {0, 0, width, height};
    POINT offset = {};
    ComPtr<IDXGISurface> dxgi_surface;
    throw_if_failed(
        hover_surface_->BeginDraw(&update, IID_PPV_ARGS(&dxgi_surface), &offset),
        "Failed to BeginDraw on DComp surface"
    );

    const auto dpi = graphics_->get_dpi();
    const float scale_x = dpi.dpi_x / 96.0f;
    const float scale_y = dpi.dpi_y / 96.0f;
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi.dpi_x,
        dpi.dpi_y
    );

    ComPtr<ID2D1Bitmap1> bitmap;
    throw_if_failed(
        d2d->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &props, &bitmap),
        "Failed to create D2D bitmap for DComp surface"
    );

    ComPtr<ID2D1Image> old_target;
    d2d->GetTarget(&old_target);
    d2d->SetTarget(bitmap.Get());

    D2D1_MATRIX_3X2_F old_transform;
    d2d->GetTransform(&old_transform);
    // BeginDraw gives the update offset in physical pixels; convert to DIPs for this D2D context.
    const float offset_x_dip = scale_x > 0.0f ? (static_cast<float>(offset.x) / scale_x) : 0.0f;
    const float offset_y_dip = scale_y > 0.0f ? (static_cast<float>(offset.y) / scale_y) : 0.0f;
    d2d->SetTransform(D2D1::Matrix3x2F::Translation(offset_x_dip, offset_y_dip));

    d2d->BeginDraw();

    // Draw rounded overlay matching the hovered control's corner radius.
    // Use transparent clear so we don't tint outside the rounded rect.
    d2d->Clear(D2D1::ColorF(0, 0, 0, 0));

    const float width_dip = scale_x > 0.0f ? (static_cast<float>(width) / scale_x) : static_cast<float>(width);
    const float height_dip = scale_y > 0.0f ? (static_cast<float>(height) / scale_y) : static_cast<float>(height);

    float radius = std::max(0.0f, hover_corner_radius_);
    radius = std::min(radius, std::min(width_dip, height_dip) * 0.5f);

    ComPtr<ID2D1SolidColorBrush> overlay_brush;
    if (SUCCEEDED(d2d->CreateSolidColorBrush(color.to_d2d(), &overlay_brush)) && overlay_brush) {
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF(0.0f, 0.0f, width_dip, height_dip),
            radius,
            radius
        );
        d2d->FillRoundedRectangle(rr, overlay_brush.Get());
    }

    HRESULT hr = d2d->EndDraw();

    d2d->SetTransform(old_transform);

    d2d->SetTarget(old_target.Get());
    if (FAILED(hr)) {
        hover_surface_->EndDraw();
        return;
    }
    hover_surface_->EndDraw();
    hover_surface_color_ = color;
}

static void animate_opacity(GraphicsPipeline* graphics, IDCompositionVisual* visual, float from, float to) {
    if (!graphics || !visual) return;
    (void)from;

#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
    auto* dcomp = graphics->get_dcomp_device();
    if (!dcomp) return;

    ComPtr<IDCompositionVisual3> v3;
    if (FAILED(visual->QueryInterface(IID_PPV_ARGS(&v3))) || !v3) {
        // If Visual3 isn't available at runtime, fall back to non-animated opacity.
        visual->SetOpacity(to);
        graphics->commit();
        return;
    }

    ComPtr<IDCompositionAnimation> anim;
    if (FAILED(dcomp->CreateAnimation(&anim)) || !anim) {
        v3->SetOpacity(to);
        graphics->commit();
        return;
    }

    const double duration = kButtonBrushTransitionSeconds;
    const float slope = static_cast<float>((to - from) / duration);

    // Linear polynomial: opacity(t) = from + slope * t
    anim->AddCubic(0.0, from, slope, 0.0f, 0.0f);
    anim->End(duration, to);

    v3->SetOpacity(anim.Get());
    graphics->commit();
#else
    // Some toolchains/headers (e.g. older clang WinSDK shims) don't expose Visual3,
    // so we compile out opacity animation. (Hover visuals still function; just no fade.)
    (void)to;
    graphics->commit();
#endif
}

void ControlRenderer::update_hover_overlay() {
    if (!graphics_) return;

    auto* dcomp = graphics_->get_dcomp_device();
    auto* root = graphics_->get_dcomp_root_visual();
    if (!dcomp || !root) return;

    const bool should_show = (frame_hovered_button_ != nullptr);
    const float target_opacity = should_show ? 1.0f : 0.0f;

    if (!hover_visual_) {
        throw_if_failed(dcomp->CreateVisual(&hover_visual_), "Failed to create hover visual");
        // Initialize opacity only when the SDK exposes Visual3.
#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
        if (ComPtr<IDCompositionVisual3> v3; SUCCEEDED(hover_visual_.As(&v3)) && v3) {
            v3->SetOpacity(0.0f);
        }
#endif
    }

    if (!hover_visual_added_) {
        graphics_->add_overlay_visual(hover_visual_.Get());
        hover_visual_added_ = true;
    }

    const auto dpi = graphics_->get_dpi();
    const float scale_x = dpi.dpi_x / 96.0f;
    const float scale_y = dpi.dpi_y / 96.0f;

    // Update surface content/size whenever hover target exists.
    if (should_show) {
        // Match CommonStyles/Button_themeresources.xaml ButtonPointerOverBackgroundThemeBrush.
        // Default(Dark): #21FFFFFF, Light: #D1CDCDCD
        Color overlay;
        switch (theme::current_mode()) {
        case theme::Mode::Light:
            overlay = Color(0xCD, 0xCD, 0xCD, 0xD1);
            break;
        case theme::Mode::Dark:
        case theme::Mode::HighContrast:
        default:
            overlay = Color(255, 255, 255, 0x21);
            break;
        }

        const int width_px = std::max(1, static_cast<int>(std::lround(hover_bounds_.width * scale_x)));
        const int height_px = std::max(1, static_cast<int>(std::lround(hover_bounds_.height * scale_y)));

        ensure_hover_overlay_surface(width_px, height_px, overlay);

        if (hover_surface_) {
            hover_visual_->SetContent(hover_surface_.Get());
        }

        hover_visual_->SetOffsetX(static_cast<float>(std::lround(hover_bounds_.x * scale_x)));
        hover_visual_->SetOffsetY(static_cast<float>(std::lround(hover_bounds_.y * scale_y)));
    } else {
        hover_corner_radius_ = 0.0f;
    }

    if (target_opacity != hover_opacity_) {
        const float from = hover_opacity_;
        hover_opacity_ = target_opacity;
        animate_opacity(graphics_, hover_visual_.Get(), from, target_opacity);
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
    Color user_bg = to_fluxent_color(data.background_color);
    auto color_equal = [](const Color& a, const Color& b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    bool is_accent = (user_bg.a > 0 && color_equal(user_bg, res.AccentDefault));
    
    // WinUI3 DefaultButtonStyle uses BackgroundSizing="InnerBorderEdge".
    // AccentButtonStyle uses BackgroundSizing="OuterBorderEdge".
    // Model this by insetting the fill for non-accent buttons by the border thickness.
    constexpr float kBorderThickness = 1.0f;
    const float fill_inset = is_accent ? 0.0f : kBorderThickness;
    const float fill_radius = std::max(0.0f, corner_radius - fill_inset);

    const float fill_left = bounds.x + fill_inset;
    const float fill_top = bounds.y + fill_inset;
    const float fill_width = std::max(0.0f, bounds.width - (fill_inset * 2.0f));
    const float fill_height = std::max(0.0f, bounds.height - (fill_inset * 2.0f));
    const float fill_right = fill_left + fill_width;
    const float fill_bottom = fill_top + fill_height;

    D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
        D2D1::RectF(fill_left, fill_top, fill_right, fill_bottom),
        fill_radius, fill_radius
    );
    
    Color fill_color;
    Color stroke_color;
    Color text_color;

    const bool use_elevation_border = !state.is_disabled && !state.is_pressed;
    
    // State mapping follows CommonStyles/Button_themeresources.xaml:
    // - Default button: Background uses ControlFill*; Foreground uses TextFill* (Pressed uses Secondary).
    // - Accent button: Background uses AccentFill*; Foreground pressed uses TextOnAccentSecondary;
    //   Disabled uses TextOnAccentDisabled; Border pressed/disabled is Transparent.
    if (state.is_disabled) {
        if (is_accent) {
            fill_color = res.AccentDisabled;
            stroke_color = Color::transparent();
            text_color = res.TextOnAccentDisabled;
        } else {
            fill_color = res.ControlFillDisabled;
            stroke_color = res.ControlStrokeDefault;
            text_color = res.TextDisabled;
        }
    } else if (is_accent) {
        if (state.is_pressed) {
            fill_color = res.AccentTertiary;
            text_color = res.TextOnAccentSecondary;
        } else if (state.is_hovered) {
            fill_color = res.AccentSecondary;
            text_color = res.TextOnAccentPrimary;
        } else {
            fill_color = res.AccentDefault;
            text_color = res.TextOnAccentPrimary;
        }
        stroke_color = Color::transparent();
    } else {
        if (state.is_pressed) {
            fill_color = res.ControlFillTertiary;
            stroke_color = res.ControlStrokeDefault;
            text_color = res.TextSecondary;
        } else if (state.is_hovered) {
            fill_color = res.ControlFillSecondary;
            stroke_color = Color::transparent();
            text_color = res.TextPrimary;
        } else {
            fill_color = res.ControlFillDefault;
            stroke_color = Color::transparent();
            text_color = res.TextPrimary;
        }
    }

    // Allow a custom background override for non-accent buttons only.
    // (Accent buttons must keep the AccentFill* state mapping to match WinUI3.)
    if (!is_accent && user_bg.a > 0 && !state.is_disabled) {
        fill_color = user_bg;
        if (state.is_pressed) {
            fill_color.a = static_cast<uint8_t>(fill_color.a * 0.8f);
        } else if (state.is_hovered) {
            fill_color.a = static_cast<uint8_t>(fill_color.a * 0.9f);
        }
    }

    // WinUI3 ContentPresenter.BackgroundTransition: BrushTransition Duration=0.083s.
    fill_color = animate_button_background(&data, fill_color);

    d2d->FillRoundedRectangle(rounded_rect, get_brush(fill_color));

    if (use_elevation_border) {
        draw_elevation_border(bounds, corner_radius, is_accent);
    } else if (stroke_color.a > 0) {
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
        Color actual_text_color = user_text.a > 0 ? user_text : text_color;
        
        std::wstring wtext(data.text_content.begin(), data.text_content.end());
        TextStyle style;
        style.font_size = data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;
        style.word_wrap = false;
        style.color = actual_text_color;
        style.alignment = TextAlignment::Center;
        style.paragraph_alignment = ParagraphAlignment::Center;

        // Respect Yoga padding (matches ContentPresenter Padding).
        Rect content_bounds = bounds;
        content_bounds.x += data.padding_left;
        content_bounds.y += data.padding_top;
        content_bounds.width = std::max(0.0f, content_bounds.width - (data.padding_left + data.padding_right));
        content_bounds.height = std::max(0.0f, content_bounds.height - (data.padding_top + data.padding_bottom));

        // Clip to the control bounds so long strings don't draw outside (WinUI clips).
        d2d->PushAxisAlignedClip(bounds.to_d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        text_renderer_->draw_text(wtext, content_bounds, style);
        d2d->PopAxisAlignedClip();
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
    
    float font_size = data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;
    
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

void ControlRenderer::draw_elevation_border(const Rect& bounds, float corner_radius, bool is_accent) {
    // Matches CommonStyles/Common_themeresources_any.xaml:
    // LinearGradientBrush MappingMode="Absolute" StartPoint="0,0" EndPoint="0,3"
    // Stops: 0.33 -> ControlStrokeColorSecondary, 1.0 -> ControlStrokeColorDefault
    if (!graphics_) return;
    auto* d2d = graphics_->get_d2d_context();
    if (!d2d) return;

    const auto& res = theme::res();

    // Use only the gradient stroke anchored to the control bottom to produce
    // the elevation edge (matches WinUI CommonStyles ControlElevationBorderBrush).
    // Do not draw a separate shifted secondary stroke which biased the ring upward.

    const bool need_recreate =
        !control_elevation_border_brush_ ||
        control_elevation_border_secondary_.r != res.ControlStrokeSecondary.r ||
        control_elevation_border_secondary_.g != res.ControlStrokeSecondary.g ||
        control_elevation_border_secondary_.b != res.ControlStrokeSecondary.b ||
        control_elevation_border_secondary_.a != res.ControlStrokeSecondary.a ||
        control_elevation_border_default_.r != res.ControlStrokeDefault.r ||
        control_elevation_border_default_.g != res.ControlStrokeDefault.g ||
        control_elevation_border_default_.b != res.ControlStrokeDefault.b ||
        control_elevation_border_default_.a != res.ControlStrokeDefault.a ||
        !control_accent_elevation_border_brush_ ||
        control_accent_elevation_border_secondary_.r != res.ControlStrokeOnAccentSecondary.r ||
        control_accent_elevation_border_secondary_.g != res.ControlStrokeOnAccentSecondary.g ||
        control_accent_elevation_border_secondary_.b != res.ControlStrokeOnAccentSecondary.b ||
        control_accent_elevation_border_secondary_.a != res.ControlStrokeOnAccentSecondary.a ||
        control_accent_elevation_border_default_.r != res.ControlStrokeOnAccentDefault.r ||
        control_accent_elevation_border_default_.g != res.ControlStrokeOnAccentDefault.g ||
        control_accent_elevation_border_default_.b != res.ControlStrokeOnAccentDefault.b ||
        control_accent_elevation_border_default_.a != res.ControlStrokeOnAccentDefault.a;

    if (need_recreate) {
        control_elevation_border_brush_.Reset();

        D2D1_GRADIENT_STOP stops[2] = {
            {0.33f, res.ControlStrokeSecondary.to_d2d()},
            {1.0f, res.ControlStrokeDefault.to_d2d()},
        };

        ComPtr<ID2D1GradientStopCollection> stop_collection;
        throw_if_failed(
            d2d->CreateGradientStopCollection(
                stops,
                2,
                D2D1_GAMMA_2_2,
                D2D1_EXTEND_MODE_CLAMP,
                &stop_collection
            ),
            "Failed to create elevation border gradient stop collection"
        );

        // Start/end points are set per-draw so the gradient anchors to this control's top.
        throw_if_failed(
            d2d->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f), D2D1::Point2F(0.0f, 3.0f)),
                stop_collection.Get(),
                &control_elevation_border_brush_
            ),
            "Failed to create elevation border brush"
        );

        control_elevation_border_secondary_ = res.ControlStrokeSecondary;
        control_elevation_border_default_ = res.ControlStrokeDefault;

        // Create accent variant using on-accent stroke colors.
        D2D1_GRADIENT_STOP accent_stops[2] = {
            {0.33f, res.ControlStrokeOnAccentSecondary.to_d2d()},
            {1.0f, res.ControlStrokeOnAccentDefault.to_d2d()},
        };
        ComPtr<ID2D1GradientStopCollection> accent_stop_collection;
        throw_if_failed(
            d2d->CreateGradientStopCollection(
                accent_stops,
                2,
                D2D1_GAMMA_2_2,
                D2D1_EXTEND_MODE_CLAMP,
                &accent_stop_collection
            ),
            "Failed to create accent elevation gradient stop collection"
        );

        control_accent_elevation_border_brush_.Reset();
        throw_if_failed(
            d2d->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f), D2D1::Point2F(0.0f, 3.0f)),
                accent_stop_collection.Get(),
                &control_accent_elevation_border_brush_
            ),
            "Failed to create accent elevation border brush"
        );

        control_accent_elevation_border_secondary_ = res.ControlStrokeOnAccentSecondary;
        control_accent_elevation_border_default_ = res.ControlStrokeOnAccentDefault;
    }

    // Match CommonStyles/Common_themeresources_any.xaml behavior precisely:
    // - MappingMode="Absolute", StartPoint="0,0", EndPoint="0,3"
    // - In Light theme, ControlElevationBorderBrush has ScaleY=-1 (flipped)
    // - AccentControlElevationBorderBrush is flipped (ScaleY=-1)
    // We simulate this by placing the 3-DIP gradient band at the top (not flipped)
    // or bottom (flipped), and swapping start/end for flipped cases.

    const bool is_light = (theme::current_mode() == theme::Mode::Light);
    const bool flip_y = is_accent || is_light;

    ID2D1LinearGradientBrush* brush = control_elevation_border_brush_.Get();
    if (is_accent && control_accent_elevation_border_brush_) {
        brush = control_accent_elevation_border_brush_.Get();
    }

    const float band_start = flip_y ? (bounds.y + bounds.height - 3.0f) : bounds.y;
    const float band_end = band_start + 3.0f;

    if (flip_y) {
        brush->SetStartPoint(D2D1::Point2F(0.0f, band_end));
        brush->SetEndPoint(D2D1::Point2F(0.0f, band_start));
    } else {
        brush->SetStartPoint(D2D1::Point2F(0.0f, band_start));
        brush->SetEndPoint(D2D1::Point2F(0.0f, band_end));
    }

    // Pixel-align the rect (centered stroke will expand inward/outward).
    const float left = bounds.x + 0.5f;
    const float top = bounds.y + 0.5f;
    const float right = bounds.x + bounds.width - 0.5f;
    const float bottom = bounds.y + bounds.height - 0.5f;

    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(left, top, right, bottom),
        corner_radius, corner_radius
    );

    // BorderThickness=1 in Button_themeresources.xaml.
    // The absolute 3-DIP gradient band + clamp makes only the top/bottom edge
    // visibly vary, while the rest stays near the end stop.
    d2d->DrawRoundedRectangle(stroke_rect, brush, 1.0f);
}

} // namespace fluxent::controls
