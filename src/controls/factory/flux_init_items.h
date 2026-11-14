#ifndef FLUX_INIT_ITEMS_H
#define FLUX_INIT_ITEMS_H

#include "fluxent/fluxent.h"
#include <xtk/xtk_treemodel.h>
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxListViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	XtkListKind kind;
	FluxInput  *input;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
	bool        realize_events;
} FluxListViewDesc;

typedef struct FluxListItemDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
} FluxListItemDesc;

XentNodeId flux_list_init(FluxListViewDesc const *info);

XentNodeId flux_listitem_init(FluxListItemDesc const *info);

XentNodeId flux_list_content_node(FluxStore *store, XentNodeId list);

void       flux_list_set_extent(
  FluxStore *store, XentNodeId list, int count, float item_height, float item_width, int columns
);

void flux_list_setmode(FluxStore *store, XentNodeId list, XtkListSelMode mode);

void flux_list_set_selected(FluxStore *store, XentNodeId list, int selected);

bool flux_list_is_selected(FluxStore *store, XentNodeId list, int index);

void flux_list_set_realized(FluxStore *store, XentNodeId list, int first, int last, int cols);

void flux_list_stage_first(FluxStore *store, XentNodeId list, int first);

void flux_list_emitrealize(struct FluxListViewData *ld);

void flux_list_onstale(FluxStore *store, XentNodeId list, void (*on_stale)(void *), void *ctx);

void flux_list_apply_pending(FluxStore *store, XentNodeId list);

bool flux_list_viewport(FluxStore *store, XentNodeId list, float *offset, float *extent, float *cross);


bool flux_list_window_stale(XentCtx *ctx, struct FluxNodeData *nd);

void flux_list_notify_stale(FluxStore *store, XentNodeId list);

void flux_list_scrollto(FluxStore *store, XentNodeId list, int index);

void fluxlistitem_bind_owner(FluxStore *store, XentNodeId item);

void fluxlistitem_set_place(FluxStore *store, XentNodeId item, int index, float x, float y);

void fluxlistitem_set_state(FluxStore *store, XentNodeId item, bool selected, bool multi);

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
void       flux_tree_set_expanded(FluxStore *store, XentNodeId tree, int node, bool expanded);
void       flux_tree_setmode(FluxStore *store, XentNodeId tree, XtkTreeSelMode mode);
void       flux_tree_set_selected(FluxStore *store, XentNodeId tree, int node, bool selected);
int        flux_tree_flat_node(FluxStore *store, XentNodeId tree, int flat_index);

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

typedef struct FluxFlipViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	bool        vertical;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxFlipViewDesc;

XentNodeId flux_flip_init(FluxFlipViewDesc const *info);

XentNodeId flux_flip_content_node(FluxStore *store, XentNodeId flip);

void       flux_flip_select(FluxStore *store, XentNodeId flip, int index);
void       flux_flip_set_vertical(FluxStore *store, XentNodeId flip, bool vertical);

void       flux_flip_sync(XentCtx *ctx, XentNodeId flip, struct FluxNodeData *nd);

bool       flux_flip_handle_wheel(FluxStore *store, XentNodeId flip, float wheel_y);

typedef struct FluxLoopingDesc {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         parent;
	FluxInput         *input;
	char const *const *items;
	int                item_count;
	int                selected;
	float              item_height;
	float              item_width;
	bool               should_loop;
	bool               accent;
	bool               left;
	void               (*on_select)(void *ctx, int index);
	void              *userdata;
} FluxLoopingDesc;

typedef struct FluxLoopingItemDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
} FluxLoopingItemDesc;

XentNodeId flux_looping_init(FluxLoopingDesc const *info);

XentNodeId flux_looping_item_init(FluxLoopingItemDesc const *info);

void       flux_looping_item_place(
  FluxStore *store, XentNodeId item, int visual, float y, float width, bool selected, float band_y
);

bool flux_looping_handle_wheel(FluxStore *store, XentNodeId id, float delta);
bool flux_looping_sync(XentNodeId node, struct FluxNodeData *nd);

void flux_looping_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id);
void flux_looping_setitems(FluxStore *store, XentNodeId id, char const *const *items, int count);
void flux_looping_setselected(FluxStore *store, XentNodeId id, int index);
void flux_looping_setitemheight(FluxStore *store, XentNodeId id, float height);
void flux_looping_setitemwidth(FluxStore *store, XentNodeId id, float width);
void flux_looping_setloop(FluxStore *store, XentNodeId id, bool should_loop);
void flux_looping_setenabled(FluxStore *store, XentNodeId id, bool enabled);

#endif
