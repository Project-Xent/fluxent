#include <functional>
#include <string>
#include <vector>

#include <yoga/Yoga.h>

#include "fluxent/config.hpp"
#include "fluxent/engine.hpp"
#include "fluxent/input.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "fluxent/utils.hpp"

namespace fluxent
{

static bool ColorEqual(const xent::Color &a, const xent::Color &b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static xent::Color ToXentColor(const Color &c)
{
  return xent::Color{
      static_cast<std::uint8_t>(c.r),
      static_cast<std::uint8_t>(c.g),
      static_cast<std::uint8_t>(c.b),
      static_cast<std::uint8_t>(c.a),
  };
}

struct ThemeRemapEntry
{
  xent::Color from;
  xent::Color to;
};

static void AppendThemeRemap(std::vector<ThemeRemapEntry> &map, const theme::ThemeResources &from,
                             const theme::ThemeResources &to)
{
  using ColorMember = Color theme::ThemeResources::*;
  static const ColorMember kMembers[] = {
      &theme::ThemeResources::TextPrimary,
      &theme::ThemeResources::TextSecondary,
      &theme::ThemeResources::TextTertiary,
      &theme::ThemeResources::TextDisabled,
      &theme::ThemeResources::TextInverse,

      &theme::ThemeResources::TextOnAccentPrimary,
      &theme::ThemeResources::TextOnAccentSecondary,
      &theme::ThemeResources::TextOnAccentDisabled,

      &theme::ThemeResources::ControlFillDefault,
      &theme::ThemeResources::ControlFillSecondary,
      &theme::ThemeResources::ControlFillTertiary,
      &theme::ThemeResources::ControlFillQuarternary,
      &theme::ThemeResources::ControlFillDisabled,
      &theme::ThemeResources::ControlFillTransparent,
      &theme::ThemeResources::ControlFillInputActive,

      &theme::ThemeResources::ControlStrongFillDefault,
      &theme::ThemeResources::ControlStrongFillDisabled,

      &theme::ThemeResources::ControlSolidFillDefault,

      &theme::ThemeResources::SubtleFillTransparent,
      &theme::ThemeResources::SubtleFillSecondary,
      &theme::ThemeResources::SubtleFillTertiary,
      &theme::ThemeResources::SubtleFillDisabled,

      &theme::ThemeResources::ControlAltFillTransparent,
      &theme::ThemeResources::ControlAltFillSecondary,
      &theme::ThemeResources::ControlAltFillTertiary,
      &theme::ThemeResources::ControlAltFillQuarternary,
      &theme::ThemeResources::ControlAltFillDisabled,

      &theme::ThemeResources::ControlOnImageFillDefault,
      &theme::ThemeResources::ControlOnImageFillSecondary,
      &theme::ThemeResources::ControlOnImageFillTertiary,
      &theme::ThemeResources::ControlOnImageFillDisabled,

      &theme::ThemeResources::AccentDefault,
      &theme::ThemeResources::AccentSecondary,
      &theme::ThemeResources::AccentTertiary,
      &theme::ThemeResources::AccentDisabled,

      &theme::ThemeResources::ControlStrokeDefault,
      &theme::ThemeResources::ControlStrokeSecondary,
      &theme::ThemeResources::ControlStrokeOnAccentDefault,
      &theme::ThemeResources::ControlStrokeOnAccentSecondary,
      &theme::ThemeResources::ControlStrokeOnAccentTertiary,
      &theme::ThemeResources::ControlStrokeOnAccentDisabled,
      &theme::ThemeResources::ControlStrokeForStrongFillWhenOnImage,

      &theme::ThemeResources::ControlStrongStrokeDefault,
      &theme::ThemeResources::ControlStrongStrokeDisabled,

      &theme::ThemeResources::CardStrokeDefault,
      &theme::ThemeResources::CardStrokeDefaultSolid,
      &theme::ThemeResources::CardBackgroundDefault,
      &theme::ThemeResources::CardBackgroundSecondary,
      &theme::ThemeResources::CardBackgroundTertiary,

      &theme::ThemeResources::SurfaceStrokeDefault,
      &theme::ThemeResources::SurfaceStrokeFlyout,
      &theme::ThemeResources::SurfaceStrokeInverse,

      &theme::ThemeResources::FocusStrokeOuter,
      &theme::ThemeResources::FocusStrokeInner,

      &theme::ThemeResources::DividerStrokeDefault,

      &theme::ThemeResources::SolidBackgroundBase,
      &theme::ThemeResources::SolidBackgroundSecondary,
      &theme::ThemeResources::SolidBackgroundTertiary,
      &theme::ThemeResources::SolidBackgroundQuarternary,
      &theme::ThemeResources::SolidBackgroundQuinary,
      &theme::ThemeResources::SolidBackgroundSenary,
      &theme::ThemeResources::SolidBackgroundTransparent,
      &theme::ThemeResources::SolidBackgroundBaseAlt,
      &theme::ThemeResources::LayerFillDefault,
      &theme::ThemeResources::LayerFillAlt,
      &theme::ThemeResources::LayerOnAcrylicFillDefault,
      &theme::ThemeResources::LayerOnAccentAcrylicFillDefault,
      &theme::ThemeResources::LayerOnMicaBaseAltDefault,
      &theme::ThemeResources::LayerOnMicaBaseAltSecondary,
      &theme::ThemeResources::LayerOnMicaBaseAltTertiary,
      &theme::ThemeResources::LayerOnMicaBaseAltTransparent,
      &theme::ThemeResources::SmokeFillDefault,

      &theme::ThemeResources::SystemSuccess,
      &theme::ThemeResources::SystemCaution,
      &theme::ThemeResources::SystemCritical,
      &theme::ThemeResources::SystemNeutral,
      &theme::ThemeResources::SystemSolidNeutral,
      &theme::ThemeResources::SystemAttentionBackground,
      &theme::ThemeResources::SystemSuccessBackground,
      &theme::ThemeResources::SystemCautionBackground,
      &theme::ThemeResources::SystemCriticalBackground,
      &theme::ThemeResources::SystemNeutralBackground,
      &theme::ThemeResources::SystemSolidAttentionBackground,
      &theme::ThemeResources::SystemSolidNeutralBackground,
  };

  for (const auto &m : kMembers)
  {
    map.push_back({ToXentColor(from.*m), ToXentColor(to.*m)});
  }
}

static std::vector<ThemeRemapEntry> BuildThemeRemap(const theme::ThemeResources &from,
                                                    const theme::ThemeResources &to)
{
  std::vector<ThemeRemapEntry> map;
  AppendThemeRemap(map, from, to);
  return map;
}

static controls::ControlState ComputeControlState(InputHandler *input, const xent::View *data,
                                                  bool overlayMode)
{
  controls::ControlState state;
  if (!input)
    return state;

  if (auto hovered = input->GetHoveredView())
  {
    state.is_hovered = (hovered == data);
  }
  if (auto pressed = input->GetPressedView())
  {
    if (pressed == data)
    {
      if (overlayMode)
      {
        state.is_pressed = true;
      }
      else
      {
        bool is_simple_control = (data->type == xent::ComponentType::Button ||
                                  data->type == xent::ComponentType::ToggleButton ||
                                  data->type == xent::ComponentType::CheckBox ||
                                  data->type == xent::ComponentType::RadioButton);

        if (is_simple_control)
        {
          state.is_pressed = state.is_hovered;
        }
        else
        {
          state.is_pressed = true;
        }
      }
    }
  }
  if (auto focused = input->GetFocusedView())
  {
    if (focused == data)
    {
      state.is_focused =
          (data->type == xent::ComponentType::TextBox) || input->ShouldShowFocusVisuals();
    }
  }

  return state;
}

static void RemapColorInPlace(xent::Color &c, const std::vector<ThemeRemapEntry> &map)
{
  for (const auto &e : map)
  {
    if (ColorEqual(c, e.from))
    {
      c = e.to;
      return;
    }
  }
}

static void RemapViewTreeThemeColors(xent::View *data, const std::vector<ThemeRemapEntry> &map)
{
  if (!data)
    return;
  RemapColorInPlace(data->text_color, map);
  RemapColorInPlace(data->background_color, map);
  RemapColorInPlace(data->border_color, map);

  for (const auto &child : data->children)
  {
    RemapViewTreeThemeColors(child.get(), map);
  }
}

static bool IsZeroPadding(const xent::View &d)
{
  return d.padding_top == 0.0f && d.padding_right == 0.0f && d.padding_bottom == 0.0f &&
         d.padding_left == 0.0f;
}

static void SetDefaultButtonPadding(xent::View *data)
{
  if (IsZeroPadding(*data))
  {
    data->padding_top = fluxent::config::Layout::DefaultButtonPaddingTop;
    data->padding_right = fluxent::config::Layout::DefaultButtonPaddingRight;
    data->padding_bottom = fluxent::config::Layout::DefaultButtonPaddingBottom;
    data->padding_left = fluxent::config::Layout::DefaultButtonPaddingLeft;
  }
}

static void EnsureToggleSwitchSize(xent::View *data)
{
  const YGValue w = YGNodeStyleGetWidth(data->node.get());
  const YGValue h = YGNodeStyleGetHeight(data->node.get());
  if (w.unit == YGUnitUndefined)
  {
    YGNodeStyleSetWidth(data->node.get(), fluxent::config::Layout::ToggleSwitchDefaultWidth);
  }
  if (h.unit == YGUnitUndefined)
  {
    YGNodeStyleSetHeight(data->node.get(), fluxent::config::Layout::ToggleSwitchDefaultHeight);
  }
}

static bool ShouldShowScrollbar(xent::ScrollBarVisibility vis, float content, float size)
{
  if (vis == xent::ScrollBarVisibility::Hidden || vis == xent::ScrollBarVisibility::Disabled)
    return false;
  if (vis == xent::ScrollBarVisibility::Visible)
    return true;
  return content > size + fluxent::config::Input::ScrollVisibilityEpsilon;
}

static std::pair<float, float> ComputeMaxOffsets(xent::View *view, float barSize)
{
  float max_child_right = 0.0f;
  float max_child_bottom = 0.0f;

  for (const auto &child : view->children)
  {
    float right = child->LayoutX() + child->LayoutWidth();
    float bottom = child->LayoutY() + child->LayoutHeight();
    if (right > max_child_right)
      max_child_right = right;
    if (bottom > max_child_bottom)
      max_child_bottom = bottom;
  }

  float content_w = std::max(view->LayoutWidth(), max_child_right);
  float content_h = std::max(view->LayoutHeight(), max_child_bottom);

  float view_w = view->LayoutWidth();
  float view_h = view->LayoutHeight();

  bool show_h = ShouldShowScrollbar(view->horizontal_scrollbar_visibility, content_w, view_w);
  bool show_v = ShouldShowScrollbar(view->vertical_scrollbar_visibility, content_h, view_h);

  float effective_view_w = view_w;
  float effective_view_h = view_h;
  if (show_v)
    effective_view_w -= barSize;
  if (show_h)
    effective_view_h -= barSize;

  float max_offset_x = std::max(0.0f, content_w - effective_view_w);
  float max_offset_y = std::max(0.0f, content_h - effective_view_h);

  return {max_offset_x, max_offset_y};
}

static void ApplyButtonDefaultsRecursive(xent::View *data)
{
  if (!data)
    return;

  switch (data->type)
  {
  case xent::ComponentType::Button:
  case xent::ComponentType::ToggleButton:
  {
    SetDefaultButtonPadding(data);
  }
  break;

  case xent::ComponentType::ToggleSwitch:
  {
    EnsureToggleSwitchSize(data);
  }
  break;

  default:
    break;
  }

  for (const auto &child : data->children)
  {
    ApplyButtonDefaultsRecursive(child.get());
  }
}

static void ClampScrollOffsetsRecursive(xent::View *view)
{
  if (!view)
    return;

  if (view->type == xent::ComponentType::ScrollView)
  {
    float max_child_right = 0.0f;
    float max_child_bottom = 0.0f;

    for (const auto &child : view->children)
    {
      float right = child->LayoutX() + child->LayoutWidth();
      float bottom = child->LayoutY() + child->LayoutHeight();
      if (right > max_child_right)
        max_child_right = right;
      if (bottom > max_child_bottom)
        max_child_bottom = bottom;
    }

    float content_w = std::max(view->LayoutWidth(), max_child_right);
    float content_h = std::max(view->LayoutHeight(), max_child_bottom);

    const float kBarSize = fluxent::config::Layout::ScrollBarSize;
    auto [max_offset_x, max_offset_y] = ComputeMaxOffsets(view, kBarSize);

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

  for (const auto &child : view->children)
  {
    ClampScrollOffsetsRecursive(child.get());
  }
}

Result<std::unique_ptr<RenderEngine>> RenderEngine::Create(GraphicsPipeline *graphics,
                                                           theme::ThemeManager *theme_manager)
{
  if (!graphics || !theme_manager)
    return tl::unexpected(E_INVALIDARG);
  auto engine = std::unique_ptr<RenderEngine>(new RenderEngine(graphics, theme_manager));
  auto res = engine->Init();
  if (!res)
    return tl::unexpected(res.error());
  return engine;
}

RenderEngine::RenderEngine(GraphicsPipeline *graphics, theme::ThemeManager *theme_manager)
    : graphics_(graphics), theme_manager_(theme_manager)
{
}

Result<void> RenderEngine::Init()
{
  d2d_context_ = graphics_->GetD2DContext();

  auto tr_res = TextRenderer::Create(graphics_);
  if (!tr_res)
    return tl::unexpected(tr_res.error());

  text_renderer_ = std::move(*tr_res);
  plugin_manager_ = std::make_unique<PluginManager>();
  control_renderer_ = std::make_unique<controls::ControlRenderer>(
      graphics_, text_renderer_.get(), theme_manager_, plugin_manager_.get());

  last_theme_resources_ = theme_manager_->Resources();
  theme_listener_id_ = theme_listener_id_ = theme_manager_->AddThemeChangedListener(
      {this, [](void *ctx, theme::Mode mode)
       {
         auto *engine = static_cast<RenderEngine *>(ctx);
         if (!engine->theme_manager_)
           return;
         const auto next = engine->theme_manager_->Resources();
         const auto map = BuildThemeRemap(engine->last_theme_resources_, next);
         engine->last_theme_resources_ = next;

         if (engine->last_root_data_)
         {
           RemapViewTreeThemeColors(engine->last_root_data_, map);
         }

         if (engine->graphics_)
         {
           engine->graphics_->RequestRedraw();
         }
       }});

  return {};
}

RenderEngine::~RenderEngine()
{
  if (theme_listener_id_ != 0 && theme_manager_)
  {
    theme_manager_->RemoveThemeChangedListener(theme_listener_id_);
    theme_listener_id_ = 0;
  }
}

std::pair<float, float> RenderEngine::MeasureTextCallback(const std::string &text, float font_size,
                                                          float max_width)
{
  if (text.empty())
    return {0.0f, 0.0f};

  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  Size measured = text_renderer_->MeasureText(wtext, style, max_width > 0 ? max_width : 0.0f);
  return {measured.width, measured.height};
}

int RenderEngine::TextHitTestCallback(const std::string &text, float font_size, float max_width,
                                      float x, float y)
{
  if (text.empty())
    return 0;

  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  return text_renderer_->HitTestPoint(wtext, style, max_width > 0 ? max_width : 0.0f, x, y);
}

std::tuple<float, float, float, float> RenderEngine::TextCaretRectCallback(const std::string &text,
                                                                           float font_size,
                                                                           float max_width,
                                                                           int cursor_index)
{
  std::wstring wtext = ToWide(text);

  TextStyle style;
  style.font_size = font_size;

  Rect r =
      text_renderer_->GetCaretRect(wtext, style, max_width > 0 ? max_width : 0.0f, cursor_index);
  return {r.x, r.y, r.width, r.height};
}

void RenderEngine::Render(const xent::View &root)
{
  if (!d2d_context_)
    return;

  DrawViewRecursive(root, 0.0f, 0.0f);
}

void RenderEngine::RenderFrame(xent::View &root)
{
  if (!graphics_)
    return;

  Size size = graphics_->GetRenderTargetSize();
  xent::SetTextMeasureFunc(
      xent::Delegate<std::pair<float, float>(const std::string &, float, float)>::From<
          RenderEngine, &RenderEngine::MeasureTextCallback>(this));

  xent::SetTextHitTestFunc(
      xent::Delegate<int(const std::string &, float, float, float,
                         float)>::From<RenderEngine, &RenderEngine::TextHitTestCallback>(this));

  xent::SetTextCaretRectFunc(
      xent::Delegate<std::tuple<float, float, float, float>(
          const std::string &, float, float,
          int)>::From<RenderEngine, &RenderEngine::TextCaretRectCallback>(this));

  xent::View *root_data = &root;
  if (root_data)
  {
    last_root_data_ = root_data;
    ApplyButtonDefaultsRecursive(root_data);
    root_data->CalculateLayout(size.width, size.height);
    ClampScrollOffsetsRecursive(root_data);
  }

  graphics_->BeginDraw();
  graphics_->Clear(Color::transparent());

  if (control_renderer_)
  {
    control_renderer_->BeginFrame();
  }
  Render(root);
  graphics_->EndDraw();

  if (control_renderer_)
  {
    control_renderer_->EndFrame();
  }
  graphics_->Present();
}

void RenderEngine::DrawViewRecursive(const xent::View &view, float parent_x, float parent_y)
{
  DrawViewDataRecursive(&view, parent_x, parent_y);
}

void RenderEngine::DrawViewDataRecursive(const xent::View *data, float parent_x, float parent_y)
{
  if (data == nullptr)
  {
    return;
  }

  float x = data->LayoutX();
  float y = data->LayoutY();
  float w = data->LayoutWidth();
  float h = data->LayoutHeight();
  float abs_x = parent_x + x;
  float abs_y = parent_y + y;
  Rect bounds(abs_x, abs_y, w, h);

  if (control_renderer_ && data->type != xent::ComponentType::View)
  {
    controls::ControlState state = ComputeControlState(input_, data, false);
    control_renderer_->Render(*data, bounds, state);
  }
  else if (control_renderer_)
  {
    control_renderer_->RenderView(*data, bounds);
  }
  else
  {
    DrawViewBackground(*data, bounds);
    DrawViewBorder(*data, bounds);
    DrawViewText(*data, bounds);
  }

  if (debug_mode_)
  {
    DrawRect(bounds, Color(255, 0, 255, 128), 1.0f);
  }

  bool should_clip = (data->type == xent::ComponentType::ScrollView);
  if (should_clip)
  {
    PushClip(bounds);
    PushTranslation(-data->scroll_offset_x, -data->scroll_offset_y);
  }

  for (const auto &child : data->children)
  {
    DrawViewDataRecursive(child.get(), abs_x, abs_y);
  }

  if (should_clip)
  {
    PopTransform();
  }

  if (control_renderer_)
  {
    controls::ControlState state = ComputeControlState(input_, data, true);
    control_renderer_->RenderOverlay(*data, bounds, state);
  }

  if (should_clip)
  {
    PopClip();
  }
}

void RenderEngine::DrawViewBackground(const xent::View &data, const Rect &bounds)
{
  Color color(data.background_color.r, data.background_color.g, data.background_color.b,
              data.background_color.a);

  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0)
  {
    FillRoundedRect(bounds, radius, color);
  }
  else
  {
    FillRect(bounds, color);
  }
}

void RenderEngine::DrawViewBorder(const xent::View &data, const Rect &bounds)
{
  float border_width = data.border_width;
  if (border_width <= 0)
    return;

  Color color(data.border_color.r, data.border_color.g, data.border_color.b, data.border_color.a);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0)
  {
    DrawRoundedRect(bounds, radius, color, border_width);
  }
  else
  {
    DrawRect(bounds, color, border_width);
  }
}

void RenderEngine::DrawViewText(const xent::View &data, const Rect &bounds)
{
  const std::string &text = data.text_content;
  if (text.empty())
    return;

  Color color(data.text_color.r, data.text_color.g, data.text_color.b, data.text_color.a);

  if (text_renderer_)
  {
    std::wstring wtext = ToWide(text);
    TextStyle style;
    style.font_size = data.font_size;
    style.color = color;
    text_renderer_->DrawText(wtext, bounds, style);
  }
}

void RenderEngine::FillRect(const Rect &rect, const Color &color)
{
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush)
  {
    d2d_context_->FillRectangle(rect.to_d2d(), brush);
  }
}

