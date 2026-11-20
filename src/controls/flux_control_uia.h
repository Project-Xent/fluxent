#ifndef FLUX_CONTROL_UIA_H
#define FLUX_CONTROL_UIA_H

#include "fluxent/flux_node_store.h"

int  flux_expander_uia_expand(FluxStore *store, XentNodeId node);
void flux_expander_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_combo_uia_expand(FluxStore *store, XentNodeId node);
void flux_combo_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_split_uia_expand(FluxStore *store, XentNodeId node);
void flux_split_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_dropdown_uia_expand(FluxStore *store, XentNodeId node);
void flux_dropdown_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_menubar_uia_expand(FluxStore *store, XentNodeId node);
void flux_menubar_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_tree_uia_expand(FluxStore *store, XentNodeId node);
void flux_tree_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_nav_uia_expand(FluxStore *store, XentNodeId node);
void flux_nav_uia_toggle(FluxStore *store, XentNodeId node);
int  flux_cmdbar_uia_expand(FluxStore *store, XentNodeId node);
void flux_cmdbar_uia_toggle(FluxStore *store, XentNodeId node);

#endif
