// FluXent Window - Win32 window management implementation
#include "fluxent/window.hpp"
#include <fluxent/theme/theme_manager.hpp>
#include <stdexcept>
#include <windowsx.h>

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#endif

// Win11 DWM constants for old SDK compatibility
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

// DWM_SYSTEMBACKDROP_TYPE values
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 2
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
#define DWMWA_USE_HOSTBACKDROPBRUSH 17
#endif

namespace fluxent {

static theme::Mode query_system_theme_mode() {
  // Reads
  // HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme
  DWORD value = 0;
  DWORD dataSize = sizeof(value);
  LSTATUS st = RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &dataSize);

  if (st == ERROR_SUCCESS) {
    return (value != 0) ? theme::Mode::Light : theme::Mode::Dark;
  }

  // Default to Dark if unknown.
  return theme::Mode::Dark;
}

bool Window::class_registered_ = false;

Window::Window(theme::ThemeManager *theme_manager, const WindowConfig &config)
    : theme_manager_(theme_manager), config_(config) {
  if (!theme_manager_) {
    // Should be fatal
    std::abort();
  }
  HINSTANCE hinstance = GetModuleHandle(nullptr);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  if (!class_registered_) {
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc)) {
      throw std::runtime_error("Failed to register window class");
    }
    class_registered_ = true;
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;
  if (!config.resizable) {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }

  UINT system_dpi = GetDpiForSystem();
  float scale = static_cast<float>(system_dpi) / 96.0f;

  int client_width = static_cast<int>(config.width * scale);
  int client_height = static_cast<int>(config.height * scale);

  RECT rect = {0, 0, client_width, client_height};
  AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, system_dpi);

  int window_width = rect.right - rect.left;
  int window_height = rect.bottom - rect.top;

  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  if (config.position.has_value()) {
    x = static_cast<int>(config.position->x * scale);
    y = static_cast<int>(config.position->y * scale);
  }

  hwnd_ = CreateWindowExW(ex_style, WINDOW_CLASS_NAME, config.title.c_str(),
                          style, x, y, window_width, window_height, nullptr,
                          nullptr, hinstance, this);

  if (!hwnd_) {
    throw std::runtime_error("Failed to create window");
  }

  UINT dpi = GetDpiForWindow(hwnd_);
  dpi_.dpi_x = static_cast<float>(dpi);
  dpi_.dpi_y = static_cast<float>(dpi);

  // Initialize theme from system preference and apply to ThemeManager + DWM
  auto sys_mode = query_system_theme_mode();
  theme_manager_->SetMode(sys_mode);
  config_.dark_mode = (sys_mode == theme::Mode::Dark);

  SetupDwmBackdrop();

  graphics_ = std::make_unique<GraphicsPipeline>();
  graphics_->AttachToWindow(hwnd_);
  graphics_->SetDpi(dpi_);

  ShowWindow(hwnd_, SW_SHOW);
  UpdateWindow(hwnd_);
}

Window::~Window() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

void Window::SetupDwmBackdrop() {
  BOOL dark_mode = config_.dark_mode ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode,
                        sizeof(dark_mode));

  BOOL use_backdrop = TRUE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_HOSTBACKDROPBRUSH, &use_backdrop,
                        sizeof(use_backdrop));

  SetBackdrop(config_.backdrop);
}

void Window::SetBackdrop(BackdropType type) {
  DWORD backdrop = DWMSBT_NONE;
  switch (type) {
  case BackdropType::None:
    backdrop = DWMSBT_NONE;
    break;
  case BackdropType::Mica:
    backdrop = DWMSBT_MAINWINDOW;
    break;
  case BackdropType::MicaAlt:
    backdrop = DWMSBT_TABBEDWINDOW;
    break;
  case BackdropType::Acrylic:
    backdrop = DWMSBT_TRANSIENTWINDOW;
    break;
  }
  DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop,
                        sizeof(backdrop));
  config_.backdrop = type;
}

void Window::SetDarkMode(bool enabled) {
  BOOL dark_mode = enabled ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode,
                        sizeof(dark_mode));
  config_.dark_mode = enabled;
}

void Window::SetTitle(const std::wstring &title) {
  SetWindowTextW(hwnd_, title.c_str());
  config_.title = title;
}

Size Window::GetClientSize() const {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  return Size(static_cast<float>(rc.right - rc.left),
              static_cast<float>(rc.bottom - rc.top));
}

