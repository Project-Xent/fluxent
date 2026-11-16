#ifndef FLUX_LIFT_H
#define FLUX_LIFT_H

#include "store/flux_node_data.h"

typedef struct FluxLiftPlace {
	XentNodeId layer;
	float      dx;
	float      dy;
} FluxLiftPlace;

bool flux_lift_place(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxLiftPlace *out);
bool flux_lift_within(XentCtx const *ctx, FluxStore const *store, XentNodeId node);

#endif
