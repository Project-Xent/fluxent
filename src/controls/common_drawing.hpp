#pragma once
#include "fluxent/controls/render_context.hpp"
#include "fluxent/types.hpp"

namespace fluxent::controls {

void DrawFocusRect(const RenderContext &ctx, const Rect &bounds,
                   float corner_radius);

} // namespace fluxent::controls
