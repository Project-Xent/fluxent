#ifndef FLUX_THEME_ANIM_H
#define FLUX_THEME_ANIM_H

#include <stdbool.h>

enum
{
	FLUX_THEMEANIM_TRANSLATE = 0,
	FLUX_THEMEANIM_SCALE     = 1,
	FLUX_THEMEANIM_OPACITY   = 2,
	FLUX_THEMEANIM_CLIP      = 3,
};

typedef struct FluxThemeAnim {
	double delay;
	double duration;
	float  x1, y1, x2, y2;
	float  from, to;
} FluxThemeAnim;

FluxThemeAnim flux_themeanim_get(int storyboard, int target, int transform, FluxThemeAnim fallback);

float         flux_themeanim_progress(FluxThemeAnim const *a, double elapsed);

double        flux_themeanim_end(FluxThemeAnim const *a);

#endif
