// Hello FluXent example

#include "fluxent/theme/theme_manager.hpp"
#include <Windows.h>
#include <cstdint>

#include <fluxent/fluxent.hpp>
#include <xent/button.hpp>
#include <xent/checkbox.hpp>
#include <xent/hstack.hpp>
#include <xent/radio_button.hpp>
#include <xent/scroll_view.hpp>
#include <xent/slider.hpp>
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
  // Main Root container
  auto root = std::make_unique<xent::VStack>();

  root->Padding(0)
      .AlignItems(YGAlignStretch)
      .JustifyContent(YGJustifyFlexStart)
      .Background(ToXentColor(tm.Resources().LayerOnMicaBaseAltTransparent))
      .Expand();

  // ScrollView covering the whole area
  auto scroll = std::make_unique<xent::ScrollView>();
  scroll->Expand()
      .HorizontalScroll(xent::ScrollMode::Auto)
      .VerticalScroll(xent::ScrollMode::Auto);

  // Main Content holding everything
  auto content = std::make_unique<xent::VStack>();
  content->Padding(32)
      .Gap(30)
      .AlignItems(YGAlignFlexStart)
      .JustifyContent(YGJustifyFlexStart)
      .MinWidth(600); // Trigger generic horizontal scrolling

  // --- Header Section ---
  auto header_stack = std::make_unique<xent::VStack>();
  header_stack->Gap(5);

  auto title = std::make_unique<xent::Text>("Hello, FluXent!");
  title->SetFontSize(32).SetColor(ToXentColor(tm.Resources().TextPrimary));
  header_stack->Add(std::move(title));

  auto subtitle = std::make_unique<xent::Text>("A modern Windows UI framework");
  subtitle->SetFontSize(14).SetColor(ToXentColor(tm.Resources().TextSecondary));
  header_stack->Add(std::move(subtitle));

  // Counter Text in header
  auto counter_text = std::make_unique<xent::Text>("Count: 0");
  counter_text->SetFontSize(24).SetColor(
      ToXentColor(tm.Resources().TextPrimary));
  app.counter_text_data = counter_text.get();
  header_stack->Add(std::move(counter_text));

  content->Add(std::move(header_stack));

  // --- Main Controls Row (Left vs Right) ---
  auto main_row = std::make_unique<xent::HStack>();
  main_row->Gap(40)
      .AlignItems(YGAlignFlexStart)
      .MinWidth(550); // Force minimum width for scrolling

  // LEFT COLUMN: Buttons (+, -, Reset)
  auto left_col = std::make_unique<xent::VStack>();
  left_col->Gap(12)
      .Padding(20)
      .Background(ToXentColor(tm.Resources().CardBackgroundDefault))
      .CornerRadius(8)
      .CornerRadius(8)
      .Border(1, ToXentColor(tm.Resources().CardStrokeDefault))
      .MinWidth(120)  // Ensure button column width
      .FlexShrink(0); // Prevent shrinking to enable scroll

  auto btn1 = std::make_unique<xent::Button>("+1");
  btn1->Icon("Add")
      .Role(xent::Semantic::Primary)
      .OnClick(&App::increment, &app);
  left_col->Add(std::move(btn1));

  auto btn2 = std::make_unique<xent::Button>("-1");
  btn2->Icon("Minus")
      .Role(xent::Semantic::Secondary)
      .OnClick(&App::decrement, &app);
  left_col->Add(std::move(btn2));

  auto btn3 = std::make_unique<xent::Button>("Reset");
  btn3->Icon("Refresh")
      .Role(xent::Semantic::Danger)
      .Style(xent::ButtonStyle::Text)
      .OnClick(&App::reset, &app);
  left_col->Add(std::move(btn3));

  main_row->Add(std::move(left_col));

  // RIGHT COLUMN: Controls
  auto right_col = std::make_unique<xent::VStack>();
  right_col->Gap(24).MinWidth(350).FlexShrink(
      0); // Prevent shrinking to force scroll

  // Right Top: Toggle + Slider
  auto right_top = std::make_unique<xent::HStack>();
  right_top->Gap(24).AlignItems(YGAlignCenter);

  auto toggle_btn = std::make_unique<xent::ToggleButton>("Sound");
  toggle_btn->IsChecked(true).Icon("Mute").CheckedIcon("Volume");
  right_top->Add(std::move(toggle_btn));

  auto slider_stack = std::make_unique<xent::VStack>();
  slider_stack->Gap(4);
  auto slider_lbl = std::make_unique<xent::Text>("Volume");
  slider_lbl->SetFontSize(12).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  slider_stack->Add(std::move(slider_lbl));

  auto slider = std::make_unique<xent::Slider>();
  slider->Range(0, 100).Value(60).Width(150);
  slider_stack->Add(std::move(slider));

  right_top->Add(std::move(slider_stack));
  right_col->Add(std::move(right_top));

  // Right Bottom: Radio + Checkbox
  auto right_bottom = std::make_unique<xent::HStack>();
  right_bottom->Gap(40).AlignItems(YGAlignFlexStart);

  // Radio Group
  auto radio_group = std::make_unique<xent::VStack>();
  radio_group->Gap(8);
  auto radio_lbl = std::make_unique<xent::Text>("Options");
  radio_lbl->SetFontSize(14).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  radio_group->Add(std::move(radio_lbl));

  auto r1 = std::make_unique<xent::RadioButton>("Option A");
  r1->Group("G1").IsChecked(true);
  auto r2 = std::make_unique<xent::RadioButton>("Option B");
  r2->Group("G1");
  radio_group->Add(std::move(r1));
  radio_group->Add(std::move(r2));
  right_bottom->Add(std::move(radio_group));

  // Checkbox Group
  auto check_group = std::make_unique<xent::VStack>();
  check_group->Gap(8);
  auto check_lbl = std::make_unique<xent::Text>("Settings");
  check_lbl->SetFontSize(14).SetColor(
      ToXentColor(tm.Resources().TextSecondary));
  check_group->Add(std::move(check_lbl));

  auto c1 = std::make_unique<xent::CheckBox>("Turbo");
  c1->IsChecked(true);
  auto c2 = std::make_unique<xent::CheckBox>("Sync");
  check_group->Add(std::move(c1));
  check_group->Add(std::move(c2));
  right_bottom->Add(std::move(check_group));

  right_col->Add(std::move(right_bottom));
  main_row->Add(std::move(right_col));

  content->Add(std::move(main_row));

  // Add a spacer to force some vertical scrolling if height is small too
  auto spacer = std::make_unique<xent::View>();
  spacer->Height(50);
  content->Add(std::move(spacer));

  scroll->Content(std::move(content));
  root->Add(std::move(scroll));

  return root;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  WindowConfig config;
  config.title = L"Hello FluXent";
  config.width = 400;
  config.height = 600;
  config.backdrop = fluxent::BackdropType::Mica;

  fluxent::theme::ThemeManager theme_manager;

  auto window_res = Window::Create(&theme_manager, config);
  if (!window_res) {
    return -1;
  }
  auto window = std::move(*window_res);

  auto engine_res = RenderEngine::Create(window->GetGraphics(), &theme_manager);
  if (!engine_res) {
    return -1;
  }
  auto engine = std::move(*engine_res);

  engine->SetDebugMode(false);

  InputHandler input;
  engine->SetInputHandler(&input);

  App app;

  g_window = window.get();
  g_engine = engine.get();
  g_input = &input;
  g_app = &app;

  input.SetInvalidateCallback(on_input_invalidate);
  app.SetInvalidateCallback(on_window_invalidate);

  auto root = build_ui(app, theme_manager);
  g_root_view = root.get();

  window->SetRenderCallback(on_window_render);
  window->SetMouseCallback(on_window_mouse_event);
  window->SetResizeCallback(on_window_resize);

  window->SetDirectManipulationUpdateCallback(
      [&input, &root](float x, float y, float scale, bool centering) {
        if (g_root_view) {
           input.HandleDirectManipulation(*g_root_view, x, y, scale, centering);
        }
      });

  window->SetDirectManipulationStatusCallback(
      [&input](DIRECTMANIPULATION_STATUS status) {
        if (status == DIRECTMANIPULATION_RUNNING) {
          input.CancelInteraction();
        }
      });

  window->RequestRender();
  window->Run();

  g_root_view = nullptr;
  return 0;
}
