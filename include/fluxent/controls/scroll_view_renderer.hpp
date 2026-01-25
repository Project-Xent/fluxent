#pragma once

#include "render_context.hpp"
#include <xent/view.hpp>

namespace fluxent::controls {

class ScrollViewRenderer {
public:
  void BeginFrame();
  bool EndFrame();

  void Render(const RenderContext &ctx, const xent::ViewData &data,
              const Rect &bounds, const ControlState &state);
  void RenderOverlay(const RenderContext &ctx, const xent::ViewData &data,
                     const Rect &bounds, const ControlState &state);

private:
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;

  // Helpers
  ID2D1SolidColorBrush *GetBrush(ID2D1DeviceContext *d2d, const Color &color);
};

} // namespace fluxent::controls
