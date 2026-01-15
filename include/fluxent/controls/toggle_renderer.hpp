#pragma once

#include "render_context.hpp"
#include <chrono>
#include <unordered_map>
#include <xent/view.hpp>

namespace fluxent::controls {

class ToggleSwitchRenderer {
public:
  void BeginFrame();
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::ViewData &data,
              const Rect &bounds, const ControlState &state);

private:
  struct ToggleProgressTransition {
    bool initialized = false;
    bool active = false;
    float current = 0.0f;
    float from = 0.0f;
    float to = 0.0f;
    float last_target = 0.0f;
    std::chrono::steady_clock::time_point start;
  };

  std::unordered_map<const xent::ViewData *, ToggleProgressTransition>
      transitions_;
  bool has_active_transitions_ = false;

  float AnimateProgress(const xent::ViewData *key, float target);
};

} // namespace fluxent::controls
