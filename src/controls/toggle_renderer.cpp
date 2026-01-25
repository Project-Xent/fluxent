#include "fluxent/controls/toggle_renderer.hpp"
#include "fluxent/controls/common_drawing.hpp"
#include "fluxent/config.hpp"
#include "fluxent/controls/renderer_utils.hpp"
#include <cmath>

namespace fluxent::controls {

using Microsoft::WRL::ComPtr;

// Helper from core.cpp
static Color LerpColor(const Color &a, const Color &b, float t) {
  return LerpColorSrgb(a, b, t);
}

void ToggleSwitchRenderer::BeginFrame() {
  has_active_transitions_ = false;
  seen_.clear();
}

bool ToggleSwitchRenderer::EndFrame() {
  for (auto it = states_.begin(); it != states_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = states_.erase(it);
    } else {
      ++it;
    }
  }
  return has_active_transitions_;
}

float ToggleSwitchRenderer::AnimateProgress(const xent::ViewData *key,
                                            float target) {
  float current;
  if (states_[key].progress_anim.Update(target, fluxent::config::Animation::Button, &current)) {
    has_active_transitions_ = true;
  }
  return current;
}

void ToggleSwitchRenderer::Render(const RenderContext &ctx,
                                  const xent::ViewData &data,
                                  const Rect &bounds,
                                  const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  const bool is_on_target = data.is_checked;
  const float progress = AnimateProgress(&data, is_on_target ? 1.0f : 0.0f);

  const float track_radius =
      std::max(0.0f, std::min(bounds.width, bounds.height) * 0.5f);
  const D2D1_ROUNDED_RECT track_rect =
      D2D1::RoundedRect(D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width,
                                    bounds.y + bounds.height),
                        track_radius, track_radius);

  Color off_fill;
  Color off_stroke;
  Color on_fill;
  Color knob_off;
  Color knob_on;

  if (state.is_disabled) {
    off_fill = res.ControlAltFillDisabled;
    off_stroke = res.ControlStrongStrokeDisabled;
    on_fill = res.AccentDisabled;
    knob_off = res.TextDisabled;
    knob_on = res.TextOnAccentDisabled;
  } else {
    if (state.is_pressed) {
      off_fill = res.ControlAltFillQuarternary;
      on_fill = res.AccentTertiary;
    } else if (state.is_hovered) {
      off_fill = res.ControlAltFillTertiary;
      on_fill = res.AccentSecondary;
    } else {
      off_fill = res.ControlAltFillSecondary;
      on_fill = res.AccentDefault;
    }
    off_stroke = res.ControlStrongStrokeDefault;
    knob_off = res.TextSecondary;
    knob_on = res.TextOnAccentPrimary;
  }

  const Color track_fill = LerpColor(off_fill, on_fill, progress);
  const Color knob_fill = LerpColor(knob_off, knob_on, progress);

  ComPtr<ID2D1SolidColorBrush> brush;
  d2d->CreateSolidColorBrush(track_fill.to_d2d(), &brush);

  d2d->FillRoundedRectangle(track_rect, brush.Get());

  if (!state.is_disabled && progress < 0.5f) {
    brush->SetColor(off_stroke.to_d2d());
    d2d->DrawRoundedRectangle(track_rect, brush.Get(), 1.0f);
  } else if (state.is_disabled && progress < 0.5f) {
    brush->SetColor(off_stroke.to_d2d());
    d2d->DrawRoundedRectangle(track_rect, brush.Get(), 1.0f);
  }

  float knob_w = fluxent::config::Toggle::KnobSizeNormal;
  float knob_h = fluxent::config::Toggle::KnobSizeNormal;
  if (!state.is_disabled) {
    if (state.is_pressed) {
      knob_w = fluxent::config::Toggle::KnobSizePressedW;
      knob_h = fluxent::config::Toggle::KnobSizePressedH;
    } else if (state.is_hovered) {
      knob_w = fluxent::config::Toggle::KnobSizeHover;
      knob_h = fluxent::config::Toggle::KnobSizeHover;
    }
  }

  const float travel = std::max(0.0f, bounds.width - bounds.height);
  const float base_cx = bounds.x + bounds.height * 0.5f;
  float cx = base_cx + progress * travel;

  if (!state.is_disabled && state.is_pressed) {
    cx += (1.0f - 2.0f * progress) * fluxent::config::Toggle::TravelExtension;
  }

  const float cy = bounds.y + bounds.height * 0.5f;
  const float left = cx - knob_w * 0.5f;
  const float top = cy - knob_h * 0.5f;
  const float right = cx + knob_w * 0.5f;
  const float bottom = cy + knob_h * 0.5f;

  const float knob_radius = std::max(0.0f, std::min(knob_w, knob_h) * 0.5f);
  const D2D1_ROUNDED_RECT knob_rect = D2D1::RoundedRect(
      D2D1::RectF(left, top, right, bottom), knob_radius, knob_radius);

  brush->SetColor(knob_fill.to_d2d());
  d2d->FillRoundedRectangle(knob_rect, brush.Get());

  if (state.is_focused && !state.is_disabled) {
    DrawFocusRect(ctx, bounds, track_radius);
  }
}

} // namespace fluxent::controls
