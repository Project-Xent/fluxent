/**
 * @file flux_init_container.h
 * @brief Private Desc + init prototypes for container/surface controls:
 * Card / Scroll / Divider / Expander / SplitView / RefreshContainer.
 */
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

/** @brief Create a card node. */
XentNodeId flux_card_init(FluxContainerDesc const *info);

/**
 * @brief Create a scroll viewport (flex column). The engine derives the
 * scrollable content extent from the laid-out children each frame; write
 * FluxScrollData.content_manual to take over the extent by hand.
 */
XentNodeId flux_scroll_init(FluxContainerDesc const *info);

/** @brief Create a divider node. */
XentNodeId flux_divider_init(FluxContainerDesc const *info);

/* -------------------------------------------------------------------------
 * Expander
 * ---------------------------------------------------------------------- */

typedef struct FluxExpanderDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;         /**< For repaint while the content slides; may be NULL. */
	char const *header;         /**< Optional convenience label placed in the header. */
	bool        expanded;
	float       width;          /**< Explicit control width in DIPs; 0 = auto. */
	float       content_height; /**< Explicit content height; 0 = content-sized. */
	void        (*on_toggle)(void *, bool);
	void       *userdata;
	XentNodeId *out_content; /**< Optional: receives the content node to fill. */
	XentNodeId *out_header;  /**< Optional: receives the header node for arbitrary content. */
} FluxExpanderDesc;

/** @brief Create an expander node; fills out_content with the child to populate. */
XentNodeId flux_expander_init(FluxExpanderDesc const *info);

/** @brief Get the expander's content node (the child the caller fills). */
XentNodeId flux_expbody_node(FluxStore *store, XentNodeId id);

/** @brief Get the expander's header node (host for arbitrary header content). */
XentNodeId flux_exphead_node(FluxStore *store, XentNodeId id);

/* -------------------------------------------------------------------------
 * SplitView (collapsible side pane beside main content)
 * ---------------------------------------------------------------------- */

typedef struct FluxSplitViewDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	int        display_mode;        /**< FluxSplitViewDisplayMode. */
	int        pane_placement;      /**< FluxSplitViewPanePlacement. */
	bool       pane_open;
	float      open_pane_length;    /**< 0 = default 320. */
	float      compact_pane_length; /**< 0 = default 48. */
} FluxSplitViewDesc;

/** @brief Create a SplitView root (ABSOLUTE host of the content + pane wrappers). */
XentNodeId flux_splitview_init(FluxSplitViewDesc const *info);

/** @brief Create the internal content wrapper (drawn under the pane). */
XentNodeId flux_splitbody_init(XentCtx *ctx, FluxStore *store, XentNodeId parent);

/** @brief Create the internal pane wrapper (drawn over content in overlay modes). */
XentNodeId flux_splitpane_init(XentCtx *ctx, FluxStore *store, XentNodeId parent);

/** @brief Engine per-frame hook: position the content + pane wrappers. */
void       flux_splitview_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd);

/** @brief Open or close the pane. */
void       flux_splitview_setopen(FluxStore *store, XentNodeId id, bool open);

/** @brief Set display mode / placement / pane lengths. */
void       flux_splitview_configure(
  FluxStore *store, XentNodeId id, int display_mode, int placement, float open_len, float compact_len
);

/* -------------------------------------------------------------------------
 * RefreshContainer (PullToRefresh)
 * ---------------------------------------------------------------------- */

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
void       flux_refresh_request_refresh(FluxStore *store, XentNodeId id);

/**
 * @brief Pointer over-pull hook: feed the pull past the scroll edge into the
 * visualizer. @p new_sx/@p new_sy are the requested (pre-clamp) scroll offsets
 * of the wrapped scroller and @p max_x/@p max_y its scrollable extents; the
 * amount beyond the pull edge drives interactionRatio. Called by the input
 * layer each pan frame while the scroller is at its pull edge.
 * @return true while an over-pull is in progress.
 */
bool flux_refresh_touch_pull(FluxStore *store, XentNodeId id, float new_sx, float new_sy, float max_x, float max_y);

/** @brief Pointer release hook: commit (refresh) or retract. */
void flux_refresh_touch_release(FluxStore *store, XentNodeId id);

/** @brief Cancel an active pointer pull without requesting a refresh. */
void flux_refresh_touch_cancel(FluxStore *store, XentNodeId id);

#endif