void Window::Run() {
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

bool Window::ProcessMessages() {
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      return false;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return true;
}

void Window::Close() { PostMessage(hwnd_, WM_CLOSE, 0, 0); }

void Window::RequestRender() { InvalidateRect(hwnd_, nullptr, FALSE); }

void Window::SetCursor(HCURSOR cursor) {
  current_cursor_ = cursor;
  ::SetCursor(cursor);
}

void Window::SetCursorArrow() { SetCursor(LoadCursor(nullptr, IDC_ARROW)); }

void Window::SetCursorHand() { SetCursor(LoadCursor(nullptr, IDC_HAND)); }

void Window::SetCursorIbeam() { SetCursor(LoadCursor(nullptr, IDC_IBEAM)); }

// Window message handling

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
  Window *window = nullptr;

  if (msg == WM_NCCREATE) {
    CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lparam);
    window = static_cast<Window *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    window->hwnd_ = hwnd;
  } else {
    window = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (window) {
    return window->HandleMessage(msg, wparam, lparam);
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
  case WM_SETTINGCHANGE:
    // Re-query system theme and apply.
    {
      auto m = query_system_theme_mode();
      theme_manager_->SetMode(m);
      // Keep DWM dark flag in sync
      SetDarkMode(m == theme::Mode::Dark);
    }
    return 0;

  case WM_THEMECHANGED: {
    auto m = query_system_theme_mode();
    theme_manager_->SetMode(m);
    SetDarkMode(m == theme::Mode::Dark);
  }
    return 0;

  case WM_CREATE:
    OnCreate();
    return 0;

  case WM_DESTROY:
    OnDestroy();
    return 0;

  case WM_CLOSE:
    DestroyWindow(hwnd_);
    return 0;

  case WM_SIZE: {
    int width = LOWORD(lparam);
    int height = HIWORD(lparam);
    OnSize(width, height);
    return 0;
  }

  case WM_DPICHANGED: {
    UINT dpi = HIWORD(wparam);
    RECT *suggested = reinterpret_cast<RECT *>(lparam);
    OnDpiChanged(dpi, suggested);
    return 0;
  }

  case WM_ERASEBKGND:
    return 1;

  case WM_PAINT:
    OnPaint();
    return 0;

  case WM_SETCURSOR:
    if (LOWORD(lparam) == HTCLIENT && current_cursor_) {
      ::SetCursor(current_cursor_);
      return TRUE;
    }
    break;

  case WM_MOUSEMOVE: {
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    OnMouseMove(x, y);
    return 0;
  }

  case WM_LBUTTONDOWN:
    OnMouseButton(MouseButton::Left, true, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_LBUTTONUP:
    OnMouseButton(MouseButton::Left, false, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_RBUTTONDOWN:
    OnMouseButton(MouseButton::Right, true, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_RBUTTONUP:
    OnMouseButton(MouseButton::Right, false, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_MBUTTONDOWN:
    OnMouseButton(MouseButton::Middle, true, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_MBUTTONUP:
    OnMouseButton(MouseButton::Middle, false, GET_X_LPARAM(lparam),
                  GET_Y_LPARAM(lparam));
    return 0;

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    OnKey(static_cast<UINT>(wparam), true);
    return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    OnKey(static_cast<UINT>(wparam), false);
    return 0;
  }

  return DefWindowProc(hwnd_, msg, wparam, lparam);
}

void Window::OnCreate() {}

void Window::OnDestroy() { PostQuitMessage(0); }

void Window::OnSize(int width, int height) {
  if (width > 0 && height > 0 && graphics_) {
    graphics_->Resize(width, height);

    if (on_resize_) {
      on_resize_(width, height);
    }

    RequestRender();
  }
}

void Window::OnDpiChanged(UINT dpi, const RECT *suggested_rect) {
  dpi_.dpi_x = static_cast<float>(dpi);
  dpi_.dpi_y = static_cast<float>(dpi);

  if (graphics_) {
    graphics_->SetDpi(dpi_);
  }

  if (suggested_rect) {
    SetWindowPos(hwnd_, nullptr, suggested_rect->left, suggested_rect->top,
                 suggested_rect->right - suggested_rect->left,
                 suggested_rect->bottom - suggested_rect->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
  }

  if (on_dpi_changed_) {
    on_dpi_changed_(dpi_);
  }
}

void Window::OnPaint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  if (on_render_ && graphics_) {
    on_render_();
  }
  EndPaint(hwnd_, &ps);
}

void Window::OnMouseMove(int x, int y) {
  if (on_mouse_) {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = MouseButton::None;
    event.is_down = false;
    on_mouse_(event);
  }
}

void Window::OnMouseButton(MouseButton button, bool is_down, int x, int y) {
  if (on_mouse_) {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = button;
    event.is_down = is_down;
    on_mouse_(event);
  }
}

void Window::OnKey(UINT vk, bool is_down) {
  if (on_key_) {
    KeyEvent event;
    event.virtual_key = vk;
    event.is_down = is_down;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    on_key_(event);
  }
}

} // namespace fluxent
