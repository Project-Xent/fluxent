/**
 * @file flux_list_view_data.h
 * @brief Data structures for the virtualized items-host family:
 * ListView, ListBox, GridView, and ItemsRepeater.
 *
 * Every flavor shares one retained spine:
 *
 *   root (flex column; control type LIST / LIST_BOX / GRID_VIEW / ITEMS_REPEATER)
 *   └── FLUX_CONTROL_SCROLL (viewport; full scroll machinery)
 *       └── items host (ABSOLUTE protocol; height = rows × item_height)
 *           └── FLUX_CONTROL_LIST_ITEM × realized window (recycled)
 *
 * Cells are uniform and virtualized by the xtk reconciler: only the window
 * intersecting the viewport exists as nodes. The engine watches the scroll
 * offset (and grid column count) each frame and fires on_stale when the
 * viewport leaves the realized window.
 *
 * Selection (WinUI ListViewBase semantics): Single mode is app-controlled
 * (desc.selected in, on_select out). Multiple/Extended selection sets are
 * control-retained as sorted, merged index ranges — the control applies the
 * Ctrl/Shift/anchor state machine and posts the lead index on every change.
 */
#ifndef FLUX_LIST_VIEW_DATA_H
#define FLUX_LIST_VIEW_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <xent/xent.h>
#include <xtk/xtk.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxInput FluxInput;

/** @brief One inclusive selected-index run. Kept sorted and non-adjacent. */
typedef struct FluxListSelRange {
	int first;
	int last;
} FluxListSelRange;

/** @brief Retained state for a virtualized items-host root. */
typedef struct FluxListViewData {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxInput     *input;          /**< Focus moves for keyboard navigation (may be NULL). */
	XentNodeId     root;
	XentNodeId     scroll;         /**< FLUX_CONTROL_SCROLL viewport child. */
	XentNodeId     host;           /**< ABSOLUTE items host inside the scroll. */

	XtkListKind    kind;           /**< Chrome flavor (list / list box / grid / repeater). */
	XtkListSelMode sel_mode;

	int            count;          /**< Total item count. */
	float          item_height;    /**< Uniform cell height in DIPs. */
	float          item_width;     /**< Grid cell width; 0 = full-width rows. */
	int            columns;        /**< Explicit grid columns; 0 = auto from width. */
	int            cols;           /**< Realized column count (≥1; reconciler-reported). */

	int            selected;       /**< Single mode: app-controlled selected index, or -1. */

	int            realized_first; /**< First realized index (0 when empty). */
	int            realized_last;  /**< Last realized index (-1 when empty). */
	bool           stale_fired;    /**< on_stale sent; cleared when a new window arrives. */

	int            focused;        /**< Item index carrying keyboard focus, or -1. */
	int            pending_focus;  /**< Focus this index once its node realizes, or -1. */
	int            anchor;         /**< Extended/Multiple range anchor (WinUI m_pAnchorIndex). */
	bool           anchor_set;
	bool           single_follows_focus; /**< WinUI SingleSelectionFollowsFocus (default true). */

	FluxListSelRange *ranges;      /**< Multiple/Extended selection set (owned). */
	int               range_count;
	int               range_cap;

	void (*on_select)(void *ctx, int index); /**< Lead-index report after every selection change. */
	void  *on_select_ctx;

	void (*on_stale)(void *ctx);   /**< Viewport left the realized window. */
	void  *on_stale_ctx;
} FluxListViewData;

/** @brief Retained state for one recycled FLUX_CONTROL_LIST_ITEM cell. */
typedef struct FluxListItemData {
	FluxListViewData *owner; /**< Owning items host (never NULL after create). */
	int               index; /**< Item index this node currently presents. */
	bool              selected; /**< Draw the selection visual (single mode mirror). */
	bool              multi;    /**< Multiple mode: draw the checkbox + content shift. */
} FluxListItemData;

#ifdef __cplusplus
}
#endif

#endif
