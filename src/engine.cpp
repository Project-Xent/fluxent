// FluXent Render Engine implementation
#include "fluxent/engine.hpp"
#include "fluxent/input.hpp"
#include "fluxent/theme/theme_manager.hpp"

#include <functional>
#include <stdexcept>
#include <string>
#include <yoga/Yoga.h>

#include <vector>

namespace fluxent {

static bool ColorEqual(const xent::Color &a, const xent::Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static xent::Color ToXentColor(const Color &c) {
  return xent::Color{
      static_cast<std::uint8_t>(c.r),
      static_cast<std::uint8_t>(c.g),
      static_cast<std::uint8_t>(c.b),
      static_cast<std::uint8_t>(c.a),
  };
}

struct ThemeRemapEntry {
  xent::Color from;
  xent::Color to;
};

static std::vector<ThemeRemapEntry>
BuildThemeRemap(const theme::ThemeResources &from,
                const theme::ThemeResources &to) {
  // Note: include every ThemeResources field so any view that "baked" a theme
  // token can be updated by matching old RGBA -> new RGBA.
  return {
      {ToXentColor(from.TextPrimary), ToXentColor(to.TextPrimary)},
      {ToXentColor(from.TextSecondary), ToXentColor(to.TextSecondary)},
      {ToXentColor(from.TextTertiary), ToXentColor(to.TextTertiary)},
      {ToXentColor(from.TextDisabled), ToXentColor(to.TextDisabled)},
      {ToXentColor(from.TextInverse), ToXentColor(to.TextInverse)},

      {ToXentColor(from.TextOnAccentPrimary),
       ToXentColor(to.TextOnAccentPrimary)},
      {ToXentColor(from.TextOnAccentSecondary),
       ToXentColor(to.TextOnAccentSecondary)},
      {ToXentColor(from.TextOnAccentDisabled),
       ToXentColor(to.TextOnAccentDisabled)},

      {ToXentColor(from.ControlFillDefault),
       ToXentColor(to.ControlFillDefault)},
      {ToXentColor(from.ControlFillSecondary),
       ToXentColor(to.ControlFillSecondary)},
      {ToXentColor(from.ControlFillTertiary),
       ToXentColor(to.ControlFillTertiary)},
      {ToXentColor(from.ControlFillQuarternary),
       ToXentColor(to.ControlFillQuarternary)},
      {ToXentColor(from.ControlFillDisabled),
       ToXentColor(to.ControlFillDisabled)},
      {ToXentColor(from.ControlFillTransparent),
       ToXentColor(to.ControlFillTransparent)},
      {ToXentColor(from.ControlFillInputActive),
       ToXentColor(to.ControlFillInputActive)},

      {ToXentColor(from.ControlStrongFillDefault),
       ToXentColor(to.ControlStrongFillDefault)},
      {ToXentColor(from.ControlStrongFillDisabled),
       ToXentColor(to.ControlStrongFillDisabled)},

      {ToXentColor(from.ControlSolidFillDefault),
       ToXentColor(to.ControlSolidFillDefault)},

      {ToXentColor(from.SubtleFillTransparent),
       ToXentColor(to.SubtleFillTransparent)},
      {ToXentColor(from.SubtleFillSecondary),
       ToXentColor(to.SubtleFillSecondary)},
      {ToXentColor(from.SubtleFillTertiary),
       ToXentColor(to.SubtleFillTertiary)},
      {ToXentColor(from.SubtleFillDisabled),
       ToXentColor(to.SubtleFillDisabled)},

      {ToXentColor(from.ControlAltFillTransparent),
       ToXentColor(to.ControlAltFillTransparent)},
      {ToXentColor(from.ControlAltFillSecondary),
       ToXentColor(to.ControlAltFillSecondary)},
      {ToXentColor(from.ControlAltFillTertiary),
       ToXentColor(to.ControlAltFillTertiary)},
      {ToXentColor(from.ControlAltFillQuarternary),
       ToXentColor(to.ControlAltFillQuarternary)},
      {ToXentColor(from.ControlAltFillDisabled),
       ToXentColor(to.ControlAltFillDisabled)},

      {ToXentColor(from.ControlOnImageFillDefault),
       ToXentColor(to.ControlOnImageFillDefault)},
      {ToXentColor(from.ControlOnImageFillSecondary),
       ToXentColor(to.ControlOnImageFillSecondary)},
      {ToXentColor(from.ControlOnImageFillTertiary),
       ToXentColor(to.ControlOnImageFillTertiary)},
      {ToXentColor(from.ControlOnImageFillDisabled),
       ToXentColor(to.ControlOnImageFillDisabled)},

      {ToXentColor(from.AccentDefault), ToXentColor(to.AccentDefault)},
      {ToXentColor(from.AccentSecondary), ToXentColor(to.AccentSecondary)},
      {ToXentColor(from.AccentTertiary), ToXentColor(to.AccentTertiary)},
      {ToXentColor(from.AccentDisabled), ToXentColor(to.AccentDisabled)},

      {ToXentColor(from.ControlStrokeDefault),
       ToXentColor(to.ControlStrokeDefault)},
      {ToXentColor(from.ControlStrokeSecondary),
       ToXentColor(to.ControlStrokeSecondary)},
      {ToXentColor(from.ControlStrokeOnAccentDefault),
       ToXentColor(to.ControlStrokeOnAccentDefault)},
      {ToXentColor(from.ControlStrokeOnAccentSecondary),
       ToXentColor(to.ControlStrokeOnAccentSecondary)},
      {ToXentColor(from.ControlStrokeOnAccentTertiary),
       ToXentColor(to.ControlStrokeOnAccentTertiary)},
      {ToXentColor(from.ControlStrokeOnAccentDisabled),
       ToXentColor(to.ControlStrokeOnAccentDisabled)},
      {ToXentColor(from.ControlStrokeForStrongFillWhenOnImage),
       ToXentColor(to.ControlStrokeForStrongFillWhenOnImage)},

      {ToXentColor(from.ControlStrongStrokeDefault),
       ToXentColor(to.ControlStrongStrokeDefault)},
      {ToXentColor(from.ControlStrongStrokeDisabled),
       ToXentColor(to.ControlStrongStrokeDisabled)},

      {ToXentColor(from.CardStrokeDefault), ToXentColor(to.CardStrokeDefault)},
      {ToXentColor(from.CardStrokeDefaultSolid),
       ToXentColor(to.CardStrokeDefaultSolid)},
      {ToXentColor(from.CardBackgroundDefault),
       ToXentColor(to.CardBackgroundDefault)},
      {ToXentColor(from.CardBackgroundSecondary),
       ToXentColor(to.CardBackgroundSecondary)},
      {ToXentColor(from.CardBackgroundTertiary),
       ToXentColor(to.CardBackgroundTertiary)},

      {ToXentColor(from.SurfaceStrokeDefault),
       ToXentColor(to.SurfaceStrokeDefault)},
      {ToXentColor(from.SurfaceStrokeFlyout),
       ToXentColor(to.SurfaceStrokeFlyout)},
      {ToXentColor(from.SurfaceStrokeInverse),
       ToXentColor(to.SurfaceStrokeInverse)},

      {ToXentColor(from.FocusStrokeOuter), ToXentColor(to.FocusStrokeOuter)},
      {ToXentColor(from.FocusStrokeInner), ToXentColor(to.FocusStrokeInner)},

      {ToXentColor(from.DividerStrokeDefault),
       ToXentColor(to.DividerStrokeDefault)},

      {ToXentColor(from.SolidBackgroundBase),
       ToXentColor(to.SolidBackgroundBase)},
      {ToXentColor(from.SolidBackgroundSecondary),
       ToXentColor(to.SolidBackgroundSecondary)},
      {ToXentColor(from.SolidBackgroundTertiary),
       ToXentColor(to.SolidBackgroundTertiary)},
      {ToXentColor(from.SolidBackgroundQuarternary),
       ToXentColor(to.SolidBackgroundQuarternary)},
      {ToXentColor(from.SolidBackgroundQuinary),
       ToXentColor(to.SolidBackgroundQuinary)},
      {ToXentColor(from.SolidBackgroundSenary),
       ToXentColor(to.SolidBackgroundSenary)},
      {ToXentColor(from.SolidBackgroundTransparent),
       ToXentColor(to.SolidBackgroundTransparent)},
      {ToXentColor(from.SolidBackgroundBaseAlt),
       ToXentColor(to.SolidBackgroundBaseAlt)},
      {ToXentColor(from.LayerFillDefault), ToXentColor(to.LayerFillDefault)},
      {ToXentColor(from.LayerFillAlt), ToXentColor(to.LayerFillAlt)},
      {ToXentColor(from.LayerOnAcrylicFillDefault),
       ToXentColor(to.LayerOnAcrylicFillDefault)},
      {ToXentColor(from.LayerOnAccentAcrylicFillDefault),
       ToXentColor(to.LayerOnAccentAcrylicFillDefault)},
      {ToXentColor(from.LayerOnMicaBaseAltDefault),
       ToXentColor(to.LayerOnMicaBaseAltDefault)},
      {ToXentColor(from.LayerOnMicaBaseAltSecondary),
       ToXentColor(to.LayerOnMicaBaseAltSecondary)},
      {ToXentColor(from.LayerOnMicaBaseAltTertiary),
       ToXentColor(to.LayerOnMicaBaseAltTertiary)},
      {ToXentColor(from.LayerOnMicaBaseAltTransparent),
       ToXentColor(to.LayerOnMicaBaseAltTransparent)},
      {ToXentColor(from.SmokeFillDefault), ToXentColor(to.SmokeFillDefault)},

      {ToXentColor(from.SystemSuccess), ToXentColor(to.SystemSuccess)},
      {ToXentColor(from.SystemCaution), ToXentColor(to.SystemCaution)},
      {ToXentColor(from.SystemCritical), ToXentColor(to.SystemCritical)},
      {ToXentColor(from.SystemNeutral), ToXentColor(to.SystemNeutral)},
      {ToXentColor(from.SystemSolidNeutral),
       ToXentColor(to.SystemSolidNeutral)},
      {ToXentColor(from.SystemAttentionBackground),
       ToXentColor(to.SystemAttentionBackground)},
      {ToXentColor(from.SystemSuccessBackground),
       ToXentColor(to.SystemSuccessBackground)},
      {ToXentColor(from.SystemCautionBackground),
       ToXentColor(to.SystemCautionBackground)},
      {ToXentColor(from.SystemCriticalBackground),
       ToXentColor(to.SystemCriticalBackground)},
      {ToXentColor(from.SystemNeutralBackground),
       ToXentColor(to.SystemNeutralBackground)},
      {ToXentColor(from.SystemSolidAttentionBackground),
       ToXentColor(to.SystemSolidAttentionBackground)},
      {ToXentColor(from.SystemSolidNeutralBackground),
       ToXentColor(to.SystemSolidNeutralBackground)},
  };
}

static void RemapColorInPlace(xent::Color &c,
                              const std::vector<ThemeRemapEntry> &map) {
  for (const auto &e : map) {
    if (ColorEqual(c, e.from)) {
      c = e.to;
      return;
    }
  }
}

static void
RemapViewTreeThemeColors(const std::shared_ptr<xent::ViewData> &data,
                         const std::vector<ThemeRemapEntry> &map) {
  if (!data)
    return;
  RemapColorInPlace(data->text_color, map);
  RemapColorInPlace(data->background_color, map);
  RemapColorInPlace(data->border_color, map);

  for (const auto &child : data->children) {
    RemapViewTreeThemeColors(child, map);
  }
}

static bool IsZeroPadding(const xent::ViewData &d) {
  return d.padding_top == 0.0f && d.padding_right == 0.0f &&
         d.padding_bottom == 0.0f && d.padding_left == 0.0f;
}

static void
ApplyButtonDefaultsRecursive(const std::shared_ptr<xent::ViewData> &data) {
  if (!data)
    return;

  switch (data->type) {
  case xent::ComponentType::Button:
  case xent::ComponentType::ToggleButton: {
    // WinUI3 DefaultButtonStyle content padding: 5,11,6,11 (T,R,B,L)
    if (IsZeroPadding(*data)) {
      data->padding_top = 5.0f;
      data->padding_right = 11.0f;
      data->padding_bottom = 6.0f;
      data->padding_left = 11.0f;
    }
    // NOTE: WinUI3 does not set a default MinHeight/MinWidth for Button in
    // XAML. Keep framework defaults (0) unless the app sets constraints
    // explicitly.
  } break;

  case xent::ComponentType::ToggleSwitch: {
    // Default size for the switch track in WinUI3 template is 40x20.
    // Ensure it's visible even when used without explicit width/height.
    const YGValue w = YGNodeStyleGetWidth(data->node.get());
    const YGValue h = YGNodeStyleGetHeight(data->node.get());
    if (w.unit == YGUnitUndefined) {
      YGNodeStyleSetWidth(data->node.get(), 40.0f);
    }
    if (h.unit == YGUnitUndefined) {
      YGNodeStyleSetHeight(data->node.get(), 20.0f);
    }
  } break;

  default:
    break;
  }

  for (const auto &child : data->children) {
    ApplyButtonDefaultsRecursive(child);
  }
}

RenderEngine::RenderEngine(GraphicsPipeline *graphics,
                           theme::ThemeManager *theme_manager)
    : graphics_(graphics), theme_manager_(theme_manager) {
  if (!graphics_) {
    throw std::invalid_argument("Graphics pipeline cannot be null");
  }
  if (!theme_manager_) {
    throw std::invalid_argument("Theme manager cannot be null");
  }
  d2d_context_ = graphics_->GetD2DContext();
  text_renderer_ = std::make_unique<TextRenderer>(graphics_);
  control_renderer_ = std::make_unique<controls::ControlRenderer>(
      graphics_, text_renderer_.get(), theme_manager_);

  // Initialize theme snapshot and subscribe for automatic UI updates.
  last_theme_resources_ = theme_manager_->Resources();
  theme_listener_id_ =
      theme_manager_->AddThemeChangedListener([this](theme::Mode) {
        if (!theme_manager_)
          return;
        const auto next = theme_manager_->Resources();
        const auto map = BuildThemeRemap(last_theme_resources_, next);
        last_theme_resources_ = next;

        if (auto root = last_root_data_.lock()) {
          RemapViewTreeThemeColors(root, map);
        }

        if (graphics_) {
          graphics_->RequestRedraw();
        }
      });
}

RenderEngine::~RenderEngine() {
  if (theme_listener_id_ != 0 && theme_manager_) {
    theme_manager_->RemoveThemeChangedListener(theme_listener_id_);
    theme_listener_id_ = 0;
  }
}

// Text measurement callback
std::pair<float, float>
RenderEngine::MeasureTextCallback(const std::string &text, float font_size,
                                  float max_width) {
  if (text.empty())
    return {0.0f, 0.0f};

  std::wstring wtext(text.begin(), text.end());

  TextStyle style;
  style.font_size = font_size;

  Size measured = text_renderer_->MeasureText(wtext, style,
                                              max_width > 0 ? max_width : 0.0f);
  return {measured.width, measured.height};
}

void RenderEngine::Render(const xent::View &root) {
  if (!d2d_context_)
    return;

  DrawViewRecursive(root, 0.0f, 0.0f);
}

void RenderEngine::RenderFrame(xent::View &root) {
  if (!graphics_)
    return;

  Size size = graphics_->GetRenderTargetSize();
  xent::SetTextMeasureFunc(
      std::bind(&RenderEngine::MeasureTextCallback, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));

  auto root_data = root.Data();
  if (root_data) {
    last_root_data_ = root_data;
    ApplyButtonDefaultsRecursive(root_data);
    root_data->CalculateLayout(size.width, size.height);
  }

  graphics_->BeginDraw();
  graphics_->Clear(Color::transparent());

  if (control_renderer_) {
    control_renderer_->BeginFrame();
  }
  Render(root);
  graphics_->EndDraw();

  if (control_renderer_) {
    control_renderer_->EndFrame();
  }
  graphics_->Present();
}

void RenderEngine::DrawViewRecursive(const xent::View &view, float parent_x,
                                     float parent_y) {
  auto view_data = view.Data();
  if (!view_data)
    return;

  DrawViewDataRecursive(view_data, parent_x, parent_y);
}

void RenderEngine::DrawViewDataRecursive(
    const std::shared_ptr<xent::ViewData> &data, float parent_x,
    float parent_y) {
  if (!data)
    return;

  float x = data->LayoutX();
  float y = data->LayoutY();
  float w = data->LayoutWidth();
  float h = data->LayoutHeight();
  float abs_x = parent_x + x;
  float abs_y = parent_y + y;
  Rect bounds(abs_x, abs_y, w, h);

  if (control_renderer_ && data->type != xent::ComponentType::View) {
    controls::ControlState state;
    if (input_) {
      if (auto hovered = input_->GetHoveredView()) {
        state.is_hovered = (hovered.get() == data.get());
      }
      if (auto pressed = input_->GetPressedView()) {
        state.is_pressed = (pressed.get() == data.get());
      }
      if (auto focused = input_->GetFocusedView()) {
        state.is_focused =
            input_->ShouldShowFocusVisuals() && (focused.get() == data.get());
      }
    }
    control_renderer_->Render(*data, bounds, state);
  } else if (control_renderer_) {
    control_renderer_->RenderView(*data, bounds);
  } else {
    DrawViewBackground(*data, bounds);
    DrawViewBorder(*data, bounds);
    DrawViewText(*data, bounds);
  }

  if (debug_mode_) {
    DrawRect(bounds, Color(255, 0, 255, 128), 1.0f);
  }

  for (const auto &child : data->children) {
    DrawViewDataRecursive(child, abs_x, abs_y);
  }
}

void RenderEngine::DrawViewBackground(const xent::ViewData &data,
                                      const Rect &bounds) {
  Color color = convert_color(data.background_color);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    FillRoundedRect(bounds, radius, color);
  } else {
    FillRect(bounds, color);
  }
}

void RenderEngine::DrawViewBorder(const xent::ViewData &data,
                                  const Rect &bounds) {
  float border_width = data.border_width;
  if (border_width <= 0)
    return;

  Color color = convert_color(data.border_color);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    DrawRoundedRect(bounds, radius, color, border_width);
  } else {
    DrawRect(bounds, color, border_width);
  }
}

