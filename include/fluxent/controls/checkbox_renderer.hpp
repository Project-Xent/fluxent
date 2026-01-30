#pragma once

#include "render_context.hpp"
#include "fluxent/controls/animation_utils.hpp"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <xent/view.hpp>

namespace fluxent::controls {

enum class CheckBoxVisualState {
  NormalOff,
  NormalOn,
  PointerOverOff,
  PointerOverOn,
  PressedOff,
  PressedOn
};

class CheckBoxRenderer {
public:
  void BeginFrame();
  bool EndFrame();

  void RenderCheckBox(const RenderContext &ctx, const xent::ViewData &data,
                      const Rect &bounds, const ControlState &state);

private:
  struct CheckBoxState {
    Animator<float> check_anim;
    Animator<float> scale_anim;
    CheckBoxVisualState visual_state = CheckBoxVisualState::NormalOff;
    CheckBoxVisualState prev_visual_state = CheckBoxVisualState::NormalOff;
    bool state_changed = false;
  };

  std::unordered_map<const xent::ViewData *, CheckBoxState> states_;
  std::unordered_set<const xent::ViewData *> seen_;
  bool has_active_transitions_ = false;

  float AnimateCheckState(const xent::ViewData *key, bool is_checked);
  float AnimateFloat(const xent::ViewData *key, float target,
                     float duration_ms = 83.0f);

  static CheckBoxVisualState DetermineVisualState(bool is_checked,
                                                   const ControlState& state);
};

} // namespace fluxent::controls
