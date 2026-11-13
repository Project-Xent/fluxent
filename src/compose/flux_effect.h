#ifndef FLUX_COMPOSE_EFFECT_H
#define FLUX_COMPOSE_EFFECT_H

#include "compose/flux_compose.h"

WUC_Brush *flux_effect_backdrop_blur(FluxCompositor *c, float blur_amount);

WUC_Brush *flux_effect_host_blur(FluxCompositor *c, float blur_amount);

#endif
