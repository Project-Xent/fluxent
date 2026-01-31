#pragma once

#include "fluxent/controls/render_context.hpp"
#include "fluxent/types.hpp"
#include <xent/view.hpp>

namespace fluxent {

// Interface for platform-specific rendering plugins
class Plugin {
public:
  virtual ~Plugin() = default;

  virtual void Render(const xent::View &view, const controls::RenderContext &ctx,
                      const Rect &bounds) = 0;

  virtual void *GetUIAProvider(const xent::View &view) { return nullptr; }

  virtual void OnUpdate(float delta_time) {}
};

} // namespace fluxent
