#ifndef FLUX_THEME_H
#define FLUX_THEME_H

#include <assert.h>

#include "flux_types.h"
#include <stdbool.h>

typedef enum FluxThemeMode
{
	FLUX_THEME_SYSTEM = 0,
	FLUX_THEME_LIGHT,
	FLUX_THEME_DARK,
} FluxThemeMode;

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
	FluxColor layer_on_mica_alt_default;
	FluxColor layer_on_mica_alt_secondary;
	FluxColor solid_background;
	FluxColor solid_background_tertiary;

	FluxColor ctrl_on_image_default;
	FluxColor ctrl_on_image_secondary;
	FluxColor ctrl_on_image_tertiary;
	FluxColor ctrl_on_image_disabled;

	FluxColor list_box_bg;

	FluxColor flip_nav_bg_default;
	FluxColor flip_view_bg;

	FluxColor flyout_background;
	FluxColor flyout_border;

	FluxColor accent_text_primary;
	FluxColor accent_text_tertiary;
	FluxColor accent_text_disabled;
} FluxThemeColors;

typedef struct FluxTheme  FluxTheme;

XENT_NODISCARD FluxTheme *flux_theme_create(void);
void                      flux_theme_destroy(FluxTheme *tm);

void                      flux_theme_setmode(FluxTheme *tm, FluxThemeMode mode);
FluxThemeMode             flux_theme_mode(FluxTheme const *tm);

FluxThemeColors const    *flux_theme_colors(FluxTheme *tm);

FluxThemeColors const    *flux_theme_defaults(void);

uint32_t                  flux_theme_ver(FluxTheme const *tm);

bool                      flux_theme_systemdark(void);

static_assert(sizeof(FluxThemeMode) == 4, "FluxThemeMode crosses the ABI as int32");

#endif
