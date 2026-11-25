#include "runtime/flux_color_name.h"

#include "controls/behavior/flux_color_picker.h"

#include <math.h>
#include <string.h>

typedef struct NameHue {
	uint8_t limit;
	uint8_t index;
} NameHue;

typedef struct NameLevel {
	uint8_t        max_saturation;
	uint8_t        count;
	NameHue const *hues;
} NameLevel;

static char const *const kLight [23] = {
  "Coral",
  "Rose",
  "Light orange",
  "Tan",
  "Tan",
  "Light yellow",
  "Light yellow",
  "Tan",
  "Light green",
  "Lime",
  "Light green",
  "Light green",
  "Aqua",
  "Sky blue",
  "Light turquoise",
  "Pale blue",
  "Light blue",
  "Ice blue",
  "Periwinkle",
  "Lavender",
  "Pink",
  "Tan",
  "Rose",
};

static char const *const kMid [23] = {
  "Coral",
  "Red",
  "Orange",
  "Brown",
  "Tan",
  "Gold",
  "Yellow",
  "Olive green",
  "Olive green",
  "Green",
  "Green",
  "Bright green",
  "Teal",
  "Aqua",
  "Turquoise",
  "Pale blue",
  "Blue",
  "Blue-gray",
  "Indigo",
  "Purple",
  "Pink",
  "Brown",
  "Red",
};

static char const *const kDark [23] = {
  "Brown",
  "Dark red",
  "Brown",
  "Brown",
  "Brown",
  "Dark yellow",
  "Dark yellow",
  "Brown",
  "Dark green",
  "Dark green",
  "Dark green",
  "Dark green",
  "Dark teal",
  "Dark teal",
  "Dark teal",
  "Dark blue",
  "Dark blue",
  "Blue-gray",
  "Indigo",
  "Dark purple",
  "Plum",
  "Brown",
  "Dark red",
};

static uint8_t const kLow [23]
  = {130, 100, 115, 100, 100, 100, 110, 75, 100, 90, 100, 100, 100, 100, 80, 100, 100, 100, 100, 100, 100, 100, 100};

static uint8_t const kHigh [23]
  = {170, 170, 170, 155, 170, 170, 170, 170, 170, 115, 170, 170, 170, 170, 170, 170, 170, 170, 150, 150, 170, 140, 165};

static NameHue const kLevel1 [] = {
  {8,   1 },
  {44,  4 },
  {63,  8 },
  {122, 11},
  {134, 13},
  {166, 18},
  {176, 19},
  {241, 20},
  {255, 22},
};

static NameHue const kLevel2 [] = {
  {10,  2 },
  {32,  4 },
  {46,  5 },
  {60,  9 },
  {120, 11},
  {131, 13},
  {144, 14},
  {158, 18},
  {166, 19},
  {241, 20},
  {255, 23},
};

static NameHue const kLevel3 [] = {
  {8,   2 },
  {39,  5 },
  {46,  6 },
  {71,  10},
  {120, 11},
  {131, 13},
  {144, 14},
  {163, 17},
  {177, 19},
  {211, 20},
  {249, 21},
  {255, 23},
};

static NameHue const kLevel4 [] = {
  {11,  2 },
  {26,  3 },
  {38,  6 },
  {45,  7 },
  {56,  10},
  {100, 11},
  {121, 12},
  {129, 13},
  {140, 15},
  {180, 17},
  {224, 20},
  {241, 21},
  {255, 23},
};

static NameHue const kLevel5 [] = {
  {13,  2 },
  {27,  3 },
  {36,  6 },
  {45,  7 },
  {59,  10},
  {118, 11},
  {127, 13},
  {136, 14},
  {142, 15},
  {185, 17},
  {216, 20},
  {239, 21},
  {255, 23},
};

static NameLevel const kLevels [5] = {
  {75,  sizeof(kLevel1) / sizeof(kLevel1 [0]), kLevel1},
  {115, sizeof(kLevel2) / sizeof(kLevel2 [0]), kLevel2},
  {150, sizeof(kLevel3) / sizeof(kLevel3 [0]), kLevel3},
  {240, sizeof(kLevel4) / sizeof(kLevel4 [0]), kLevel4},
  {255, sizeof(kLevel5) / sizeof(kLevel5 [0]), kLevel5},
};

static double name_hue(double r, double g, double b, double hi, double c) {
	if (c == 0.0) return 0.0;
	if (hi == r) return fmod((g - b) / c + 6.0, 6.0);
	return hi == g ? (b - r) / c + 2.0 : (r - g) / c + 4.0;
}

