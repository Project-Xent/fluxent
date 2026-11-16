#ifndef FLUX_COLHEADER_H
#define FLUX_COLHEADER_H

#include "controls/data/flux_colheader_data.h"

#include <stdbool.h>
#include <stdint.h>

XentNodeId flux_colheader_init(XentCtx *ctx, FluxStore *store, FluxWindow *window);
XentNodeId flux_colitem_init(XentCtx *ctx, FluxStore *store, FluxWindow *window);
void       flux_colheader_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
void       flux_colitem_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
bool       flux_colheader_step(void *ctx, unsigned long now);

void       flux_colitem_settext(FluxStore *store, XentNodeId node, char const *text);
void       flux_colitem_setwidth(FluxStore *store, XentNodeId node, float width);
void       flux_colitem_setmin(FluxStore *store, XentNodeId node, float width);
void       flux_colitem_setmax(FluxStore *store, XentNodeId node, float width);
void       flux_colitem_setalign(FluxStore *store, XentNodeId node, uint32_t align);
void       flux_colitem_setsort(FluxStore *store, XentNodeId node, uint32_t sort);
void       flux_colitem_setsortable(FluxStore *store, XentNodeId node, bool sortable);
void       flux_colitem_setresizable(FluxStore *store, XentNodeId node, bool resizable);
int        flux_colitem_cursor(FluxStore *store, XentNodeId node);

#endif
