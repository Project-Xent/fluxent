#ifndef FLUX_COMPOSE_SHADOW_H
#define FLUX_COMPOSE_SHADOW_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

WUC_DropShadow *
flux_shadow_apply(FluxCompositor *c, WUC_Sprite *sprite, FluxColor color, float blur, float dx, float dy);

#endif
