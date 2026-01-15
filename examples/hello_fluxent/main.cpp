// Hello FluXent example

#include "fluxent/theme/theme_manager.hpp"
#include <Windows.h>
#include <cstdint>
#include <fluxent/controls/toggle_switch.hpp>
#include <fluxent/fluxent.hpp>
#include <xent/button.hpp>
#include <xent/checkbox.hpp>
#include <xent/hstack.hpp>
#include <xent/radio_button.hpp>
#include <xent/text.hpp>
#include <xent/toggle_button.hpp>
#include <xent/vstack.hpp>
#include <xent/xent.hpp>

using namespace fluxent;

template <typename T> static xent::Color ToXentColor(const T &c) {
  return xent::Color{
      static_cast<std::uint8_t>(c.r),
      static_cast<std::uint8_t>(c.g),
      static_cast<std::uint8_t>(c.b),
      static_cast<std::uint8_t>(c.a),
  };
}

// Forward declare build_ui
xent::View build_ui(class App &app, fluxent::theme::ThemeManager &tm);

// Global context

class App;
Window *g_window = nullptr;
RenderEngine *g_engine = nullptr;
InputHandler *g_input = nullptr;
App *g_app = nullptr;
xent::View *g_root_view = nullptr;

// App state

class App {
public:
  int counter = 0;
  std::shared_ptr<xent::ViewData> counter_text_data;
  std::function<void()> invalidate_callback;

  void SetInvalidateCallback(std::function<void()> callback) {
    invalidate_callback = std::move(callback);
  }

  void increment() {
    counter++;
    if (counter_text_data && invalidate_callback) {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node) {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }

  void decrement() {
    counter--;
    if (counter_text_data && invalidate_callback) {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node) {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }

  void reset() {
    counter = 0;
    if (counter_text_data && invalidate_callback) {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node) {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }
};

// Callbacks

void on_window_render() {
  if (g_engine && g_window && g_root_view) {
    g_engine->RenderFrame(*g_root_view);
  }
}

void on_window_mouse_event(const MouseEvent &event) {
  if (g_input && g_window && g_root_view) {
    g_input->HandleMouseEvent(*g_root_view, event);
  }
}

void on_window_resize(int width, int height) {
  (void)width;
  (void)height;
  if (g_window) {
    g_window->RequestRender();
  }
}

void on_window_invalidate() {
  if (g_window) {
    g_window->RequestRender();
  }
}

void on_input_invalidate() {
  if (g_window) {
    g_window->RequestRender();
  }
}

// Build UI

xent::View build_ui(App &app, fluxent::theme::ThemeManager &tm) {
  auto counter_text = xent::Text{"Count: 0"}.SetFontSize(36).SetColor(
      ToXentColor(tm.Resources().TextPrimary));

  app.counter_text_data = counter_text.Data();

  xent::View root =
      xent::VStack{}
          .Padding(32)
          .Gap(20)
          .AlignItems(YGAlignCenter)
          .JustifyContent(YGJustifyCenter)
          .Background(ToXentColor(tm.Resources().LayerOnMicaBaseAltTransparent))
          .Expand()
          .Add(xent::Text{"Hello, FluXent!"}.SetFontSize(32).SetColor(
              ToXentColor(tm.Resources().TextPrimary)))
          .Add(xent::Text{"A modern Windows UI framework"}
                   .SetFontSize(14)
                   .SetColor(ToXentColor(tm.Resources().TextSecondary)))
          .Add(counter_text)
          .Add(
              xent::VStack{}
                  .Padding(20)
                  .Gap(12)
                  .AlignItems(YGAlignCenter)
                  .Background(ToXentColor(tm.Resources().CardBackgroundDefault))
                  .CornerRadius(8)
                  .Add(xent::Button{"+1"}
                           .Icon("Add")
                           .Role(xent::Semantic::Primary)
                           .OnClick(&App::increment, &app))
                  .Add(xent::Button{"-1"}
                           .Icon("Minus")
                           .Role(xent::Semantic::Secondary)
                           .Style(xent::ButtonStyle::Outline)
                           .OnClick(&App::decrement, &app))
                  .Add(xent::Button{"Reset"}
                           .Icon("Refresh")
                           .Role(xent::Semantic::Danger)
                           .Style(xent::ButtonStyle::Text)
                           .OnClick(&App::reset, &app)))
          .Add(xent::HStack{}
                   .Gap(10)
                   .AlignItems(YGAlignCenter)
                   .Add(xent::Text{"Toggle: "}.SetFontSize(14).SetColor(
                       ToXentColor(tm.Resources().TextPrimary)))
                   .Add(xent::ToggleButton{"Sound"}
                            .IsChecked(true)
                            .Icon("Mute")
                            .CheckedIcon("Volume")))
          .Add(
              xent::HStack{}
                  .Gap(40)
                  .AlignItems(YGAlignFlexStart)
                  .Add(
                      xent::VStack{}
                          .Gap(10)
                          .Add(
                              xent::Text{"Checkboxes"}.SetFontSize(14).SetColor(
                                  ToXentColor(tm.Resources().TextSecondary)))
                          .Add(xent::CheckBox("Enable Turbo").IsChecked(true))
                          .Add(xent::CheckBox("Auto-Save")))
                  .Add(xent::VStack{}
                           .Gap(10)
                           .Add(xent::Text{"Radio Group"}
                                    .SetFontSize(14)
                                    .SetColor(ToXentColor(
                                        tm.Resources().TextSecondary)))
                           .Add(xent::RadioButton("Option 1")
                                    .Group("G1")
                                    .IsChecked(true))
                           .Add(xent::RadioButton("Option 2").Group("G1"))))
          .Add(xent::Text{"Click the buttons!"}.SetFontSize(12).SetColor(
              ToXentColor(tm.Resources().TextSecondary)));

  return root;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  WindowConfig config;
  config.title = L"Hello FluXent";
  config.width = 400;
  config.height = 600;
  config.backdrop = fluxent::BackdropType::Mica;

  fluxent::theme::ThemeManager theme_manager;
  Window window(&theme_manager, config);
  if (!window.IsValid()) {
    return -1;
  }
  RenderEngine engine(window.GetGraphics(), &theme_manager);
  engine.SetDebugMode(false);

  InputHandler input;
  engine.SetInputHandler(&input);

  App app;

  g_window = &window;
  g_engine = &engine;
  g_input = &input;
  g_app = &app;

  input.SetInvalidateCallback(on_input_invalidate);
  app.SetInvalidateCallback(on_window_invalidate);

  xent::View root = build_ui(app, theme_manager);
  g_root_view = &root;

  window.SetRenderCallback(on_window_render);
  window.SetMouseCallback(on_window_mouse_event);
  window.SetResizeCallback(on_window_resize);

  window.RequestRender();

  window.Run();

  g_root_view = nullptr;

  return 0;
}