static void name_hsl(uint8_t r, uint8_t g, uint8_t b, int *h, int *s, int *l) {
	double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
	double hi = fmax(rf, fmax(gf, bf)), lo = fmin(rf, fmin(gf, bf)), c = hi - lo;
	double lum = (hi + lo) / 2.0;
	double sat = c == 0.0 ? 0.0 : c / (1.0 - fabs(2.0 * lum - 1.0));
	*h         = ( int ) floor(name_hue(rf, gf, bf, hi, c) / 6.0 * 255.0) % 256;
	*s         = ( int ) floor(sat * 255.0);
	*l         = ( int ) floor(lum * 255.0);
}

static char const *name_gray(int l) {
	if (l > 170) return "Light gray";
	return l > 100 ? "Gray" : "Dark gray";
}

static int name_index(int h, int s) {
	NameLevel const *level = &kLevels [4];
	for (int i = 0; i < 5; i++)
		if (s <= kLevels [i].max_saturation) {
			level = &kLevels [i];
			break;
		}
	for (int i = 0; i < level->count; i++)
		if (h < level->hues [i].limit) return level->hues [i].index - 1;
	return level->hues [level->count - 1].index - 1;
}

char const *flux_color_name(uint8_t r, uint8_t g, uint8_t b) {
	int h = 0, s = 0, l = 0;
	name_hsl(r, g, b, &h, &s, &l);
	if (l < 20) return "Black";
	if (l > 240) return "White";
	if (s <= 20) return name_gray(l);
	int i = name_index(h, s);
	if (l > kHigh [i]) return kLight [i];
	return l >= kLow [i] ? kMid [i] : kDark [i];
}

typedef struct NameStep {
	int    c;
	double dir, step, lo, hi, wrap_inc;
	bool   wrap;
} NameStep;

static char const *name_of_hsv(double const *hsv) {
	FluxColor c = flux_color_fromhsv(( float ) hsv [0], ( float ) hsv [1], ( float ) hsv [2], 1.0f);
	return flux_color_name(
	  ( uint8_t ) (c.rgba >> 24), ( uint8_t ) (c.rgba >> 16 & 255u), ( uint8_t ) (c.rgba >> 8 & 255u)
	);
}

static int name_sgn(double v) { return (v > 0.0) - (v < 0.0); }

static int name_bound(NameStep const *n, double *v) {
	if (*v <= n->hi && *v >= n->lo) return 0;
	bool over = *v > n->hi;
	if (n->wrap) *v = over ? n->lo : n->hi;
	else *v = over ? n->hi : n->lo;
	return n->wrap ? 1 : 2;
}

static bool name_seek(NameStep const *n, double *hsv, char const *origin) {
	double orig = hsv [n->c];
	while (strcmp(name_of_hsv(hsv), origin) == 0) {
		double prev  = hsv [n->c];
		hsv [n->c]  += n->dir * n->step;
		int bound    = name_bound(n, &hsv [n->c]);
		if (bound == 2) return false;
		if (bound == 0 && prev != orig && name_sgn(hsv [n->c] - orig) != name_sgn(prev - orig)) return false;
	}
	return true;
}

static double name_snap_midpoint(NameStep const *n, double v) {
	double left = fabs(v);
	while (left > n->step) left -= n->step;
	v -= left;
	while (v < n->lo) v += n->wrap_inc;
	while (v > n->hi) v -= n->wrap_inc;
	return v;
}

static void name_mid(NameStep const *n, double *hsv) {
	double      cur [3] = {hsv [0], hsv [1], hsv [2]}, start = hsv [n->c], offset = 0.0;
	char const *name = name_of_hsv(hsv);
	while (strcmp(name_of_hsv(cur), name) == 0) {
		cur [n->c]    += n->dir * n->step;
		double before  = cur [n->c];
		int    bound   = name_bound(n, &cur [n->c]);
		if (bound == 2) break;
		if (bound == 1) offset = before > n->hi ? n->hi - n->lo : n->lo - n->hi;
	}
	hsv [n->c] = name_snap_midpoint(n, (start + cur [n->c] + offset) / 2.0);
}

void flux_color_next_named(double *hsv, int c, bool lower, bool wrap, double max_hue) {
	NameStep n = {
	  .c        = c,
	  .dir      = lower ? -1.0 : 1.0,
	  .step     = c == 0 ? 1.0 : 0.01,
	  .lo       = 0.0,
	  .hi       = c == 0 ? max_hue : 1.0,
	  .wrap_inc = c == 0 ? 360.0 : 1.0,
	  .wrap     = wrap};
	if (name_seek(&n, hsv, name_of_hsv(hsv))) name_mid(&n, hsv);
}
