#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "renderer_utils.hpp"

#include <cmath>

namespace fluxent::controls {

Color ControlRenderer::animate_button_background(const xent::ViewData *key,
                                                 const Color &target) {
  auto &s = button_bg_transitions_[key];
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
    const float t =
        clamp01(elapsed / static_cast<float>(kButtonBrushTransitionSeconds));
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

void ControlRenderer::render_button(const xent::ViewData &data,
                                    const Rect &bounds,
                                    const ControlState &state) {
  auto d2d = graphics_->get_d2d_context();
  const auto &res = theme::res();

  float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
  Color user_bg = to_fluxent_color(data.background_color);
  bool is_accent =
      (user_bg.a > 0 && color_equal_rgba(user_bg, res.AccentDefault));

  const auto dpi = graphics_->get_dpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;

  // Snap bounds to physical pixels
  const float snapped_x = std::floor(bounds.x * scale_x + 0.5f) / scale_x;
  const float snapped_y = std::floor(bounds.y * scale_y + 0.5f) / scale_y;
  const float snapped_w =
      std::floor((bounds.x + bounds.width) * scale_x + 0.5f) / scale_x -
      snapped_x;
  const float snapped_h =
      std::floor((bounds.y + bounds.height) * scale_y + 0.5f) / scale_y -
      snapped_y;
  Rect snapped_bounds(snapped_x, snapped_y, snapped_w, snapped_h);

  // Border thickness is exactly 1 physical pixel
  const float thickness = 1.0f / scale_y;
  const float fill_inset = is_accent ? 0.0f : thickness;
  const float fill_radius = std::max(0.0f, corner_radius - fill_inset);

  const float fill_left = snapped_bounds.x + fill_inset;
  const float fill_top = snapped_bounds.y + fill_inset;
  const float fill_width =
      std::max(0.0f, snapped_bounds.width - (fill_inset * 2.0f));
  const float fill_height =
      std::max(0.0f, snapped_bounds.height - (fill_inset * 2.0f));
  const float fill_right = fill_left + fill_width;
  const float fill_bottom = fill_top + fill_height;

  D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
      D2D1::RectF(fill_left, fill_top, fill_right, fill_bottom), fill_radius,
      fill_radius);

  Color fill_color;
  Color stroke_color;
  Color text_color;

  const bool use_elevation_border = !state.is_disabled && !state.is_pressed;

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

  if (!is_accent && user_bg.a > 0 && !state.is_disabled) {
    fill_color = user_bg;
    if (state.is_pressed) {
      fill_color.a = static_cast<uint8_t>(fill_color.a * 0.8f);
    } else if (state.is_hovered) {
      fill_color.a = static_cast<uint8_t>(fill_color.a * 0.9f);
    }
  }

  fill_color = animate_button_background(&data, fill_color);

  d2d->FillRoundedRectangle(rounded_rect, get_brush(fill_color));

  if (use_elevation_border) {
    draw_elevation_border(snapped_bounds, corner_radius, is_accent);
  } else if (stroke_color.a > 0) {
    const float half_thick = thickness * 0.5f;
    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(snapped_bounds.x + half_thick,
                    snapped_bounds.y + half_thick,
                    snapped_bounds.x + snapped_bounds.width - half_thick,
                    snapped_bounds.y + snapped_bounds.height - half_thick),
        corner_radius, corner_radius);
    d2d->DrawRoundedRectangle(stroke_rect, get_brush(stroke_color), thickness);
  }

  if (state.is_focused && !state.is_disabled) {
    draw_focus_rect(snapped_bounds, corner_radius);
  }

