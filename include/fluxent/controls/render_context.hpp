#pragma once

#include "../graphics.hpp"
#include "../text.hpp"
#include "../theme/theme_manager.hpp"

namespace fluxent {
class PluginManager;
}

namespace fluxent::controls {

struct ControlState {
  bool is_hovered = false;
  bool is_pressed = false;
  bool is_focused = false;
  bool is_disabled = false;
  float mouse_x = 0.0f;
  float mouse_y = 0.0f;
};

struct RenderContext {
  GraphicsPipeline *graphics;
  TextRenderer *text;
  theme::ThemeManager *theme_manager;
  PluginManager *plugins;

  const theme::ThemeResources &Resources() const {
    return theme_manager->Resources();
  }

  theme::Mode Mode() const { return theme_manager->GetMode(); }
};

} // namespace fluxent::controls
