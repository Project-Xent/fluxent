#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "renderer_utils.hpp"

#include <cmath>

namespace fluxent::controls {

float ControlRenderer::animate_toggle_progress(const xent::ViewData *key,
                                               float target) {
  auto &s = toggle_transitions_[key];
  const auto now = std::chrono::steady_clock::now();

  target = std::max(0.0f, std::min(1.0f, target));

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

  if (s.active) {
    const float elapsed = std::chrono::duration<float>(now - s.start).count();
    const float t =
        clamp01(elapsed / static_cast<float>(kButtonBrushTransitionSeconds));
    s.current = s.from + (s.to - s.from) * t;
    if (t >= 1.0f) {
      s.active = false;
      s.current = s.to;
    }
  }

  if (std::fabs(target - s.last_target) > 0.0001f) {
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

void ControlRenderer::render_toggle_switch(const xent::ViewData &data,
                                           const Rect &bounds,
                                           const ControlState &state) {
  auto d2d = graphics_->get_d2d_context();
  const auto &res = theme::res();

  const bool is_on_target = (data.text_content == "1");
  const float progress =
      animate_toggle_progress(&data, is_on_target ? 1.0f : 0.0f);

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

  const Color track_fill = lerp_color_srgb(off_fill, on_fill, progress);
  const Color knob_fill = lerp_color_srgb(knob_off, knob_on, progress);

  d2d->FillRoundedRectangle(track_rect, get_brush(track_fill));

  if (!state.is_disabled && progress < 0.5f) {
    d2d->DrawRoundedRectangle(track_rect, get_brush(off_stroke), 1.0f);
  } else if (state.is_disabled && progress < 0.5f) {
    d2d->DrawRoundedRectangle(track_rect, get_brush(off_stroke), 1.0f);
  }

  float knob_w = 12.0f;
  float knob_h = 12.0f;
  if (!state.is_disabled) {
    if (state.is_pressed) {
      knob_w = 17.0f;
      knob_h = 14.0f;
    } else if (state.is_hovered) {
      knob_w = 14.0f;
      knob_h = 14.0f;
    }
  }

  const float travel = std::max(0.0f, bounds.width - bounds.height);
  const float base_cx = bounds.x + bounds.height * 0.5f;
  float cx = base_cx + progress * travel;

  if (!state.is_disabled && state.is_pressed) {
    cx += (1.0f - 2.0f * progress) * 3.0f;
  }

  const float cy = bounds.y + bounds.height * 0.5f;
  const float left = cx - knob_w * 0.5f;
  const float top = cy - knob_h * 0.5f;
  const float right = cx + knob_w * 0.5f;
  const float bottom = cy + knob_h * 0.5f;

  const float knob_radius = std::max(0.0f, std::min(knob_w, knob_h) * 0.5f);
  const D2D1_ROUNDED_RECT knob_rect = D2D1::RoundedRect(
      D2D1::RectF(left, top, right, bottom), knob_radius, knob_radius);
  d2d->FillRoundedRectangle(knob_rect, get_brush(knob_fill));

  if (state.is_focused && !state.is_disabled) {
    draw_focus_rect(bounds, track_radius);
  }
}

} // namespace fluxent::controls
