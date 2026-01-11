// FluXent Theme Manager implementation
#include "fluxent/theme/theme_manager.hpp"

#include <xent/button.hpp>

#include <algorithm>

namespace fluxent::theme {

static xent::Color to_xent_color(const Color& c) {
    return xent::Color{
        static_cast<std::uint8_t>(c.r),
        static_cast<std::uint8_t>(c.g),
        static_cast<std::uint8_t>(c.b),
        static_cast<std::uint8_t>(c.a),
    };
}

static void sync_xent_button_palette_from_theme(const ThemeResources& r) {
    xent::ButtonPalette p;
    // Map semantic roles to Fluent/WinUI-ish theme tokens.
    p.primary = to_xent_color(r.AccentDefault);
    // Secondary should behave like a default WinUI3 button (i.e., no explicit background override).
    p.secondary = to_xent_color(r.ControlFillTransparent);
    p.success = to_xent_color(r.SystemSuccess);
    p.warning = to_xent_color(r.SystemCaution);
    // Danger should follow WinUI system critical color:
    // Light: deep red, Dark: light pink (see CommonStyles/Common_themeresources_any.xaml).
    p.danger = to_xent_color(r.SystemCritical);
    xent::set_button_palette(p);
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    apply_dark_theme();
}

size_t ThemeManager::add_theme_changed_listener(ThemeChangedCallback callback) {
    const size_t id = next_listener_id_++;
    theme_changed_listeners_.push_back({id, std::move(callback)});
    return id;
}

void ThemeManager::remove_theme_changed_listener(size_t id) {
    theme_changed_listeners_.erase(
        std::remove_if(
            theme_changed_listeners_.begin(),
            theme_changed_listeners_.end(),
            [id](const auto& p) { return p.first == id; }
        ),
        theme_changed_listeners_.end()
    );
}

void ThemeManager::set_mode(Mode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    
    switch (mode) {
        case Mode::Dark:
            apply_dark_theme();
            break;
        case Mode::Light:
            apply_light_theme();
            break;
        case Mode::HighContrast:
            apply_dark_theme();
            break;
    }
    
    update_accent_resources();

    for (auto& [id, cb] : theme_changed_listeners_) {
        if (cb) cb(mode);
    }
}

void ThemeManager::set_accent(const AccentPalette& palette) {
    accent_ = palette;
    update_accent_resources();
}

// Helper functions

uint8_t ThemeManager::clamp_color(int v) {
    return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v));
}

Color ThemeManager::adjust_brightness(const Color& c, float factor) {
    if (factor > 1.0f) {
        float t = factor - 1.0f;
        return Color{
            clamp_color(static_cast<int>(c.r + (255 - c.r) * t)),
            clamp_color(static_cast<int>(c.g + (255 - c.g) * t)),
            clamp_color(static_cast<int>(c.b + (255 - c.b) * t)),
            c.a
        };
    } else {
        return Color{
            clamp_color(static_cast<int>(c.r * factor)),
            clamp_color(static_cast<int>(c.g * factor)),
            clamp_color(static_cast<int>(c.b * factor)),
            c.a
        };
    }
}

// Theme setup

void ThemeManager::set_accent_color(const Color& base) {
    accent_.base = base;
    accent_.light1 = adjust_brightness(base, 1.15f);
    accent_.light2 = adjust_brightness(base, 1.30f);
    accent_.light3 = adjust_brightness(base, 1.50f);
    accent_.dark1 = adjust_brightness(base, 0.85f);
    accent_.dark2 = adjust_brightness(base, 0.70f);
    accent_.dark3 = adjust_brightness(base, 0.55f);
    
    update_accent_resources();
}

void ThemeManager::update_accent_resources() {
    if (mode_ == Mode::Dark) {
        resources_.AccentDefault = accent_.light2;
        resources_.AccentSecondary = Color{
            accent_.light2.r, accent_.light2.g, accent_.light2.b,
            static_cast<uint8_t>(accent_.light2.a * 0.9f)
        };
        resources_.AccentTertiary = Color{
            accent_.light2.r, accent_.light2.g, accent_.light2.b,
            static_cast<uint8_t>(accent_.light2.a * 0.8f)
        };
    } else {
        resources_.AccentDefault = accent_.dark1;
        resources_.AccentSecondary = Color{
            accent_.dark1.r, accent_.dark1.g, accent_.dark1.b,
            static_cast<uint8_t>(accent_.dark1.a * 0.9f)
        };
        resources_.AccentTertiary = Color{
            accent_.dark1.r, accent_.dark1.g, accent_.dark1.b,
            static_cast<uint8_t>(accent_.dark1.a * 0.8f)
        };
    }

    // Keep xent-core semantic role colors aligned to the active theme.
    sync_xent_button_palette_from_theme(resources_);
}

