#ifndef FLUX_THEME_H
#define FLUX_THEME_H

#include "flux_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FluxThemeMode {
    FLUX_THEME_LIGHT,
    FLUX_THEME_DARK,
    FLUX_THEME_SYSTEM,
} FluxThemeMode;

/* All Fluent Design color tokens needed by controls */
typedef struct FluxThemeColors {
    /* Control fill */
    FluxColor ctrl_fill_default;
    FluxColor ctrl_fill_secondary;
    FluxColor ctrl_fill_tertiary;
    FluxColor ctrl_fill_disabled;
    FluxColor ctrl_fill_input_active;

    /* Control alt fill */
    FluxColor ctrl_alt_fill_secondary;
    FluxColor ctrl_alt_fill_tertiary;
    FluxColor ctrl_alt_fill_quarternary;
    FluxColor ctrl_alt_fill_disabled;

    /* Control stroke */
    FluxColor ctrl_stroke_default;
    FluxColor ctrl_stroke_secondary;
    FluxColor ctrl_stroke_on_accent_default;
    FluxColor ctrl_stroke_on_accent_secondary;
    FluxColor ctrl_strong_stroke_default;
    FluxColor ctrl_strong_stroke_disabled;

    /* Solid fill */
    FluxColor ctrl_solid_fill_default;

    /* Strong fill */
    FluxColor ctrl_strong_fill_default;
    FluxColor ctrl_strong_fill_disabled;

    /* Accent */
    FluxColor accent_default;
    FluxColor accent_secondary;
    FluxColor accent_tertiary;
    FluxColor accent_disabled;

    /* Text */
    FluxColor text_primary;
    FluxColor text_secondary;
    FluxColor text_disabled;
    FluxColor text_on_accent_primary;
    FluxColor text_on_accent_secondary;
    FluxColor text_on_accent_disabled;

    /* Card & divider */
    FluxColor card_bg_default;
    FluxColor card_stroke_default;
    FluxColor divider_stroke_default;

    /* Focus */
    FluxColor focus_stroke_outer;
    FluxColor focus_stroke_inner;

    /* Layer / background (for app background when using mica etc) */
    FluxColor layer_default;
    FluxColor layer_alt;
    FluxColor solid_background;
} FluxThemeColors;

typedef struct FluxThemeManager FluxThemeManager;

FluxThemeManager   *flux_theme_create(void);
void                flux_theme_destroy(FluxThemeManager *tm);

void                flux_theme_set_mode(FluxThemeManager *tm, FluxThemeMode mode);
FluxThemeMode       flux_theme_get_mode(const FluxThemeManager *tm);

/* Returns the resolved colors for the currently active theme */
const FluxThemeColors *flux_theme_colors(const FluxThemeManager *tm);

/* Version counter — incremented on every mode change. Controls can cache
   colors and re-query only when the version changes. */
uint32_t            flux_theme_version(const FluxThemeManager *tm);

/* Detect system dark mode from Windows registry/API */
bool                flux_theme_system_is_dark(void);

#ifdef __cplusplus
}
#endif

#endif