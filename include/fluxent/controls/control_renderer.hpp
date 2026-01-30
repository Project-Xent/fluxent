#pragma once

#include "../graphics.hpp"
#include "../text.hpp"

#include <xent/view.hpp>

#include <chrono>
#include <unordered_set>

namespace theme {
class ThemeManager;
}

namespace fluxent {
class PluginManager;
}

#include "render_context.hpp"
#include <memory>

namespace fluxent::controls {

class ButtonRenderer;
class CheckBoxRenderer;
class RadioButtonRenderer;
class ScrollViewRenderer;
class SliderRenderer;
class ToggleSwitchRenderer;
class TextBoxRenderer;


class ControlRenderer {
public:
  ControlRenderer(GraphicsPipeline *graphics, TextRenderer *text,
                  theme::ThemeManager *theme_manager,
                  PluginManager *plugin_manager = nullptr);
  ~ControlRenderer();

  void BeginFrame();
  void EndFrame();

  void Render(const xent::ViewData &data, const Rect &bounds,
              const ControlState &state);

  void RenderButton(const xent::ViewData &data, const Rect &bounds,
                    const ControlState &state);

  void RenderToggleSwitch(const xent::ViewData &data, const Rect &bounds,
                          const ControlState &state);

  void RenderToggleButton(const xent::ViewData &data, const Rect &bounds,
                          const ControlState &state);

  void RenderCheckBox(const xent::ViewData &data, const Rect &bounds,
                      const ControlState &state);

  void RenderRadioButton(const xent::ViewData &data, const Rect &bounds,
                         const ControlState &state);

  void RenderSlider(const xent::ViewData &data, const Rect &bounds,
                    const ControlState &state);

  void RenderTextBox(const xent::ViewData &data, const Rect &bounds,
                     const ControlState &state);

  void RenderScrollView(const xent::ViewData &data, const Rect &bounds,
                        const ControlState &state);


  void RenderOverlay(const xent::ViewData &data, const Rect &bounds,
                     const ControlState &state);

  void RenderText(const xent::ViewData &data, const Rect &bounds);

  void RenderCard(const xent::ViewData &data, const Rect &bounds);

  void RenderDivider(const Rect &bounds, bool horizontal = true);

  void RenderView(const xent::ViewData &data, const Rect &bounds);

private:
  void UpdateHoverOverlay();
  void EnsureHoverOverlaySurface(int width, int height, const Color &color);

  ID2D1SolidColorBrush *GetBrush(const Color &color);

  void DrawElevationBorder(const Rect &bounds, float corner_radius,
                           bool is_accent);

  Rect DrawControlSurface(const Rect &bounds, float corner_radius,
                          const Color &fill_color, const Color &stroke_color,
                          bool is_accent, const ControlState &state);

  void DrawFocusRect(const Rect &bounds, float corner_radius);

  GraphicsPipeline *graphics_;
  TextRenderer *text_renderer_;
  theme::ThemeManager *theme_manager_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;

  Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
      control_elevation_border_brush_;
  Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
      control_accent_elevation_border_brush_;

  Microsoft::WRL::ComPtr<IDCompositionVisual> hover_visual_;
  Microsoft::WRL::ComPtr<IDCompositionSurface> hover_surface_;
  Rect hover_bounds_;
  float hover_corner_radius_ = 0.0f;
  float hover_opacity_ = 0.0f;
  bool hover_visual_added_ = false;
  const xent::ViewData *frame_hovered_button_ = nullptr;

  int hover_surface_width_ = 0;
  int hover_surface_height_ = 0;
  Color hover_surface_color_ = Color::transparent();

  std::unordered_set<const xent::ViewData *> frame_buttons_seen_;

  uint64_t last_theme_version_ = 0;
  void CheckResources();

  std::unique_ptr<ButtonRenderer> button_renderer_;
  std::unique_ptr<ToggleSwitchRenderer> toggle_renderer_;
  std::unique_ptr<CheckBoxRenderer> checkbox_renderer_;
  std::unique_ptr<RadioButtonRenderer> radio_renderer_;
  std::unique_ptr<SliderRenderer> slider_renderer_;
  std::unique_ptr<TextBoxRenderer> textbox_renderer_;
  std::unique_ptr<ScrollViewRenderer> scroll_view_renderer_;

  RenderContext ctx_;
};

} // namespace fluxent::controls
