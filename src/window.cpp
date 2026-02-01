#include "fluxent/window.hpp"
#include <fluxent/theme/theme_manager.hpp>
#include "dm_handler.hpp"

#include <windowsx.h>
#include <shobjidl_core.h>
#include <shellapi.h>
#include <objbase.h>
#include <vector>
#include <imm.h>

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "shell32.lib")
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

namespace fluxent
{

static theme::Mode query_system_theme_mode()
{
  DWORD value = 0;
  DWORD dataSize = sizeof(value);
  LSTATUS st = RegGetValueW(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &dataSize);

  if (st == ERROR_SUCCESS)
  {
    return (value != 0) ? theme::Mode::Light : theme::Mode::Dark;
  }

  return theme::Mode::Dark;
}

bool Window::class_registered_ = false;

Result<std::unique_ptr<Window>> Window::Create(theme::ThemeManager *theme_manager,
                                               const WindowConfig &config)
{
  if (!theme_manager)
    return tl::unexpected(E_INVALIDARG);
  auto window = std::unique_ptr<Window>(new Window(theme_manager, config));
  auto res = window->Init();
  if (!res)
    return tl::unexpected(res.error());
  return window;
}

Window::Window(theme::ThemeManager *theme_manager, const WindowConfig &config)
    : config_(config), theme_manager_(theme_manager)
{
  if (!theme_manager_)
  {
    std::abort();
  }
}

Result<void> Window::Init()
{
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
  {
    return tl::unexpected(hr);
  }
  com_initialized_ = (hr == S_OK || hr == S_FALSE);
  HINSTANCE hinstance = GetModuleHandle(nullptr);

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  if (!class_registered_)
  {
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc))
    {
      return tl::unexpected(HRESULT_FROM_WIN32(GetLastError()));
    }
    class_registered_ = true;
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP;
  if (!config_.resizable)
  {
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }

  UINT system_dpi = GetDpiForSystem();
  float scale = static_cast<float>(system_dpi) / 96.0f;

  int client_width = static_cast<int>(config_.width * scale);
  int client_height = static_cast<int>(config_.height * scale);

  RECT rect = {0, 0, client_width, client_height};
  AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, system_dpi);

  int window_width = rect.right - rect.left;
  int window_height = rect.bottom - rect.top;

  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  if (config_.position.has_value())
  {
    x = static_cast<int>(config_.position->x * scale);
    y = static_cast<int>(config_.position->y * scale);
  }

  hwnd_ = CreateWindowExW(ex_style, WINDOW_CLASS_NAME, config_.title.c_str(), style, x, y,
                          window_width, window_height, nullptr, nullptr, hinstance, this);

  if (!hwnd_)
  {
    return tl::unexpected(HRESULT_FROM_WIN32(GetLastError()));
  }

  UINT dpi = GetDpiForWindow(hwnd_);
  dpi_.dpi_x = static_cast<float>(dpi);
  dpi_.dpi_y = static_cast<float>(dpi);

  auto sys_mode = query_system_theme_mode();
  theme_manager_->SetMode(sys_mode);
  config_.dark_mode = (sys_mode == theme::Mode::Dark);

  SetupDwmBackdrop();

  auto graphics_res = GraphicsPipeline::Create();
  if (!graphics_res)
  {
    return tl::unexpected(graphics_res.error());
  }
  graphics_ = std::move(*graphics_res);

  auto attach_res = graphics_->AttachToWindow(hwnd_);
  if (!attach_res)
  {
    return tl::unexpected(attach_res.error());
  }
  graphics_->SetDpi(dpi_);

  ShowWindow(hwnd_, SW_SHOW);
  UpdateWindow(hwnd_);

  auto dm_res = InitDirectManipulation();
  dm_enabled_ = static_cast<bool>(dm_res);

  return {};
}

