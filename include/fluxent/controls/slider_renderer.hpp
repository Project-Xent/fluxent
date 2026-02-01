#pragma once

#include "render_context.hpp"
#include "fluxent/controls/animation_utils.hpp"
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <wrl/client.h>
#include <xent/view.hpp>

namespace fluxent::controls
{

class SliderRenderer
{
public:
  struct SliderState
  {
    Animator<float> scale_anim;
    Animator<Color> color_anim;
  };

  void BeginFrame();
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::ViewData &data, const Rect &bounds,
              const ControlState &state);

private:
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
  std::unordered_map<const xent::ViewData *, SliderState> states_;
  std::unordered_set<const xent::ViewData *> seen_;
  bool has_active_transitions_ = false;

  // Helpers
  ID2D1SolidColorBrush *GetBrush(ID2D1DeviceContext *d2d, const Color &color);

  float AnimateScale(const xent::ViewData *key, float target);
  Color AnimateColor(const xent::ViewData *key, const Color &target);
};

} // namespace fluxent::controls
