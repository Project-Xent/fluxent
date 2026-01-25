#include "fluxent/controls/slider_renderer.hpp"
#include "fluxent/controls/layout.hpp"
#include "fluxent/config.hpp"
#include "fluxent/controls/renderer_utils.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent::controls {

// Metrics matching WinUI
// Metrics matching WinUI
static constexpr float kSliderInnerThumbSize = fluxent::config::Layout::SliderInnerThumbSize;
static constexpr double kSliderAnimationSeconds =
    fluxent::config::Animation::Slider;

// Visual States Scale Factors (relative to InnerThumbSize)
// Visual States Scale Factors (relative to InnerThumbSize)
static constexpr float kScaleNormal = fluxent::config::Layout::Slider::ScaleNormal;
static constexpr float kScaleHover = fluxent::config::Layout::Slider::ScaleHover; 
static constexpr float kScalePressed = fluxent::config::Layout::Slider::ScalePressed;

void SliderRenderer::BeginFrame() {
  has_active_transitions_ = false;
  seen_.clear();
}

bool SliderRenderer::EndFrame() {
  for (auto it = states_.begin(); it != states_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = states_.erase(it);
    } else {
      ++it;
    }
  }
  return has_active_transitions_;
}

float SliderRenderer::AnimateScale(const xent::ViewData *key, float target) {
  float current;
  if (states_[key].scale_anim.Update(target, kSliderAnimationSeconds, &current)) {
    has_active_transitions_ = true;
  }
  return current;
}

Color SliderRenderer::AnimateColor(const xent::ViewData *key,
                                   const Color &target) {
  Color current;
  if (states_[key].color_anim.Update(target, kSliderAnimationSeconds, &current)) {
    has_active_transitions_ = true;
  }
  return current;
}

ID2D1SolidColorBrush *SliderRenderer::GetBrush(ID2D1DeviceContext *d2d,
                                               const Color &color) {
  if (!brush_) {
    d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
  }
  brush_->SetColor(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f,
                                color.b / 255.0f, color.a / 255.0f));
  return brush_.Get();
}

void SliderRenderer::Render(const RenderContext &ctx,
                            const xent::ViewData &data, const Rect &bounds,
                            const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  float min = data.min_value;
  float max = data.max_value;
  float val = std::clamp(data.value, min, max);
  float range = max - min;
  float pct = (range > 0.0f) ? (val - min) / range : 0.0f;

  const auto dpi = ctx.graphics->GetDpi();
  const float scale_x = dpi.scale_x();
  const float scale_y = dpi.scale_y();

  // Snap bounds
  float snapped_x = std::floor(bounds.x * scale_x + 0.5f) / scale_x;
  float snapped_y = std::floor(bounds.y * scale_y + 0.5f) / scale_y;
  float snapped_w =
      std::floor((bounds.x + bounds.width) * scale_x + 0.5f) / scale_x -
      snapped_x;
  float snapped_h =
      std::floor((bounds.y + bounds.height) * scale_y + 0.5f) / scale_y -
      snapped_y;
  Rect snapped_bounds(snapped_x, snapped_y, snapped_w, snapped_h);

  // Calculate Layout using shared logic
  auto layout = CalculateSliderLayout(snapped_bounds, data.min_value, data.max_value, data.value);

  // Constants
  const float outer_radius = layout.thumb_rect.width / 2.0f;

  // Colors & States
  Color track_fill = res.ControlStrongFillDefault;
  Color value_fill = res.AccentDefault;
  Color outer_thumb_fill = res.ControlSolidFillDefault;
  Color outer_thumb_border = res.ControlStrokeDefault;
  Color inner_thumb_fill = res.AccentDefault;

  float target_scale = kScaleNormal;

  if (state.is_disabled) {
    track_fill = res.ControlStrongFillDisabled;
    value_fill = res.AccentDisabled;
    outer_thumb_fill = res.ControlSolidFillDefault;
    outer_thumb_border = res.ControlStrongStrokeDisabled;
    inner_thumb_fill = res.AccentDisabled;
    target_scale = kScaleNormal;
  } else if (state.is_pressed) {
    value_fill = res.AccentTertiary;
    inner_thumb_fill = res.AccentTertiary;
    track_fill = res.ControlStrongFillDefault;
    target_scale = kScalePressed;
  } else if (state.is_hovered) {
    value_fill = res.AccentSecondary;
    inner_thumb_fill = res.AccentSecondary;
    target_scale = kScaleHover;
  }

  // Animate
  float current_scale = AnimateScale(&data, target_scale);
  inner_thumb_fill = AnimateColor(&data, inner_thumb_fill);

  // 1. Draw Background Track
  {
    D2D1_ROUNDED_RECT track_rect = D2D1::RoundedRect(
        layout.track_rect.to_d2d(),
        layout.track_rect.height / 2, layout.track_rect.height / 2);
    d2d->FillRoundedRectangle(track_rect, GetBrush(d2d, track_fill));
  }

  // 2. Draw Value Track
  if (layout.thumb_center_x > layout.track_rect.x) {
    D2D1_ROUNDED_RECT value_r = D2D1::RoundedRect(
        layout.value_rect.to_d2d(),
        layout.value_rect.height / 2, layout.value_rect.height / 2);
    d2d->FillRoundedRectangle(value_r, GetBrush(d2d, value_fill));
  }

  // 3. Draw Outer Thumb (Container/Border)
  {
    D2D1_ELLIPSE outer_ellipse = D2D1::Ellipse(
        D2D1::Point2F(layout.thumb_center_x, layout.thumb_rect.y + layout.thumb_rect.height * 0.5f), outer_radius, outer_radius);

    // Fill
    d2d->FillEllipse(outer_ellipse, GetBrush(d2d, outer_thumb_fill));

    // Border
    d2d->DrawEllipse(outer_ellipse, GetBrush(d2d, outer_thumb_border), 1.0f);
  }

  // 4. Draw Inner Thumb (Animated)
  {
    float inner_radius = (kSliderInnerThumbSize / 2.0f) * current_scale;

    D2D1_ELLIPSE inner_ellipse = D2D1::Ellipse(
        D2D1::Point2F(layout.thumb_center_x, layout.thumb_rect.y + layout.thumb_rect.height * 0.5f), inner_radius, inner_radius);

    d2d->FillEllipse(inner_ellipse, GetBrush(d2d, inner_thumb_fill));
  }
}

} // namespace fluxent::controls
