/**
 * @file flux_items_view_data.h
 * @brief Retained state for the ItemsView items-host root (WinUI 1.5
 * ItemsView = ScrollView -> ItemsRepeater(+Layout) + SelectionModel).
 *
 * ItemsView is a thin configuration layer over the existing virtualized
 * items-host spine (see flux_list_view_data.h). It reuses that machinery
 * wholesale — the same root -> SCROLL -> ABSOLUTE host, the same recycled
 * FLUX_CONTROL_LIST_ITEM cells, the same reconciler windowing, and the same
 * WinUI selection state machine (None/Single/Multiple/Extended, anchors,
 * Ctrl/Shift ranges). To make that reuse literal, FluxItemsViewData embeds
 * a FluxListViewData as its FIRST member: a FluxItemsViewData* aliases a
 * FluxListViewData* at offset 0, so every flux_list_view_* accessor and the
 * shared LIST_ITEM behavior/snapshot operate on it unchanged once the type
 * checks admit FLUX_CONTROL_ITEMS_VIEW.
 *
 * What ItemsView adds on top of the list spine:
 *   - the ItemContainer chrome (XTK_LIST_KIND_ITEMS): a 3px accent border +
 *     1px inner solid ring instead of the classic ListViewItem pill;
 *   - ItemInvoked (raised on release/Enter when SelectionMode == None), wired
 *     through FluxListViewData::on_invoke so the shared cell behavior fires it;
 *   - a layout mode that selects the spine's column configuration at build.
 */
#ifndef FLUX_ITEMS_VIEW_DATA_H
#define FLUX_ITEMS_VIEW_DATA_H

#include "flux_list_view_data.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief WinUI ItemsView layout mode. Maps onto the shared items-host spine:
 * STACK_V is a single-column list; the grid/flow modes carry item_width +
 * columns so the reconciler wraps indices (xtk_list_auto_columns).
 *
 * (XtkItemsViewLayout is declared in <xtk/xtk.h>, pulled in via
 * flux_list_view_data.h, so the xtk builder and the backend agree on it.)
 */

/** @brief Retained state for a FLUX_CONTROL_ITEMS_VIEW root. */
typedef struct FluxItemsViewData {
	FluxListViewData   list;         /**< MUST be first: the reused items-host spine (offset-0 aliasing). */
	XtkItemsViewLayout layout;       /**< Resolved layout mode (informational; windowing uses list.cols/item_width). */
	float              item_spacing; /**< Requested inter-cell gap (DIP); folded into the cell pitch at build. */
} FluxItemsViewData;

#ifdef __cplusplus
}
#endif

#endif
