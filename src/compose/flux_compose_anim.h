#ifndef FLUX_COMPOSE_ANIM_H
#define FLUX_COMPOSE_ANIM_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

typedef struct FluxAnimKit FluxAnimKit;

typedef enum FluxEaseId
{
	FLUX_EASE_LINEAR = 0,
	FLUX_EASE_STANDARD,
	FLUX_EASE_ACCELERATE,
	FLUX_EASE_DECELERATE,
	FLUX_EASE_COUNT,
} FluxEaseId;

FluxAnimKit *flux_anim_kit_create(FluxCompositor *c);

void         flux_anim_kit_destroy(FluxAnimKit *kit);

HRESULT
flux_anim_scalar(FluxAnimKit *kit, void *target, wchar_t const *prop, float to, double seconds, FluxEaseId ease);

HRESULT
flux_anim_color(FluxAnimKit *kit, void *target, wchar_t const *prop, FluxColor to, double seconds, FluxEaseId ease);

#endif
