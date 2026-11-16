#ifndef FLUX_DRAGDROP_H
#define FLUX_DRAGDROP_H

#include "fluxent/flux_app.h"
#include "store/flux_node_data.h"

typedef struct FluxDragDrop FluxDragDrop;

typedef struct FluxDragDropEnv {
	FluxApp    *app;
	FluxWindow *window;
	FluxInput  *input;
	FluxStore  *store;
	XentNodeId  root;
} FluxDragDropEnv;

FluxDragDrop *flux_dragdrop_create(FluxDragDropEnv const *env);
void          flux_dragdrop_destroy(FluxDragDrop *dd);

#endif