Result<void> Window::InitDirectManipulation()
{
  auto fail = [&](HRESULT error) -> Result<void>
  {
    viewport_.Reset();
    result_manager_.Reset();
    viewport_handler_cookie_ = 0;
    return tl::unexpected(error);
  };

  HRESULT hr = CoCreateInstance(CLSID_DirectManipulationManager, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&result_manager_));
  if (FAILED(hr))
    return fail(hr);

  hr = result_manager_->CreateViewport(nullptr, hwnd_, IID_PPV_ARGS(&viewport_));
  if (FAILED(hr))
    return fail(hr);

  DIRECTMANIPULATION_CONFIGURATION config = DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
                                            DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
                                            DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
                                            DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
                                            DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
                                            DIRECTMANIPULATION_CONFIGURATION_RAILS_Y;

  hr = viewport_->ActivateConfiguration(config);
  if (FAILED(hr))
    return fail(hr);

  hr = viewport_->SetViewportOptions(DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE |
                                     DIRECTMANIPULATION_VIEWPORT_OPTIONS_INPUT);
  if (FAILED(hr))
    return fail(hr);

  auto handler = Microsoft::WRL::Make<DirectManipulationEventHandler>(this);
  hr = viewport_->AddEventHandler(hwnd_, handler.Get(), &viewport_handler_cookie_);
  if (FAILED(hr))
    return fail(hr);

  hr = viewport_->Enable();
  if (FAILED(hr))
    return fail(hr);

  hr = result_manager_->Activate(hwnd_);
  if (FAILED(hr))
    return fail(hr);

  return {};
}

Window::~Window()
{
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
  if (com_initialized_)
  {
    CoUninitialize();
    com_initialized_ = false;
  }
}

void Window::SetupDwmBackdrop()
{
  BOOL dark_mode = config_.dark_mode ? TRUE : FALSE;
  HRESULT hr = DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode,
                                     sizeof(dark_mode));
  if (FAILED(hr))
    SetSystemError(hr);

  BOOL use_backdrop = TRUE;
  hr = DwmSetWindowAttribute(hwnd_, DWMWA_USE_HOSTBACKDROPBRUSH, &use_backdrop,
                             sizeof(use_backdrop));
  if (FAILED(hr))
    SetSystemError(hr);

  SetBackdrop(config_.backdrop);
}

void Window::SetBackdrop(BackdropType type)
{
  DWORD backdrop = DWMSBT_NONE;
  switch (type)
  {
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
  HRESULT hr =
      DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
  if (FAILED(hr))
    SetSystemError(hr);
  config_.backdrop = type;
}

void Window::SetDarkMode(bool enabled)
{
  BOOL dark_mode = enabled ? TRUE : FALSE;
  HRESULT hr = DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode,
                                     sizeof(dark_mode));
  if (FAILED(hr))
    SetSystemError(hr);
  config_.dark_mode = enabled;
}

void Window::SetTitle(const std::wstring &title)
{
  SetWindowTextW(hwnd_, title.c_str());
  config_.title = title;
}

Size Window::GetClientSize() const
{
  RECT rc;
  GetClientRect(hwnd_, &rc);
  return Size(static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top));
}

void Window::Run()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

