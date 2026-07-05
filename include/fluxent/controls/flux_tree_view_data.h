/**
 * @file flux_tree_view_data.h
 * @brief Data structures for the TreeView control (WinUI 3 TreeView).
 *
 * WinUI's TreeView is not a nested-panel tree: it is a flat, observable list
 * of *visible* nodes (the ViewModel) fed to a plain virtualized ListView
 * (TreeViewList); hierarchy is drawn purely with per-row indentation.
 * Fluxent mirrors that shape exactly:
 *
 *   root (FLUX_CONTROL_TREE_VIEW; owns the tree model + the flat list)
 *   └── FLUX_CONTROL_LIST spine (flux_create_list_view: scroll + ABSOLUTE host)
 *       └── FLUX_CONTROL_TREE_ITEM × realized window (recycled leaf rows)
 *
 * The control owns the retained node pool and the flattened array
 * (@ref FluxTreeViewData.flat — a node is present iff every ancestor is
 * expanded; order is the pre-order DFS of the expanded tree, matching
 * ViewModel.cpp). The inner LIST spine supplies the scroll viewport, the
 * host extent (flat_count × row pitch), and the per-frame staleness watch
 * (flux_list_view_update_window fires on_stale when scrolling leaves the
 * realized window); the TreeView answers on_stale by re-realizing its own
 * recycled TREE_ITEM rows — reconciler involvement ends at the root element.
 *
 * Selection follows WinUI TreeView semantics: Single is one retained node
 * handle; Multiple is the tri-state cascade (setting a node Selected /
 * UnSelected cascades to all descendants, then ancestors re-derive
 * bottom-up: any Partial child or a Selected+UnSelected mix → Partial).
 * Collapsing never changes selection.
 */
#ifndef FLUX_TREE_VIEW_DATA_H
#define FLUX_TREE_VIEW_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <xent/xent.h>
#include <xtk/xtk.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxInput  FluxInput;
typedef struct FluxWindow FluxWindow;

/* Row metrics (TreeView_themeresources.xaml / TreeViewItem.cpp), shared by
 * the behavior's hit-testing and the renderer. All DIPs.
 *
 * Horizontal row layout (left→right, inside the 4px side margin):
 *   [indent = 16 × depth] [chevron zone 14+12+14] [content]
 * Multiple selection mode:
 *   [indent] [checkbox 10+32] [chevron 12+14] [content]                  */
#define FLUX_TREE_ROW_H           28.0f /**< TreeViewItemMinHeight. */
#define FLUX_TREE_ROW_MARGIN_H    4.0f  /**< TreeViewItemPresenterMargin sides. */
#define FLUX_TREE_ROW_MARGIN_V    2.0f  /**< TreeViewItemPresenterMargin top/bottom (pitch = 28 + 4 = 32). */
#define FLUX_TREE_INDENT          16.0f /**< TreeViewItem::UpdateIndentation per depth level. */
#define FLUX_TREE_CHEVRON_PAD     14.0f /**< Chevron hit padding each side of the glyph box. */
#define FLUX_TREE_CHEVRON_BOX     12.0f /**< Chevron glyph box (glyph cell 8×8 inside). */
#define FLUX_TREE_CHEVRON_ZONE    (FLUX_TREE_CHEVRON_PAD + FLUX_TREE_CHEVRON_BOX + FLUX_TREE_CHEVRON_PAD)
#define FLUX_TREE_CHEVRON_FONT    8.0f  /**< TreeViewItem GlyphSize. */
#define FLUX_TREE_CHECK_MARGIN_L  10.0f /**< Multi-select checkbox margin 10,0,0,0. */
#define FLUX_TREE_CHECK_W         32.0f /**< Multi-select checkbox slot width. */
#define FLUX_TREE_CHECK_BOX       20.0f /**< Drawn checkbox box (standard CheckBox). */
#define FLUX_TREE_MULTI_CHEVRON_X (FLUX_TREE_CHECK_MARGIN_L + FLUX_TREE_CHECK_W)
#define FLUX_TREE_CHEVRON_ZONE_MULTI (FLUX_TREE_CHEVRON_BOX + FLUX_TREE_CHEVRON_PAD) /**< Padding 0,0,14,0. */
#define FLUX_TREE_PILL_W          3.0f  /**< Selection indicator (Single mode). */
#define FLUX_TREE_PILL_H          16.0f
#define FLUX_TREE_PILL_R          1.5f
#define FLUX_TREE_FONT            14.0f /**< Body text (default item template). */
#define FLUX_TREE_ICON_GLYPH      16.0f /**< Optional leading icon glyph size. */
#define FLUX_TREE_ICON_ADVANCE    24.0f /**< Icon cell + gap before the label. */
#define FLUX_TREE_ENTRANCE_MS     300.0f /**< Expand row entrance (EntranceThemeTransition feel). */
#define FLUX_TREE_ENTRANCE_DY     40.0f  /**< Entrance vertical offset (fade in + slide up). */

