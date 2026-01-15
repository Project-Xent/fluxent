#pragma once

// FluXent control renderer (Fluent-style)

#include "../graphics.hpp"
#include "../text.hpp"

#include <xent/view.hpp>

#include <unordered_set>

namespace theme {
class ThemeManager;
}

#include "button_renderer.hpp"
#include "checkbox_renderer.hpp"
#include "radio_renderer.hpp"
#include "render_context.hpp"
#include "toggle_renderer.hpp"
#include <memory>

namespace fluxent::controls {

// ControlRenderer
class ControlRenderer {
public:
  ControlRenderer(GraphicsPipeline *graphics, TextRenderer *text,
                  theme::ThemeManager *theme_manager);
  ~ControlRenderer(); // Defined in .cpp to allow unique_ptr with incomplete
                      // types if needed

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

  void RenderText(const xent::ViewData &data, const Rect &bounds);

  void RenderCard(const xent::ViewData &data, const Rect &bounds);

  void RenderDivider(const Rect &bounds, bool horizontal = true);

  void RenderView(const xent::ViewData &data, const Rect &bounds);

private:
  void UpdateHoverOverlay();
  void EnsureHoverOverlaySurface(int width, int height, const Color &color);

  ID2D1SolidColorBrush *GetBrush(const Color &color);

  // Elevation border (gradient stroke). `is_accent` selects accent brush.
  void DrawElevationBorder(const Rect &bounds, float corner_radius,
                           bool is_accent);

  // Render surface helper (background, border, focus)
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

  // Sub-renderers
  std::unique_ptr<ButtonRenderer> button_renderer_;
  std::unique_ptr<ToggleSwitchRenderer> toggle_renderer_;
  std::unique_ptr<CheckBoxRenderer> checkbox_renderer_;
  std::unique_ptr<RadioButtonRenderer> radio_renderer_;
  RenderContext ctx_;
};

} // namespace fluxent::controls
