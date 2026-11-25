#ifndef FLUX_THEME_CONTRAST_H
#define FLUX_THEME_CONTRAST_H

#include "fluxent/flux_theme.h"

bool               flux_theme_systemcontrast(void);
FluxContrastColors flux_theme_contrast_colors(void);
void               flux_theme_contrast_palette(FluxThemeColors *colors);

#endif
