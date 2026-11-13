#ifndef FLUX_PERSON_PICTURE_DATA_H
#define FLUX_PERSON_PICTURE_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore FluxStore;

#define FLUX_PP_DEFAULT_SIZE   72.0f
#define FLUX_PP_INITIALS_RATIO 0.42f
#define FLUX_PP_BADGE_RATIO    0.5f
#define FLUX_PP_BADGE_FONT     0.6f
#define FLUX_PP_GLYPH_CONTACT  0xe77bu
#define FLUX_PP_GLYPH_GROUP    0xe716u

typedef struct FluxPersonPictureData {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId node;

	float      diameter;
	char      *display_name;
	char      *initials;
	char      *image_path;
	char      *badge_glyph;
	int        badge_number;
	bool       is_group;

	char       resolved [12];
} FluxPersonPictureData;

#endif
