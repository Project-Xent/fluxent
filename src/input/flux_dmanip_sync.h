#ifndef FLUX_DMANIP_SYNC_H
#define FLUX_DMANIP_SYNC_H

#include "fluxent/flux_node_store.h"
#include "flux_dmanip.h"
#include <xent/xent.h>

typedef struct FluxInput FluxInput;

void flux_dmanip_sync_tree(FluxDManip *dmanip, XentCtx *ctx, FluxStore *store, XentNodeId root, float scale);

bool flux_dmanip_pan_handoff(FluxDManip *dmanip, FluxInput *input, FluxStore *store, uint32_t pointer_id);

void flux_dmanip_cleanup_tree(XentCtx *ctx, FluxStore *store, XentNodeId root);

void flux_dmanip_vp_release(FluxStore *store, XentNodeId node);

#endif
