/**
 * @file flux_breadcrumb_data.h
 * @brief Shared runtime for the FluxBreadcrumbBar control (WinUI 3 BreadcrumbBar).
 */
#ifndef FLUX_BREADCRUMB_DATA_H
#define FLUX_BREADCRUMB_DATA_H

#include <stdbool.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore         FluxNodeStore;
typedef struct FluxWindow            FluxWindow;
typedef struct FluxThemeManager      FluxThemeManager;
typedef struct FluxTextRenderer      FluxTextRenderer;
typedef struct FluxInput             FluxInput;
typedef struct FluxMenuFlyout        FluxMenuFlyout;
typedef struct FluxBreadcrumbBarData FluxBreadcrumbBarData;

/* BreadcrumbBar metrics (BreadcrumbBar_themeresources.xaml / generic.xaml).
 * Crumb button padding 1,3 around a 20px line at 14px text -> 26px items;
 * separator chevron E974 (E973 in RTL) at 12px in a 2,0,2,0 cell; ellipsis
 * glyph E712 at 14px with 3px padding all sides. */
#define FLUX_BREADCRUMB_ITEM_H        26.0f
#define FLUX_BREADCRUMB_FONT          14.0f
#define FLUX_BREADCRUMB_PAD_H         1.0f
#define FLUX_BREADCRUMB_CHEVRON_FONT  12.0f
#define FLUX_BREADCRUMB_CHEVRON_PAD   2.0f
#define FLUX_BREADCRUMB_ELLIPSIS_PAD  3.0f
#define FLUX_BREADCRUMB_ELLIPSIS_FONT 14.0f
#define FLUX_BREADCRUMB_CHEVRON_LTR   L"\xE974" /* ChevronRightMed */
#define FLUX_BREADCRUMB_CHEVRON_RTL   L"\xE973" /* ChevronLeftMed  */
#define FLUX_BREADCRUMB_ELLIPSIS      L"\xE712" /* More            */

/**
 * @brief One rendered element of the bar: the synthetic ellipsis (elem 0) or a
 * crumb (elem i = data index i-1). Owned by its FLUX_CONTROL_BREADCRUMB_ITEM
 * node's component_data; points back at the bar for labels, collapse state,
 * and input routing (WinUI BreadcrumbIterator prepends one null element, so
 * element index 0 is ALWAYS the ellipsis).
 */
typedef struct FluxBreadcrumbItem {
	FluxBreadcrumbBarData *bar;
	XentNodeId             node; /**< FLUX_CONTROL_BREADCRUMB_ITEM node. */
	int                    elem; /**< Element index: 0 = ellipsis, i = crumb i-1. */
} FluxBreadcrumbItem;

/** @brief Stable per-row context for the ellipsis flyout (rebuilt each open). */
typedef struct FluxBreadcrumbFlyoutSlot {
	FluxBreadcrumbBarData *bar;
	int                    index; /**< Hidden crumb's data index. */
} FluxBreadcrumbFlyoutSlot;

/**
 * @brief BreadcrumbBar runtime, owned by the root FLUX_CONTROL_BREADCRUMB_BAR node.
 *
 * The bar is an ABSOLUTE row of count+1 item nodes placed by the
 * BreadcrumbLayout collapse algorithm, re-run from the engine's collect pass
 * whenever the assigned width or the labels change. The last crumb is the
 * non-clickable current location; overflowing leading crumbs collapse into
 * the ellipsis, whose flyout lists them reversed (closest-to-visible first).
 */
struct FluxBreadcrumbBarData {
	XentContext              *ctx;
	FluxNodeStore            *store;
	FluxWindow               *window; /**< Ellipsis flyout + repaint. */
	FluxTextRenderer         *text;   /**< Measures crumb labels for the collapse. */
	FluxThemeManager         *theme;  /**< Flyout palette. */
	FluxInput                *input;  /**< Arrow-key focus moves between crumbs. */
	XentNodeId                root;

	char const              **labels;    /**< Owned deep copies, count entries. */
	int                       count;

	XentNodeId               *nodes;     /**< count+1 element nodes ([0] = ellipsis). */
	float                    *widths;    /**< count+1 measured element widths. */
	FluxBreadcrumbFlyoutSlot *fly_slots; /**< count flyout-row contexts. */

	float                     chevron_cell_w; /**< 2 + E974 advance @12 + 2. */
	float                     natural_w;      /**< Sum of crumb widths (ellipsis excluded). */
	float                     last_avail;     /**< Width the collapse last ran against. */
	bool                      widths_dirty;   /**< Labels changed: re-measure. */
	bool                      arrange_dirty;  /**< Re-run the collapse even at the same width. */

	bool                      ellipsis_rendered; /**< Overflow: element 0 is visible. */
	int                       first_rendered;    /**< First rendered element index after the ellipsis. */
	bool                      disabled;

	FluxMenuFlyout           *flyout; /**< Owned; rebuilt from the hidden list on every open. */

	void                      (*on_item_clicked)(void *ctx, int index); /**< Data index (0-based). */
	void                     *on_item_clicked_ctx;
};

#ifdef __cplusplus
}
#endif

#endif
