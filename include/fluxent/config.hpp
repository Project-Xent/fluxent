#pragma once

namespace fluxent::config {

// Layout Metrics
namespace Layout {
    constexpr float ScrollBarSize = 12.0f;
    constexpr float ScrollBarPadding = 2.0f;
    constexpr float ScrollBarMinThumbSize = 20.0f;

    constexpr float SliderTrackHeight = 4.0f;
    constexpr float SliderThumbSize = 18.0f;
    constexpr float SliderInnerThumbSize = 12.0f;

    // Slider Scale Factors relative to InnerThumbSize
    namespace Slider {
        constexpr float ScaleNormal = 0.86f;
        constexpr float ScaleHover = 1.0f;
        constexpr float ScalePressed = 0.71f;
    }
    
    constexpr float DefaultCornerRadius = 4.0f;
    
    constexpr float CheckBoxSize = 20.0f;
    constexpr float RadioButtonSize = 20.0f;
    constexpr float RadioButtonGap = 8.0f; // Gap between glyph and text
    
    namespace Button {
        constexpr float ContentGap = 8.0f;
        constexpr float IconSizeMultiplier = 1.14f;
    }

    constexpr float DefaultFontSize = 14.0f;
}

// Animation Durations (seconds)
namespace Animation {
    constexpr double Fast = 0.083;  // ~1/12s
    constexpr double Normal = 0.167; // ~1/6s
    constexpr double Slow = 0.250;   // ~1/4s
    
    constexpr double Slider = Fast;
    constexpr double Button = Fast;
    constexpr double CheckBox = Fast;
    constexpr double CheckBoxPress = 0.100; // 100ms
    constexpr double RadioButton = Slow;
}

// Theme Constants
namespace Theme {
    constexpr float Lighten1 = 1.15f;
    constexpr float Lighten2 = 1.30f;
    constexpr float Lighten3 = 1.50f;
    
    constexpr float Darken1 = 0.85f;
    constexpr float Darken2 = 0.70f;
    constexpr float Darken3 = 0.55f;

}

// Window Defaults
namespace Defaults {
    constexpr int WindowWidth = 800;
    constexpr int WindowHeight = 600;
}

// Input Configuration
namespace Input {
    constexpr float TouchSlop = 8.0f;
    constexpr float WheelScrollSpeed = 40.0f;
}

// Toggle Switch Layout
namespace Toggle {
    constexpr float KnobSizeNormal = 12.0f;
    constexpr float KnobSizeHover = 14.0f;
    constexpr float KnobSizePressedW = 17.0f;
    constexpr float KnobSizePressedH = 14.0f;
    constexpr float TravelExtension = 3.0f;
}

namespace RadioButton {
    constexpr float PressedGlyphSize = 10.0f;
}

} // namespace fluxent::config
