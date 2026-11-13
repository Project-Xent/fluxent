#ifndef FLUX_COMPOSE_SHAPE_H
#define FLUX_COMPOSE_SHAPE_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

typedef struct FluxShapeRect FluxShapeRect;

FluxShapeRect               *flux_shape_rect_create(FluxCompositor *c, float w, float h, float radius);

void                         flux_shape_rect_destroy(FluxShapeRect *s);

void                         flux_shape_setsize(FluxShapeRect *s, float w, float h);

void                         flux_shape_setradius(FluxShapeRect *s, float radius);

void                         flux_shape_setfillbrush(FluxShapeRect *s, WUC_Brush *brush);

WUC_Visual                  *flux_shape_rect_visual(FluxShapeRect *s);

#endif
