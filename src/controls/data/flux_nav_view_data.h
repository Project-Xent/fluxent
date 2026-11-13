#ifndef FLUX_NAV_VIEW_DATA_H
#define FLUX_NAV_VIEW_DATA_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>
#include "store/flux_node_data.h"

typedef struct FluxNavTween {
	float current;
	float start;
	float target;
	DWORD start_ms;
	float duration;
	bool  active;
	bool  lead;
} FluxNavTween;

typedef struct FluxStore       FluxStore;
typedef struct FluxMenu        FluxMenu;
typedef struct FluxTheme       FluxTheme;
typedef struct FluxWindow      FluxWindow;
typedef struct FluxText        FluxText;
typedef struct FluxNavViewData FluxNavViewData;
struct FluxScrollData;

#define FLUX_NAV_VIEW_MAX_ITEMS 64

typedef enum FluxNavDisplayMode
{
	FLUX_NAV_EXPANDED = 0,
	FLUX_NAV_COMPACT  = 1,
	FLUX_NAV_MINIMAL  = 2,
	FLUX_NAV_TOP      = 3,
} FluxNavDisplayMode;

typedef enum FluxNavPaneDisplayMode
{
	FLUX_NAV_PANE_AUTO         = 0,
	FLUX_NAV_PANE_LEFT         = 1,
	FLUX_NAV_PANE_LEFT_COMPACT = 2,
	FLUX_NAV_PANE_LEFT_MINIMAL = 3,
	FLUX_NAV_PANE_TOP          = 4,
} FluxNavPaneDisplayMode;

typedef enum FluxNavItemKind
{
	FLUX_NAV_ITEM_TOGGLE    = 0,
	FLUX_NAV_ITEM_MENU      = 1,
	FLUX_NAV_ITEM_FOOTER    = 2,
	FLUX_NAV_ITEM_SEPARATOR = 3,
	FLUX_NAV_ITEM_HEADER    = 4,
} FluxNavItemKind;

typedef struct FluxNavViewItem {
	FluxNavViewData *nav;
	XentNodeId       node;
	char const      *label;
	char const      *icon_name;
	FluxNavItemKind  kind;
	int              index;
	int              parent;
	int              depth;
	bool             has_children;
	bool             expanded;
	bool             press_on_chevron;
	XentNodeId       children_host;
	FluxNavTween     expand_t;
} FluxNavViewItem;

struct FluxNavViewData {
	XentCtx               *ctx;
	FluxStore             *store;
	FluxWindow            *window;
	FluxText              *text;

	XentNodeId             root;
	XentNodeId             pane;
	XentNodeId             toggle;
	XentNodeId             items_scroll;
	XentNodeId             items_host;
	XentNodeId             footer_host;
	XentNodeId             content;
	XentNodeId             scrim;
	XentNodeId             title_bar;
	char                  *app_title;
	bool                   window_title_bar;

	struct FluxScrollData *items_scroll_data;
	bool                   ind_in_scroll;

	FluxNavViewItem        toggle_item;
	FluxNavViewItem        items [FLUX_NAV_VIEW_MAX_ITEMS];
	int                    count;
	int                    selected;
	uint64_t               selection_revision;
	int                    ind_index;
	int                    ind_visible;

	FluxNavDisplayMode     mode;
	FluxMenu              *child_flyout;
	int                    flyout_parent;
	FluxTheme             *theme;
	bool                   adaptive;
	bool                   minimal_open;
	bool                   fill_window;

	float                  width;
	float                  max_width;
	float                  height;

	FluxNavTween           pane_w;
	FluxNavTween           minimal_t;
	FluxNavTween           ind_top;
	FluxNavTween           ind_bot;
	FluxNavTween           ind_op;
	bool                   anim_active;
	bool                   ind_primed;
	bool                   labels_visible;

	void                   (*on_selection_changed)(void *ctx, int index);
	void                  *on_sel_ctx;
};

#endif
