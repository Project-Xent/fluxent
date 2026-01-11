#pragma once

#include "fluxent/graphics.hpp"
#include <algorithm>
#include <cmath>
#include <xent/color.hpp>


namespace fluxent::controls {

static constexpr double kButtonBrushTransitionSeconds = 0.083;
static constexpr float kControlContentThemeFontSize = 14.0f;

inline Color to_fluxent_color(const xent::Color &c) {
  return Color(c.r, c.g, c.b, c.a);
}

inline bool color_equal_rgba(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

inline float srgb_to_linear(float c) {
  if (c <= 0.04045f)
    return c / 12.92f;
  return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float c) {
  if (c <= 0.0031308f)
    return 12.92f * c;
  return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

inline Color lerp_color_srgb(const Color &a, const Color &b, float t) {
  t = clamp01(t);

  const float ar = a.r / 255.0f;
  const float ag = a.g / 255.0f;
  const float ab = a.b / 255.0f;
  const float aa = a.a / 255.0f;

  const float br = b.r / 255.0f;
  const float bg = b.g / 255.0f;
  const float bb = b.b / 255.0f;
  const float ba = b.a / 255.0f;

  const float lr =
      srgb_to_linear(ar) + (srgb_to_linear(br) - srgb_to_linear(ar)) * t;
  const float lg =
      srgb_to_linear(ag) + (srgb_to_linear(bg) - srgb_to_linear(ag)) * t;
  const float lb =
      srgb_to_linear(ab) + (srgb_to_linear(bb) - srgb_to_linear(ab)) * t;
  const float la = aa + (ba - aa) * t;

  const float sr = clamp01(linear_to_srgb(lr));
  const float sg = clamp01(linear_to_srgb(lg));
  const float sb = clamp01(linear_to_srgb(lb));
  const float sa = clamp01(la);

  return Color(static_cast<uint8_t>(std::lround(sr * 255.0f)),
               static_cast<uint8_t>(std::lround(sg * 255.0f)),
               static_cast<uint8_t>(std::lround(sb * 255.0f)),
               static_cast<uint8_t>(std::lround(sa * 255.0f)));
}

} // namespace fluxent::controls
