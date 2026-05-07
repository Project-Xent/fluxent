#ifndef FLUX_CONTROL_CURSOR_H
#define FLUX_CONTROL_CURSOR_H

#include "fluxent/flux_node_store.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

int flux_control_cursor_for_node(XentContext *ctx, FluxNodeStore *store, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif
