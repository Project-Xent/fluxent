#ifndef FLUX_GRAPHICS_COMPOSE_H
#define FLUX_GRAPHICS_COMPOSE_H

#include "fluxent/flux_gfx.h"
#include "compose/flux_compose.h"

#include <stdbool.h>

void                           flux_gfx_setopacity(FluxGfx *gfx, float opacity);
void                           flux_gfx_setplate(FluxGfx *gfx, float x, float y, float w, float h, float radius);

bool                           flux_graphics_composition_enabled(FluxGfx const *gfx);

FluxCompositor                *flux_graphics_compositor(FluxGfx *gfx);

WUC_Visual                    *flux_graphics_composition_root(FluxGfx *gfx);

WUC_CompositionGraphicsDevice *flux_graphics_graphics_device(FluxGfx *gfx);

bool                           flux_gfx_enable_acrylic(FluxGfx *gfx, float blur);

#endif