bool Window::ProcessMessages()
{
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    if (msg.message == WM_QUIT)
    {
      return false;
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return true;
}

void Window::Close() { PostMessage(hwnd_, WM_CLOSE, 0, 0); }

void Window::RequestRender() { InvalidateRect(hwnd_, nullptr, FALSE); }

void Window::SetCursor(HCURSOR cursor)
{
  current_cursor_ = cursor;
  ::SetCursor(cursor);
}

void Window::SetCursorArrow() { SetCursor(LoadCursor(nullptr, IDC_ARROW)); }

void Window::SetCursorHand() { SetCursor(LoadCursor(nullptr, IDC_HAND)); }

void Window::SetCursorIbeam() { SetCursor(LoadCursor(nullptr, IDC_IBEAM)); }

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  Window *window = nullptr;

  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lparam);
    window = static_cast<Window *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    window->hwnd_ = hwnd;
  }
  else
  {
    window = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (window)
  {
    return window->HandleMessage(msg, wparam, lparam);
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
  {
  case WM_SETTINGCHANGE:
  {
    auto m = query_system_theme_mode();
    theme_manager_->SetMode(m);
    SetDarkMode(m == theme::Mode::Dark);
  }
    return 0;

  case WM_THEMECHANGED:
  {
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

  case WM_SIZE:
  {
    int width = LOWORD(lparam);
    int height = HIWORD(lparam);
    OnSize(width, height);
    return 0;
  }

  case WM_DPICHANGED:
  {
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
    if (LOWORD(lparam) == HTCLIENT && current_cursor_)
    {
      ::SetCursor(current_cursor_);
      return TRUE;
    }
    break;

  case WM_MOUSEMOVE:
  {
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    OnMouseMove(x, y);
    return 0;
  }

  case WM_MOUSEWHEEL:
  {
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    POINT pt = {x, y};
    ScreenToClient(hwnd_, &pt);
    float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
    OnMouseWheel(0.0f, delta, pt.x, pt.y);
    return 0;
  }

  case 0x020E:
  { // WM_MOUSEHWHEEL
    int x = GET_X_LPARAM(lparam);
    int y = GET_Y_LPARAM(lparam);
    POINT pt = {x, y};
    ScreenToClient(hwnd_, &pt);
    float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
    OnMouseWheel(delta, 0.0f, pt.x, pt.y);
    return 0;
  }

  case WM_POINTERDOWN:
  case WM_POINTERUPDATE:
  case WM_POINTERUP:
    if (HandlePointerMessage(msg, wparam, lparam))
      return 0;
    break;

  case WM_LBUTTONDOWN:
    SetCapture(hwnd_);
    OnMouseButton(MouseButton::Left, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    return 0;

  case WM_LBUTTONUP:
    OnMouseButton(MouseButton::Left, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    ReleaseCapture();
    return 0;

  case WM_RBUTTONDOWN:
    SetCapture(hwnd_);
    OnMouseButton(MouseButton::Right, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    return 0;

  case WM_RBUTTONUP:
    OnMouseButton(MouseButton::Right, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    ReleaseCapture();
    return 0;

  case WM_MBUTTONDOWN:
    SetCapture(hwnd_);
    OnMouseButton(MouseButton::Middle, true, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    return 0;

  case WM_MBUTTONUP:
    OnMouseButton(MouseButton::Middle, false, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
    ReleaseCapture();
    return 0;

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    OnKey(static_cast<UINT>(wparam), true);
    return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    OnKey(static_cast<UINT>(wparam), false);
    return 0;

  case WM_CHAR:
    if (is_ime_composing_)
      return 0;
    OnChar(static_cast<wchar_t>(wparam));
    return 0;

  case WM_PASTE:
    if (on_paste_)
    {
      on_paste_();
    }
    else if (on_key_)
    {
      KeyEvent event;
      event.virtual_key = 'V';
      event.is_down = true;
      event.ctrl = true;
      event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      on_key_(event);
    }
    return 0;

  case WM_COPY:
    if (on_copy_)
    {
      on_copy_();
    }
    else if (on_key_)
    {
      KeyEvent event;
      event.virtual_key = 'C';
      event.is_down = true;
      event.ctrl = true;
      event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      on_key_(event);
    }
    return 0;

  case WM_CUT:
    if (on_cut_)
    {
      on_cut_();
    }
    else if (on_key_)
    {
      KeyEvent event;
      event.virtual_key = 'X';
      event.is_down = true;
      event.ctrl = true;
      event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
      event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
      on_key_(event);
    }
    return 0;

  case WM_IME_STARTCOMPOSITION:
  case WM_IME_SETCONTEXT:
  case WM_IME_COMPOSITION:
  case WM_IME_ENDCOMPOSITION:
    if (HandleImeMessage(msg, wparam, lparam))
      return 0;
    break;
  }

  return DefWindowProc(hwnd_, msg, wparam, lparam);
}

bool Window::HandlePointerMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
  UINT32 pointerId = GET_POINTERID_WPARAM(wparam);
  POINTER_INPUT_TYPE pointerType;

  if (GetPointerType(pointerId, &pointerType))
  {
    if (pointerType == PT_TOUCH)
    {
      if (msg == WM_POINTERDOWN)
      {
        if (viewport_)
        {
          bool should_contact = true;
          if (on_dm_hittest_)
          {
            POINT pt_check;
            pt_check.x = GET_X_LPARAM(lparam);
            pt_check.y = GET_Y_LPARAM(lparam);
            ScreenToClient(hwnd_, &pt_check);
            should_contact = on_dm_hittest_(pointerId, pt_check.x, pt_check.y);
          }

          if (should_contact)
          {
            viewport_->SetContact(pointerId);
          }
        }
      }
    }

    InputSource source = InputSource::Mouse;
    if (pointerType == PT_TOUCH)
      source = InputSource::Touch;
    else if (pointerType == PT_PEN)
      source = InputSource::Pen;

    POINT pt;
    pt.x = GET_X_LPARAM(lparam);
    pt.y = GET_Y_LPARAM(lparam);
    ScreenToClient(hwnd_, &pt);

    MouseButton btn = MouseButton::Left;
    bool is_down = false;

    if (msg == WM_POINTERDOWN)
    {
      is_down = true;
    }
    else if (msg == WM_POINTERUPDATE)
    {
      btn = MouseButton::None;
    }

    OnPointer(source, pointerId, btn, is_down, pt.x, pt.y);
    if (source == InputSource::Touch)
      return true; // handled
  }

  return false;
}

bool Window::HandleImeMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
  {
  case WM_IME_STARTCOMPOSITION:
  {
    is_ime_composing_ = true;
    if (on_ime_position_)
    {
      auto [x, y, h] = on_ime_position_();

      x *= dpi_.scale_x();
      y *= dpi_.scale_y();
      h *= dpi_.scale_y();

      HIMC himc = ImmGetContext(hwnd_);
      if (himc)
      {
        COMPOSITIONFORM cf = {};
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = static_cast<LONG>(x);
        cf.ptCurrentPos.y = static_cast<LONG>(y + h);
        ImmSetCompositionWindow(himc, &cf);
        ImmReleaseContext(hwnd_, himc);
      }
    }
    return false; // allow default processing
  }

  case WM_IME_SETCONTEXT:
    // keep behavior: clear show UI flag and continue to DefWindowProc
    lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
    return false;

  case WM_IME_COMPOSITION:
  {
    HIMC hIMC = ImmGetContext(hwnd_);
    if (hIMC)
    {
      if (lparam & GCS_RESULTSTR)
      {
        LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
        if (len > 0)
        {
          std::vector<wchar_t> buf(len / sizeof(wchar_t) + 1);
          ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf.data(), len);
          buf[len / sizeof(wchar_t)] = 0;
          for (size_t i = 0; i < wcslen(buf.data()); ++i)
          {
            OnChar(buf[i]);
          }
        }
      }
      if (lparam & GCS_COMPSTR)
      {
        LONG len = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, NULL, 0);
        if (len > 0)
        {
          std::vector<wchar_t> buf(len / sizeof(wchar_t) + 1);
          ImmGetCompositionStringW(hIMC, GCS_COMPSTR, buf.data(), len);
          buf[len / sizeof(wchar_t)] = 0;
          if (on_ime_composition_)
          {
            on_ime_composition_(std::wstring(buf.data()));
          }
        }
        else
        {
          if (on_ime_composition_)
          {
            on_ime_composition_(L"");
          }
        }
      }
      ImmReleaseContext(hwnd_, hIMC);
    }
    return true; // handled
  }

  case WM_IME_ENDCOMPOSITION:
    is_ime_composing_ = false;
    if (on_ime_composition_)
    {
      on_ime_composition_(L"");
    }
    return false;
  }

  return false;
}

void Window::OnCreate() { default_himc_ = ImmAssociateContext(hwnd_, nullptr); }

void Window::EnableIme(bool enable)
{
  if (enable)
  {
    if (default_himc_)
      ImmAssociateContext(hwnd_, default_himc_);
  }
  else
  {
    ImmAssociateContext(hwnd_, nullptr);
  }
}

// Touch keyboard invocation (undocumented COM approach)
MIDL_INTERFACE("37c994e7-432b-4834-a2f7-dce1f13b834b")
ITipInvocation : public IUnknown
{
public:
  virtual HRESULT STDMETHODCALLTYPE Toggle(HWND hwnd) = 0;
};

const GUID CLSID_UIHostNoLaunch = {
    0x4ce576fa, 0x83dc, 0x4f88, {0x95, 0x1c, 0x9d, 0x07, 0x82, 0xb4, 0xe3, 0x76}};

static bool IsTouchKeyboardVisible()
{
  HWND hwnd = FindWindowW(L"IPTip_Main_Window", nullptr);
  if (hwnd)
  {
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    return (style & WS_VISIBLE) != 0;
  }
  return false;
}

static void ToggleTouchKeyboard(HWND host_hwnd)
{
  ITipInvocation *tip = nullptr;
  HRESULT hr =
      CoCreateInstance(CLSID_UIHostNoLaunch, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tip));
  if (SUCCEEDED(hr))
  {
    tip->Toggle(host_hwnd);
    tip->Release();
  }
}

void Window::ShowTouchKeyboard()
{
  auto res = ShellExecuteW(nullptr, L"open",
                           L"C:\\Program Files\\Common Files\\microsoft shared\\ink\\TabTip.exe",
                           nullptr, nullptr, SW_SHOW);
  auto code = reinterpret_cast<INT_PTR>(res);
  if (code <= 32)
  {
    SetSystemError(HRESULT_FROM_WIN32(static_cast<DWORD>(code)));
  }
}

void Window::HideTouchKeyboard()
{
  if (IsTouchKeyboardVisible())
  {
    ToggleTouchKeyboard(hwnd_);
  }
}

void Window::OnDestroy() { PostQuitMessage(0); }

void Window::OnSize(int width, int height)
{
  if (width > 0 && height > 0 && graphics_)
  {
    (void)graphics_->Resize(width, height);

    if (on_resize_)
    {
      on_resize_(width, height);
    }

    RequestRender();
  }
}

void Window::OnDpiChanged(UINT dpi, const RECT *suggested_rect)
{
  dpi_.dpi_x = static_cast<float>(dpi);
  dpi_.dpi_y = static_cast<float>(dpi);

  if (graphics_)
  {
    graphics_->SetDpi(dpi_);
  }

  if (suggested_rect)
  {
    SetWindowPos(hwnd_, nullptr, suggested_rect->left, suggested_rect->top,
                 suggested_rect->right - suggested_rect->left,
                 suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
  }

  if (on_dpi_changed_)
  {
    on_dpi_changed_(dpi_);
  }
}

void Window::OnPaint()
{
  PAINTSTRUCT ps;
  BeginPaint(hwnd_, &ps);
  if (on_render_ && graphics_)
  {
    on_render_();
  }
  EndPaint(hwnd_, &ps);
}

void Window::OnMouseMove(int x, int y)
{
  if (on_mouse_)
  {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = MouseButton::None;
    event.is_down = false;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    on_mouse_(event);
  }
}

void Window::OnMouseButton(MouseButton button, bool is_down, int x, int y)
{
  if (on_mouse_)
  {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    if (is_down && button == MouseButton::Left)
    {
      ULONGLONG now = GetTickCount64();
      if (now - last_click_time_ < GetDoubleClickTime())
      {
        click_count_++;
      }
      else
      {
        click_count_ = 1;
      }
      last_click_time_ = now;
    }
    else if (is_down)
    {
      click_count_ = 1; // Reset for other buttons
    }

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = button;
    event.is_down = is_down;
    event.click_count = click_count_;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    on_mouse_(event);
  }
}

void Window::OnMouseWheel(float delta_x, float delta_y, int x, int y)
{
  if (on_mouse_)
  {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = MouseButton::None;
    event.is_down = false;
    event.wheel_delta_x = delta_x;
    event.wheel_delta_y = delta_y;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    on_mouse_(event);
  }
}

void Window::OnPointer(InputSource source, UINT32 pointer_id, MouseButton button, bool is_down,
                       int x, int y)
{
  if (on_mouse_)
  {
    float dpi_scale = dpi_.dpi_x / 96.0f;
    float dip_x = x / dpi_scale;
    float dip_y = y / dpi_scale;

    MouseEvent event;
    event.position = Point(dip_x, dip_y);
    event.button = button;
    event.is_down = is_down;
    event.source = source;
    event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    on_mouse_(event);
  }
}

void Window::OnKey(UINT vk, bool is_down)
{
  if (on_key_)
  {
    KeyEvent event;
    event.virtual_key = vk;
    event.is_down = is_down;
    event.alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    event.ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    event.shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    on_key_(event);
  }
}

void Window::OnChar(wchar_t ch)
{
  if (on_char_)
  {
    on_char_(ch);
  }
}

} // namespace fluxent
