#ifndef FLUX_UIA_PART_H
#define FLUX_UIA_PART_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>

struct FluxUiaHooks;

XentNodeId flux_uia_part(XentCtx *ctx, FluxStore *store, XentNodeId owner, XentNodeId parent, int index);
void       flux_uia_part_hooks(FluxStore *store, XentNodeId part, struct FluxUiaHooks const *uia);
void       flux_uia_part_place(XentCtx *ctx, XentNodeId part, XentRect local, XentPoint pad);
void       flux_uia_part_label(XentCtx *ctx, XentNodeId part, char const *name, bool enabled);
XentNodeId flux_uia_part_owner(FluxStore *store, XentNodeId part, int *index);

#endif
