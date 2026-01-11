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

static bool color_equal(const xent::Color &a, const xent::Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static xent::Color to_xent_color(const Color &c) {
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
build_theme_remap(const theme::ThemeResources &from,
                  const theme::ThemeResources &to) {
  // Note: include every ThemeResources field so any view that "baked" a theme
  // token can be updated by matching old RGBA -> new RGBA.
  return {
      {to_xent_color(from.TextPrimary), to_xent_color(to.TextPrimary)},
      {to_xent_color(from.TextSecondary), to_xent_color(to.TextSecondary)},
      {to_xent_color(from.TextTertiary), to_xent_color(to.TextTertiary)},
      {to_xent_color(from.TextDisabled), to_xent_color(to.TextDisabled)},
      {to_xent_color(from.TextInverse), to_xent_color(to.TextInverse)},

      {to_xent_color(from.TextOnAccentPrimary),
       to_xent_color(to.TextOnAccentPrimary)},
      {to_xent_color(from.TextOnAccentSecondary),
       to_xent_color(to.TextOnAccentSecondary)},
      {to_xent_color(from.TextOnAccentDisabled),
       to_xent_color(to.TextOnAccentDisabled)},

      {to_xent_color(from.ControlFillDefault),
       to_xent_color(to.ControlFillDefault)},
      {to_xent_color(from.ControlFillSecondary),
       to_xent_color(to.ControlFillSecondary)},
      {to_xent_color(from.ControlFillTertiary),
       to_xent_color(to.ControlFillTertiary)},
      {to_xent_color(from.ControlFillQuarternary),
       to_xent_color(to.ControlFillQuarternary)},
      {to_xent_color(from.ControlFillDisabled),
       to_xent_color(to.ControlFillDisabled)},
      {to_xent_color(from.ControlFillTransparent),
       to_xent_color(to.ControlFillTransparent)},
      {to_xent_color(from.ControlFillInputActive),
       to_xent_color(to.ControlFillInputActive)},

      {to_xent_color(from.ControlStrongFillDefault),
       to_xent_color(to.ControlStrongFillDefault)},
      {to_xent_color(from.ControlStrongFillDisabled),
       to_xent_color(to.ControlStrongFillDisabled)},

      {to_xent_color(from.ControlSolidFillDefault),
       to_xent_color(to.ControlSolidFillDefault)},

      {to_xent_color(from.SubtleFillTransparent),
       to_xent_color(to.SubtleFillTransparent)},
      {to_xent_color(from.SubtleFillSecondary),
       to_xent_color(to.SubtleFillSecondary)},
      {to_xent_color(from.SubtleFillTertiary),
       to_xent_color(to.SubtleFillTertiary)},
      {to_xent_color(from.SubtleFillDisabled),
       to_xent_color(to.SubtleFillDisabled)},

      {to_xent_color(from.ControlAltFillTransparent),
       to_xent_color(to.ControlAltFillTransparent)},
      {to_xent_color(from.ControlAltFillSecondary),
       to_xent_color(to.ControlAltFillSecondary)},
      {to_xent_color(from.ControlAltFillTertiary),
       to_xent_color(to.ControlAltFillTertiary)},
      {to_xent_color(from.ControlAltFillQuarternary),
       to_xent_color(to.ControlAltFillQuarternary)},
      {to_xent_color(from.ControlAltFillDisabled),
       to_xent_color(to.ControlAltFillDisabled)},

      {to_xent_color(from.ControlOnImageFillDefault),
       to_xent_color(to.ControlOnImageFillDefault)},
      {to_xent_color(from.ControlOnImageFillSecondary),
       to_xent_color(to.ControlOnImageFillSecondary)},
      {to_xent_color(from.ControlOnImageFillTertiary),
       to_xent_color(to.ControlOnImageFillTertiary)},
      {to_xent_color(from.ControlOnImageFillDisabled),
       to_xent_color(to.ControlOnImageFillDisabled)},

      {to_xent_color(from.AccentDefault), to_xent_color(to.AccentDefault)},
      {to_xent_color(from.AccentSecondary), to_xent_color(to.AccentSecondary)},
      {to_xent_color(from.AccentTertiary), to_xent_color(to.AccentTertiary)},
      {to_xent_color(from.AccentDisabled), to_xent_color(to.AccentDisabled)},

      {to_xent_color(from.ControlStrokeDefault),
       to_xent_color(to.ControlStrokeDefault)},
      {to_xent_color(from.ControlStrokeSecondary),
       to_xent_color(to.ControlStrokeSecondary)},
      {to_xent_color(from.ControlStrokeOnAccentDefault),
       to_xent_color(to.ControlStrokeOnAccentDefault)},
      {to_xent_color(from.ControlStrokeOnAccentSecondary),
       to_xent_color(to.ControlStrokeOnAccentSecondary)},
      {to_xent_color(from.ControlStrokeOnAccentTertiary),
       to_xent_color(to.ControlStrokeOnAccentTertiary)},
      {to_xent_color(from.ControlStrokeOnAccentDisabled),
       to_xent_color(to.ControlStrokeOnAccentDisabled)},
      {to_xent_color(from.ControlStrokeForStrongFillWhenOnImage),
       to_xent_color(to.ControlStrokeForStrongFillWhenOnImage)},

      {to_xent_color(from.ControlStrongStrokeDefault),
       to_xent_color(to.ControlStrongStrokeDefault)},
      {to_xent_color(from.ControlStrongStrokeDisabled),
       to_xent_color(to.ControlStrongStrokeDisabled)},

      {to_xent_color(from.CardStrokeDefault),
       to_xent_color(to.CardStrokeDefault)},
      {to_xent_color(from.CardStrokeDefaultSolid),
       to_xent_color(to.CardStrokeDefaultSolid)},
      {to_xent_color(from.CardBackgroundDefault),
       to_xent_color(to.CardBackgroundDefault)},
      {to_xent_color(from.CardBackgroundSecondary),
       to_xent_color(to.CardBackgroundSecondary)},
      {to_xent_color(from.CardBackgroundTertiary),
       to_xent_color(to.CardBackgroundTertiary)},

      {to_xent_color(from.SurfaceStrokeDefault),
       to_xent_color(to.SurfaceStrokeDefault)},
      {to_xent_color(from.SurfaceStrokeFlyout),
       to_xent_color(to.SurfaceStrokeFlyout)},
      {to_xent_color(from.SurfaceStrokeInverse),
       to_xent_color(to.SurfaceStrokeInverse)},

      {to_xent_color(from.FocusStrokeOuter),
       to_xent_color(to.FocusStrokeOuter)},
      {to_xent_color(from.FocusStrokeInner),
       to_xent_color(to.FocusStrokeInner)},

      {to_xent_color(from.DividerStrokeDefault),
       to_xent_color(to.DividerStrokeDefault)},

      {to_xent_color(from.SolidBackgroundBase),
       to_xent_color(to.SolidBackgroundBase)},
      {to_xent_color(from.SolidBackgroundSecondary),
       to_xent_color(to.SolidBackgroundSecondary)},
      {to_xent_color(from.SolidBackgroundTertiary),
       to_xent_color(to.SolidBackgroundTertiary)},
      {to_xent_color(from.SolidBackgroundQuarternary),
       to_xent_color(to.SolidBackgroundQuarternary)},
      {to_xent_color(from.SolidBackgroundQuinary),
       to_xent_color(to.SolidBackgroundQuinary)},
      {to_xent_color(from.SolidBackgroundSenary),
       to_xent_color(to.SolidBackgroundSenary)},
      {to_xent_color(from.SolidBackgroundTransparent),
       to_xent_color(to.SolidBackgroundTransparent)},
      {to_xent_color(from.SolidBackgroundBaseAlt),
       to_xent_color(to.SolidBackgroundBaseAlt)},
      {to_xent_color(from.LayerFillDefault),
       to_xent_color(to.LayerFillDefault)},
      {to_xent_color(from.LayerFillAlt), to_xent_color(to.LayerFillAlt)},
      {to_xent_color(from.LayerOnAcrylicFillDefault),
       to_xent_color(to.LayerOnAcrylicFillDefault)},
      {to_xent_color(from.LayerOnAccentAcrylicFillDefault),
       to_xent_color(to.LayerOnAccentAcrylicFillDefault)},
      {to_xent_color(from.LayerOnMicaBaseAltDefault),
       to_xent_color(to.LayerOnMicaBaseAltDefault)},
      {to_xent_color(from.LayerOnMicaBaseAltSecondary),
       to_xent_color(to.LayerOnMicaBaseAltSecondary)},
      {to_xent_color(from.LayerOnMicaBaseAltTertiary),
       to_xent_color(to.LayerOnMicaBaseAltTertiary)},
      {to_xent_color(from.LayerOnMicaBaseAltTransparent),
       to_xent_color(to.LayerOnMicaBaseAltTransparent)},
      {to_xent_color(from.SmokeFillDefault),
       to_xent_color(to.SmokeFillDefault)},

      {to_xent_color(from.SystemSuccess), to_xent_color(to.SystemSuccess)},
      {to_xent_color(from.SystemCaution), to_xent_color(to.SystemCaution)},
      {to_xent_color(from.SystemCritical), to_xent_color(to.SystemCritical)},
      {to_xent_color(from.SystemNeutral), to_xent_color(to.SystemNeutral)},
      {to_xent_color(from.SystemSolidNeutral),
       to_xent_color(to.SystemSolidNeutral)},
      {to_xent_color(from.SystemAttentionBackground),
       to_xent_color(to.SystemAttentionBackground)},
      {to_xent_color(from.SystemSuccessBackground),
       to_xent_color(to.SystemSuccessBackground)},
      {to_xent_color(from.SystemCautionBackground),
       to_xent_color(to.SystemCautionBackground)},
      {to_xent_color(from.SystemCriticalBackground),
       to_xent_color(to.SystemCriticalBackground)},
      {to_xent_color(from.SystemNeutralBackground),
       to_xent_color(to.SystemNeutralBackground)},
      {to_xent_color(from.SystemSolidAttentionBackground),
       to_xent_color(to.SystemSolidAttentionBackground)},
      {to_xent_color(from.SystemSolidNeutralBackground),
       to_xent_color(to.SystemSolidNeutralBackground)},
  };
}

static void remap_color_in_place(xent::Color &c,
                                 const std::vector<ThemeRemapEntry> &map) {
  for (const auto &e : map) {
    if (color_equal(c, e.from)) {
      c = e.to;
      return;
    }
  }
}

static void
remap_view_tree_theme_colors(const std::shared_ptr<xent::ViewData> &data,
                             const std::vector<ThemeRemapEntry> &map) {
  if (!data)
    return;
  remap_color_in_place(data->text_color, map);
  remap_color_in_place(data->background_color, map);
  remap_color_in_place(data->border_color, map);

  for (const auto &child : data->children) {
    remap_view_tree_theme_colors(child, map);
  }
}

static bool is_zero_padding(const xent::ViewData &d) {
  return d.padding_top == 0.0f && d.padding_right == 0.0f &&
         d.padding_bottom == 0.0f && d.padding_left == 0.0f;
}

static void
apply_button_defaults_recursive(const std::shared_ptr<xent::ViewData> &data) {
  if (!data)
    return;

  if (data->component_type == "Button" ||
      data->component_type == "ToggleButton") {
    // WinUI3 DefaultButtonStyle content padding: 5,11,6,11 (T,R,B,L)
    if (is_zero_padding(*data)) {
      data->padding_top = 5.0f;
      data->padding_right = 11.0f;
      data->padding_bottom = 6.0f;
      data->padding_left = 11.0f;
    }

    // NOTE: WinUI3 does not set a default MinHeight/MinWidth for Button in
    // XAML. Keep framework defaults (0) unless the app sets constraints
    // explicitly.
  }

  if (data->component_type == "ToggleSwitch") {
    // Default state: off.
    if (data->text_content.empty()) {
      data->text_content = "0";
    }

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
  }

  for (const auto &child : data->children) {
    apply_button_defaults_recursive(child);
  }
}

RenderEngine::RenderEngine(GraphicsPipeline *graphics) : graphics_(graphics) {
  if (!graphics_) {
    throw std::invalid_argument("Graphics pipeline cannot be null");
  }
  d2d_context_ = graphics_->get_d2d_context();
  text_renderer_ = std::make_unique<TextRenderer>(graphics_);
  control_renderer_ = std::make_unique<controls::ControlRenderer>(
      graphics_, text_renderer_.get());

  // Initialize theme snapshot and subscribe for automatic UI updates.
  last_theme_resources_ = theme::res();
  theme_listener_id_ =
      theme::ThemeManager::instance().add_theme_changed_listener(
          [this](theme::Mode) {
            const auto next = theme::res();
            const auto map = build_theme_remap(last_theme_resources_, next);
            last_theme_resources_ = next;

            if (auto root = last_root_data_.lock()) {
              remap_view_tree_theme_colors(root, map);
            }

            if (graphics_) {
              graphics_->request_redraw();
            }
          });
}

RenderEngine::~RenderEngine() {
  if (theme_listener_id_ != 0) {
    theme::ThemeManager::instance().remove_theme_changed_listener(
        theme_listener_id_);
    theme_listener_id_ = 0;
  }
}

// Text measurement callback
std::pair<float, float>
RenderEngine::measure_text_callback(const std::string &text, float font_size,
                                    float max_width) {
  if (text.empty())
    return {0.0f, 0.0f};

  std::wstring wtext(text.begin(), text.end());

  TextStyle style;
  style.font_size = font_size;

  Size measured = text_renderer_->measure_text(
      wtext, style, max_width > 0 ? max_width : 0.0f);
  return {measured.width, measured.height};
}

void RenderEngine::render(const xent::View &root) {
  if (!d2d_context_)
    return;

  draw_view_recursive(root, 0.0f, 0.0f);
}

void RenderEngine::render_frame(xent::View &root) {
  if (!graphics_)
    return;

  Size size = graphics_->get_render_target_size();
  xent::set_text_measure_func(std::bind(
      &RenderEngine::measure_text_callback, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3));

  auto root_data = root.data();
  if (root_data) {
    last_root_data_ = root_data;
    apply_button_defaults_recursive(root_data);
    root_data->calculate_layout(size.width, size.height);
  }

  graphics_->begin_draw();
  graphics_->clear(Color::transparent());

  if (control_renderer_) {
    control_renderer_->begin_frame();
  }
  render(root);
  graphics_->end_draw();

  if (control_renderer_) {
    control_renderer_->end_frame();
  }
  graphics_->present();
}

void RenderEngine::draw_view_recursive(const xent::View &view, float parent_x,
                                       float parent_y) {
  auto view_data = view.data();
  if (!view_data)
    return;

  draw_view_data_recursive(view_data, parent_x, parent_y);
}

void RenderEngine::draw_view_data_recursive(
    const std::shared_ptr<xent::ViewData> &data, float parent_x,
    float parent_y) {
  if (!data)
    return;

  float x = data->layout_x();
  float y = data->layout_y();
  float w = data->layout_width();
  float h = data->layout_height();
  float abs_x = parent_x + x;
  float abs_y = parent_y + y;
  Rect bounds(abs_x, abs_y, w, h);

  if (control_renderer_ && !data->component_type.empty()) {
    controls::ControlState state;
    if (input_) {
      if (auto hovered = input_->get_hovered_view()) {
        state.is_hovered = (hovered.get() == data.get());
      }
      if (auto pressed = input_->get_pressed_view()) {
        state.is_pressed = (pressed.get() == data.get());
      }
      if (auto focused = input_->get_focused_view()) {
        state.is_focused = input_->should_show_focus_visuals() &&
                           (focused.get() == data.get());
      }
    }
    control_renderer_->render(*data, bounds, state);
  } else if (control_renderer_) {
    control_renderer_->render_view(*data, bounds);
  } else {
    draw_view_background(*data, bounds);
    draw_view_border(*data, bounds);
    draw_view_text(*data, bounds);
  }

  if (debug_mode_) {
    draw_rect(bounds, Color(255, 0, 255, 128), 1.0f);
  }

  for (const auto &child : data->children) {
    draw_view_data_recursive(child, abs_x, abs_y);
  }
}

void RenderEngine::draw_view_background(const xent::ViewData &data,
                                        const Rect &bounds) {
  Color color = convert_color(data.background_color);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    fill_rounded_rect(bounds, radius, color);
  } else {
    fill_rect(bounds, color);
  }
}

void RenderEngine::draw_view_border(const xent::ViewData &data,
                                    const Rect &bounds) {
  float border_width = data.border_width;
  if (border_width <= 0)
    return;

  Color color = convert_color(data.border_color);
  if (color.is_transparent())
    return;

  float radius = data.corner_radius;

  if (radius > 0) {
    draw_rounded_rect(bounds, radius, color, border_width);
  } else {
    draw_rect(bounds, color, border_width);
  }
}

void RenderEngine::draw_view_text(const xent::ViewData &data,
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

  text_renderer_->draw_text(wtext, bounds, style);
}

// Primitives

void RenderEngine::fill_rect(const Rect &rect, const Color &color) {
  if (!d2d_context_)
    return;

  auto *brush = get_brush(color);
  if (brush) {
    d2d_context_->FillRectangle(rect.to_d2d(), brush);
  }
}

void RenderEngine::fill_rounded_rect(const Rect &rect, float radius,
                                     const Color &color) {
  if (!d2d_context_)
    return;

  auto *brush = get_brush(color);
  if (brush) {
    D2D1_ROUNDED_RECT rounded =
        D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->FillRoundedRectangle(rounded, brush);
  }
}

void RenderEngine::draw_rect(const Rect &rect, const Color &color,
                             float stroke_width) {
  if (!d2d_context_)
    return;

  auto *brush = get_brush(color);
  if (brush) {
    d2d_context_->DrawRectangle(rect.to_d2d(), brush, stroke_width);
  }
}

void RenderEngine::draw_rounded_rect(const Rect &rect, float radius,
                                     const Color &color, float stroke_width) {
  if (!d2d_context_)
    return;

  auto *brush = get_brush(color);
  if (brush) {
    D2D1_ROUNDED_RECT rounded =
        D2D1::RoundedRect(rect.to_d2d(), radius, radius);
    d2d_context_->DrawRoundedRectangle(rounded, brush, stroke_width);
  }
}

// Clipping

void RenderEngine::push_clip(const Rect &rect) {
  if (d2d_context_) {
    d2d_context_->PushAxisAlignedClip(rect.to_d2d(),
                                      D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void RenderEngine::pop_clip() {
  if (d2d_context_) {
    d2d_context_->PopAxisAlignedClip();
  }
}

// Transform

void RenderEngine::push_transform(const D2D1_MATRIX_3X2_F &transform) {
  if (!d2d_context_)
    return;

  D2D1_MATRIX_3X2_F current;
  d2d_context_->GetTransform(&current);
  transform_stack_.push_back(current);

  D2D1_MATRIX_3X2_F combined = transform * current;
  d2d_context_->SetTransform(combined);
}

void RenderEngine::pop_transform() {
  if (!d2d_context_ || transform_stack_.empty())
    return;

  d2d_context_->SetTransform(transform_stack_.back());
  transform_stack_.pop_back();
}

void RenderEngine::push_translation(float x, float y) {
  push_transform(D2D1::Matrix3x2F::Translation(x, y));
}

// Resources

ID2D1SolidColorBrush *RenderEngine::get_brush(const Color &color) {
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

void RenderEngine::clear_brush_cache() { brush_cache_.clear(); }

} // namespace fluxent
