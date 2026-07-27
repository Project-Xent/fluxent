/**
 * @file flux_init_items.h
 * @brief Private Desc + init prototypes for the virtualized items hosts:
 * ListView / ListBox / GridView / ItemsRepeater / TreeView / ItemsView /
 * FlipView.
 */
#ifndef FLUX_INIT_ITEMS_H
#define FLUX_INIT_ITEMS_H

#include "fluxent/fluxent.h"
#include <xtk/xtk_tree_model.h>
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxListViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	XtkListKind kind;                               /**< Chrome flavor (list / list box / grid / repeater). */
	FluxInput  *input;                              /**< Keyboard focus moves (may be NULL for headless). */
	void        (*on_select)(void *ctx, int index); /**< Lead index after every selection change. */
	void       *userdata;
} FluxListViewDesc;

typedef struct FluxListItemDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent; /**< The list's items host (flux_list_content_node). */
} FluxListItemDesc;

/** @brief Create a virtualized items-host spine (root + scroll + items host). */
XentNodeId flux_list_init(FluxListViewDesc const *info);

/** @brief Create one recycled cell node inside the items host (wires input + selection). */
XentNodeId flux_listitem_init(FluxListItemDesc const *info);

/** @brief The ABSOLUTE items host realized rows mount into. */
XentNodeId flux_list_content_node(FluxStore *store, XentNodeId list);

/** @brief Set count/cell metrics/columns; resizes the scrollable extent. */
void       flux_list_set_extent(
  FluxStore *store, XentNodeId list, int count, float item_height, float item_width, int columns
);

/** @brief WinUI SelectionMode (Single default). Switching modes clears the retained set. */
void flux_list_setmode(FluxStore *store, XentNodeId list, XtkListSelMode mode);

/** @brief Single mode: app-controlled selected index (-1 = none). */
void flux_list_set_selected(FluxStore *store, XentNodeId list, int selected);

/** @brief Multiple/Extended: query the control-retained selection set. */
bool flux_list_is_selected(FluxStore *store, XentNodeId list, int index);

/** @brief Multiple/Extended: number of selected items. */
int  flux_list_selection_count(FluxStore *store, XentNodeId list);

/** @brief Record the realized window + column count (reconciler-reported). */
void flux_list_set_realized(FluxStore *store, XentNodeId list, int first, int last, int cols);

/** @brief Stage a realized window until Host children match (pre-commit). */
void flux_list_stage_realized(FluxStore *store, XentNodeId list, int first, int last, int cols);

/** @brief Apply staged/fallback realized window and place recycled cells. */
void flux_list_apply_pending(FluxStore *store, XentNodeId list);

/** @brief Scroll offset + viewport extent + cross width in DIPs; false while layout hasn't run. */
bool flux_list_viewport(FluxStore *store, XentNodeId list, float *offset, float *extent, float *cross);

/** @brief Wire the fired-once-per-window staleness callback (re-realize hook). */
void flux_list_onstale(FluxStore *store, XentNodeId list, void (*on_stale)(void *), void *ctx);

/** @brief True when the viewport (or column count) has left the realized window. */
bool flux_list_window_stale(XentCtx *ctx, struct FluxNodeData *nd);

/** @brief Deliver one deferred staleness notification after the current frame. */
void flux_list_notify_stale(FluxStore *store, XentNodeId list);

/** @brief Minimal-edge scroll to bring an index into view (WinUI Default alignment). */
void flux_list_scrollto(FluxStore *store, XentNodeId list, int index);

/** @brief Bind item→list owner after Host place (seed create has no parent). */
void fluxlistitem_bind_owner(FluxStore *store, XentNodeId item);

/** @brief Update a recycled cell: index + content-space slot placement. */
void fluxlistitem_set_place(FluxStore *store, XentNodeId item, int index, float x, float y);

/** @brief Single-mode selected visual + Multiple-mode checkbox flag. */
void fluxlistitem_set_state(FluxStore *store, XentNodeId item, bool selected, bool multi);

/* -------------------------------------------------------------------------
 * TreeView (flattened-tree ViewModel over the virtualized items host)
 * ---------------------------------------------------------------------- */

