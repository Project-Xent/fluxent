#pragma once

#include "render_context.hpp"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <xent/view.hpp>

namespace fluxent::controls {

class CheckBoxRenderer {
public:
  void BeginFrame();
  bool EndFrame();

  void RenderCheckBox(const RenderContext &ctx, const xent::ViewData &data,
                      const Rect &bounds, const ControlState &state);

private:
  struct CheckTransition {
    bool initialized = false;
    bool active = false;
    float current = 0.0f;
    float from = 0.0f;
    float to = 0.0f;
    std::chrono::steady_clock::time_point start;
  };

  struct FloatTransition {
    bool initialized = false;
    bool active = false;
    float current = 0.0f;
    float from = 0.0f;
    float to = 0.0f;
    std::chrono::steady_clock::time_point start;
  };

  std::unordered_map<const xent::ViewData *, CheckTransition>
      check_transitions_;
  std::unordered_map<const xent::ViewData *, FloatTransition>
      scale_transitions_;

  std::unordered_set<const xent::ViewData *> seen_;
  bool has_active_transitions_ = false;

  float AnimateCheckState(const xent::ViewData *key, bool is_checked);
  float AnimateFloat(const xent::ViewData *key, float target,
                     float duration_ms = 83.0f);
};

} // namespace fluxent::controls
