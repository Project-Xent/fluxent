#include "fluxent/engine.hpp"
#include "fluxent/input.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "fluxent/utils.hpp"

#include <functional>

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

static void RemapViewTreeThemeColors(xent::View *data,
                                     const std::vector<ThemeRemapEntry> &map) {
  if (!data)
    return;
  RemapColorInPlace(data->text_color, map);
  RemapColorInPlace(data->background_color, map);
  RemapColorInPlace(data->border_color, map);

  for (const auto &child : data->children) {
    RemapViewTreeThemeColors(child.get(), map);
  }
}

static bool IsZeroPadding(const xent::View &d) {
  return d.padding_top == 0.0f && d.padding_right == 0.0f &&
         d.padding_bottom == 0.0f && d.padding_left == 0.0f;
}

static void ApplyButtonDefaultsRecursive(xent::View *data) {
  if (!data)
    return;

  switch (data->type) {
  case xent::ComponentType::Button:
  case xent::ComponentType::ToggleButton: {
    if (IsZeroPadding(*data)) {
      data->padding_top = 5.0f;
      data->padding_right = 11.0f;
      data->padding_bottom = 6.0f;
      data->padding_left = 11.0f;
    }
  } break;

  case xent::ComponentType::ToggleSwitch: {
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
    ApplyButtonDefaultsRecursive(child.get());
  }
}

static void ClampScrollOffsetsRecursive(xent::View *view) {
  if (!view)
    return;

  if (view->type == xent::ComponentType::ScrollView) {
    float max_child_right = 0.0f;
    float max_child_bottom = 0.0f;

    for (const auto &child : view->children) {
      float right = child->LayoutX() + child->LayoutWidth();
      float bottom = child->LayoutY() + child->LayoutHeight();
      if (right > max_child_right)
        max_child_right = right;
      if (bottom > max_child_bottom)
        max_child_bottom = bottom;
    }

    float content_w = std::max(view->LayoutWidth(), max_child_right);
    float content_h = std::max(view->LayoutHeight(), max_child_bottom);

    auto should_show = [](xent::ScrollBarVisibility vis, float content,
                          float size) {
      if (vis == xent::ScrollBarVisibility::Hidden ||
          vis == xent::ScrollBarVisibility::Disabled)
        return false;
      if (vis == xent::ScrollBarVisibility::Visible)
        return true;
      return content > size + 0.5f;
    };

    float view_w = view->LayoutWidth();
    float view_h = view->LayoutHeight();

    bool show_h =
        should_show(view->horizontal_scrollbar_visibility, content_w, view_w);
    bool show_v =
        should_show(view->vertical_scrollbar_visibility, content_h, view_h);

    const float kBarSize = 12.0f;

    float effective_view_w = view_w;
    float effective_view_h = view_h;
    if (show_v)
      effective_view_w -= kBarSize;
    if (show_h)
      effective_view_h -= kBarSize;

    float max_offset_x = std::max(0.0f, content_w - effective_view_w);
    float max_offset_y = std::max(0.0f, content_h - effective_view_h);

    if (view->scroll_offset_x > max_offset_x)
      view->scroll_offset_x = max_offset_x;
    if (view->scroll_offset_y > max_offset_y)
      view->scroll_offset_y = max_offset_y;

    // Also enforce >= 0 just in case
    if (view->scroll_offset_x < 0)
      view->scroll_offset_x = 0;
    if (view->scroll_offset_y < 0)
      view->scroll_offset_y = 0;
  }

  for (const auto &child : view->children) {
    ClampScrollOffsetsRecursive(child.get());
  }
}

Result<std::unique_ptr<RenderEngine>>
RenderEngine::Create(GraphicsPipeline *graphics,
                     theme::ThemeManager *theme_manager) {
  if (!graphics || !theme_manager)
    return tl::unexpected(E_INVALIDARG);
  auto engine =
      std::unique_ptr<RenderEngine>(new RenderEngine(graphics, theme_manager));
  auto res = engine->Init();
  if (!res)
    return tl::unexpected(res.error());
  return engine;
}

RenderEngine::RenderEngine(GraphicsPipeline *graphics,
                           theme::ThemeManager *theme_manager)
    : graphics_(graphics), theme_manager_(theme_manager) {}

Result<void> RenderEngine::Init() {
  d2d_context_ = graphics_->GetD2DContext();

  auto tr_res = TextRenderer::Create(graphics_);
  if (!tr_res)
    return tl::unexpected(tr_res.error());

  text_renderer_ = std::move(*tr_res);

  control_renderer_ = std::make_unique<controls::ControlRenderer>(
      graphics_, text_renderer_.get(), theme_manager_);

  last_theme_resources_ = theme_manager_->Resources();
  theme_listener_id_ =
  theme_listener_id_ =
      theme_manager_->AddThemeChangedListener({this, [](void* ctx, theme::Mode mode) {
        auto* engine = static_cast<RenderEngine*>(ctx);
        if (!engine->theme_manager_)
          return;
        const auto next = engine->theme_manager_->Resources();
        const auto map = BuildThemeRemap(engine->last_theme_resources_, next);
        engine->last_theme_resources_ = next;

        if (engine->last_root_data_) {
          RemapViewTreeThemeColors(engine->last_root_data_, map);
        }

        if (engine->graphics_) {
          engine->graphics_->RequestRedraw();
        }
      }});

  return {};
}

RenderEngine::~RenderEngine() {
  if (theme_listener_id_ != 0 && theme_manager_) {
    theme_manager_->RemoveThemeChangedListener(theme_listener_id_);
    theme_listener_id_ = 0;
  }
}

std::pair<float, float>
RenderEngine::MeasureTextCallback(const std::string &text, float font_size,
                                  float max_width) {
  if (text.empty())
    return {0.0f, 0.0f};

  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  Size measured = text_renderer_->MeasureText(wtext, style,
                                              max_width > 0 ? max_width : 0.0f);
  return {measured.width, measured.height};
}

int RenderEngine::TextHitTestCallback(const std::string &text, float font_size,
                                      float max_width, float x, float y) {
  if (text.empty())
    return 0;

  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  return text_renderer_->HitTestPoint(wtext, style,
                                      max_width > 0 ? max_width : 0.0f, x, y);
}

std::tuple<float, float, float, float>
RenderEngine::TextCaretRectCallback(const std::string &text, float font_size,
                                    float max_width, int cursor_index) {
  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  Rect r = text_renderer_->GetCaretRect(wtext, style,
                                        max_width > 0 ? max_width : 0.0f,
                                        cursor_index);
  return {r.x, r.y, r.width, r.height};
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
      xent::Delegate<std::pair<float, float>(const std::string &, float, float)>
      ::From<RenderEngine, &RenderEngine::MeasureTextCallback>(this));

  xent::SetTextHitTestFunc(
      xent::Delegate<int(const std::string &, float, float, float, float)>
      ::From<RenderEngine, &RenderEngine::TextHitTestCallback>(this));
      
  xent::SetTextCaretRectFunc(
      xent::Delegate<std::tuple<float, float, float, float>(const std::string &, float, float, int)>
      ::From<RenderEngine, &RenderEngine::TextCaretRectCallback>(this));

  xent::View *root_data = &root;
  if (root_data) {
    last_root_data_ = root_data;
    ApplyButtonDefaultsRecursive(root_data);
    root_data->CalculateLayout(size.width, size.height);
    ClampScrollOffsetsRecursive(root_data);
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
  DrawViewDataRecursive(&view, parent_x, parent_y);
}

void RenderEngine::DrawViewDataRecursive(const xent::View *data, float parent_x,
                                         float parent_y) {
  if (data == nullptr) {
    return;
  }

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
        state.is_hovered = (hovered == data);
      }
      if (auto pressed = input_->GetPressedView()) {
        if (pressed == data) {
          bool is_simple_control =
              (data->type == xent::ComponentType::Button ||
               data->type == xent::ComponentType::ToggleButton ||
               data->type == xent::ComponentType::CheckBox ||
               data->type == xent::ComponentType::RadioButton);

          if (is_simple_control) {
            state.is_pressed = state.is_hovered;
          } else {
            state.is_pressed = true;
          }
        }
      }
      if (auto focused = input_->GetFocusedView()) {
        if (focused == data) {
          state.is_focused = (data->type == xent::ComponentType::TextBox) ||
                             input_->ShouldShowFocusVisuals();
        }
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

  bool should_clip = (data->type == xent::ComponentType::ScrollView);
  if (should_clip) {
    PushClip(bounds);
    PushTranslation(-data->scroll_offset_x, -data->scroll_offset_y);
  }

  for (const auto &child : data->children) {
    DrawViewDataRecursive(child.get(), abs_x, abs_y);
  }

  if (should_clip) {
    PopTransform();
  }

  if (control_renderer_) {
    controls::ControlState state;
    if (input_) {
      if (auto hovered = input_->GetHoveredView()) {
        state.is_hovered = (hovered == data);
      }
      if (auto pressed = input_->GetPressedView()) {
        state.is_pressed = (pressed == data);
      }
      if (auto focused = input_->GetFocusedView()) {
        if (focused == data) {
          state.is_focused = (data->type == xent::ComponentType::TextBox) ||
                             input_->ShouldShowFocusVisuals();
        }
      }
    }
    control_renderer_->RenderOverlay(*data, bounds, state);
  }

  if (should_clip) {
    PopClip();
  }
}

void RenderEngine::DrawViewBackground(const xent::View &data,
                                      const Rect &bounds) {
  Color color(data.background_color.r, data.background_color.g,
              data.background_color.b, data.background_color.a);

  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    FillRoundedRect(bounds, radius, color);
  } else {
    FillRect(bounds, color);
  }
}

void RenderEngine::DrawViewBorder(const xent::View &data, const Rect &bounds) {
  float border_width = data.border_width;
  if (border_width <= 0)
    return;

  Color color(data.border_color.r, data.border_color.g, data.border_color.b,
              data.border_color.a);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    DrawRoundedRect(bounds, radius, color, border_width);
  } else {
    DrawRect(bounds, color, border_width);
  }
}

void RenderEngine::DrawViewText(const xent::View &data, const Rect &bounds) {
  const std::string &text = data.text_content;
  if (text.empty())
    return;

  Color color(data.text_color.r, data.text_color.g, data.text_color.b,
              data.text_color.a);

  if (text_renderer_) {
    std::wstring wtext = ToWide(text);
    TextStyle style;
    style.font_size = data.font_size;
    style.color = color;
    text_renderer_->DrawText(wtext, bounds, style);
  }
}

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
