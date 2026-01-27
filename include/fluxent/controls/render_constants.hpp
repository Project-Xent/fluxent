#pragma once
#include <d2d1.h>

namespace fluxent::controls::constants {

constexpr float kControlCornerRadius = 4.0f;
constexpr float kFocusStrokeThicknessOuter = 2.0f;
constexpr float kFocusStrokeThicknessInner = 1.0f;
constexpr float kFocusRectPadding = 3.0f;

constexpr float kElevationBorderHeight = 3.0f;
constexpr D2D1_POINT_2F kElevationGradientStart{0.0f, 0.0f};
constexpr D2D1_POINT_2F kElevationGradientEnd{0.0f, 3.0f};

constexpr double kButtonBrushTransitionSeconds = 0.083;

constexpr float kToggleSwitchWidth = 40.0f;
constexpr float kToggleSwitchHeight = 20.0f;
constexpr float kToggleKnobRadius = 6.0f;
constexpr float kToggleKnobPadding = 4.0f;

constexpr float kCheckBoxSize = 20.0f;
constexpr float kCheckBoxGap = 12.0f;
constexpr float kRadioButtonSize = 20.0f;
constexpr float kRadioButtonGap = 12.0f;
constexpr float kRadioBulletRadius = 5.0f;

constexpr float kTextBoxMinHeight = 32.0f;
constexpr float kTextBoxMinWidth = 64.0f;
constexpr float kTextBoxPaddingLeft = 10.0f;
constexpr float kTextBoxPaddingTop = 5.0f;
constexpr float kTextBoxPaddingRight = 6.0f;
constexpr float kTextBoxPaddingBottom = 6.0f;

} // namespace fluxent::controls::constants

