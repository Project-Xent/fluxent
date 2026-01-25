#include "fluxent/controls/radio_renderer.hpp"
#include "fluxent/controls/common_drawing.hpp"
#include "fluxent/config.hpp"
#include "fluxent/controls/renderer_utils.hpp"

#include <cmath>

namespace fluxent::controls {

using Microsoft::WRL::ComPtr;

void RadioButtonRenderer::BeginFrame() {
  has_active_transitions_ = false;
  seen_.clear();
}

bool RadioButtonRenderer::EndFrame() {
  for (auto it = check_transitions_.begin(); it != check_transitions_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = check_transitions_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = scale_transitions_.begin(); it != scale_transitions_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = scale_transitions_.erase(it);
    } else {
      ++it;
    }
  }
  return has_active_transitions_;
}

float RadioButtonRenderer::AnimateCheckState(const xent::ViewData *key,
                                             bool is_checked) {
  auto &state = check_transitions_[key];
  float target = is_checked ? 1.0f : 0.0f;

  if (!state.initialized) {
    state.current = target;
    state.to = target;
    state.from = target;
    state.initialized = true;
    return target;
  }

  if (state.to != target) {
    state.from = state.current;
    state.to = target;
    state.start = std::chrono::steady_clock::now();
    state.active = true;
    has_active_transitions_ = true;
  }

  if (state.active) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - state.start).count();
    constexpr double kDuration = fluxent::config::Animation::RadioButton;

    if (elapsed >= kDuration) {
      state.current = state.to;
      state.active = false;
    } else {
      float t =
          Clamp01(static_cast<float>(elapsed / kButtonBrushTransitionSeconds));
      t = 1.0f - std::pow(1.0f - t, 3.0f); // Ease out
      state.current = state.from + (state.to - state.from) * t;
      has_active_transitions_ = true;
    }
  }
  return state.current;
}

float RadioButtonRenderer::AnimateFloat(const xent::ViewData *key, float target,
                                        float duration_ms) {
  auto &state = scale_transitions_[key];
  if (!state.initialized) {
    state.current = target;
    state.to = target;
    state.from = target;
    state.initialized = true;
    return target;
  }
  if (std::abs(state.to - target) > 0.001f) {
    state.from = state.current;
    state.to = target;
    state.start = std::chrono::steady_clock::now();
    state.active = true;
    has_active_transitions_ = true;
  }
  if (state.active) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - state.start).count();
    double duration = duration_ms / 1000.0;
    if (elapsed >= duration) {
      state.current = state.to;
      state.active = false;
    } else {
      float t = Clamp01(static_cast<float>(elapsed / duration));
      t = 1.0f - std::pow(1.0f - t, 3.0f);
      state.current = state.from + (state.to - state.from) * t;
      has_active_transitions_ = true;
    }
  }
  return state.current;
}

