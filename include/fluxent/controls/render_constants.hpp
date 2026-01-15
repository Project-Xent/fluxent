#pragma once
#include <d2d1.h>

namespace fluxent::controls::constants {

// General Control
constexpr float kControlCornerRadius = 4.0f;
constexpr float kFocusStrokeThicknessOuter = 2.0f;
constexpr float kFocusStrokeThicknessInner = 1.0f;
constexpr float kFocusRectPadding = 3.0f;

// Elevation Borders
constexpr float kElevationBorderHeight = 3.0f;
constexpr D2D1_POINT_2F kElevationGradientStart{0.0f, 0.0f};
constexpr D2D1_POINT_2F kElevationGradientEnd{0.0f, 3.0f};

// Button
constexpr double kButtonBrushTransitionSeconds = 0.083;

// ToggleSwitch
constexpr float kToggleSwitchWidth = 40.0f;
constexpr float kToggleSwitchHeight = 20.0f;
constexpr float kToggleKnobRadius = 6.0f; // 12px diameter
constexpr float kToggleKnobPadding = 4.0f;

// CheckBox & RadioButton
constexpr float kCheckBoxSize = 20.0f;
constexpr float kCheckBoxGap = 12.0f;
constexpr float kRadioButtonSize = 20.0f;
constexpr float kRadioButtonGap = 12.0f;
constexpr float kRadioBulletRadius = 5.0f;

} // namespace fluxent::controls::constants
