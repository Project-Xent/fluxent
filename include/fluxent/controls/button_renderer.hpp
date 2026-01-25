#pragma once

#include "render_context.hpp"
#include "fluxent/controls/animation_utils.hpp"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <xent/view.hpp>

namespace fluxent::controls {

class ButtonRenderer {
public:
  void BeginFrame();
  // Returns true if active animations are pending
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::ViewData &data,
              const Rect &bounds, const ControlState &state);
  void RenderToggleButton(const RenderContext &ctx, const xent::ViewData &data,
                          const Rect &bounds, const ControlState &state);

private:
  struct ButtonBackgroundTransition {
    Animator<Color> bg_anim;
  };

  std::unordered_map<const xent::ViewData *, ButtonBackgroundTransition>
      transitions_;
  std::unordered_set<const xent::ViewData *> seen_;
  bool has_active_transitions_ = false;

  Color AnimateBackground(const xent::ViewData *key, const Color &target);

  // Resources
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
  Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
      control_elevation_border_brush_;
  Color control_elevation_border_secondary_ = Color::transparent();
  Color control_elevation_border_default_ = Color::transparent();
  Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
      control_accent_elevation_border_brush_;
  Color control_accent_elevation_border_secondary_ = Color::transparent();
  Color control_accent_elevation_border_default_ = Color::transparent();

  // Helpers
  ID2D1SolidColorBrush *GetBrush(ID2D1DeviceContext *d2d, const Color &color);
  void DrawElevationBorder(const RenderContext &ctx, const Rect &bounds,
                           float corner_radius, bool is_accent);
  Rect DrawControlSurface(const RenderContext &ctx, const Rect &bounds,
                          float corner_radius, const Color &fill_color,
                          const Color &stroke_color, bool is_accent,
                          const ControlState &state);
};

} // namespace fluxent::controls