void RadioButtonRenderer::Render(const RenderContext &ctx,
                                 const xent::ViewData &data, const Rect &bounds,
                                 const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  // RadioButton logic from XAML:
  // OuterEllipse: 20x20. Stroke 1.
  // CheckGlyph: Size varies (12 Normal, 14 PointerOver, 10 Pressed)

  const bool is_checked = data.is_checked;

  // Animation states
  const float check_t = AnimateCheckState(&data, is_checked);

  // PressedCheckGlyph logic (Unchecked + Pressed)
  // XAML: PressedCheckGlyph Opacity 0->1, Width/Height 4->10 (Spline)
  // We'll approximate this by checking if we are pressed and unchecked.
  // Since we don't have a separate dedicated animation state for
  // "pressed_check" just yet, we can use the `is_pressed` state combined with
  // `!is_checked`. Ideally, we'd animate an opacity float, but immediate
  // feedback is okay for now or we can use animate_float for
  // pressed_check_scale.

  float target_glyph_size = res.RadioButton.GlyphSize;
  if (state.is_pressed)
    target_glyph_size = res.RadioButton.GlyphSizePressed; // 10
  else if (state.is_hovered)
    target_glyph_size = res.RadioButton.GlyphSizePointerOver; // 14

  // The main check glyph size animation
  const float current_glyph_size =
      AnimateFloat(&data, target_glyph_size, fluxent::config::Animation::Normal * 1000.0f); // Fast duration

  const float size = fluxent::config::Layout::RadioButtonSize;
  const float centerY = bounds.y + bounds.height * 0.5f;
  const Rect circle_rect(bounds.x, centerY - size * 0.5f, size, size);
  const D2D1_POINT_2F center =
      D2D1::Point2F(circle_rect.x + size * 0.5f, circle_rect.y + size * 0.5f);

  // Color logic from XAML
  Color outer_fill, outer_stroke, glyph_fill;

  if (state.is_disabled) {
    outer_stroke = res.ControlStrongStrokeDisabled;
    outer_fill = res.ControlAltFillDisabled;
    glyph_fill = is_checked ? res.TextOnAccentDisabled : Color::transparent();
  } else if (is_checked) {
    outer_stroke = state.is_pressed ? res.AccentTertiary
                                    : (state.is_hovered ? res.AccentSecondary
                                                        : res.AccentDefault);
    outer_fill = outer_stroke;
    glyph_fill = res.TextOnAccentPrimary;
  } else {
    // Unchecked
    outer_stroke = state.is_pressed ? res.ControlStrongStrokeDisabled
                                    : res.ControlStrongStrokeDefault;
    outer_fill = state.is_pressed
                     ? res.ControlAltFillQuarternary
                     : (state.is_hovered ? res.ControlAltFillTertiary
                                         : res.ControlAltFillSecondary);
    glyph_fill = Color::transparent();
  }

  ComPtr<ID2D1SolidColorBrush> brush;
  d2d->CreateSolidColorBrush(outer_fill.to_d2d(), &brush);

  // Draw Outer Ellipse
  D2D1_ELLIPSE outer_ellipse =
      D2D1::Ellipse(center, size * 0.5f - 0.5f, size * 0.5f - 0.5f);

  d2d->FillEllipse(outer_ellipse, brush.Get());

  brush->SetColor(outer_stroke.to_d2d());
  d2d->DrawEllipse(outer_ellipse, brush.Get(), 1.0f);

  // Inner Glyph checkmark dot (Checked State)
  if (check_t > 0.01f) {
    float r = (current_glyph_size * 0.5f) * check_t;
    D2D1_ELLIPSE glyph_ellipse = D2D1::Ellipse(center, r, r);
    brush->SetColor(glyph_fill.to_d2d());
    d2d->FillEllipse(glyph_ellipse, brush.Get());
  }

  // PressedCheckGlyph (Unchecked + Pressed) implementation
  // Only visible when Pressed AND Unchecked (and not disabled)
  if (state.is_pressed && !is_checked && !state.is_disabled) {
    // XAML targets: Width="10", Height="10", Fill="{ThemeResource
    // RadioButtonCheckGlyphFillPressed}" RadioButtonCheckGlyphFillPressed ->
    // TextOnAccentFillColorPrimaryBrush -> TextOnAccentPrimary
    const float pressed_glyph_radius = fluxent::config::RadioButton::PressedGlyphSize * 0.5f;
    D2D1_ELLIPSE pressed_glyph =
        D2D1::Ellipse(center, pressed_glyph_radius, pressed_glyph_radius);

    // We can use a simple direct draw here since it's a transient state
    brush->SetColor(res.TextOnAccentPrimary.to_d2d());
    d2d->FillEllipse(pressed_glyph, brush.Get());
  }

  // Text
  if (!data.text_content.empty()) {
    Rect text_rect = bounds;
    text_rect.x += size + res.RadioButton.Gap;
    text_rect.width -= (size + res.RadioButton.Gap);

    TextStyle style;
    style.color = state.is_disabled ? res.TextDisabled : res.TextPrimary;
    style.font_size = fluxent::config::Layout::DefaultFontSize;
    style.alignment = TextAlignment::Leading;
    style.paragraph_alignment = ParagraphAlignment::Center;
    style.word_wrap = false;

    std::wstring wtext(data.text_content.begin(), data.text_content.end());
    ctx.text->DrawText(wtext, text_rect, style);
  }

  if (state.is_focused && !state.is_disabled) {
    DrawFocusRect(ctx, circle_rect, size * 0.5f);
  }
}

} // namespace fluxent::controls
