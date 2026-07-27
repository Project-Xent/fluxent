/**
 * @file flux_menubar_data.h
 * @brief Shared runtime for the FluxMenuBar control (WinUI 3 MenuBar).
 */
#ifndef FLUX_MENU_BAR_DATA_H
#define FLUX_MENU_BAR_DATA_H

#include <xent/xent.h>
#include "store/flux_node_data.h"


typedef struct FluxStore       FluxStore;
typedef struct FluxWindow      FluxWindow;
typedef struct FluxTheme       FluxTheme;
typedef struct FluxText        FluxText;
typedef struct FluxMenu        FluxMenu;
typedef struct FluxMenuBarData FluxMenuBarData;

#define FLUX_MENU_BAR_MAX_ITEMS 16

/* Both MENUITEM implementations embed this first. A NULL label marks a
 * flyout-only data item, which has no scene visual. */
typedef struct FluxMenuItemVisual {
	char const *label;
	bool        open;
} FluxMenuItemVisual;

/**
 * @brief One top-level menu: its bar item node and the flyout it opens.
 * Stored inline in FluxMenuBarData so the item node's borrowed component_data
 * can point straight at it (and back at the bar via @ref bar) for snapshots and
 * input routing. The label is an owned copy of the caller's title.
 */
typedef struct FluxMenuBarItem {
	FluxMenuItemVisual visual;
	FluxMenuBarData   *bar;
	XentNodeId         node;
	FluxMenu          *flyout;
	int                index;
	bool               owns_flyout;
} FluxMenuBarItem;

/**
 * @brief MenuBar runtime, owned by the root FLUX_CONTROL_MENU_BAR node.
 *
 * The bar is a horizontal stack of FLUX_CONTROL_MENU_BAR_ITEM nodes. At most
 * one flyout is open at a time (@ref open_index, -1 when none); while one is
 * open, moving the pointer onto another item switches to it (WinUI behavior).
 */
struct FluxMenuBarData {
	XentCtx        *ctx;
	FluxStore      *store;
	FluxWindow     *window;
	FluxTheme      *theme;
	FluxText       *text;
	XentNodeId      root;
	XentNodeId      items_host;
	XentNodeId      wire_host;
	FluxMenuBarItem items [FLUX_MENU_BAR_MAX_ITEMS];
	int             count;
	int             open_index; /**< Item whose flyout is open, or -1. */
};


#endif
