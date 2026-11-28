#ifndef FLUX_THEME_ANIM_H
#define FLUX_THEME_ANIM_H

#include <stdbool.h>
#include <stdint.h>

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

typedef struct FluxThemeTrack {
	bool   found;
	double delay;
	double duration;
	float  x1, y1, x2, y2;
	bool   has_initial;
	bool   has_origin;
	float  init_x, init_y;
	float  to_x, to_y;
	float  origin_x, origin_y;
} FluxThemeTrack;

typedef struct FluxThemeStagger {
	bool   valid;
	double delay_ms;
	double maximum_ms;
	double reduce;
} FluxThemeStagger;

FluxThemeAnim    flux_themeanim_get(int storyboard, int target, int transform, FluxThemeAnim fallback);

FluxThemeTrack   flux_themeanim_track(int storyboard, int target, int transform, FluxThemeTrack fallback);

FluxThemeStagger flux_themeanim_stagger(int storyboard, int target, FluxThemeStagger fallback);

double           flux_themeanim_stagger_delay(FluxThemeStagger const *s, uint32_t index);

float            flux_themeanim_progress(FluxThemeAnim const *a, double elapsed);

double           flux_themeanim_end(FluxThemeAnim const *a);

#endif
