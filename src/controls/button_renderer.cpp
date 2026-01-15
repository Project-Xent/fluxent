#include "fluxent/controls/button_renderer.hpp"
#include "common_drawing.hpp"
#include "icon_map.hpp"
#include "renderer_utils.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent::controls {

using Microsoft::WRL::ComPtr;

void ButtonRenderer::BeginFrame() {
  has_active_transitions_ = false;
  seen_.clear();
}

bool ButtonRenderer::EndFrame() {
  for (auto it = transitions_.begin(); it != transitions_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = transitions_.erase(it);
    } else {
      ++it;
    }
  }
  return has_active_transitions_;
}

Color ButtonRenderer::AnimateBackground(const xent::ViewData *key,
                                        const Color &target) {
  auto &s = transitions_[key];
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

  if (s.active) {
    const float elapsed = std::chrono::duration<float>(now - s.start).count();
    const float t =
        Clamp01(elapsed / static_cast<float>(kButtonBrushTransitionSeconds));
    s.current = LerpColorSrgb(s.from, s.to, t);
    if (t >= 1.0f) {
      s.active = false;
      s.current = s.to;
    }
  }

  if (!ColorEqualRgba(target, s.last_target)) {
    s.from = s.current;
    s.to = target;
    s.last_target = target;
    s.start = now;
    s.active = true;
  }

  if (s.active) {
    has_active_transitions_ = true;
  }

  return s.current;
}

ID2D1SolidColorBrush *ButtonRenderer::GetBrush(ID2D1DeviceContext *d2d,
                                               const Color &color) {
  if (!brush_) {
    d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
  }
  brush_->SetColor(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f,
                                color.b / 255.0f, color.a / 255.0f));
  return brush_.Get();
}

