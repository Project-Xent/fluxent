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

/* Records everything drawn through p->rc until flux_projection_end, then draws it onto parent rotated about
   (cx, cy) (parent world DIPs) with PlaneProjection's camera. content is the world transform the recorded
   content would have had on parent. False leaves p untouched and parent unchanged. */
bool flux_projection_begin(
  FluxProjection *p, FluxRenderCtx const *parent, D2D1_MATRIX_3X2_F const *content, float cx, float cy, float rotate_x,
  float rotate_y
);
void flux_projection_end(FluxProjection *p, FluxRenderCtx const *parent);

#endif
