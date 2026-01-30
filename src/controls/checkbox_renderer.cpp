#include "fluxent/controls/layout.hpp"
#include "fluxent/controls/checkbox_renderer.hpp"
#include "fluxent/config.hpp"
#include "fluxent/controls/common_drawing.hpp"
#include "fluxent/controls/renderer_utils.hpp"
#include <algorithm>
#include <cmath>

namespace fluxent::controls {

using Microsoft::WRL::ComPtr;

void CheckBoxRenderer::BeginFrame() {
  has_active_transitions_ = false;
  seen_.clear();
}

bool CheckBoxRenderer::EndFrame() {
  for (auto it = states_.begin(); it != states_.end();) {
    if (seen_.find(it->first) == seen_.end()) {
      it = states_.erase(it);
    } else {
      ++it;
    }
  }
  return has_active_transitions_;
}

float CheckBoxRenderer::AnimateCheckState(const xent::ViewData *key,
                                          bool is_checked) {
  float target = is_checked ? 1.0f : 0.0f;
  float current;
  if (states_[key].check_anim.Update(target, fluxent::config::Animation::CheckBox, &current)) {
    has_active_transitions_ = true;
  }
  return current;
}

float CheckBoxRenderer::AnimateFloat(const xent::ViewData *key, float target,
                                     float duration_ms) {
  float current;
  if (states_[key].scale_anim.Update(target, duration_ms / 1000.0, &current)) {
    has_active_transitions_ = true;
  }
  return current;
}

CheckBoxVisualState CheckBoxRenderer::DetermineVisualState(bool is_checked,
                                                            const ControlState& state) {
  if (state.is_pressed) {
    return is_checked ? CheckBoxVisualState::PressedOn : CheckBoxVisualState::PressedOff;
  } else if (state.is_hovered) {
    return is_checked ? CheckBoxVisualState::PointerOverOn : CheckBoxVisualState::PointerOverOff;
  } else {
    return is_checked ? CheckBoxVisualState::NormalOn : CheckBoxVisualState::NormalOff;
  }
}

void CheckBoxRenderer::RenderCheckBox(const RenderContext &ctx,
                                      const xent::ViewData &data,
                                      const Rect &bounds,
                                      const ControlState &state) {
  seen_.insert(&data);
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  float press_scale =
      AnimateFloat(&data, state.is_pressed ? 0.9f : 1.0f, fluxent::config::Animation::CheckBoxPress * 1000.0f);

  auto layout = CalculateCheckBoxLayout(bounds, res.CheckBox.Size, res.CheckBox.Gap, res.CheckBox.GlyphFontSize);
  const Rect &box_rect = layout.box_rect;

  auto& cb_state = states_[&data];
  CheckBoxVisualState new_visual_state = DetermineVisualState(data.is_checked, state);
  bool state_changed = false;
  if (new_visual_state != cb_state.visual_state) {
    cb_state.prev_visual_state = cb_state.visual_state;
    cb_state.visual_state = new_visual_state;
    cb_state.state_changed = true;
    state_changed = true;
    has_active_transitions_ = true;
  }

  Color fill;
  Color stroke;
  Color glyph_color = res.TextOnAccentPrimary;

  if (state.is_disabled) {
    fill = data.is_checked ? res.AccentDisabled : res.ControlFillDisabled;
    stroke = res.ControlStrongStrokeDisabled;
    glyph_color = res.TextOnAccentDisabled;
  } else {
    if (data.is_checked) {
      fill = state.is_pressed
                 ? res.AccentTertiary
                 : (state.is_hovered ? res.AccentSecondary : res.AccentDefault);
      stroke = Color::transparent();
    } else {
      fill = state.is_pressed ? res.ControlFillTertiary
                              : (state.is_hovered ? res.ControlFillSecondary
                                                  : res.ControlFillDefault);
      stroke = res.ControlStrongStrokeDefault;
    }
  }

  ComPtr<ID2D1SolidColorBrush> brush;
  d2d->CreateSolidColorBrush(fill.to_d2d(), &brush);

  D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
      box_rect.to_d2d(), res.CheckBox.CornerRadius, res.CheckBox.CornerRadius);

  D2D1_MATRIX_3X2_F old;
  d2d->GetTransform(&old);
  d2d->SetTransform(
      D2D1::Matrix3x2F::Scale(
          press_scale, press_scale,
          D2D1::Point2F(box_rect.x + res.CheckBox.Size * 0.5f,
                        box_rect.y + res.CheckBox.Size * 0.5f)) *
      old);

  d2d->FillRoundedRectangle(rr, brush.Get());
  if (stroke.a > 0) {
    brush->SetColor(stroke.to_d2d());
    d2d->DrawRoundedRectangle(rr, brush.Get(), 1.0f);
  }

  // Draw checkmark using Segoe Fluent Icons
  float check_progress = AnimateCheckState(&data, data.is_checked);
  if (check_progress > 0.01f) {
    TextStyle icon_style;
    icon_style.font_family = L"Segoe Fluent Icons";
    icon_style.font_size = res.CheckBox.GlyphFontSize * check_progress;
    icon_style.color = glyph_color;
    icon_style.alignment = TextAlignment::Center;
    icon_style.paragraph_alignment = ParagraphAlignment::Center;
    ctx.text->DrawText(L"\uE73E", box_rect, icon_style);
  }

  d2d->SetTransform(old);

  if (!data.text_content.empty()) {
    Rect text_rect = layout.text_rect;

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
    DrawFocusRect(ctx, box_rect, res.CheckBox.CornerRadius);
  }

  cb_state.state_changed = false;
}

} // namespace fluxent::controls
