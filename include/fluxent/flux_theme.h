/**
 * @file flux_theme.h
 * @brief Fluent Design theme colors and token definitions.
 *
 * Provides FluxThemeColors containing all color tokens needed by controls
 * to render in light or dark mode. Use `flux_theme_get_colors()` to obtain
 * the appropriate palette based on the current theme mode.
 */

#ifndef FLUX_THEME_H
#define FLUX_THEME_H

#include "flux_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Theme mode selection. */
typedef enum FluxThemeMode
{
	FLUX_THEME_LIGHT,  /**< Light theme. */
	FLUX_THEME_DARK,   /**< Dark theme. */
	FLUX_THEME_SYSTEM, /**< Follow system setting. */
} FluxThemeMode;

/** @brief All Fluent Design color tokens needed by controls. */
typedef struct FluxThemeColors {
	FluxColor ctrl_fill_default;
	FluxColor ctrl_fill_secondary;
	FluxColor ctrl_fill_tertiary;
	FluxColor ctrl_fill_disabled;
	FluxColor ctrl_fill_input_active;

	FluxColor ctrl_alt_fill_secondary;
	FluxColor ctrl_alt_fill_tertiary;
	FluxColor ctrl_alt_fill_quarternary;
	FluxColor ctrl_alt_fill_disabled;

	FluxColor subtle_fill_secondary;
	FluxColor subtle_fill_tertiary;

	FluxColor ctrl_stroke_default;
	FluxColor ctrl_stroke_secondary;
	FluxColor ctrl_stroke_on_accent_default;
	FluxColor ctrl_stroke_on_accent_secondary;
	FluxColor ctrl_strong_stroke_default;
	FluxColor ctrl_strong_stroke_disabled;

	FluxColor ctrl_solid_fill_default;

	FluxColor ctrl_strong_fill_default;
	FluxColor ctrl_strong_fill_disabled;

	FluxColor accent_default;
	FluxColor accent_secondary;
	FluxColor accent_tertiary;
	FluxColor accent_disabled;

	FluxColor text_primary;
	FluxColor text_secondary;
	FluxColor text_tertiary;
	FluxColor text_disabled;
	FluxColor text_on_accent_primary;
	FluxColor text_on_accent_secondary;
	FluxColor text_on_accent_disabled;

	FluxColor card_bg_default;
	FluxColor card_stroke_default;
	FluxColor divider_stroke_default;

	FluxColor focus_stroke_outer;
	FluxColor focus_stroke_inner;

	FluxColor layer_default;
	FluxColor layer_alt;
	FluxColor solid_background;
} FluxThemeColors;

/** @brief Opaque theme manager. */
typedef struct FluxThemeManager FluxThemeManager;

/** @brief Create a theme manager. */
FluxThemeManager               *flux_theme_create(void);
/** @brief Destroy a theme manager. */
void                            flux_theme_destroy(FluxThemeManager *tm);

/** @brief Set the active theme mode. */
void                            flux_theme_set_mode(FluxThemeManager *tm, FluxThemeMode mode);
/** @brief Get the active theme mode. */
FluxThemeMode                   flux_theme_get_mode(FluxThemeManager const *tm);

/** @brief Return the resolved colors for the currently active theme. */
FluxThemeColors const          *flux_theme_colors(FluxThemeManager const *tm);

/** @brief Return a version counter incremented whenever theme colors may have changed. */
uint32_t                        flux_theme_version(FluxThemeManager const *tm);

/** @brief Return true if the operating system is currently using dark mode. */
bool                            flux_theme_system_is_dark(void);

#ifdef __cplusplus
}
#endif

#endif
