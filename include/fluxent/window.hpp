#pragma once

// FluXent Window (Win32)

#include "graphics.hpp"
#include "types.hpp"
#include <memory>

#include "theme/theme_manager.hpp"

namespace fluxent {

class Window {
public:
  explicit Window(theme::ThemeManager *theme_manager,
                  const WindowConfig &config = {});
  ~Window();

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  HWND GetHwnd() const { return hwnd_; }
  bool IsValid() const { return hwnd_ != nullptr; }

  Size GetClientSize() const;

  DpiInfo GetDpi() const { return dpi_; }

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

  // Message handlers
  void OnCreate();
  void OnDestroy();
  void OnSize(int width, int height);
  void OnDpiChanged(UINT dpi, const RECT *suggested_rect);
  void OnPaint();

  // Input handlers
  void OnMouseMove(int x, int y);
  void OnMouseButton(MouseButton button, bool is_down, int x, int y);
  void OnKey(UINT vk, bool is_down);

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

  static constexpr const wchar_t *WINDOW_CLASS_NAME = L"FluXentWindowClass";
  static bool class_registered_;
};

} // namespace fluxent