void ThemeManager::apply_dark_theme() {
    resources_.TextPrimary = dark::TextPrimary;
    resources_.TextSecondary = dark::TextSecondary;
    resources_.TextTertiary = dark::TextTertiary;
    resources_.TextDisabled = dark::TextDisabled;
    resources_.TextInverse = dark::TextInverse;
    
    resources_.TextOnAccentPrimary = dark::TextOnAccentPrimary;
    resources_.TextOnAccentSecondary = dark::TextOnAccentSecondary;
    resources_.TextOnAccentDisabled = dark::TextOnAccentDisabled;
    
    resources_.ControlFillDefault = dark::ControlFillDefault;
    resources_.ControlFillSecondary = dark::ControlFillSecondary;
    resources_.ControlFillTertiary = dark::ControlFillTertiary;
    resources_.ControlFillQuarternary = dark::ControlFillQuarternary;
    resources_.ControlFillDisabled = dark::ControlFillDisabled;
    resources_.ControlFillTransparent = dark::ControlFillTransparent;
    resources_.ControlFillInputActive = dark::ControlFillInputActive;
    
    resources_.ControlStrongFillDefault = dark::ControlStrongFillDefault;
    resources_.ControlStrongFillDisabled = dark::ControlStrongFillDisabled;
    resources_.ControlSolidFillDefault = dark::ControlSolidFillDefault;
    
    resources_.SubtleFillTransparent = dark::SubtleFillTransparent;
    resources_.SubtleFillSecondary = dark::SubtleFillSecondary;
    resources_.SubtleFillTertiary = dark::SubtleFillTertiary;
    resources_.SubtleFillDisabled = dark::SubtleFillDisabled;
    
    resources_.ControlAltFillTransparent = dark::ControlAltFillTransparent;
    resources_.ControlAltFillSecondary = dark::ControlAltFillSecondary;
    resources_.ControlAltFillTertiary = dark::ControlAltFillTertiary;
    resources_.ControlAltFillQuarternary = dark::ControlAltFillQuarternary;
    resources_.ControlAltFillDisabled = dark::ControlAltFillDisabled;

    resources_.ControlOnImageFillDefault = dark::ControlOnImageFillDefault;
    resources_.ControlOnImageFillSecondary = dark::ControlOnImageFillSecondary;
    resources_.ControlOnImageFillTertiary = dark::ControlOnImageFillTertiary;
    resources_.ControlOnImageFillDisabled = dark::ControlOnImageFillDisabled;
    
    resources_.AccentDisabled = dark::AccentFillDisabled;
    
    resources_.ControlStrokeDefault = dark::ControlStrokeDefault;
    resources_.ControlStrokeSecondary = dark::ControlStrokeSecondary;
    resources_.ControlStrokeOnAccentDefault = dark::ControlStrokeOnAccentDefault;
    resources_.ControlStrokeOnAccentSecondary = dark::ControlStrokeOnAccentSecondary;
    resources_.ControlStrokeOnAccentTertiary = dark::ControlStrokeOnAccentTertiary;
    resources_.ControlStrokeOnAccentDisabled = dark::ControlStrokeOnAccentDisabled;
    resources_.ControlStrokeForStrongFillWhenOnImage = dark::ControlStrokeForStrongFillWhenOnImage;
    resources_.ControlStrongStrokeDefault = dark::ControlStrongStrokeDefault;
    resources_.ControlStrongStrokeDisabled = dark::ControlStrongStrokeDisabled;
    
    resources_.CardStrokeDefault = dark::CardStrokeDefault;
    resources_.CardStrokeDefaultSolid = dark::CardStrokeDefaultSolid;
    resources_.CardBackgroundDefault = dark::CardBackgroundDefault;
    resources_.CardBackgroundSecondary = dark::CardBackgroundSecondary;
    resources_.CardBackgroundTertiary = dark::CardBackgroundTertiary;

    resources_.SurfaceStrokeDefault = dark::SurfaceStrokeDefault;
    resources_.SurfaceStrokeFlyout = dark::SurfaceStrokeFlyout;
    resources_.SurfaceStrokeInverse = dark::SurfaceStrokeInverse;
    
    resources_.FocusStrokeOuter = dark::FocusStrokeOuter;
    resources_.FocusStrokeInner = dark::FocusStrokeInner;
    
    resources_.DividerStrokeDefault = dark::DividerStrokeDefault;
    
    resources_.SolidBackgroundBase = dark::SolidBackgroundBase;
    resources_.SolidBackgroundSecondary = dark::SolidBackgroundSecondary;
    resources_.SolidBackgroundTertiary = dark::SolidBackgroundTertiary;
    resources_.SolidBackgroundQuarternary = dark::SolidBackgroundQuarternary;
    resources_.SolidBackgroundQuinary = dark::SolidBackgroundQuinary;
    resources_.SolidBackgroundSenary = dark::SolidBackgroundSenary;
    resources_.SolidBackgroundTransparent = dark::SolidBackgroundTransparent;
    resources_.SolidBackgroundBaseAlt = dark::SolidBackgroundBaseAlt;
    resources_.LayerFillDefault = dark::LayerFillDefault;
    resources_.LayerFillAlt = dark::LayerFillAlt;
    resources_.LayerOnAcrylicFillDefault = dark::LayerOnAcrylicFillDefault;
    resources_.LayerOnAccentAcrylicFillDefault = dark::LayerOnAccentAcrylicFillDefault;
    resources_.LayerOnMicaBaseAltDefault = dark::LayerOnMicaBaseAltDefault;
    resources_.LayerOnMicaBaseAltSecondary = dark::LayerOnMicaBaseAltSecondary;
    resources_.LayerOnMicaBaseAltTertiary = dark::LayerOnMicaBaseAltTertiary;
    resources_.LayerOnMicaBaseAltTransparent = dark::LayerOnMicaBaseAltTransparent;
    resources_.SmokeFillDefault = dark::SmokeFillDefault;
    
    resources_.SystemSuccess = dark::SystemSuccess;
    resources_.SystemCaution = dark::SystemCaution;
    resources_.SystemCritical = dark::SystemCritical;
    resources_.SystemNeutral = dark::SystemNeutral;
    resources_.SystemSolidNeutral = dark::SystemSolidNeutral;
    resources_.SystemAttentionBackground = dark::SystemAttentionBackground;
    resources_.SystemSuccessBackground = dark::SystemSuccessBackground;
    resources_.SystemCautionBackground = dark::SystemCautionBackground;
    resources_.SystemCriticalBackground = dark::SystemCriticalBackground;
    resources_.SystemNeutralBackground = dark::SystemNeutralBackground;
    resources_.SystemSolidAttentionBackground = dark::SystemSolidAttentionBackground;
    resources_.SystemSolidNeutralBackground = dark::SystemSolidNeutralBackground;
    
    update_accent_resources();
}