void RenderEngine::FillRoundedRect(const Rect &rect, float radius, const Color &color)
{
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush)
  {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->FillRoundedRectangle(rounded, brush);
  }
}

void RenderEngine::DrawRect(const Rect &rect, const Color &color, float stroke_width)
{
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush)
  {
    d2d_context_->DrawRectangle(rect.to_d2d(), brush, stroke_width);
  }
}

void RenderEngine::DrawRoundedRect(const Rect &rect, float radius, const Color &color,
                                   float stroke_width)
{
  if (!d2d_context_)
    return;

  auto *brush = GetBrush(color);
  if (brush)
  {
    D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->DrawRoundedRectangle(rounded, brush, stroke_width);
  }
}

void RenderEngine::PushClip(const Rect &rect)
{
  if (d2d_context_)
  {
    d2d_context_->PushAxisAlignedClip(rect.to_d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void RenderEngine::PopClip()
{
  if (d2d_context_)
  {
    d2d_context_->PopAxisAlignedClip();
  }
}

void RenderEngine::PushTransform(const D2D1_MATRIX_3X2_F &transform)
{
  if (!d2d_context_)
    return;

  D2D1_MATRIX_3X2_F current;
  d2d_context_->GetTransform(&current);
  transform_stack_.push_back(current);

  D2D1_MATRIX_3X2_F combined = transform * current;
  d2d_context_->SetTransform(combined);
}

void RenderEngine::PopTransform()
{
  if (!d2d_context_ || transform_stack_.empty())
    return;

  d2d_context_->SetTransform(transform_stack_.back());
  transform_stack_.pop_back();
}

void RenderEngine::PushTranslation(float x, float y)
{
  PushTransform(D2D1::Matrix3x2F::Translation(x, y));
}

ID2D1SolidColorBrush *RenderEngine::GetBrush(const Color &color)
{
  uint32_t key = (static_cast<uint32_t>(color.r) << 24) | (static_cast<uint32_t>(color.g) << 16) |
                 (static_cast<uint32_t>(color.b) << 8) | static_cast<uint32_t>(color.a);

  auto it = brush_cache_.find(key);
  if (it != brush_cache_.end())
  {
    return it->second.Get();
  }

  ComPtr<ID2D1SolidColorBrush> brush;
  if (d2d_context_)
  {
    d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
    brush_cache_[key] = brush;
    return brush.Get();
  }

  return nullptr;
}

void RenderEngine::ClearBrushCache() { brush_cache_.clear(); }

} // namespace fluxent
