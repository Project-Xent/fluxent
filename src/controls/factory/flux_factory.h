#ifndef FLUX_FACTORY_H
#define FLUX_FACTORY_H

#include "fluxent/flux_node_store.h"
#include "store/flux_node_data.h"
#include "controls/data/flux_component_data.h"
#include "host/flux_prop_apply.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_menu.h"
#include <xent/xent.h>

typedef struct FluxWindow FluxWindow;
typedef struct FluxFlyout FluxFlyout;

XentNodeId flux_factory_create_node(XentCtx *ctx, FluxStore *store, XentNodeId parent, FluxControlType type);

XentNodeId flux_factory_fail_node(XentCtx *ctx, FluxStore *store, XentNodeId node);

typedef struct FluxLeafMetrics {
	XentCtx    *ctx;
	XentNodeId  node;
	char const *label;
	XentInsets  padding;
	XentSize    min_size;
	XentSize    fallback;
} FluxLeafMetrics;

void     flux_leaf_default_metrics(FluxLeafMetrics const *m);
void     flux_toggle_applymetrics(XentCtx *ctx, XentNodeId node, FluxControlType type, char const *label);

void     flux_button_data_destroy(void *component_data);

FluxRect flux_binding_screen_anchor(FluxWindow *window, XentCtx *ctx, FluxStore *store, XentNodeId node);

#endif
