#ifndef FLUX_PROJECTION_H
#define FLUX_PROJECTION_H

#include "render/flux_render_internal.h"

typedef struct FluxProjection {
	ID2D1DeviceContext *dc;
	ID2D1CommandList   *list;
	FluxRenderCtx       rc;
	float               cx;
	float               cy;
	float               rotate_x;
	float               rotate_y;
} FluxProjection;

bool flux_projection_begin(
  FluxProjection *p, FluxRenderCtx const *parent, D2D1_MATRIX_3X2_F const *content, float cx, float cy, float rotate_x,
  float rotate_y
);
void flux_projection_end(FluxProjection *p, FluxRenderCtx const *parent);

#endif
