#ifndef FLUX_TREE_INTERNAL_H
#define FLUX_TREE_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <xent/xent.h>
#include <xtk/xtk.h>

typedef struct FluxStore  FluxStore;
typedef struct FluxInput  FluxInput;
typedef struct FluxWindow FluxWindow;

#define FLUX_TREE_ROW_H              32.0f
#define FLUX_TREE_ROW_MARGIN_H       4.0f
#define FLUX_TREE_ROW_MARGIN_V       2.0f
#define FLUX_TREE_CONTENT_PAD_T      3.0f
#define FLUX_TREE_CONTENT_PAD_B      5.0f
#define FLUX_TREE_INDENT             16.0f
#define FLUX_TREE_CHEVRON_PAD        14.0f
#define FLUX_TREE_CHEVRON_BOX        12.0f
#define FLUX_TREE_CHEVRON_ZONE       (FLUX_TREE_CHEVRON_PAD + FLUX_TREE_CHEVRON_BOX + FLUX_TREE_CHEVRON_PAD)
#define FLUX_TREE_CHEVRON_FONT       8.0f
#define FLUX_TREE_CHECK_MARGIN_L     10.0f
#define FLUX_TREE_CHECK_W            32.0f
#define FLUX_TREE_MULTI_CHEVRON_X    (FLUX_TREE_CHECK_MARGIN_L + FLUX_TREE_CHECK_W)
#define FLUX_TREE_CHEVRON_ZONE_MULTI (FLUX_TREE_CHEVRON_BOX + FLUX_TREE_CHEVRON_PAD)
#define FLUX_TREE_PILL_W             3.0f
#define FLUX_TREE_PILL_H             16.0f
#define FLUX_TREE_PILL_R             1.5f
#define FLUX_TREE_FONT               14.0f
#define FLUX_TREE_ICON_GLYPH         16.0f
#define FLUX_TREE_ICON_ADVANCE       24.0f
#define FLUX_TREE_ENTRANCE_MS        300.0f
#define FLUX_TREE_ENTRANCE_DY        40.0f

enum
{
	FLUX_TREE_SEL_NONE     = 0,
	FLUX_TREE_SEL_SELECTED = 1,
	FLUX_TREE_SEL_PARTIAL  = 2,
};

enum
{
	FLUX_TREE_ROW_HAS_CHILDREN = 1 << 0,
	FLUX_TREE_ROW_EXPANDED     = 1 << 1,
	FLUX_TREE_ROW_MULTI        = 1 << 2,
	FLUX_TREE_ROW_DISABLED     = 1 << 3,
};

typedef struct FluxTreeNode {
	int         parent;
	int         first_child;
	int         next_sibling;
	char const *text;
	char const *icon_name;
	uint32_t    glyph;
	int16_t     depth;
	uint8_t     sel_state;
	bool        expanded;
	bool        disabled;
	bool        in_use;
} FluxTreeNode;

typedef struct FluxTreeViewData {
	XentCtx       *ctx;
	FluxStore     *store;
	FluxWindow    *window;
	FluxInput     *input;

	XentNodeId     root;
	XentNodeId     list;
	XentNodeId     host;

	FluxTreeNode  *nodes;
	int            node_count;
	int            node_cap;
	int            free_head;
	int            first_root;

	int           *flat;
	int            flat_count;
	int            flat_cap;

	XtkTreeSelMode sel_mode;
	int            selected;
	float          row_height;

	XentNodeId    *rows;
	int            row_count;
	int            row_cap;
	int            win_first;
	int            win_count;

	int            focused_flat;

	int            anim_first;
	int            anim_count;
	DWORD          anim_start;
	bool           realize_pending;

	void           (*on_invoke)(void *ctx, int flat_index);
	void          *invoke_ctx;
	void           (*on_expand)(void *ctx, int flat_index, bool expanded);
	void          *expand_ctx;
	void           (*on_select)(void *ctx, int flat_index, bool selected);
	void          *select_ctx;
} FluxTreeViewData;

typedef struct FluxTreeItemData {
	FluxTreeViewData *owner;
	int               flat_index;
	bool              press_on_chevron;
	bool              press_on_checkbox;
} FluxTreeItemData;

#endif
