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
  static Result<std::unique_ptr<Application>>
  Create(const WindowConfig &config = {}) {
    auto app = std::unique_ptr<Application>(new Application());
    auto win_res = Window::Create(&app->theme_manager_, config);
    if (!win_res)
      return tl::unexpected(win_res.error());
    app->window_ = std::move(*win_res);

    auto eng_res =
        RenderEngine::Create(app->window_->GetGraphics(), &app->theme_manager_);
    if (!eng_res)
      return tl::unexpected(eng_res.error());
    app->engine_ = std::move(*eng_res);

    auto txt_res = TextRenderer::Create(app->window_->GetGraphics());
    if (!txt_res)
      return tl::unexpected(txt_res.error());
    app->text_renderer_ = std::move(*txt_res);

    app->input_.SetInvalidateCallback(
        {app.get(), [](void *ctx) { static_cast<Application *>(ctx)->window_->RequestRender(); }});

    app->window_->SetMouseCallback({app.get(), [](void *ctx, const MouseEvent &e) {
      auto *ptr = static_cast<Application *>(ctx);
      if (ptr->has_root_) {
        ptr->input_.HandleMouseEvent(ptr->root_view_, e);
      }
    }});

    app->window_->SetDirectManipulationUpdateCallback(
        {app.get(), [](void *ctx, float x, float y, float scale, bool centering) {
           auto *ptr = static_cast<Application *>(ctx);
           if (ptr->has_root_) {
             ptr->input_.HandleDirectManipulation(ptr->root_view_, x, y, scale,
                                                  centering);
           }
         }});

    app->window_->SetDirectManipulationHitTestCallback(
        {app.get(), [](void *ctx, UINT pointer_id, int x, int y) -> bool {
           auto *ptr = static_cast<Application *>(ctx);
           if (ptr->has_root_) {
              return true;
           }
           return false;
         }});

    app->window_->SetRenderCallback({app.get(), [](void *ctx) {
      auto *ptr = static_cast<Application *>(ctx);
      if (ptr->has_root_) {
        ptr->engine_->RenderFrame(ptr->root_view_);
      }
    }});

    return app;
  }

  void SetRoot(xent::View root) {
    root_view_ = std::move(root);
    has_root_ = true;
    window_->RequestRender();
  }

  void Run() { window_->Run(); }

  Window &GetWindow() { return *window_; }
  RenderEngine &GetEngine() { return *engine_; }
  InputHandler &GetInput() { return input_; }
  TextRenderer &GetText() { return *text_renderer_; }

private:
  Application() = default;

  theme::ThemeManager theme_manager_;
  std::unique_ptr<Window> window_;
  std::unique_ptr<RenderEngine> engine_;
  InputHandler input_;
  std::unique_ptr<TextRenderer> text_renderer_;
  xent::View root_view_;
  bool has_root_ = false;
};

} // namespace fluxent
