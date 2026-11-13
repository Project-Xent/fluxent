#ifndef FLUX_CONTROL_BUILD_H
#define FLUX_CONTROL_BUILD_H

#include "fluxent/flux_control_type.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_node_store.h"

#include <xtk/xtk_batch.h>
#include <xent/xent.h>
#include <stdbool.h>

typedef struct FluxWindow FluxWindow;
typedef struct FluxText   FluxText;
typedef struct FluxTheme  FluxTheme;
typedef struct FluxInput  FluxInput;
typedef struct FluxApp    FluxApp;

typedef struct FluxControlEnv {
	FluxApp    *app;
	FluxWindow *window;
	FluxText   *text;
	FluxTheme  *theme;
	FluxInput  *input;
	XentNodeId  overlay;
} FluxControlEnv;

typedef struct FluxControlMount {
	FluxControlEnv const *env;
	XtkPropPatch const   *props;
	uint32_t              prop_count;
} FluxControlMount;

bool flux_control_build(
  XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlType type, FluxControlMount const *mount
);

bool flux_control_mount_consumed(FluxControlType type, XtkPropertyId property);
void flux_control_unmount(FluxStore *store, XentNodeId node, FluxControlType type);

#endif
