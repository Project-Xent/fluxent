/**
 * @file flux_menu_bar_data.h
 * @brief Shared runtime for the FluxMenuBar control (WinUI 3 MenuBar).
 */
#ifndef FLUX_MENU_BAR_DATA_H
#define FLUX_MENU_BAR_DATA_H

#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore    FluxNodeStore;
typedef struct FluxWindow       FluxWindow;
typedef struct FluxThemeManager FluxThemeManager;
typedef struct FluxTextRenderer FluxTextRenderer;
typedef struct FluxMenuFlyout   FluxMenuFlyout;
typedef struct FluxMenuBarData  FluxMenuBarData;

#define FLUX_MENU_BAR_MAX_ITEMS 16

/**
 * @brief One top-level menu: its bar item node and the flyout it opens.
 * Stored inline in FluxMenuBarData so the item node's borrowed component_data
 * can point straight at it (and back at the bar via @ref bar) for snapshots and
 * input routing. The label is an owned copy of the caller's title.
 */
typedef struct FluxMenuBarItem {
	FluxMenuBarData *bar;
	XentNodeId       node;   /**< FLUX_CONTROL_MENU_BAR_ITEM node. */
	char const      *label;  /**< Owned copy of the title text. */
	FluxMenuFlyout  *flyout; /**< Owned drop-down menu (caller populates it). */
	int              index;
} FluxMenuBarItem;

/**
 * @brief MenuBar runtime, owned by the root FLUX_CONTROL_MENU_BAR node.
 *
 * The bar is a horizontal stack of FLUX_CONTROL_MENU_BAR_ITEM nodes. At most
 * one flyout is open at a time (@ref open_index, -1 when none); while one is
 * open, moving the pointer onto another item switches to it (WinUI behavior).
 */
struct FluxMenuBarData {
	XentContext      *ctx;
	FluxNodeStore    *store;
	FluxWindow       *window;
	FluxThemeManager *theme;
	FluxTextRenderer *text;
	XentNodeId        root;
	FluxMenuBarItem   items [FLUX_MENU_BAR_MAX_ITEMS];
	int               count;
	int               open_index; /**< Item whose flyout is open, or -1. */
};

#ifdef __cplusplus
}
#endif

#endif
