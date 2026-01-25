#pragma once

// FluXent core types

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Ensure newer DirectComposition interfaces (e.g. IDCompositionVisual3) are
// available. Some toolchains (clangd/IntelliSense) default to an older
// _WIN32_WINNT.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Windows 10
#endif

#include <d2d1_3.h>
#include <d3d11_1.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <windows.h>
#include <wrl/client.h>

#include "fluxent/config.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>

namespace fluxent {

// COM smart pointer alias
template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declarations
class Window;
class GraphicsPipeline;
class RenderEngine;
class InputHandler;
class TextRenderer;

// Geometry

struct Point {
  float x = 0.0f;
  float y = 0.0f;

  constexpr Point() = default;
  constexpr Point(float x_, float y_) : x(x_), y(y_) {}

  D2D1_POINT_2F to_d2d() const { return D2D1::Point2F(x, y); }
};

struct Size {
  float width = 0.0f;
  float height = 0.0f;

  constexpr Size() = default;
  constexpr Size(float w, float h) : width(w), height(h) {}

  D2D1_SIZE_F to_d2d() const { return D2D1::SizeF(width, height); }
};

struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  constexpr Rect() = default;
  constexpr Rect(float x_, float y_, float w, float h)
      : x(x_), y(y_), width(w), height(h) {}

  constexpr float left() const { return x; }
  constexpr float top() const { return y; }
  constexpr float right() const { return x + width; }
  constexpr float bottom() const { return y + height; }

  constexpr bool contains(float px, float py) const {
    return px >= x && px < right() && py >= y && py < bottom();
  }

  constexpr bool contains(Point p) const { return contains(p.x, p.y); }

  D2D1_RECT_F to_d2d() const { return D2D1::RectF(x, y, right(), bottom()); }
};

// Color

struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;

  constexpr Color() = default;
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
      : r(r_), g(g_), b(b_), a(a_) {}

  // Accepts 0xRRGGBBAA or 0xRRGGBB.
  static constexpr Color from_hex(uint32_t hex, bool has_alpha = false) {
    if (has_alpha) {
      return Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF,
                   hex & 0xFF);
    } else {
      return Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, 255);
    }
  }

  D2D1_COLOR_F to_d2d() const {
    return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }

  constexpr bool is_transparent() const { return a == 0; }

  // Common colors
  static constexpr Color transparent() { return Color(0, 0, 0, 0); }
  static constexpr Color black() { return Color(0, 0, 0, 255); }
  static constexpr Color white() { return Color(255, 255, 255, 255); }
};

// DPI scale

struct DpiInfo {
  float dpi_x = 96.0f;
  float dpi_y = 96.0f;

  float scale_x() const { return dpi_x / 96.0f; }
  float scale_y() const { return dpi_y / 96.0f; }
  float scale() const { return scale_x(); }
};

// Window configuration

enum class BackdropType {
  None = 0,
  Mica = 1,    // DWMSBT_MAINWINDOW
  MicaAlt = 2, // DWMSBT_TABBEDWINDOW
  Acrylic = 3  // DWMSBT_TRANSIENTWINDOW
};

struct WindowConfig {
  std::wstring title = L"FluXent Window";
  int width = config::Defaults::WindowWidth;
  int height = config::Defaults::WindowHeight;
  bool dark_mode = true;
  BackdropType backdrop = BackdropType::MicaAlt;
  bool resizable = true;
  std::optional<Point> position = std::nullopt; // CW_USEDEFAULT if nullopt
};

// Input events

enum class MouseButton { None = 0, Left = 1, Right = 2, Middle = 3 };

enum class InputSource { Mouse, Touch, Pen };

struct MouseEvent {
  Point position;
  MouseButton button = MouseButton::None;
  bool is_down = false;
  float wheel_delta_x = 0.0f;
  float wheel_delta_y = 0.0f;
  InputSource source = InputSource::Mouse;
};

struct KeyEvent {
  UINT virtual_key = 0;
  bool is_down = false;
  bool alt = false;
  bool ctrl = false;
  bool shift = false;
};

// Callbacks

using RenderCallback = std::function<void()>;
using ResizeCallback = std::function<void(int width, int height)>;
using DpiChangedCallback = std::function<void(DpiInfo dpi)>;
using MouseEventCallback = std::function<void(const MouseEvent &)>;
using KeyEventCallback = std::function<void(const KeyEvent &)>;

} // namespace fluxent

// Error handling
#include "../../third_party/tl/expected.hpp"

namespace fluxent {

// ... existing types ...

// Error handling
template <typename T> using Result = tl::expected<T, HRESULT>;

// Helper to bridge HRESULT to Result<void>
inline Result<void> check_result(HRESULT hr) {
  if (FAILED(hr)) {
    return tl::unexpected(hr);
  }
  return {};
}

// Fatal error check (replaces exceptions)

// Keep a version for quick checks if really needed, but generally prefer Result
// return.
inline bool succeeded(HRESULT hr) { return SUCCEEDED(hr); }

} // namespace fluxent
