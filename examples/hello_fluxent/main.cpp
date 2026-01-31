#include <fluxent/theme/theme_manager.hpp>
#include <xent/delegate.hpp>
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

template <typename T> static xent::Color ToXentColor(const T &c)
{
  return xent::Color{
      static_cast<std::uint8_t>(c.r),
      static_cast<std::uint8_t>(c.g),
      static_cast<std::uint8_t>(c.b),
      static_cast<std::uint8_t>(c.a),
  };
}

std::unique_ptr<xent::View> build_ui(class App &app, fluxent::theme::ThemeManager &tm);

class App;
Window *g_window = nullptr;
RenderEngine *g_engine = nullptr;
InputHandler *g_input = nullptr;
App *g_app = nullptr;
xent::View *g_root_view = nullptr;

class App
{
public:
  int counter = 0;
  xent::View *counter_text_data = nullptr;
  xent::Delegate<void()> invalidate_callback;

  void SetInvalidateCallback(xent::Delegate<void()> callback) { invalidate_callback = callback; }

  void increment()
  {
    counter++;
    if (counter_text_data && invalidate_callback)
    {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node)
      {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }

  void decrement()
  {
    counter--;
    if (counter_text_data && invalidate_callback)
    {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node)
      {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }

  void reset()
  {
    counter = 0;
    if (counter_text_data && invalidate_callback)
    {
      counter_text_data->text_content = "Count: " + std::to_string(counter);
      if (counter_text_data->node)
      {
        YGNodeMarkDirty(counter_text_data->node.get());
      }
      invalidate_callback();
    }
  }
};

void on_window_render()
{
  if (g_engine && g_window && g_root_view)
  {
    g_engine->RenderFrame(*g_root_view);
  }
}

void on_window_mouse_event(const MouseEvent &event)
{
  if (g_input && g_window && g_root_view)
  {
    g_input->HandleMouseEvent(*g_root_view, event);
  }
}

void on_window_resize(int width, int height)
{
  (void)width;
  (void)height;
  if (g_window)
  {
    g_window->RequestRender();
  }
}

void on_window_invalidate()
{
  if (g_window)
  {
    g_window->RequestRender();
  }
}

void on_input_invalidate()
{
  if (g_window)
  {
    g_window->RequestRender();
  }
}

void on_window_key_event(const KeyEvent &event)
{
  if (g_input && g_window && g_root_view)
  {
    g_input->HandleKeyEvent(*g_root_view, event);
  }
}

void on_window_char_event(wchar_t ch)
{
  if (g_input && g_window && g_root_view)
  {
    g_input->HandleCharEvent(*g_root_view, ch);
  }
}

// Build UI

std::unique_ptr<xent::View> build_ui(App &app, fluxent::theme::ThemeManager &tm)
{
  // Main Root container
  auto root = std::make_unique<xent::VStack>();

  root->Padding(0)
      .AlignItems(YGAlignStretch)
      .JustifyContent(YGJustifyFlexStart)
      .Background(ToXentColor(tm.Resources().LayerOnMicaBaseAltTransparent))
      .Expand();

  // ScrollView covering the whole area
  auto scroll = std::make_unique<xent::ScrollView>();
  scroll->Expand().HorizontalScroll(xent::ScrollMode::Auto).VerticalScroll(xent::ScrollMode::Auto);

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
  counter_text->SetFontSize(24).SetColor(ToXentColor(tm.Resources().TextPrimary));
  app.counter_text_data = counter_text.get();
  header_stack->Add(std::move(counter_text));

  content->Add(std::move(header_stack));

  // --- TEXTBOX DEMO ---
  auto textbox_stack = std::make_unique<xent::VStack>();
  textbox_stack->Gap(8);
  auto tb_label = std::make_unique<xent::Text>("Input Demo:");
  tb_label->SetFontSize(14).SetColor(ToXentColor(tm.Resources().TextSecondary));
  textbox_stack->Add(std::move(tb_label));

  auto textbox = std::make_unique<xent::View>();
  textbox->type = xent::ComponentType::TextBox;
  textbox->Width(300)
      .Height(32)
      .Background(ToXentColor(tm.Resources().ControlFillInputActive)) // Default
      .CornerRadius(4)
      .Padding(10, 6) // L,R, T,B? View logic: T,R,B,L = Padding(top, right, bottom, left)
                      // Wait, Padding(vertical, horizontal) -> T=V, B=V, L=H, R=H.
                      // Padding(6, 10).
      .Placeholder("Type something...")
      .OnTextInput({nullptr, [](void *, const std::string &t)
                    {
                      // Debug
                      std::string msg = "Input: " + t + "\n";
                      OutputDebugStringA(msg.c_str());
                    }});
  // Explicitly use Padding(6, 10) to match standard look
  textbox->Padding(6, 10);

  textbox_stack->Add(std::move(textbox));
  content->Add(std::move(textbox_stack));

  // --- Main Controls Row (Left vs Right) ---
  auto main_row = std::make_unique<xent::HStack>();
  main_row->Gap(40).AlignItems(YGAlignFlexStart).MinWidth(550); // Force minimum width for scrolling

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
  btn1->Icon("Add").Role(xent::Semantic::Primary).OnClick<App, &App::increment>(&app);
  left_col->Add(std::move(btn1));

  auto btn2 = std::make_unique<xent::Button>("-1");
  btn2->Icon("Minus").Role(xent::Semantic::Secondary).OnClick<App, &App::decrement>(&app);
  left_col->Add(std::move(btn2));

  auto btn3 = std::make_unique<xent::Button>("Reset");
  btn3->Icon("Refresh")
      .Role(xent::Semantic::Danger)
      .Style(xent::ButtonStyle::Text)
      .OnClick<App, &App::reset>(&app);
  left_col->Add(std::move(btn3));

  main_row->Add(std::move(left_col));

  // RIGHT COLUMN: Controls
  auto right_col = std::make_unique<xent::VStack>();
  right_col->Gap(24).MinWidth(350).FlexShrink(0); // Prevent shrinking to force scroll

  // Right Top: Toggle + Slider
  auto right_top = std::make_unique<xent::HStack>();
  right_top->Gap(24).AlignItems(YGAlignCenter);

  auto toggle_btn = std::make_unique<xent::ToggleButton>("Sound");
  toggle_btn->IsChecked(true).Icon("Mute").CheckedIcon("Volume");
  right_top->Add(std::move(toggle_btn));

  auto slider_stack = std::make_unique<xent::VStack>();
  slider_stack->Gap(4);
  auto slider_lbl = std::make_unique<xent::Text>("Volume");
  slider_lbl->SetFontSize(12).SetColor(ToXentColor(tm.Resources().TextSecondary));
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
  radio_lbl->SetFontSize(14).SetColor(ToXentColor(tm.Resources().TextSecondary));
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
  check_lbl->SetFontSize(14).SetColor(ToXentColor(tm.Resources().TextSecondary));
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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  WindowConfig config;
  config.title = L"Hello FluXent";
  config.width = 400;
  config.height = 600;
  config.backdrop = fluxent::BackdropType::Mica;

  fluxent::theme::ThemeManager theme_manager;

  auto window_res = Window::Create(&theme_manager, config);
  if (!window_res)
  {
    return -1;
  }
  auto window = std::move(*window_res);

  auto engine_res = RenderEngine::Create(window->GetGraphics(), &theme_manager);
  if (!engine_res)
  {
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

  input.SetInvalidateCallback({nullptr, [](void *) { on_input_invalidate(); }});
  input.SetHoverChangedCallback({window.get(),
                                 [](void *ctx, xent::View *old_view, xent::View *new_view)
                                 {
                                   (void)old_view;
                                   auto *win_ptr = static_cast<Window *>(ctx);
                                   if (new_view && new_view->type == xent::ComponentType::TextBox)
                                   {
                                     win_ptr->SetCursorIbeam();
                                   }
                                   else
                                   {
                                     win_ptr->SetCursorArrow();
                                   }
                                 }});
  app.SetInvalidateCallback({nullptr, [](void *) { on_window_invalidate(); }});

  auto root = build_ui(app, theme_manager);
  g_root_view = root.get();

  window->SetRenderCallback({nullptr, [](void *) { on_window_render(); }});
  window->SetMouseCallback(
      {nullptr, [](void *, const MouseEvent &e) { on_window_mouse_event(e); }});
  window->SetKeyCallback({nullptr, [](void *, const KeyEvent &e) { on_window_key_event(e); }});
  window->SetCharCallback({nullptr, [](void *, wchar_t c) { on_window_char_event(c); }});
  window->SetCopyCallback(
      {&input, [](void *ctx) { static_cast<InputHandler *>(ctx)->PerformCopy(); }});
  window->SetCutCallback(
      {&input, [](void *ctx) { static_cast<InputHandler *>(ctx)->PerformCut(); }});
  window->SetPasteCallback(
      {&input, [](void *ctx) { static_cast<InputHandler *>(ctx)->PerformPaste(); }});
  window->SetResizeCallback({nullptr, [](void *, int w, int h) { on_window_resize(w, h); }});

  // IME candidate window positioning
  window->SetImePositionCallback({&input, [](void *ctx) -> std::tuple<float, float, float>
                                  {
                                    auto *input_ptr = static_cast<InputHandler *>(ctx);
                                    auto *focused = input_ptr->GetFocusedView();
                                    if (focused && focused->type == xent::ComponentType::TextBox)
                                    {
                                      auto bounds = input_ptr->GetFocusedViewBounds();
                                      float x = bounds.x + focused->padding_left;
                                      float y = bounds.y + focused->padding_top;
                                      float h = focused->font_size > 0 ? focused->font_size * 1.3f
                                                                       : 18.0f;
                                      return {x, y, h};
                                    }
                                    return {0.f, 0.f, 0.f};
                                  }});

  // IME composition text
  // IME composition text
  window->SetImeCompositionCallback(
      {&input, [](void *ctx, const std::wstring &text)
       { static_cast<InputHandler *>(ctx)->SetCompositionText(text); }});

  auto *win_ptr = window.get();
  input.SetImeStateChangeCallback({win_ptr, [](void *ctx, bool enable)
                                   {
                                     if (ctx)
                                       static_cast<Window *>(ctx)->EnableIme(enable);
                                   }});

  input.SetShowTouchKeyboardCallback({win_ptr, [](void *ctx, bool show)
                                      {
                                        auto *w = static_cast<Window *>(ctx);
                                        if (w)
                                        {
                                          if (show)
                                            w->ShowTouchKeyboard();
                                          else
                                            w->HideTouchKeyboard();
                                        }
                                      }});

  // DM hit test: return false for controls that need direct touch handling
  // DM hit test: return false for controls that need direct touch handling
  // Uses globals (g_root_view, g_input, g_window) to avoid complex context
  window->SetDirectManipulationHitTestCallback(
      {nullptr, [](void *, UINT pointer_id, int px, int py) -> bool
       {
         (void)pointer_id;
         if (!g_root_view || !g_input || !g_window)
           return true;

         // Convert physical pixels to DIPs
         auto dpi = g_window->GetDpi();
         float dip_x = static_cast<float>(px) / dpi.scale_x();
         float dip_y = static_cast<float>(py) / dpi.scale_y();

         // Use g_input
         auto hit = g_input->HitTest(*g_root_view, Point(dip_x, dip_y));
         if (hit.view_data)
         {
           auto type = hit.view_data->type;
           // These controls need direct touch; don't let DM capture them
           if (type == xent::ComponentType::Slider || type == xent::ComponentType::ToggleSwitch ||
               type == xent::ComponentType::Button || type == xent::ComponentType::ToggleButton ||
               type == xent::ComponentType::CheckBox || type == xent::ComponentType::RadioButton ||
               type == xent::ComponentType::TextBox)
           {
             return false;
           }
         }
         return true; // Allow DM for scroll areas
       }});

  window->SetDirectManipulationUpdateCallback(
      {nullptr, [](void *, float x, float y, float scale, bool centering)
       {
         if (g_root_view && g_input)
         {
           g_input->HandleDirectManipulation(*g_root_view, x, y, scale, centering);
         }
       }});

  window->SetDirectManipulationStatusCallback(
      {&input, [](void *ctx, DIRECTMANIPULATION_STATUS status)
       {
         if (status == DIRECTMANIPULATION_RUNNING)
         {
           auto *input_ptr = static_cast<InputHandler *>(ctx);
           // Only cancel if not interacting with a control that needs touch
           auto *pressed = input_ptr->GetPressedView();
           bool is_touch_control = pressed && (pressed->type == xent::ComponentType::Slider ||
                                               pressed->type == xent::ComponentType::ToggleSwitch ||
                                               pressed->type == xent::ComponentType::Button ||
                                               pressed->type == xent::ComponentType::ToggleButton ||
                                               pressed->type == xent::ComponentType::CheckBox ||
                                               pressed->type == xent::ComponentType::RadioButton ||
                                               pressed->type == xent::ComponentType::TextBox);
           if (!is_touch_control)
           {
             input_ptr->CancelInteraction();
           }
         }
       }});

  window->RequestRender();
  window->Run();

  g_root_view = nullptr;
  return 0;
}