  if (!data.text_content.empty() && text_renderer_) {
    Color user_text = to_fluxent_color(data.text_color);
    Color actual_text_color = user_text.a > 0 ? user_text : text_color;

    std::wstring wtext(data.text_content.begin(), data.text_content.end());
    TextStyle style;
    style.font_size =
        data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;
    style.word_wrap = false;
    style.color = actual_text_color;
    style.alignment = TextAlignment::Center;
    style.paragraph_alignment = ParagraphAlignment::Center;

    Rect content_bounds = snapped_bounds;
    content_bounds.x += data.padding_left;
    content_bounds.y += data.padding_top;
    content_bounds.width = std::max(
        0.0f, content_bounds.width - (data.padding_left + data.padding_right));
    content_bounds.height = std::max(
        0.0f, content_bounds.height - (data.padding_top + data.padding_bottom));

    d2d->PushAxisAlignedClip(snapped_bounds.to_d2d(),
                             D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    text_renderer_->draw_text(wtext, content_bounds, style);
    d2d->PopAxisAlignedClip();
  }
}

void ControlRenderer::render_toggle_button(const xent::ViewData &data,
                                           const Rect &bounds,
                                           const ControlState &state) {
  auto d2d = graphics_->get_d2d_context();
  const auto &res = theme::res();

  float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
  bool p_checked = data.is_checked;
  bool is_accent = p_checked;

  const auto dpi = graphics_->get_dpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;

  const float snapped_x = std::floor(bounds.x * scale_x + 0.5f) / scale_x;
  const float snapped_y = std::floor(bounds.y * scale_y + 0.5f) / scale_y;
  const float snapped_w =
      std::floor((bounds.x + bounds.width) * scale_x + 0.5f) / scale_x -
      snapped_x;
  const float snapped_h =
      std::floor((bounds.y + bounds.height) * scale_y + 0.5f) / scale_y -
      snapped_y;
  Rect snapped_bounds(snapped_x, snapped_y, snapped_w, snapped_h);

  const float thickness = 1.0f / scale_y;
  const float fill_inset = is_accent ? 0.0f : thickness;
  const float fill_radius = std::max(0.0f, corner_radius - fill_inset);

  const float fill_left = snapped_bounds.x + fill_inset;
  const float fill_top = snapped_bounds.y + fill_inset;
  const float fill_width =
      std::max(0.0f, snapped_bounds.width - (fill_inset * 2.0f));
  const float fill_height =
      std::max(0.0f, snapped_bounds.height - (fill_inset * 2.0f));
  const float fill_right = fill_left + fill_width;
  const float fill_bottom = fill_top + fill_height;

  D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
      D2D1::RectF(fill_left, fill_top, fill_right, fill_bottom), fill_radius,
      fill_radius);

  Color fill_color;
  Color stroke_color;
  Color text_color;

  const bool use_elevation_border = !state.is_disabled && !state.is_pressed;

  if (p_checked) {
    if (state.is_disabled) {
      fill_color = res.AccentDisabled;
      stroke_color = Color::transparent();
      text_color = res.TextOnAccentDisabled;
    } else {
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
    }
  } else {
    if (state.is_disabled) {
      fill_color = res.ControlFillDisabled;
      stroke_color = res.ControlStrokeDefault;
      text_color = res.TextDisabled;
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
  }

  fill_color = animate_button_background(&data, fill_color);

  d2d->FillRoundedRectangle(rounded_rect, get_brush(fill_color));

  if (use_elevation_border) {
    draw_elevation_border(snapped_bounds, corner_radius, is_accent);
  } else if (stroke_color.a > 0) {
    const float half_thick = thickness * 0.5f;
    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(snapped_bounds.x + half_thick,
                    snapped_bounds.y + half_thick,
                    snapped_bounds.x + snapped_bounds.width - half_thick,
                    snapped_bounds.y + snapped_bounds.height - half_thick),
        corner_radius, corner_radius);
    d2d->DrawRoundedRectangle(stroke_rect, get_brush(stroke_color), thickness);
  }

  if (state.is_focused && !state.is_disabled) {
    draw_focus_rect(snapped_bounds, corner_radius);
  }

  if (!data.text_content.empty() && text_renderer_) {
    std::wstring wtext(data.text_content.begin(), data.text_content.end());
    TextStyle style;
    style.font_size =
        data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;
    style.word_wrap = false;
    style.color = text_color;
    style.alignment = TextAlignment::Center;
    style.paragraph_alignment = ParagraphAlignment::Center;

    Rect content_bounds = snapped_bounds;
    content_bounds.x += data.padding_left;
    content_bounds.y += data.padding_top;
    content_bounds.width = std::max(
        0.0f, content_bounds.width - (data.padding_left + data.padding_right));
    content_bounds.height = std::max(
        0.0f, content_bounds.height - (data.padding_top + data.padding_bottom));

    d2d->PushAxisAlignedClip(snapped_bounds.to_d2d(),
                             D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    text_renderer_->draw_text(wtext, content_bounds, style);
    d2d->PopAxisAlignedClip();
  }
}

} // namespace fluxent::controls
