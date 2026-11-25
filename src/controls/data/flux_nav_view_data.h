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
} FluxNavTween;

typedef struct FluxStore       FluxStore;
typedef struct FluxPopup       FluxPopup;
typedef struct FluxInput       FluxInput;
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
	FLUX_NAV_PANE_TOP          = 2,
	FLUX_NAV_PANE_LEFT_COMPACT = 3,
	FLUX_NAV_PANE_LEFT_MINIMAL = 4,
} FluxNavPaneDisplayMode;

typedef enum FluxNavItemKind
{
	FLUX_NAV_ITEM_TOGGLE    = 0,
	FLUX_NAV_ITEM_MENU      = 1,
	FLUX_NAV_ITEM_FOOTER    = 2,
	FLUX_NAV_ITEM_SEPARATOR = 3,
	FLUX_NAV_ITEM_HEADER    = 4,
	FLUX_NAV_ITEM_MORE      = 5,
	FLUX_NAV_ITEM_SETTINGS  = 6,
} FluxNavItemKind;

enum
{
	FLUX_NAV_SETTINGS = -2,
};

typedef enum FluxNavItemPlace
{
	FLUX_NAV_PLACE_LEFT     = 0,
	FLUX_NAV_PLACE_TOP      = 1,
	FLUX_NAV_PLACE_OVERFLOW = 2,
} FluxNavItemPlace;

enum
{
	FLUX_NAV_FLY_NONE     = -1,
	FLUX_NAV_FLY_OVERFLOW = -2,
};

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
	bool             in_overflow;
	bool             footer;
	bool             restore_expanded;
	uint8_t          place;
	float            top_w;
	XentNodeId       children_host;
	FluxNavTween     expand_t;
} FluxNavViewItem;

typedef struct FluxNavIndicator {
	XentPoint position;
	int       item;
	uint8_t   place;
	bool      in_flyout;
} FluxNavIndicator;

typedef struct FluxNavIndicatorFrame {
	float scale;
	float offset;
	float opacity;
	bool  horizontal;
	bool  in_flyout;
} FluxNavIndicatorFrame;

struct FluxNavViewData {
	XentCtx               *ctx;
	FluxStore             *store;
	FluxWindow            *window;
	FluxText              *text;

	XentNodeId             root;
	XentNodeId             pane_clip;
	XentNodeId             pane;
	XentNodeId             toggle;
	XentNodeId             back;
	XentNodeId             close;
	uint32_t               back_button;
	bool                   always_show_header;
	XentNodeId             items_scroll;
	XentNodeId             items_host;
	XentNodeId             footer_scroll;
	XentNodeId             footer_host;
	XentNodeId             pane_separator;
	XentNodeId             content;
	XentNodeId             content_grid;
	XentNodeId             page_header;
	XentNodeId             content_overlay;
	float                  content_overlay_h;
	XentNodeId             scrim;
	XentNodeId             title_bar;
	char                  *app_title;
	bool                   window_title_bar;
	bool                   tall_title_bar;

	struct FluxScrollData *items_scroll_data;

	FluxNavViewItem        toggle_item;
	FluxNavViewItem        items [FLUX_NAV_VIEW_MAX_ITEMS];
	int                    count;
	int                    selected;
	uint64_t               selection_revision;
	FluxNavIndicator       indicator;
	FluxNavIndicator       outgoing;

	FluxNavDisplayMode     mode;
	int                    pane_mode;
	FluxInput             *input;
	FluxPopup             *fly_popup;
	XentNodeId             fly_root;
	XentNodeId             fly_scroll;
	int                    fly_owner;
	XentNodeId             more;
	FluxNavViewItem        more_item;
	XentNodeId             top_grid;
	XentNodeId             top_measure_row;
	XentNodeId             top_measure_icon;
	XentNodeId             top_measure_label;
	XentNodeId             top_measure_chevron;
	XentNodeId             top_spacer;
	XentNodeId             overflow_host;
	bool                   top_more;
	float                  top_more_width;
	FluxTheme             *theme;
	bool                   adaptive;
	bool                   pane_open;
	bool                   pane_force_closed;
	FluxSize               pane_viewport;
	XentNodeId             pane_previous_focus;
	bool                   pane_previous_suppressed;

	float                  width;
	float                  height;

	FluxNavTween           pane_progress;
	FluxNavTween           ind_progress;
	float                  ind_travel;
	bool                   anim_active;
	bool                   ind_same_level;
	bool                   ind_down;
	bool                   ind_layout_pending;
	bool                   labels_visible;

	void                   (*on_selection_changed)(void *ctx, int index);
	void                  *on_sel_ctx;

	XentNodeId             settings;
	FluxNavViewItem        settings_item;
	FluxNavViewItem        search_item;
	bool                   settings_visible;

	XentNodeId             chrome_column;
	XentNodeId             chrome_header;
	XentNodeId             chrome_title;
	XentNodeId             chrome_custom;
	XentNodeId             chrome_search;
	XentNodeId             chrome_search_area;
	XentNodeId             chrome_footer;
	XentNodeId             chrome_search_button;
	char const            *pane_title;
	float                  chrome_header_w;
	float                  chrome_custom_w;
	float                  chrome_search_w;
	float                  chrome_footer_w;
	float                  footer_viewport_h;
	float                  layout_scale;
	uint8_t                chrome_laid;
};

#endif
