// Hello FluXent example

#include "fluxent/theme/theme_manager.hpp"
#include <Windows.h>
#include <cstdint>

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
std::unique_ptr<xent::View> build_ui(class App &app,
                                     fluxent::theme::ThemeManager &tm);

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
  xent::View *counter_text_data = nullptr;
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

std::unique_ptr<xent::View> build_ui(App &app,
                                     fluxent::theme::ThemeManager &tm) {
  auto counter_text = std::make_unique<xent::Text>("Count: 0");
  counter_text->SetFontSize(36).SetColor(
      ToXentColor(tm.Resources().TextPrimary));

  app.counter_text_data = counter_text.get();

  auto root = std::make_unique<xent::VStack>();

  root->Padding(32)
      .Gap(20)
      .AlignItems(YGAlignCenter)
      .JustifyContent(YGJustifyCenter)
      .Background(ToXentColor(tm.Resources().LayerOnMicaBaseAltTransparent))
      .Expand();

  // Add child
  auto title = std::make_unique<xent::Text>("Hello, FluXent!");
  title->SetFontSize(32).SetColor(ToXentColor(tm.Resources().TextPrimary));
  root->Add(std::move(title));

  auto subtitle = std::make_unique<xent::Text>("A modern Windows UI framework");
  subtitle->SetFontSize(14).SetColor(ToXentColor(tm.Resources().TextSecondary));
  root->Add(std::move(subtitle));

  root->Add(std::move(counter_text));

  // Button stack
  auto button_stack = std::make_unique<xent::VStack>();
  button_stack->Padding(20)
      .Gap(12)
      .AlignItems(YGAlignCenter)
      .Background(ToXentColor(tm.Resources().CardBackgroundDefault))
      .CornerRadius(8);

  auto btn1 = std::make_unique<xent::Button>("+1");
  btn1->Icon("Add")
      .Role(xent::Semantic::Primary)
      .OnClick(&App::increment, &app);
  button_stack->Add(std::move(btn1));

  auto btn2 = std::make_unique<xent::Button>("-1");
  btn2->Icon("Minus")
      .Role(xent::Semantic::Secondary)
      .Style(xent::ButtonStyle::Outline)
      .OnClick(&App::decrement, &app);
  button_stack->Add(std::move(btn2));

  auto btn3 = std::make_unique<xent::Button>("Reset");
  btn3->Icon("Refresh")
      .Role(xent::Semantic::Danger)
      .Style(xent::ButtonStyle::Text)
      .OnClick(&App::reset, &app);
  button_stack->Add(std::move(btn3));

  root->Add(std::move(button_stack));

  // Toggles
  auto toggle_row = std::make_unique<xent::HStack>();
  toggle_row->Gap(10).AlignItems(YGAlignCenter);

  auto toggle_label = std::make_unique<xent::Text>("Toggle: ");
  toggle_label->SetFontSize(14).SetColor(
      ToXentColor(tm.Resources().TextPrimary));
  toggle_row->Add(std::move(toggle_label));

  auto toggle_btn = std::make_unique<xent::ToggleButton>("Sound");
  toggle_btn->IsChecked(true).Icon("Mute").CheckedIcon("Volume");
  toggle_row->Add(std::move(toggle_btn));

  root->Add(std::move(toggle_row));

  // Checkboxes/Radios
  auto controls_row = std::make_unique<xent::HStack>();
  controls_row->Gap(40).AlignItems(YGAlignFlexStart);

  auto check_col = std::make_unique<xent::VStack>();
  check_col->Gap(10);

  auto check_label = std::make_unique<xent::Text>("Checkboxes");
  check_label->SetFontSize(14).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  check_col->Add(std::move(check_label));

  auto check1 = std::make_unique<xent::CheckBox>("Enable Turbo");
  check1->IsChecked(true);
  check_col->Add(std::move(check1));

  auto check2 = std::make_unique<xent::CheckBox>("Auto-Save");
  check_col->Add(std::move(check2));

  controls_row->Add(std::move(check_col));

  auto radio_col = std::make_unique<xent::VStack>();
  radio_col->Gap(10);

  auto radio_label = std::make_unique<xent::Text>("Radio Group");
  radio_label->SetFontSize(14).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  radio_col->Add(std::move(radio_label));

  auto radio1 = std::make_unique<xent::RadioButton>("Option 1");
  radio1->Group("G1").IsChecked(true);
  radio_col->Add(std::move(radio1));

  auto radio2 = std::make_unique<xent::RadioButton>("Option 2");
  radio2->Group("G1");
  radio_col->Add(std::move(radio2));

  controls_row->Add(std::move(radio_col));

  root->Add(std::move(controls_row));

  auto click_hint = std::make_unique<xent::Text>("Click the buttons!");
  click_hint->SetFontSize(12).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  root->Add(std::move(click_hint));

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

  auto root = build_ui(app, theme_manager);
  g_root_view = root.get();

  window.SetRenderCallback(on_window_render);
  window.SetMouseCallback(on_window_mouse_event);
  window.SetResizeCallback(on_window_resize);

  window.RequestRender();
  window.Run();

  g_root_view = nullptr;
  return 0;
}
