#pragma once

#include "graphics.hpp"
#include "types.hpp"
#include <memory>
#include <tuple>

#include "theme/theme_manager.hpp"

#include <directmanipulation.h>

namespace fluxent {

using DirectManipulationUpdateCallback =
    std::function<void(float x, float y, float scale, bool centering)>;
using DirectManipulationHitTestCallback = std::function<bool(UINT pointer_id, int x, int y)>;

class Window {
public:
  static Result<std::unique_ptr<Window>>
  Create(theme::ThemeManager *theme_manager, const WindowConfig &config = {});
  ~Window();

private:
  Window(theme::ThemeManager *theme_manager, const WindowConfig &config);
  Result<void> Init();

public:
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  HWND GetHwnd() const { return hwnd_; }
  bool IsValid() const { return hwnd_ != nullptr; }

  Size GetClientSize() const;

  DpiInfo GetDpi() const { return dpi_; }
  void EnableIme(bool enable);
  void ShowTouchKeyboard();
  void HideTouchKeyboard();

  void Run();
  bool ProcessMessages();
  void Close();

  void RequestRender();

  GraphicsPipeline *GetGraphics() const { return graphics_.get(); }

  void SetRenderCallback(RenderCallback callback) {
    on_render_ = std::move(callback);
  }
  void SetResizeCallback(ResizeCallback callback) {
    on_resize_ = std::move(callback);
  }
  void SetDpiChangedCallback(DpiChangedCallback callback) {
    on_dpi_changed_ = std::move(callback);
  }
  void SetMouseCallback(MouseEventCallback callback) {
    on_mouse_ = std::move(callback);
  }
  void SetKeyCallback(KeyEventCallback callback) {
    on_key_ = std::move(callback);
  }
  void SetCharCallback(CharEventCallback callback) {
    on_char_ = std::move(callback);
  }

  using ImePositionCallback = std::function<std::tuple<float, float, float>()>;
  void SetImePositionCallback(ImePositionCallback callback) {
    on_ime_position_ = std::move(callback);
  }

  using ImeCompositionCallback = std::function<void(const std::wstring &)>;
  void SetImeCompositionCallback(ImeCompositionCallback callback) {
    on_ime_composition_ = std::move(callback);
  }


  void SetDirectManipulationUpdateCallback(
      DirectManipulationUpdateCallback callback) {
    on_dm_update_ = std::move(callback);
  }

  void SetDirectManipulationHitTestCallback(
      DirectManipulationHitTestCallback callback) {
    on_dm_hittest_ = std::move(callback);
  }

  void DispatchDirectManipulationUpdate(float x, float y, float scale,
                                        bool centering) {
    if (on_dm_update_) {
      on_dm_update_(x, y, scale, centering);
    }
  }

  using DirectManipulationStatusCallback =
      std::function<void(DIRECTMANIPULATION_STATUS status)>;

  void SetDirectManipulationStatusCallback(
      DirectManipulationStatusCallback callback) {
    on_dm_status_ = std::move(callback);
  }

  void DispatchDirectManipulationStatusChanged(
      DIRECTMANIPULATION_STATUS status) {
    if (on_dm_status_) {
      on_dm_status_(status);
    }
  }

  void SetBackdrop(BackdropType type);
  void SetDarkMode(bool enabled);
  void SetTitle(const std::wstring &title);

  void SetCursor(HCURSOR cursor);
  void SetCursorArrow();
  void SetCursorHand();
  void SetCursorIbeam();

private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);
  LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

  void OnCreate();
  void OnDestroy();
  void OnSize(int width, int height);
  void OnDpiChanged(UINT dpi, const RECT *suggested_rect);
  void OnPaint();

  void OnMouseMove(int x, int y);
  void OnMouseButton(MouseButton button, bool is_down, int x, int y);
  void OnMouseWheel(float delta_x, float delta_y, int x, int y);
  void OnPointer(InputSource source, UINT32 pointer_id, MouseButton button,
                 bool is_down, int x, int y);
  void OnKey(UINT vk, bool is_down);
  void OnChar(wchar_t ch);

  void SetupDwmBackdrop();


  HWND hwnd_ = nullptr;
  DpiInfo dpi_;
  WindowConfig config_;
  HCURSOR current_cursor_ = nullptr;

  std::unique_ptr<GraphicsPipeline> graphics_;
  theme::ThemeManager *theme_manager_;

  RenderCallback on_render_;
  ResizeCallback on_resize_;
  DpiChangedCallback on_dpi_changed_;
  MouseEventCallback on_mouse_;
  KeyEventCallback on_key_;
  CharEventCallback on_char_;
  ImePositionCallback on_ime_position_;
  ImeCompositionCallback on_ime_composition_;
  bool is_ime_composing_ = false;
  HIMC default_himc_ = nullptr;

  ULONGLONG last_click_time_ = 0;
  int click_count_ = 0;


  static constexpr const wchar_t *WINDOW_CLASS_NAME = L"FluXentWindowClass";

  static bool class_registered_;

  void InitDirectManipulation();

  ComPtr<IDirectManipulationManager> result_manager_;
  ComPtr<IDirectManipulationViewport> viewport_;
  DWORD viewport_handler_cookie_ = 0;

  DirectManipulationUpdateCallback on_dm_update_;
  DirectManipulationHitTestCallback on_dm_hittest_;
  DirectManipulationStatusCallback on_dm_status_;
};

} // namespace fluxent
