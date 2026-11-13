#ifndef FLUX_HOST_MENU_H
#define FLUX_HOST_MENU_H

#include "controls/factory/flux_control_build.h"

typedef struct FluxMenu FluxMenu;

bool       flux_host_menufly_build(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlEnv const *env);
bool       flux_host_menuitem_build(XentCtx *ctx, FluxStore *store, XentNodeId node);

void       flux_host_menufly_set_text(FluxStore *store, XentNodeId node, char const *text);
void       flux_host_menuitem_set_text(FluxStore *store, XentNodeId node, char const *text);
void       flux_host_menuitem_set_icon(FluxStore *store, XentNodeId node, char const *icon);
void       flux_host_menuitem_set_enabled(FluxStore *store, XentNodeId node, bool enabled);

void       flux_host_menu_sync_fly(FluxStore *store, XentNodeId node);
void       flux_host_menu_sync_bar(FluxStore *store, XentNodeId node);
XentNodeId flux_host_menufly_parent(FluxStore *store, XentNodeId node);

#endif
