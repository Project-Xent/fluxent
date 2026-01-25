#pragma once
#include "render_context.hpp"

namespace fluxent::controls {

void DrawFocusRect(const RenderContext &ctx, const Rect &bounds,
                     float corner_radius);

} // namespace fluxent::controls
