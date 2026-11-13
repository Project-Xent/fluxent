#ifndef FLUX_CONTROL_CURSOR_H
#define FLUX_CONTROL_CURSOR_H

#include "fluxent/flux_node_store.h"
#include <xent/xent.h>

int flux_control_cursor_for(XentCtx *ctx, FluxStore *store, XentNodeId node);

#endif
