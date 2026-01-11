// Hello FluXent example

#include "fluxent/theme/theme_manager.hpp"
#include <cstdint>
#include <fluxent/fluxent.hpp>
#include <xent/xent.hpp>
#include <xent/vstack.hpp>
#include <xent/text.hpp>
#include <xent/button.hpp>
#include <Windows.h>

using namespace fluxent;

template <typename T>
static xent::Color to_xent_color(const T& c) {
    return xent::Color{
        static_cast<std::uint8_t>(c.r),
        static_cast<std::uint8_t>(c.g),
        static_cast<std::uint8_t>(c.b),
        static_cast<std::uint8_t>(c.a),
    };
}

// Forward declare build_ui
xent::View build_ui(class App& app);

// Global context

class App;
Window* g_window = nullptr;
RenderEngine* g_engine = nullptr;
InputHandler* g_input = nullptr;
App* g_app = nullptr;
xent::View* g_root_view = nullptr;

// App state

class App {
public:
    int counter = 0;
    std::shared_ptr<xent::ViewData> counter_text_data;
    std::function<void()> invalidate_callback;
    
    void set_invalidate_callback(std::function<void()> callback) {
        invalidate_callback = std::move(callback);
    }
    
    void increment() {
        counter++;
        if (counter_text_data && invalidate_callback) {
            counter_text_data->text_content = "Count: " + std::to_string(counter);
            invalidate_callback();
        }
    }
    
    void decrement() {
        counter--;
        if (counter_text_data && invalidate_callback) {
            counter_text_data->text_content = "Count: " + std::to_string(counter);
            invalidate_callback();
        }
    }
    
    void reset() {
        counter = 0;
        if (counter_text_data && invalidate_callback) {
            counter_text_data->text_content = "Count: " + std::to_string(counter);
            invalidate_callback();
        }
    }
};

// Callbacks

void on_window_render() {
    if (g_engine && g_window && g_root_view) {
        g_engine->render_frame(*g_root_view);
    }
}

void on_window_mouse_event(const MouseEvent& event) {
    if (g_input && g_window && g_root_view) {
        g_input->handle_mouse_event(*g_root_view, event);
    }
}

void on_window_resize(int width, int height) {
    (void)width;
    (void)height;
    if (g_window) {
        g_window->request_render();
    }
}

void on_window_invalidate() {
    if (g_window) {
        g_window->request_render();
    }
}

void on_input_invalidate() {
    if (g_window) {
        g_window->request_render();
    }
}

// Build UI

xent::View build_ui(App& app) {
    auto counter_text = xent::Text{"Count: 0"}
        .font_size(36)
        .color(to_xent_color(theme::res().TextPrimary));

    app.counter_text_data = counter_text.data();
    
    auto root = xent::VStack{}
        .padding(32)
        .gap(20)
        .align_items(YGAlignCenter)
        .justify_content(YGJustifyCenter)
        .background(to_xent_color(theme::res().SolidBackgroundBase))
        .expand()
        .add(
            xent::Text{"Hello, FluXent!"}
                .font_size(32)
                .color(to_xent_color(theme::res().TextPrimary))
        )
        .add(
            xent::Text{"A modern Windows UI framework"}
                .font_size(14)
                .color(to_xent_color(theme::res().TextSecondary))
        )
        .add(counter_text)
        .add(
            xent::VStack{}
                .padding(20)
                .gap(12)
                .align_items(YGAlignCenter)
                .background(to_xent_color(theme::res().CardBackgroundDefault))
                .corner_radius(8)
                .add(
                    xent::Button{"+1"}
                        .role(xent::Semantic::Primary)
                        .on_click(&App::increment, &app)
                )
                .add(
                    xent::Button{"-1"}
                        .role(xent::Semantic::Secondary)
                        .style(xent::ButtonStyle::Outline)
                        .on_click(&App::decrement, &app)
                )
                .add(
                    xent::Button{"Reset"}
                        .role(xent::Semantic::Danger)
                        .style(xent::ButtonStyle::Text)
                        .on_click(&App::reset, &app)
                )
        )
        .add(
            xent::Text{"Click the buttons!"}
                .font_size(12)
                .color(to_xent_color(theme::res().TextSecondary))
        );

    return root;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WindowConfig config;
    config.title = L"Hello FluXent";
    config.width = 400;
    config.height = 500;
    
    Window window(config);
    if (!window.is_valid()) {
        return -1;
    }
    
    RenderEngine engine(window.get_graphics());
    engine.set_debug_mode(false);

    InputHandler input;
    engine.set_input_handler(&input);

    App app;

    g_window = &window;
    g_engine = &engine;
    g_input = &input;
    g_app = &app;

    input.set_invalidate_callback(on_input_invalidate);
    app.set_invalidate_callback(on_window_invalidate);

    xent::View root = build_ui(app);
    g_root_view = &root;

    window.set_render_callback(on_window_render);
    window.set_mouse_callback(on_window_mouse_event);
    window.set_resize_callback(on_window_resize);

    window.request_render();

    window.run();

    g_root_view = nullptr;
    
    return 0;
}
