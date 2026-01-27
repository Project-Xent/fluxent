#pragma once

#include "render_context.hpp"
#include <chrono>
#include <unordered_map>
#include <xent/view.hpp>

namespace fluxent::controls {

class TextBoxRenderer {
public:
  void BeginFrame();
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::View &data,
              const Rect &bounds, const ControlState &state);

private:
  void DrawElevationBorder(const RenderContext &ctx, const Rect &bounds,
                           float corner_radius, bool is_focused,
                           bool is_hovered);

  Rect DrawChrome(const RenderContext &ctx, const Rect &bounds,
                  const ControlState &state, float corner_radius);

  // Caret blink state per TextBox
  struct CaretState {
    std::chrono::steady_clock::time_point last_toggle;
    bool visible = true;
  };
  std::unordered_map<const xent::View *, CaretState> caret_states_;
  bool needs_redraw_ = false;
};

} // namespace fluxent::controls

