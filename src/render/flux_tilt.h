#ifndef FLUX_TILT_H
#define FLUX_TILT_H

#include "store/flux_node_data.h"

typedef struct FluxWindow FluxWindow;

typedef struct FluxTilt {
	FluxStore    *store;
	FluxWindow   *window;
	XentNodeId    node;
	float         scale;
	float         rotate_x;
	float         rotate_y;
	unsigned long start;
} FluxTilt;

void flux_tilt_press(FluxTilt *t, XentNodeId node, float x, float y);
void flux_tilt_release(FluxTilt *t);
void flux_tilt_stop(FluxTilt *t);

#endif
