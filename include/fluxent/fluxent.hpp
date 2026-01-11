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

// Application: optional convenience wrapper around Window/RenderEngine/InputHandler

class Application {
public:
    explicit Application(const WindowConfig& config = {})
        : window_(config)
        , engine_(window_.get_graphics())
        , text_renderer_(window_.get_graphics())
    {
        input_.set_invalidate_callback([this]() {
            window_.request_render();
        });

        window_.set_mouse_callback([this](const MouseEvent& e) {
            if (has_root_) {
                input_.handle_mouse_event(root_view_, e);
            }
        });

        window_.set_render_callback([this]() {
            if (has_root_) {
                engine_.render_frame(root_view_);
            }
        });
    }

    void set_root(xent::View root) {
        root_view_ = std::move(root);
        has_root_ = true;
        window_.request_render();
    }

    void run() {
        window_.run();
    }

    Window& window() { return window_; }
    RenderEngine& engine() { return engine_; }
    InputHandler& input() { return input_; }
    TextRenderer& text() { return text_renderer_; }

private:
    Window window_;
    RenderEngine engine_;
    InputHandler input_;
    TextRenderer text_renderer_;
    xent::View root_view_;
    bool has_root_ = false;
};

} // namespace fluxent
