#ifndef FLUX_UIA_TREE_H
#define FLUX_UIA_TREE_H

#include "a11y/flux_uia.h"

#include <stdbool.h>

bool       flux_uia_exposed(FluxUiaContext const *info, XentNodeId node);
bool       flux_uia_in_tree(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_visible(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_parent(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_first(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_last(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_next(FluxUiaContext const *info, XentNodeId node);
XentNodeId flux_uia_prev(FluxUiaContext const *info, XentNodeId node);

#endif