void RenderEngine::DrawViewText(const xent::ViewData &data,
                                const Rect &bounds) {
  const std::string &text = data.text_content;
  if (text.empty())
    return;

  std::wstring wtext(text.begin(), text.end());

  TextStyle style;
  style.font_size = data.font_size;
  style.color = convert_color(data.text_color);
  style.alignment = TextAlignment::Center;
  style.paragraph_alignment = ParagraphAlignment::Center;

  text_renderer_->DrawText(wtext, bounds, style);
}

// Primitives

void RenderEngine::FillRect(const Rect &rect, const Color &color) {
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush) {
    d2d_context_->FillRectangle(rect.to_d2d(), brush);
  }
}

void RenderEngine::FillRoundedRect(const Rect &rect, float radius,
                                   const Color &color) {
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush) {
    D2D1_ROUNDED_RECT rounded =
        D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->FillRoundedRectangle(rounded, brush);
  }
}

void RenderEngine::DrawRect(const Rect &rect, const Color &color,
                            float stroke_width) {
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush) {
    d2d_context_->DrawRectangle(rect.to_d2d(), brush, stroke_width);
  }
}

void RenderEngine::DrawRoundedRect(const Rect &rect, float radius,
                                   const Color &color, float stroke_width) {
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush) {
    D2D1_ROUNDED_RECT rounded =
        D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->DrawRoundedRectangle(rounded, brush, stroke_width);
  }
}

