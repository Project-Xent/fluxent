#pragma once

#include "render_context.hpp"
#include "fluxent/controls/animation_utils.hpp"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <xent/view.hpp>

namespace fluxent::controls
{

class ToggleSwitchRenderer
{
public:
  void BeginFrame();
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::ViewData &data, const Rect &bounds,
              const ControlState &state);

private:
  struct ToggleState
  {
    Animator<float> progress_anim;
  };

  std::unordered_map<const xent::ViewData *, ToggleState> states_;
  std::unordered_set<const xent::ViewData *> seen_;
  bool has_active_transitions_ = false;

  float AnimateProgress(const xent::ViewData *key, float target);
};

} // namespace fluxent::controls
