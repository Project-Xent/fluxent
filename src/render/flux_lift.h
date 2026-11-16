#ifndef FLUX_LIFT_H
#define FLUX_LIFT_H

#include "store/flux_node_data.h"

typedef struct FluxLiftPlace {
	XentNodeId layer;
	float      dx;
	float      dy;
} FluxLiftPlace;

/* Where a lifted node paints: its layer and the shift its ancestors' translations and scroll offsets put on
   its layout rect. False when the node is the root or a layer, which a lift leaves in place. */
bool flux_lift_place(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxLiftPlace *out);
bool flux_lift_within(XentCtx const *ctx, FluxStore const *store, XentNodeId node);

#endif