typedef struct FluxTreeViewDesc {
	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     parent;
	FluxWindow    *window;
	FluxInput     *input;
	XtkTreeSelMode selection_mode;
	float          row_height;
	void           (*on_invoke)(void *ctx, int flat_index);
	void          *invoke_ctx;
	void           (*on_expand)(void *ctx, int flat_index, bool expanded);
	void          *expand_ctx;
	void           (*on_select)(void *ctx, int flat_index, bool selected);
	void          *select_ctx;
} FluxTreeViewDesc;

XentNodeId flux_tree_init(FluxTreeViewDesc const *info);
int        flux_tree_add_node(FluxStore *store, XentNodeId tree, int parent, char const *text, char const *icon);
bool       flux_tree_set_model(FluxStore *store, XentNodeId tree, XtkTreeModelView const *model);
void       flux_tree_remove_node(FluxStore *store, XentNodeId tree, int node);
void       flux_tree_clear(FluxStore *store, XentNodeId tree);
void       flux_tree_set_expanded(FluxStore *store, XentNodeId tree, int node, bool expanded);
void       flux_tree_setdisabled(FluxStore *store, XentNodeId tree, int node, bool disabled);
void       flux_tree_setmode(FluxStore *store, XentNodeId tree, XtkTreeSelMode mode);
void       flux_tree_set_selected(FluxStore *store, XentNodeId tree, int node, bool selected);
void       flux_tree_select_all(FluxStore *store, XentNodeId tree);
bool       flux_tree_is_expanded(FluxStore *store, XentNodeId tree, int node);
bool       flux_tree_is_selected(FluxStore *store, XentNodeId tree, int node);
int        flux_tree_flat_count(FluxStore *store, XentNodeId tree);
int        flux_tree_flat_node(FluxStore *store, XentNodeId tree, int flat_index);

/* -------------------------------------------------------------------------
 * ItemsView (ItemsRepeater + Layout + SelectionModel; ItemContainer chrome)
 * ---------------------------------------------------------------------- */

typedef struct FluxItemsViewDesc {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         parent;
	XtkItemsViewLayout layout;
	XtkListSelMode     selection_mode;
	float              item_spacing;
	bool               item_invoked_enabled;
	FluxInput         *input;
	void               (*on_select)(void *ctx, int index);
	void               (*on_invoke)(void *ctx, int index);
	void              *userdata;
} FluxItemsViewDesc;

XentNodeId flux_itemsview_init(FluxItemsViewDesc const *info);
XentNodeId flux_itemsview_content_node(FluxStore *store, XentNodeId view);
void       flux_itemsview_set_extent(
  FluxStore *store, XentNodeId view, int count, float item_height, float item_width, int columns
);
void flux_itemsview_setmode(FluxStore *store, XentNodeId view, XtkListSelMode mode);
void flux_itemsview_set_selected(FluxStore *store, XentNodeId view, int index);
bool flux_itemsview_is_selected(FluxStore *store, XentNodeId view, int index);
int  flux_itemsview_selection_count(FluxStore *store, XentNodeId view);

/* -------------------------------------------------------------------------
 * FlipView
 * ---------------------------------------------------------------------- */

typedef struct FluxFlipViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window; /**< Repaint target for slide-animation frames. */
	bool        vertical;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxFlipViewDesc;

/** @brief Create a FlipView (viewport-sized pages, nav buttons, wheel + keys). */
XentNodeId flux_flip_init(FluxFlipViewDesc const *info);

/** @brief The ABSOLUTE pages host xtk children mount into. */
XentNodeId flux_flip_content_node(FluxStore *store, XentNodeId flip);

/** @brief Select a page: adjacent moves animate, longer jumps snap. */
void       flux_flip_select(FluxStore *store, XentNodeId flip, int index);
void       flux_flip_set_vertical(FluxStore *store, XentNodeId flip, bool vertical);

/** @brief Engine per-frame hook: size/lay pages to the viewport + advance the slide. */
void       flux_flip_sync(XentCtx *ctx, XentNodeId flip, struct FluxNodeData *nd);

/** @brief Input hook: wheel over a FlipView flips (200 ms distinct-scroll gate).
 * Returns true when consumed (edges chain to outer scrolls, per WinUI). */
bool       flux_flip_handle_wheel(FluxStore *store, XentNodeId flip, float wheel_y);

#endif
