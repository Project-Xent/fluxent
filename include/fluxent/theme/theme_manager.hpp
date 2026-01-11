#pragma once

// FluXent theme manager

#include "colors.hpp"
#include <functional>
#include <vector>

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
    Color ControlFillQuarternary;
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
    Color ControlAltFillDisabled;

    // On-image fill
    Color ControlOnImageFillDefault;
    Color ControlOnImageFillSecondary;
    Color ControlOnImageFillTertiary;
    Color ControlOnImageFillDisabled;
    
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
    Color ControlStrokeOnAccentTertiary;
    Color ControlStrokeOnAccentDisabled;
    Color ControlStrokeForStrongFillWhenOnImage;
    
    // Strong strokes
    Color ControlStrongStrokeDefault;
    Color ControlStrongStrokeDisabled;
    
    // Cards
    Color CardStrokeDefault;
    Color CardStrokeDefaultSolid;
    Color CardBackgroundDefault;
    Color CardBackgroundSecondary;
    Color CardBackgroundTertiary;

    // Surface strokes
    Color SurfaceStrokeDefault;
    Color SurfaceStrokeFlyout;
    Color SurfaceStrokeInverse;
    
    // Focus
    Color FocusStrokeOuter;
    Color FocusStrokeInner;
    
    // Dividers
    Color DividerStrokeDefault;
    
    // Backgrounds
    Color SolidBackgroundBase;
    Color SolidBackgroundSecondary;
    Color SolidBackgroundTertiary;
    Color SolidBackgroundQuarternary;
    Color SolidBackgroundQuinary;
    Color SolidBackgroundSenary;
    Color SolidBackgroundTransparent;
    Color SolidBackgroundBaseAlt;
    Color LayerFillDefault;
    Color LayerFillAlt;
    Color LayerOnAcrylicFillDefault;
    Color LayerOnAccentAcrylicFillDefault;
    Color LayerOnMicaBaseAltDefault;
    Color LayerOnMicaBaseAltSecondary;
    Color LayerOnMicaBaseAltTertiary;
    Color LayerOnMicaBaseAltTransparent;
    Color SmokeFillDefault;
    
    // System
    Color SystemSuccess;
    Color SystemCaution;
    Color SystemCritical;
    Color SystemNeutral;
    Color SystemSolidNeutral;
    Color SystemAttentionBackground;
    Color SystemSuccessBackground;
    Color SystemCautionBackground;
    Color SystemCriticalBackground;
    Color SystemNeutralBackground;
    Color SystemSolidAttentionBackground;
    Color SystemSolidNeutralBackground;
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
    // Registers a callback invoked when mode changes (Dark/Light/HighContrast).
    // Returns an id that can be used to remove the callback.
    size_t add_theme_changed_listener(ThemeChangedCallback callback);
    void remove_theme_changed_listener(size_t id);
    
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
    std::vector<std::pair<size_t, ThemeChangedCallback>> theme_changed_listeners_;
    size_t next_listener_id_ = 1;
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