/** @brief Tri-state selection (WinUI TreeViewNode::SelectionState). */
enum
{
	FLUX_TREE_SEL_NONE     = 0, /**< UnSelected. */
	FLUX_TREE_SEL_SELECTED = 1,
	FLUX_TREE_SEL_PARTIAL  = 2, /**< PartialSelected (some descendants selected). */
};

/** @brief FluxTreeItemSnapshot.flags bits. */
enum
{
	FLUX_TREE_ROW_HAS_CHILDREN = 1 << 0, /**< Show the expand chevron. */
	FLUX_TREE_ROW_EXPANDED     = 1 << 1, /**< Chevron E70D (down) instead of E76C (right). */
	FLUX_TREE_ROW_MULTI        = 1 << 2, /**< Multiple mode: checkbox + content shift, no pill. */
	FLUX_TREE_ROW_DISABLED     = 1 << 3, /**< Per-node disable (Disabled brushes). */
};

/**
 * @brief One retained tree node (pool-allocated; handles are pool indices).
 *
 * Children form an intrusive singly-linked chain; free slots are chained
 * through @ref next_sibling. depth is maintained eagerly at insertion
 * (parent.depth + 1; roots are 0 — the hidden origin of WinUI is implicit).
 */
typedef struct FluxTreeNode {
	int         parent;       /**< Parent handle, or -1 for roots. */
	int         first_child;  /**< First child handle, or -1 (leaf). */
	int         next_sibling; /**< Next sibling handle, or -1; free-list link when !in_use. */
	char const *text;         /**< Owned copy of the row label. */
	char const *icon_name;    /**< Owned copy of the icon name, or NULL. */
	uint32_t    glyph;        /**< Resolved Segoe Fluent Icons codepoint (0 = none). */
	int16_t     depth;        /**< 0 for roots; rows indent 16px per level. */
	uint8_t     sel_state;    /**< FLUX_TREE_SEL_* (Multiple mode tri-state). */
	bool        expanded;     /**< Descendants visible (default false, WinUI). */
	bool        disabled;
	bool        in_use;       /**< Slot is allocated (false = on the free list). */
} FluxTreeNode;

/** @brief TreeView runtime, owned by the root FLUX_CONTROL_TREE_VIEW node. */
typedef struct FluxTreeViewData {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxWindow    *window;      /**< Repaint + entrance animation (may be NULL headless). */
	FluxInput     *input;       /**< Keyboard focus moves (may be NULL). */

	XentNodeId     root;        /**< FLUX_CONTROL_TREE_VIEW root. */
	XentNodeId     list;        /**< Inner FLUX_CONTROL_LIST spine (viewport + extent + stale watch). */
	XentNodeId     host;        /**< The list's ABSOLUTE items host (rows live here). */

	FluxTreeNode  *nodes;       /**< Node pool (owned). */
	int            node_count;  /**< Pool high-water mark. */
	int            node_cap;
	int            free_head;   /**< Free-list head handle, or -1. */
	int            first_root;  /**< First root handle, or -1. */

	int           *flat;        /**< flat[i] = visible node handle (THE ViewModel; owned). */
	int            flat_count;
	int            flat_cap;

	XtkTreeSelMode sel_mode;
	int            selected;    /**< Single mode: selected node handle, or -1. */
	float          row_height;  /**< Row height (28); pitch adds the 2+2 margins. */

	XentNodeId    *rows;        /**< Recycled TREE_ITEM nodes for the realized window (owned array). */
	int            row_count;   /**< Rows currently created. */
	int            row_cap;
	int            win_first;   /**< First realized flat index. */
	int            win_count;

	int            focused_flat; /**< Flat index carrying keyboard focus, or -1. */

	/* Expand entrance: new rows fade in + slide up (collapse is instant). */
	int            anim_first;  /**< First entering flat index. */
	int            anim_count;  /**< Entering row count (0 = idle). */
	DWORD          anim_start;  /**< GetTickCount at expand. */
	bool           realize_pending; /**< Stale watch fired mid-paint; realize on the next tick. */

	void           (*on_invoke)(void *ctx, int flat_index);
	void           (*on_expand)(void *ctx, int flat_index, bool expanded);
	void           (*on_select)(void *ctx, int flat_index, bool selected);
	void          *userdata;
} FluxTreeViewData;

/** @brief Retained state for one recycled FLUX_CONTROL_TREE_ITEM row. */
typedef struct FluxTreeItemData {
	FluxTreeViewData *owner;             /**< Owning TreeView (never NULL after create). */
	int               flat_index;        /**< Flat row this node currently presents, or -1. */
	bool              press_on_chevron;  /**< The active press landed on the chevron zone. */
	bool              press_on_checkbox; /**< The active press landed on the checkbox zone. */
} FluxTreeItemData;

#ifdef __cplusplus
}
#endif

#endif
