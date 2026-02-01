#pragma once

#include <cstdint>

namespace fluxent::config
{

// Layout Metrics
namespace Layout
{
constexpr float ScrollBarSize = 12.0f;
constexpr float ScrollBarPadding = 2.0f;
constexpr float ScrollBarMinThumbSize = 20.0f;

constexpr float SliderTrackHeight = 4.0f;
constexpr float SliderThumbSize = 18.0f;
constexpr float SliderInnerThumbSize = 12.0f;

constexpr float DefaultButtonPaddingTop = 5.0f;
constexpr float DefaultButtonPaddingRight = 11.0f;
constexpr float DefaultButtonPaddingBottom = 6.0f;
constexpr float DefaultButtonPaddingLeft = 11.0f;

constexpr float ToggleSwitchDefaultWidth = 40.0f;
constexpr float ToggleSwitchDefaultHeight = 20.0f;

// Slider Scale Factors relative to InnerThumbSize
namespace Slider
{
constexpr float ScaleNormal = 0.86f;
constexpr float ScaleHover = 1.0f;
constexpr float ScalePressed = 0.71f;
} // namespace Slider

constexpr float DefaultCornerRadius = 4.0f;

constexpr float CheckBoxSize = 20.0f;
constexpr float RadioButtonSize = 20.0f;
constexpr float RadioButtonGap = 8.0f; // Gap between glyph and text

namespace Button
{
constexpr float ContentGap = 8.0f;
constexpr float IconSizeMultiplier = 1.14f;
} // namespace Button

constexpr float DefaultFontSize = 14.0f;
} // namespace Layout

// Animation Durations (seconds)
namespace Animation
{
constexpr double Fast = 0.083;   // ~1/12s
constexpr double Normal = 0.167; // ~1/6s
constexpr double Slow = 0.250;   // ~1/4s

constexpr double Slider = Fast;
constexpr double Button = Fast;
constexpr double CheckBox = Fast;
constexpr double CheckBoxPress = 0.100; // 100ms
constexpr double RadioButton = Slow;
} // namespace Animation

// Theme Constants
namespace Theme
{
constexpr float Lighten1 = 1.15f;
constexpr float Lighten2 = 1.30f;
constexpr float Lighten3 = 1.50f;

constexpr float Darken1 = 0.85f;
constexpr float Darken2 = 0.70f;
constexpr float Darken3 = 0.55f;

constexpr float AccentSecondaryAlpha = 0.9f;
constexpr float AccentTertiaryAlpha = 0.8f;

} // namespace Theme

// Window Defaults
namespace Defaults
{
constexpr int WindowWidth = 800;
constexpr int WindowHeight = 600;
} // namespace Defaults

// Input Configuration
namespace Input
{
constexpr float TouchSlop = 8.0f;
constexpr float WheelScrollSpeed = 40.0f;
constexpr float WheelEpsilon = 0.001f;
constexpr float ScrollVisibilityEpsilon = 0.5f;
constexpr float CaretEdgePadding = 10.0f;
constexpr float SliderThumbHitInflate = 10.0f;
constexpr float DefaultLineHeightMultiplier = 1.3f;
constexpr std::uint64_t TypingMergeMs = 1000;
} // namespace Input

namespace Text
{
constexpr float UnboundedExtent = 100000.0f;
constexpr float HitTestExtent = 10000.0f;
constexpr float ExtentQuantizeStep = 1.0f;
constexpr size_t MaxCachedTextLength = 4096;
constexpr size_t LayoutCacheSize = 512;
} // namespace Text

namespace Render
{
constexpr float DpiBase = 96.0f;
constexpr int MinSurfaceSize = 1;
constexpr float PixelSnap = 0.5f;
constexpr float Epsilon = 0.001f;
constexpr float VisibilityThreshold = 0.01f;
constexpr float StrokeWidth = 1.0f;
} // namespace Render

namespace Interaction
{
constexpr float PressedScale = 0.9f;
constexpr float PressedAlpha = 0.8f;
constexpr float HoverAlpha = 0.9f;
constexpr float EaseOutExponent = 3.0f;
} // namespace Interaction

namespace Animation
{
constexpr float ValueEpsilon = 0.001f;
} // namespace Animation

namespace Graphics
{
constexpr std::uint32_t SwapChainSampleCount = 1;
constexpr std::uint32_t SwapChainSampleQuality = 0;
constexpr std::uint32_t SwapChainBufferCount = 2;
constexpr std::uint32_t SwapChainFlags = 0;
constexpr std::uint32_t ResizeBufferCount = 0;
constexpr std::uint32_t ResizeBufferFlags = 0;
constexpr std::uint32_t PresentVsyncInterval = 1;
constexpr std::uint32_t PresentNoVsyncInterval = 0;
} // namespace Graphics

// Toggle Switch Layout
namespace Toggle
{
constexpr float KnobSizeNormal = 12.0f;
constexpr float KnobSizeHover = 14.0f;
constexpr float KnobSizePressedW = 17.0f;
constexpr float KnobSizePressedH = 14.0f;
constexpr float TravelExtension = 3.0f;
} // namespace Toggle

namespace RadioButton
{
constexpr float PressedGlyphSize = 10.0f;
}

} // namespace fluxent::config
