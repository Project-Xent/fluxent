#pragma once

// FluXent: Windows rendering backend for Xent.
//

// Core types
#include "fluxent/types.hpp"

// Graphics pipeline (D3D11/D2D1/DComp)
#include "fluxent/graphics.hpp" // IWYU pragma: export

// Window management (Win32/DWM)
#include "fluxent/window.hpp"

// Render engine (View tree traversal)
#include "fluxent/engine.hpp"

// Input handling (Hit testing/Event dispatch)
#include "fluxent/input.hpp"

// Text rendering (DirectWrite)
#include "fluxent/text.hpp"

// Theme system (Fluent Design colors)
#include "fluxent/theme.hpp" // IWYU pragma: export

// Control rendering (Fluent Design controls)
#include "fluxent/controls/control_renderer.hpp" // IWYU pragma: export

// Controls
#include "fluxent/controls/toggle_switch.hpp" // IWYU pragma: export

namespace fluxent {

// Application: optional convenience wrapper around
// Window/RenderEngine/InputHandler

class Application {
public:
  explicit Application(const WindowConfig &config = {})
      : window_(&theme_manager_, config),
        engine_(window_.GetGraphics(), &theme_manager_),
        text_renderer_(window_.GetGraphics()) {
    input_.SetInvalidateCallback([this]() { window_.RequestRender(); });

    window_.SetMouseCallback([this](const MouseEvent &e) {
      if (has_root_) {
        input_.HandleMouseEvent(root_view_, e);
      }
    });

    window_.SetRenderCallback([this]() {
      if (has_root_) {
        engine_.RenderFrame(root_view_);
      }
    });
  }

  void SetRoot(xent::View root) {
    root_view_ = std::move(root);
    has_root_ = true;
    window_.RequestRender();
  }

  void Run() { window_.Run(); }

  Window &GetWindow() { return window_; }
  RenderEngine &GetEngine() { return engine_; }
  InputHandler &GetInput() { return input_; }
  TextRenderer &GetText() { return text_renderer_; }

private:
  theme::ThemeManager theme_manager_;
  Window window_;
  RenderEngine engine_;
  InputHandler input_;
  TextRenderer text_renderer_;
  xent::View root_view_;
  bool has_root_ = false;
};

} // namespace fluxent