void ThemeManager::apply_light_theme() {
    resources_.TextPrimary = light::TextPrimary;
    resources_.TextSecondary = light::TextSecondary;
    resources_.TextTertiary = light::TextTertiary;
    resources_.TextDisabled = light::TextDisabled;
    resources_.TextInverse = light::TextInverse;
    
    resources_.TextOnAccentPrimary = light::TextOnAccentPrimary;
    resources_.TextOnAccentSecondary = light::TextOnAccentSecondary;
    resources_.TextOnAccentDisabled = light::TextOnAccentDisabled;
    
    resources_.ControlFillDefault = light::ControlFillDefault;
    resources_.ControlFillSecondary = light::ControlFillSecondary;
    resources_.ControlFillTertiary = light::ControlFillTertiary;
    resources_.ControlFillQuarternary = light::ControlFillQuarternary;
    resources_.ControlFillDisabled = light::ControlFillDisabled;
    resources_.ControlFillTransparent = light::ControlFillTransparent;
    resources_.ControlFillInputActive = light::ControlFillInputActive;
    
    resources_.ControlStrongFillDefault = light::ControlStrongFillDefault;
    resources_.ControlStrongFillDisabled = light::ControlStrongFillDisabled;
    resources_.ControlSolidFillDefault = light::ControlSolidFillDefault;
    
    resources_.SubtleFillTransparent = light::SubtleFillTransparent;
    resources_.SubtleFillSecondary = light::SubtleFillSecondary;
    resources_.SubtleFillTertiary = light::SubtleFillTertiary;
    resources_.SubtleFillDisabled = light::SubtleFillDisabled;
    
    resources_.ControlAltFillTransparent = light::ControlAltFillTransparent;
    resources_.ControlAltFillSecondary = light::ControlAltFillSecondary;
    resources_.ControlAltFillTertiary = light::ControlAltFillTertiary;
    resources_.ControlAltFillQuarternary = light::ControlAltFillQuarternary;
    resources_.ControlAltFillDisabled = light::ControlAltFillDisabled;

    resources_.ControlOnImageFillDefault = light::ControlOnImageFillDefault;
    resources_.ControlOnImageFillSecondary = light::ControlOnImageFillSecondary;
    resources_.ControlOnImageFillTertiary = light::ControlOnImageFillTertiary;
    resources_.ControlOnImageFillDisabled = light::ControlOnImageFillDisabled;
    
    resources_.AccentDisabled = light::AccentFillDisabled;
    
    resources_.ControlStrokeDefault = light::ControlStrokeDefault;
    resources_.ControlStrokeSecondary = light::ControlStrokeSecondary;
    resources_.ControlStrokeOnAccentDefault = light::ControlStrokeOnAccentDefault;
    resources_.ControlStrokeOnAccentSecondary = light::ControlStrokeOnAccentSecondary;
    resources_.ControlStrokeOnAccentTertiary = light::ControlStrokeOnAccentTertiary;
    resources_.ControlStrokeOnAccentDisabled = light::ControlStrokeOnAccentDisabled;
    resources_.ControlStrokeForStrongFillWhenOnImage = light::ControlStrokeForStrongFillWhenOnImage;
    resources_.ControlStrongStrokeDefault = light::ControlStrongStrokeDefault;
    resources_.ControlStrongStrokeDisabled = light::ControlStrongStrokeDisabled;
    
    resources_.CardStrokeDefault = light::CardStrokeDefault;
    resources_.CardStrokeDefaultSolid = light::CardStrokeDefaultSolid;
    resources_.CardBackgroundDefault = light::CardBackgroundDefault;
    resources_.CardBackgroundSecondary = light::CardBackgroundSecondary;
    resources_.CardBackgroundTertiary = light::CardBackgroundTertiary;

    resources_.SurfaceStrokeDefault = light::SurfaceStrokeDefault;
    resources_.SurfaceStrokeFlyout = light::SurfaceStrokeFlyout;
    resources_.SurfaceStrokeInverse = light::SurfaceStrokeInverse;
    
    resources_.FocusStrokeOuter = light::FocusStrokeOuter;
    resources_.FocusStrokeInner = light::FocusStrokeInner;
    
    resources_.DividerStrokeDefault = light::DividerStrokeDefault;
    
    resources_.SolidBackgroundBase = light::SolidBackgroundBase;
    resources_.SolidBackgroundSecondary = light::SolidBackgroundSecondary;
    resources_.SolidBackgroundTertiary = light::SolidBackgroundTertiary;
    resources_.SolidBackgroundQuarternary = light::SolidBackgroundQuarternary;
    resources_.SolidBackgroundQuinary = light::SolidBackgroundQuinary;
    resources_.SolidBackgroundSenary = light::SolidBackgroundSenary;
    resources_.SolidBackgroundTransparent = light::SolidBackgroundTransparent;
    resources_.SolidBackgroundBaseAlt = light::SolidBackgroundBaseAlt;
    resources_.LayerFillDefault = light::LayerFillDefault;
    resources_.LayerFillAlt = light::LayerFillAlt;
    resources_.LayerOnAcrylicFillDefault = light::LayerOnAcrylicFillDefault;
    resources_.LayerOnAccentAcrylicFillDefault = light::LayerOnAccentAcrylicFillDefault;
    resources_.LayerOnMicaBaseAltDefault = light::LayerOnMicaBaseAltDefault;
    resources_.LayerOnMicaBaseAltSecondary = light::LayerOnMicaBaseAltSecondary;
    resources_.LayerOnMicaBaseAltTertiary = light::LayerOnMicaBaseAltTertiary;
    resources_.LayerOnMicaBaseAltTransparent = light::LayerOnMicaBaseAltTransparent;
    resources_.SmokeFillDefault = light::SmokeFillDefault;
    
    resources_.SystemSuccess = light::SystemSuccess;
    resources_.SystemCaution = light::SystemCaution;
    resources_.SystemCritical = light::SystemCritical;
    resources_.SystemNeutral = light::SystemNeutral;
    resources_.SystemSolidNeutral = light::SystemSolidNeutral;
    resources_.SystemAttentionBackground = light::SystemAttentionBackground;
    resources_.SystemSuccessBackground = light::SystemSuccessBackground;
    resources_.SystemCautionBackground = light::SystemCautionBackground;
    resources_.SystemCriticalBackground = light::SystemCriticalBackground;
    resources_.SystemNeutralBackground = light::SystemNeutralBackground;
    resources_.SystemSolidAttentionBackground = light::SystemSolidAttentionBackground;
    resources_.SystemSolidNeutralBackground = light::SystemSolidNeutralBackground;
    
    update_accent_resources();
}

} // namespace fluxent::theme
