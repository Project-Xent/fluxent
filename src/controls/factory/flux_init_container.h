#ifndef FLUX_INIT_CONTAINER_H
#define FLUX_INIT_CONTAINER_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"
#include "controls/behavior/flux_refresh_internal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxContainerDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
} FluxContainerDesc;

XentNodeId flux_scroll_init(FluxContainerDesc const *info);

typedef struct FluxExpanderDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	char const *header;
	bool        expanded;
	float       width;
	float       content_height;
	void        (*on_toggle)(void *, bool);
	void       *userdata;
	XentNodeId *out_content;
	XentNodeId *out_header;
} FluxExpanderDesc;

XentNodeId flux_expander_init(FluxExpanderDesc const *info);

XentNodeId flux_expbody_node(FluxStore *store, XentNodeId id);

typedef struct FluxSplitViewDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	int        display_mode;
	int        pane_placement;
	bool       pane_open;
	float      open_pane_length;
	float      compact_pane_length;
} FluxSplitViewDesc;

XentNodeId flux_splitview_init(FluxSplitViewDesc const *info);

XentNodeId flux_splitbody_init(XentCtx *ctx, FluxStore *store, XentNodeId parent);

XentNodeId flux_splitpane_init(XentCtx *ctx, FluxStore *store, XentNodeId parent);

void       flux_splitview_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd);

void       flux_splitview_setopen(FluxStore *store, XentNodeId id, bool open);

void       flux_splitview_configure(
  FluxStore *store, XentNodeId id, int display_mode, int placement, float open_len, float compact_len
);

typedef struct FluxRefreshContainerDesc {
	XentCtx          *ctx;
	FluxStore        *store;
	XentNodeId        parent;
	FluxWindow       *window;
	FluxPullDirection direction;
	float             visualizer_size;
	float             threshold_ratio;
	void              (*on_refresh)(void *ud, int direction);
	void             *userdata;
} FluxRefreshContainerDesc;

XentNodeId flux_refresh_init(FluxRefreshContainerDesc const *info);
XentNodeId flux_refresh_content_node(FluxStore *store, XentNodeId id);
void       flux_refresh_set_direction(FluxStore *store, XentNodeId id, FluxPullDirection dir);
void       flux_refresh_set_visualizer_size(FluxStore *store, XentNodeId id, float size);
void       flux_refresh_set_threshold_ratio(FluxStore *store, XentNodeId id, float ratio);
void       flux_refresh_set_refreshing(FluxStore *store, XentNodeId id, bool refreshing);

bool flux_refresh_touch_pull(FluxStore *store, XentNodeId id, float new_sx, float new_sy, float max_x, float max_y);

void flux_refresh_touch_release(FluxStore *store, XentNodeId id);

void flux_refresh_touch_cancel(FluxStore *store, XentNodeId id);

#endif