static void DrawButtonContent(const RenderContext &ctx,
                              const xent::ViewData &data,
                              const Rect &snapped_bounds,
                              const Color &text_color) {
  if (!ctx.text)
    return;

  // Resolve Icon
  const wchar_t *icon_code = nullptr;
  if (!data.icon_name.empty()) {
    icon_code = GetIconCodepoint(data.icon_name);
  }

  bool has_icon = (icon_code != nullptr);
  bool has_text = !data.text_content.empty();

  if (!has_icon && !has_text)
    return;

  // Prepare styles
  Color user_text = Color(data.text_color.r, data.text_color.g,
                          data.text_color.b, data.text_color.a);
  Color actual_text_color = user_text.a > 0 ? user_text : text_color;
  float font_size =
      data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;

  TextStyle text_style;
  text_style.font_size = font_size;
  text_style.color = actual_text_color;
  text_style.alignment = TextAlignment::Center;
  text_style.paragraph_alignment = ParagraphAlignment::Center;
  text_style.word_wrap = false;

  TextStyle icon_style = text_style;
  icon_style.font_family = L"Segoe Fluent Icons";
  icon_style.font_size = font_size * 1.14f;

  // Measure content
  float icon_width = 0.0f;
  float text_width = 0.0f;
  float content_gap = 8.0f;

  if (has_icon) {
    Size icon_size = ctx.text->MeasureText(icon_code, icon_style);
    icon_width = icon_size.width;
  }

  std::wstring wtext;
  if (has_text) {
    wtext = std::wstring(data.text_content.begin(), data.text_content.end());
    Size text_size = ctx.text->MeasureText(wtext, text_style);
    text_width = text_size.width;
  }

  float total_content_width = icon_width + text_width;
  if (has_icon && has_text) {
    total_content_width += content_gap;
  }

  // Calculate layout
  Rect content_area = snapped_bounds;
  content_area.x += data.padding_left;
  content_area.y += data.padding_top;
  content_area.width = std::max(
      0.0f, content_area.width - (data.padding_left + data.padding_right));
  content_area.height = std::max(
      0.0f, content_area.height - (data.padding_top + data.padding_bottom));

  float start_x =
      content_area.x + (content_area.width - total_content_width) * 0.5f;

  auto d2d = ctx.graphics->GetD2DContext();
  d2d->PushAxisAlignedClip(snapped_bounds.to_d2d(),
                           D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

  float current_x = start_x;

  if (has_icon) {
    Rect icon_rect(current_x, content_area.y, icon_width, content_area.height);
    ctx.text->DrawText(icon_code, icon_rect, icon_style);
    current_x += icon_width + content_gap;
  }

  if (has_text) {
    Rect text_rect(current_x, content_area.y, text_width, content_area.height);
    ctx.text->DrawText(wtext, text_rect, text_style);
  }

  d2d->PopAxisAlignedClip();
}

void ButtonRenderer::DrawElevationBorder(const RenderContext &ctx,
                                         const Rect &bounds,
                                         float corner_radius, bool is_accent) {
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();
  const auto dpi = ctx.graphics->GetDpi();
  const float scale_y = dpi.scale_y();
  const float thickness = 1.0f;

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
      control_accent_elevation_border_secondary_.r !=
          res.ControlStrokeOnAccentSecondary.r ||
      control_accent_elevation_border_secondary_.g !=
          res.ControlStrokeOnAccentSecondary.g ||
      control_accent_elevation_border_secondary_.b !=
          res.ControlStrokeOnAccentSecondary.b ||
      control_accent_elevation_border_secondary_.a !=
          res.ControlStrokeOnAccentSecondary.a ||
      control_accent_elevation_border_default_.r !=
          res.ControlStrokeOnAccentDefault.r ||
      control_accent_elevation_border_default_.g !=
          res.ControlStrokeOnAccentDefault.g ||
      control_accent_elevation_border_default_.b !=
          res.ControlStrokeOnAccentDefault.b ||
      control_accent_elevation_border_default_.a !=
          res.ControlStrokeOnAccentDefault.a;

  if (need_recreate) {
    control_elevation_border_brush_.Reset();

    D2D1_GRADIENT_STOP stops[2] = {
        {res.Elevation.GradientStop1, res.ControlStrokeSecondary.to_d2d()},
        {res.Elevation.GradientStop2, res.ControlStrokeDefault.to_d2d()},
    };
    ComPtr<ID2D1GradientStopCollection> collection;
    d2d->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2,
                                      D2D1_EXTEND_MODE_CLAMP, &collection);
    if (collection) {
      d2d->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(
              D2D1::Point2F(0, 0), D2D1::Point2F(0, res.Elevation.Height)),
          collection.Get(), &control_elevation_border_brush_);
    }

    control_elevation_border_secondary_ = res.ControlStrokeSecondary;
    control_elevation_border_default_ = res.ControlStrokeDefault;

    control_accent_elevation_border_brush_.Reset();

    D2D1_GRADIENT_STOP accent_stops[2] = {
        {res.Elevation.GradientStop1,
         res.ControlStrokeOnAccentSecondary.to_d2d()},
        {res.Elevation.GradientStop2,
         res.ControlStrokeOnAccentDefault.to_d2d()},
    };
    ComPtr<ID2D1GradientStopCollection> accent_collection;
    d2d->CreateGradientStopCollection(accent_stops, 2, D2D1_GAMMA_2_2,
                                      D2D1_EXTEND_MODE_CLAMP,
                                      &accent_collection);
    if (accent_collection) {
      d2d->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(
              D2D1::Point2F(0, 0), D2D1::Point2F(0, res.Elevation.Height)),
          accent_collection.Get(), &control_accent_elevation_border_brush_);
    }

    control_accent_elevation_border_secondary_ =
        res.ControlStrokeOnAccentSecondary;
    control_accent_elevation_border_default_ = res.ControlStrokeOnAccentDefault;
  }

  const bool is_light = (ctx.Mode() == theme::Mode::Light);
  const bool flip_y = is_accent || is_light;

  ID2D1LinearGradientBrush *brush = control_elevation_border_brush_.Get();
  if (is_accent && control_accent_elevation_border_brush_) {
    brush = control_accent_elevation_border_brush_.Get();
  }

  if (!brush)
    return;

  const float band_start =
      flip_y ? (bounds.y + bounds.height - res.Elevation.Height) : bounds.y;
  const float band_end = band_start + res.Elevation.Height;

  if (flip_y) {
    brush->SetStartPoint(D2D1::Point2F(0.0f, band_end));
    brush->SetEndPoint(D2D1::Point2F(0.0f, band_start));
  } else {
    brush->SetStartPoint(D2D1::Point2F(0.0f, band_start));
    brush->SetEndPoint(D2D1::Point2F(0.0f, band_end));
  }

  const float inset = thickness * 0.5f;
  const float left = bounds.x + inset;
  const float top = bounds.y + inset;
  const float right = bounds.x + bounds.width - inset;
  const float bottom = bounds.y + bounds.height - inset;

  D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
      D2D1::RectF(left, top, right, bottom), corner_radius, corner_radius);
  d2d->DrawRoundedRectangle(stroke_rect, brush, thickness);
}

