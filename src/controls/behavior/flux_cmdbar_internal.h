#ifndef FLUX_CMDBAR_INTERNAL_H
#define FLUX_CMDBAR_INTERNAL_H

#include "controls/behavior/flux_cmdbar.h"
#include "fluxent/flux_types.h"

FluxAppBarItemData *flux_cmdbar_item(FluxStore *store, XentNodeId node);
bool                flux_cmdbar_moved(FluxCmdBarData const *b, XentNodeId node);
void                flux_cmdbar_open(FluxCmdBarData *b, bool open, bool user, bool keyboard);
void                flux_cmdbar_execute(FluxAppBarItemData *d);
void                flux_cmdbar_repaint(FluxCmdBarData const *b);

bool                flux_cmdbar_ovf_create(FluxCmdBarData *b);
void                flux_cmdbar_ovf_destroy(FluxCmdBarData *b);
void                flux_cmdbar_ovf_build(FluxCmdBarData *b);
void                flux_cmdbar_ovf_show(FluxCmdBarData *b);
bool                flux_cmdbar_ovf_key(FluxCmdBarData *b, FluxKey key);
int                 flux_cmdbar_ovf_edge(FluxCmdBarData const *b, bool last);

#endif
