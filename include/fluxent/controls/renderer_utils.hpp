#pragma once

#include "fluxent/graphics.hpp"
#include <algorithm>
#include <cmath>
#include <xent/color.hpp>

namespace fluxent::controls
{

static constexpr double kButtonBrushTransitionSeconds = 0.083;
static constexpr float kControlContentThemeFontSize = 14.0f;

inline Color ToFluXentColor(const xent::Color &c) { return Color(c.r, c.g, c.b, c.a); }

inline bool ColorEqualRgba(const Color &a, const Color &b)
{
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

inline float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

inline float SrgbToLinear(float c)
{
  if (c <= 0.04045f)
    return c / 12.92f;
  return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float LinearToSrgb(float c)
{
  if (c <= 0.0031308f)
    return 12.92f * c;
  return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

inline Color LerpColorSrgb(const Color &a, const Color &b, float t)
{
  t = Clamp01(t);

  const float ar = a.r / 255.0f;
  const float ag = a.g / 255.0f;
  const float ab = a.b / 255.0f;
  const float aa = a.a / 255.0f;

  const float br = b.r / 255.0f;
  const float bg = b.g / 255.0f;
  const float bb = b.b / 255.0f;
  const float ba = b.a / 255.0f;

  const float lr = SrgbToLinear(ar) + (SrgbToLinear(br) - SrgbToLinear(ar)) * t;
  const float lg = SrgbToLinear(ag) + (SrgbToLinear(bg) - SrgbToLinear(ag)) * t;
  const float lb = SrgbToLinear(ab) + (SrgbToLinear(bb) - SrgbToLinear(ab)) * t;
  const float la = aa + (ba - aa) * t;

  const float sr = Clamp01(LinearToSrgb(lr));
  const float sg = Clamp01(LinearToSrgb(lg));
  const float sb = Clamp01(LinearToSrgb(lb));
  const float sa = Clamp01(la);

  return Color(static_cast<uint8_t>(std::lround(sr * 255.0f)),
               static_cast<uint8_t>(std::lround(sg * 255.0f)),
               static_cast<uint8_t>(std::lround(sb * 255.0f)),
               static_cast<uint8_t>(std::lround(sa * 255.0f)));
}

} // namespace fluxent::controls