// Clipping

void RenderEngine::PushClip(const Rect &rect) {
  if (d2d_context_) {
    d2d_context_->PushAxisAlignedClip(rect.to_d2d(),
                                      D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void RenderEngine::PopClip() {
  if (d2d_context_) {
    d2d_context_->PopAxisAlignedClip();
  }
}

// Transform

void RenderEngine::PushTransform(const D2D1_MATRIX_3X2_F &transform) {
  if (!d2d_context_)
    return;

  D2D1_MATRIX_3X2_F current;
  d2d_context_->GetTransform(&current);
  transform_stack_.push_back(current);

  D2D1_MATRIX_3X2_F combined = transform * current;
  d2d_context_->SetTransform(combined);
}

void RenderEngine::PopTransform() {
  if (!d2d_context_ || transform_stack_.empty())
    return;

  d2d_context_->SetTransform(transform_stack_.back());
  transform_stack_.pop_back();
}

void RenderEngine::PushTranslation(float x, float y) {
  PushTransform(D2D1::Matrix3x2F::Translation(x, y));
}

// Resources

ID2D1SolidColorBrush *RenderEngine::GetBrush(const Color &color) {
  uint32_t key = (static_cast<uint32_t>(color.r) << 24) |
                 (static_cast<uint32_t>(color.g) << 16) |
                 (static_cast<uint32_t>(color.b) << 8) |
                 static_cast<uint32_t>(color.a);

  auto it = brush_cache_.find(key);
  if (it != brush_cache_.end()) {
    return it->second.Get();
  }

  ComPtr<ID2D1SolidColorBrush> brush;
  if (d2d_context_) {
    d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
    brush_cache_[key] = brush;
    return brush.Get();
  }

  return nullptr;
}

void RenderEngine::ClearBrushCache() { brush_cache_.clear(); }

} // namespace fluxent
