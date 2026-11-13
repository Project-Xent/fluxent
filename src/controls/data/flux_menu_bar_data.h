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

typedef struct FluxMenuItemVisual {
	char const *label;
	bool        open;
} FluxMenuItemVisual;

typedef struct FluxMenuBarItem {
	FluxMenuItemVisual visual;
	FluxMenuBarData   *bar;
	XentNodeId         node;
	FluxMenu          *flyout;
	int                index;
	bool               owns_flyout;
} FluxMenuBarItem;

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
	int             open_index;
};

#endif
