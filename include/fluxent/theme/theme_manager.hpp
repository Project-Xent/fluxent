#pragma once

// FluXent theme manager

#include "colors.hpp"
#include <functional>

namespace fluxent::theme {

// ThemeResources
struct ThemeResources {
    // Text
    Color TextPrimary;
    Color TextSecondary;
    Color TextTertiary;
    Color TextDisabled;
    Color TextInverse;
    
    // On-accent text
    Color TextOnAccentPrimary;
    Color TextOnAccentSecondary;
    Color TextOnAccentDisabled;
    
    // Control fill
    Color ControlFillDefault;
    Color ControlFillSecondary;
    Color ControlFillTertiary;
    Color ControlFillDisabled;
    Color ControlFillTransparent;
    Color ControlFillInputActive;
    
    // Strong fill
    Color ControlStrongFillDefault;
    Color ControlStrongFillDisabled;
    
    // Solid fill
    Color ControlSolidFillDefault;
    
    // Subtle fill
    Color SubtleFillTransparent;
    Color SubtleFillSecondary;
    Color SubtleFillTertiary;
    Color SubtleFillDisabled;
    
    // Alt fill
    Color ControlAltFillTransparent;
    Color ControlAltFillSecondary;
    Color ControlAltFillTertiary;
    Color ControlAltFillQuarternary;
    
    // Accent fill
    Color AccentDefault;
    Color AccentSecondary;
    Color AccentTertiary;
    Color AccentDisabled;
    
    // Strokes
    Color ControlStrokeDefault;
    Color ControlStrokeSecondary;
    Color ControlStrokeOnAccentDefault;
    Color ControlStrokeOnAccentSecondary;
    
    // Strong strokes
    Color ControlStrongStrokeDefault;
    Color ControlStrongStrokeDisabled;
    
    // Cards
    Color CardStrokeDefault;
    Color CardBackgroundDefault;
    Color CardBackgroundSecondary;
    
    // Focus
    Color FocusStrokeOuter;
    Color FocusStrokeInner;
    
    // Dividers
    Color DividerStrokeDefault;
    
    // Backgrounds
    Color SolidBackgroundBase;
    Color SolidBackgroundSecondary;
    Color SolidBackgroundTertiary;
    Color LayerFillDefault;
    Color SmokeFillDefault;
    
    // System
    Color SystemSuccess;
    Color SystemCaution;
    Color SystemCritical;
    Color SystemNeutral;
    Color SystemSuccessBackground;
    Color SystemCautionBackground;
    Color SystemCriticalBackground;
};

// ThemeManager
class ThemeManager {
public:
    static ThemeManager& instance();

    Mode mode() const { return mode_; }
    void set_mode(Mode mode);

    const ThemeResources& resources() const { return resources_; }

    const AccentPalette& accent() const { return accent_; }
    void set_accent(const AccentPalette& palette);
    void set_accent_color(const Color& base);

    using ThemeChangedCallback = std::function<void(Mode)>;
    void on_theme_changed(ThemeChangedCallback callback) {
        on_theme_changed_ = std::move(callback);
    }
    
private:
    ThemeManager();
    void apply_dark_theme();
    void apply_light_theme();
    void update_accent_resources();
    
    // Static helpers (no lambda).
    static Color adjust_brightness(const Color& c, float factor);
    static uint8_t clamp_color(int v);
    
    Mode mode_ = Mode::Dark;
    ThemeResources resources_;
    AccentPalette accent_ = AccentPalette::Default();
    ThemeChangedCallback on_theme_changed_;
};

// Convenience accessors
inline const ThemeResources& res() { 
    return ThemeManager::instance().resources(); 
}

inline const AccentPalette& accent() { 
    return ThemeManager::instance().accent(); 
}

inline Mode current_mode() {
    return ThemeManager::instance().mode();
}

} // namespace fluxent::theme
