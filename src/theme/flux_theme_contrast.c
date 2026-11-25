#include "theme/flux_theme_contrast.h"

#include <windows.h>

bool flux_theme_systemcontrast(void) {
	HIGHCONTRASTW hc = {.cbSize = sizeof(hc)};
	return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) && (hc.dwFlags & HCF_HIGHCONTRASTON);
}

static FluxColor system_color(int index) {
	COLORREF rgb = GetSysColor(index);
	return flux_color_rgb(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}

FluxContrastColors flux_theme_contrast_colors(void) {
	return (FluxContrastColors) {
	  .active         = true,
	  .window         = system_color(COLOR_WINDOW),
	  .window_text    = system_color(COLOR_WINDOWTEXT),
	  .button_face    = system_color(COLOR_BTNFACE),
	  .button_text    = system_color(COLOR_BTNTEXT),
	  .gray_text      = system_color(COLOR_GRAYTEXT),
	  .highlight      = system_color(COLOR_HIGHLIGHT),
	  .highlight_text = system_color(COLOR_HIGHLIGHTTEXT),
	  .hotlight       = system_color(COLOR_HOTLIGHT),
	};
}

void flux_theme_contrast_palette(FluxThemeColors *c) {
	FluxContrastColors p = flux_theme_contrast_colors();
	*c                   = (FluxThemeColors) {
	  .ctrl_fill_default               = p.button_face,
	  .ctrl_fill_secondary             = p.button_face,
	  .ctrl_fill_tertiary              = p.button_face,
	  .ctrl_fill_disabled              = p.button_face,
	  .ctrl_fill_input_active          = p.button_face,
	  .ctrl_alt_fill_secondary         = p.button_face,
	  .ctrl_alt_fill_tertiary          = p.button_face,
	  .ctrl_alt_fill_quarternary       = p.button_face,
	  .ctrl_alt_fill_disabled          = p.button_face,
	  .subtle_fill_secondary           = p.button_face,
	  .subtle_fill_tertiary            = p.button_face,
	  .ctrl_stroke_default             = p.button_text,
	  .ctrl_stroke_secondary           = p.button_text,
	  .ctrl_stroke_on_accent_default   = p.button_text,
	  .ctrl_stroke_on_accent_secondary = p.button_text,
	  .ctrl_strong_stroke_default      = p.button_text,
	  .ctrl_strong_stroke_disabled     = p.button_text,
	  .ctrl_solid_fill_default         = p.button_face,
	  .ctrl_strong_fill_default        = p.button_face,
	  .ctrl_strong_fill_disabled       = p.button_face,
	  .accent_default                  = p.window,
	  .accent_secondary                = p.window,
	  .accent_tertiary                 = p.window,
	  .accent_disabled                 = p.window,
	  .text_primary                    = p.window_text,
	  .text_secondary                  = p.window_text,
	  .text_tertiary                   = p.window_text,
	  .text_disabled                   = p.gray_text,
	  .text_on_accent_primary          = p.window_text,
	  .text_on_accent_secondary        = p.window_text,
	  .text_on_accent_disabled         = p.gray_text,
	  .card_bg_default                 = p.window,
	  .card_stroke_default             = p.window_text,
	  .card_stroke_solid               = p.window_text,
	  .divider_stroke_default          = p.window_text,
	  .base_low                        = p.button_face,
	  .system_accent                   = p.highlight,
	  .system_accent_light1            = p.highlight,
	  .focus_stroke_outer              = p.button_text,
	  .focus_stroke_inner              = p.button_face,
	  .layer_default                   = p.window,
	  .layer_alt                       = p.window,
	  .layer_on_mica_alt_default       = p.window,
	  .layer_on_mica_alt_secondary     = p.window,
	  .solid_background                = p.window,
	  .solid_background_secondary      = p.window,
	  .solid_background_tertiary       = p.window,
	  .ctrl_on_image_default           = p.button_face,
	  .ctrl_on_image_secondary         = p.button_face,
	  .ctrl_on_image_tertiary          = p.button_face,
	  .ctrl_on_image_disabled          = p.button_face,
	  .list_box_bg                     = p.window,
	  .flip_nav_bg_default             = p.button_face,
	  .flip_view_bg                    = p.button_face,
	  .flyout_background               = p.window,
	  .flyout_border                   = p.window_text,
	  .accent_text_primary             = p.window_text,
	  .accent_text_tertiary            = p.window_text,
	  .accent_text_disabled            = p.gray_text,
	  .contrast                        = p,
	};
}
