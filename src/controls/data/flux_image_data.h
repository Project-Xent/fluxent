#ifndef FLUX_IMAGE_DATA_H
#define FLUX_IMAGE_DATA_H

#include <xtk/xtk_types.h>

typedef XtkImageStretch FluxImageStretch;

#define FLUX_IMAGE_NONE            XTK_IMAGE_NONE
#define FLUX_IMAGE_FILL            XTK_IMAGE_FILL
#define FLUX_IMAGE_UNIFORM         XTK_IMAGE_UNIFORM
#define FLUX_IMAGE_UNIFORM_TO_FILL XTK_IMAGE_UNIFORM_TO_FILL

typedef struct FluxImageData {
	char const      *source;
	FluxImageStretch stretch;
	float            natural_w;
	float            natural_h;
} FluxImageData;

#endif
