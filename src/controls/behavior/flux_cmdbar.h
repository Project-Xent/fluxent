#ifndef FLUX_CMDBAR_H
#define FLUX_CMDBAR_H

#include "controls/data/flux_cmdbar_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxControlEnv FluxControlEnv;
typedef struct FluxNodeData   FluxNodeData;

XentNodeId                    flux_cmdbar_init(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env);
XentNodeId                    flux_appbar_init(XentCtx *ctx, FluxStore *store, FluxText *text, FluxAppBarKind kind);
void                          flux_cmdbar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
void                          flux_appbar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
XentNodeId                    flux_cmdbar_slot(FluxStore *store, XentNodeId bar, XentNodeId child);
XentNodeId                    flux_cmdbar_content(FluxStore *store, XentNodeId bar);
uint32_t                      flux_cmdbar_frame(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd);
bool                          flux_cmdovf_wheel(FluxStore *store, XentNodeId node, float delta, float *remaining);

void                          flux_cmdbar_setopen(FluxStore *store, XentNodeId node, bool open);
void                          flux_cmdbar_setsticky(FluxStore *store, XentNodeId node, bool sticky);
void                          flux_cmdbar_setmode(FluxStore *store, XentNodeId node, uint32_t mode);
void                          flux_cmdbar_setlabels(FluxStore *store, XentNodeId node, uint32_t position);
void                          flux_cmdbar_setmore(FluxStore *store, XentNodeId node, uint32_t visibility);
void                          flux_cmdbar_setdynamic(FluxStore *store, XentNodeId node, bool dynamic);
void                          flux_cmdbar_setenabled(FluxStore *store, XentNodeId node, bool enabled);

void                          flux_appbar_settext(FluxStore *store, XentNodeId node, char const *text);
void                          flux_appbar_seticon(FluxStore *store, XentNodeId node, char const *icon);
void                          flux_appbar_setaccel(FluxStore *store, XentNodeId node, char const *accel);
void                          flux_appbar_setenabled(FluxStore *store, XentNodeId node, bool enabled);
void                          flux_appbar_setchecked(FluxStore *store, XentNodeId node, bool checked);
void                          flux_appbar_setsecondary(FluxStore *store, XentNodeId node, bool secondary);
void                          flux_appbar_setorder(FluxStore *store, XentNodeId node, int32_t order);
void                          flux_appbar_setlabelpos(FluxStore *store, XentNodeId node, uint32_t position);
void                          flux_appbar_setwidth(FluxStore *store, XentNodeId node, float width);

#endif
