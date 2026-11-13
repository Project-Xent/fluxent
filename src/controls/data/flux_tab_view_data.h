#ifndef FLUX_TAB_VIEW_DATA_H
#define FLUX_TAB_VIEW_DATA_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>
#include "store/flux_node_data.h"

typedef struct FluxStore       FluxStore;
typedef struct FluxWindow      FluxWindow;
typedef struct FluxText        FluxText;
typedef struct FluxInput       FluxInput;
typedef struct FluxTabViewData FluxTabViewData;

#define FLUX_TAB_VIEW_MAX_TABS 32

typedef enum FluxTabKind
{
	FLUX_TAB_KIND_TAB        = 0,
	FLUX_TAB_KIND_ADD        = 1,
	FLUX_TAB_KIND_SCROLL_DEC = 2,
	FLUX_TAB_KIND_SCROLL_INC = 3,
	FLUX_TAB_KIND_CLOSE      = 4,
} FluxTabKind;

typedef enum FluxTabCloseOverlayMode
{
	FLUX_TAB_CLOSE_OVERLAY_AUTO     = 0,
	FLUX_TAB_CLOSE_OVERLAY_ON_HOVER = 1,
	FLUX_TAB_CLOSE_OVERLAY_ALWAYS   = 2,
} FluxTabCloseOverlayMode;

typedef enum FluxTabWidthMode
{
	FLUX_TAB_WIDTH_EQUAL           = 0,
	FLUX_TAB_WIDTH_SIZE_TO_CONTENT = 1,
	FLUX_TAB_WIDTH_COMPACT         = 2,
} FluxTabWidthMode;

typedef struct FluxTabViewItem {
	FluxTabViewData *tv;
	XentNodeId       tab_node;
	XentNodeId       close_node;
	XentNodeId       content;
	char const      *label;
	char const      *icon_name;
	FluxTabKind      kind;
	int              index;
	bool             closed;
	bool             closable;
	bool             closing;
	bool             defer_width_update;
	float            width;
	float            cur_w;
	float            disp_w;
	float            w_from;
	DWORD            w_start;
	bool             w_anim;
	float            slide_x0;
	DWORD            slide_start;
	bool             sliding;
	float            close_w0;
	DWORD            close_start;
} FluxTabViewItem;

struct FluxTabViewData {
	XentCtx                *ctx;
	FluxStore              *store;
	FluxWindow             *window;
	FluxText               *text;
	FluxInput              *input;

	XentNodeId              root;
	XentNodeId              strip;
	XentNodeId              tabs_host;
	XentNodeId              content;
	XentNodeId              add_node;
	XentNodeId              scroll_dec;
	XentNodeId              scroll_inc;

	FluxTabViewItem         add_item;
	FluxTabViewItem         scroll_items [2];
	FluxTabViewItem         tabs [FLUX_TAB_VIEW_MAX_TABS];
	FluxTabViewItem         close_items [FLUX_TAB_VIEW_MAX_TABS];
	int                     order [FLUX_TAB_VIEW_MAX_TABS];
	int                     count;
	int                     selected;

	FluxTabWidthMode        width_mode;
	FluxTabCloseOverlayMode close_overlay;
	bool                    can_reorder;
	bool                    add_visible;
	bool                    scroll_visible;
	bool                    width_update_pending;
	bool                    width_update_deferred;
	bool                    widths_valid;
	bool                    layout_sync_pending;
	float                   layout_w;
	XentNodeId              strip_header;
	XentNodeId              strip_footer;

	int                     drag_slot;
	bool                    drag_active;
	float                   drag_press_x;

	int                     scroll_held;
	DWORD                   scroll_next;
	float                   scroll_from;
	float                   scroll_target;
	DWORD                   scroll_start;
	bool                    scroll_anim;

	bool                    anim_active;

	void                    (*on_selection_changed)(void *ctx, int index);
	void                   *selection_ctx;
	bool                    (*on_close_requested)(void *ctx, int index);
	void                   *close_requested_ctx;
	void                    (*on_tab_closed)(void *ctx, int index);
	void                   *tab_closed_ctx;
	void                    (*on_add_tab)(void *ctx);
	void                   *add_tab_ctx;
};

#endif
