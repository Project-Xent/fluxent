#ifndef FLUX_COMPOSE_SURFACE_H
#define FLUX_COMPOSE_SURFACE_H

#include "compose/flux_compose.h"

typedef struct ID2D1DeviceContext ID2D1DeviceContext;

typedef struct FluxSurface        FluxSurface;

FluxSurface        *flux_surface_create(FluxCompositor *c, WUC_CompositionGraphicsDevice *device, int w, int h);

void                flux_surface_destroy(FluxSurface *s);

HRESULT             flux_surface_resize(FluxSurface *s, int w, int h);

ID2D1DeviceContext *flux_surface_begin(FluxSurface *s, POINT *out_offset);

void                flux_surface_end(FluxSurface *s);

WUC_CompositionSurfaceBrush *flux_surface_brush(FluxSurface *s);

#endif