void ButtonRenderer::Render(const RenderContext &ctx,
                            const xent::ViewData &data, const Rect &bounds,
                            const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
  Color user_bg = Color(data.background_color.r, data.background_color.g,
                        data.background_color.b, data.background_color.a);
  bool is_accent =
      (user_bg.a > 0 && ColorEqualRgba(user_bg, res.AccentDefault)) ||
      (data.role == xent::Semantic::Primary);

  const auto dpi = ctx.graphics->GetDpi();
  const float scale_x = dpi.scale_x();
  const float scale_y = dpi.scale_y();

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

  D2D1_ROUNDED_RECT rounded_rect =
      D2D1::RoundedRect(D2D1::RectF(fill_left, fill_top, fill_left + fill_width,
                                    fill_top + fill_height),
                        fill_radius, fill_radius);

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

  fill_color = AnimateBackground(&data, fill_color);

  d2d->FillRoundedRectangle(rounded_rect, GetBrush(d2d, fill_color));

  if (use_elevation_border) {
    DrawElevationBorder(ctx, snapped_bounds, corner_radius, is_accent);
  } else if (stroke_color.a > 0) {
    const float half_thick = thickness * 0.5f;
    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(snapped_bounds.x + half_thick,
                    snapped_bounds.y + half_thick,
                    snapped_bounds.x + snapped_bounds.width - half_thick,
                    snapped_bounds.y + snapped_bounds.height - half_thick),
        corner_radius, corner_radius);
    d2d->DrawRoundedRectangle(stroke_rect, GetBrush(d2d, stroke_color),
                              thickness);
  }

  if (state.is_focused && !state.is_disabled) {
    DrawFocusRect(ctx, snapped_bounds, corner_radius);
  }

  DrawButtonContent(ctx, data, snapped_bounds, text_color);
}

void ButtonRenderer::RenderToggleButton(const RenderContext &ctx,
                                        const xent::ViewData &data,
                                        const Rect &bounds,
                                        const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  float corner_radius = data.corner_radius > 0 ? data.corner_radius : 4.0f;
  bool p_checked = data.is_checked;
  bool is_accent = p_checked;

  const auto dpi = ctx.graphics->GetDpi();
  const float scale_x = dpi.scale_x();
  const float scale_y = dpi.scale_y();

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

  D2D1_ROUNDED_RECT rounded_rect =
      D2D1::RoundedRect(D2D1::RectF(fill_left, fill_top, fill_left + fill_width,
                                    fill_top + fill_height),
                        fill_radius, fill_radius);

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

  fill_color = AnimateBackground(&data, fill_color);

  d2d->FillRoundedRectangle(rounded_rect, GetBrush(d2d, fill_color));

  if (use_elevation_border) {
    DrawElevationBorder(ctx, snapped_bounds, corner_radius, is_accent);
  } else if (stroke_color.a > 0) {
    const float half_thick = thickness * 0.5f;
    D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
        D2D1::RectF(snapped_bounds.x + half_thick,
                    snapped_bounds.y + half_thick,
                    snapped_bounds.x + snapped_bounds.width - half_thick,
                    snapped_bounds.y + snapped_bounds.height - half_thick),
        corner_radius, corner_radius);
    d2d->DrawRoundedRectangle(stroke_rect, GetBrush(d2d, stroke_color),
                              thickness);
  }

  if (state.is_focused && !state.is_disabled) {
    DrawFocusRect(ctx, snapped_bounds, corner_radius);
  }

  DrawButtonContent(ctx, data, snapped_bounds, text_color);
}

} // namespace fluxent::controls
