#ifndef FLUX_IMAGE_DATA_H
#define FLUX_IMAGE_DATA_H

#include <xent/xent.h>
#include <xtk/xtk_types.h>

#include <stdint.h>

struct FluxImage;

typedef XtkImageStretch FluxImageStretch;

#define FLUX_IMAGE_NONE            XTK_IMAGE_NONE
#define FLUX_IMAGE_FILL            XTK_IMAGE_FILL
#define FLUX_IMAGE_UNIFORM         XTK_IMAGE_UNIFORM
#define FLUX_IMAGE_UNIFORM_TO_FILL XTK_IMAGE_UNIFORM_TO_FILL

typedef struct FluxImageData {
	char const       *source;
	FluxImageStretch  stretch;
	XentInsets        nine;
	struct FluxImage *image;
	uint32_t          frame;
	float             seen_w;
	float             seen_h;
	double            start;
	uint8_t           reported;
} FluxImageData;

#endif
