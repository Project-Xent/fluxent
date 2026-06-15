/**
 * @file flux_tab_view_data.h
 * @brief Shared runtime for the FluxTabView control (WinUI 3 TabView).
 */
#ifndef FLUX_TAB_VIEW_DATA_H
#define FLUX_TAB_VIEW_DATA_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore    FluxNodeStore;
typedef struct FluxWindow       FluxWindow;
typedef struct FluxTextRenderer FluxTextRenderer;
typedef struct FluxInput        FluxInput;
typedef struct FluxTabViewData  FluxTabViewData;

#define FLUX_TAB_VIEW_MAX_TABS 32

/** @brief Strip-item role. */
typedef enum FluxTabKind
{
	FLUX_TAB_KIND_TAB        = 0, /**< A closable tab. */
	FLUX_TAB_KIND_ADD        = 1, /**< The trailing "new tab" (+) button. */
	FLUX_TAB_KIND_SCROLL_DEC = 2, /**< Left overflow scroll repeat-button. */
	FLUX_TAB_KIND_SCROLL_INC = 3, /**< Right overflow scroll repeat-button. */
	FLUX_TAB_KIND_CLOSE      = 4, /**< A tab's close button (real child node, like
	                               * the template's CloseButton). */
} FluxTabKind;

/** @brief TabView.CloseButtonOverlayMode. */
typedef enum FluxTabCloseOverlayMode
{
	FLUX_TAB_CLOSE_OVERLAY_AUTO     = 0, /**< Framework default; currently maps to Always. */
	FLUX_TAB_CLOSE_OVERLAY_ON_HOVER = 1, /**< Visible on the selected or pointer-over tab. */
	FLUX_TAB_CLOSE_OVERLAY_ALWAYS   = 2, /**< Visible on every closable tab. */
} FluxTabCloseOverlayMode;

/** @brief TabView.TabWidthMode. */
typedef enum FluxTabWidthMode
{
	FLUX_TAB_WIDTH_EQUAL           = 0, /**< Tabs share the strip equally (WinUI default). */
	FLUX_TAB_WIDTH_SIZE_TO_CONTENT = 1, /**< Tabs take their natural header width. */
	FLUX_TAB_WIDTH_COMPACT         = 2, /**< Unselected tabs collapse to icon-only. */
} FluxTabWidthMode;

/**
 * @brief One tab: its strip node and the content subtree it owns. Stored inline
 * in FluxTabViewData so the strip node's borrowed component_data points straight
 * at it. Label and icon name are owned copies. The content node is owned by the
 * tab (orphaned while another tab is selected, destroyed on close).
 */
typedef struct FluxTabViewItem {
	FluxTabViewData *tv;
	XentNodeId       tab_node;   /**< FLUX_CONTROL_TAB_VIEW_ITEM strip node. */
	XentNodeId       close_node; /**< Child close button (kind CLOSE), or invalid. */
	XentNodeId       content;    /**< Owned content subtree (caller fills). */
	char const      *label;      /**< Owned copy. */
	char const      *icon_name;  /**< Owned copy of the Segoe icon name, or NULL. */
	FluxTabKind      kind;
	int              index;
	bool             closed;      /**< Slot retired (kept for stable address). */
	bool             closable;    /**< TabViewItem.IsClosable (default true). */
	bool             closing;     /**< Width-collapse close animation running. */
	float            width;       /**< Natural tab width (px), clamped [MinW, MaxW]. */
	float            cur_w;       /**< Target strip width (per width mode). */
	float            disp_w;      /**< Displayed width, tweening toward cur_w (AddDeleteThemeTransition). */
	float            w_from;      /**< Width-tween start value. */
	DWORD            w_start;     /**< Width-tween start tick. */
	bool             w_anim;      /**< Width tween running. */
	float            slide_x0;    /**< Reorder slide start offset (ReorderThemeTransition). */
	DWORD            slide_start; /**< Reorder slide start tick. */
	bool             sliding;     /**< Reorder slide running. */
	float            close_w0;    /**< Width at the moment close started. */
	DWORD            close_start; /**< GetTickCount when the close animation began. */
} FluxTabViewItem;

/**
 * @brief TabView runtime, owned by the root FLUX_CONTROL_TAB_VIEW node.
 *
 * Layout mirrors TabView.xaml: a strip row of [scroll-dec][scrollable tab host]
 * [scroll-inc][add] over a content host. The selected tab owns the visible
 * content subtree and its fill matches the content fill so it reads as connected
 * (TabViewItemHeaderBackgroundSelected). Tabs share the strip equally by default
 * (TabWidthMode::Equal) and overflow into a scrollable host with repeat-button
 * scrolling. Closing collapses the tab's width to 0 on a shared timer before the
 * strip node is detached; per WinUI, widths recompute only once the pointer
 * leaves the strip so the next close button lands under the cursor.
 */
struct FluxTabViewData {
	XentContext            *ctx;
	FluxNodeStore          *store;
	FluxWindow             *window;
	FluxTextRenderer       *text;
	FluxInput              *input;            /**< Focus moves for arrow-key navigation. */

	XentNodeId              root;             /**< FLUX_CONTROL_TAB_VIEW (flex column). */
	XentNodeId              strip;            /**< Flex row: scroll buttons + tab host + add. */
	XentNodeId              tabs_host;        /**< FLUX_CONTROL_SCROLL hosting the tab row. */
	XentNodeId              content;          /**< Content host (selected tab's content attaches here). */
	XentNodeId              add_node;         /**< The (+) strip item, or invalid. */
	XentNodeId              scroll_dec;       /**< Left overflow button, or invalid. */
	XentNodeId              scroll_inc;       /**< Right overflow button, or invalid. */

	FluxTabViewItem         add_item;         /**< Backing item for the add button. */
	FluxTabViewItem         scroll_items [2]; /**< Backing items for dec/inc buttons. */
	FluxTabViewItem         tabs [FLUX_TAB_VIEW_MAX_TABS];
	FluxTabViewItem         close_items [FLUX_TAB_VIEW_MAX_TABS]; /**< Backing items for per-tab close buttons. */
	int                     order [FLUX_TAB_VIEW_MAX_TABS];       /**< Display order -> slot index. */
	int                     count;                                /**< Slots used (including closed). */
	int                     selected;                             /**< Selected tab slot, or -1. */

	FluxTabWidthMode        width_mode;
	FluxTabCloseOverlayMode close_overlay;        /**< CloseButtonOverlayMode (Auto = always visible). */
	bool                    can_reorder;          /**< CanReorderTabs (WinUI default true). */
	bool                    scroll_visible;       /**< Overflow scroll buttons shown. */
	bool                    width_update_pending; /**< Recompute widths when pointer leaves the strip. */
	bool                    widths_valid;         /**< Widths computed against a real strip layout. */
	float                   settle_w;             /**< Last strip width probed while waiting for layout to settle. */
	XentNodeId              strip_header;         /**< TabStripHeader node, or invalid. */
	XentNodeId              strip_footer;         /**< TabStripFooter node, or invalid. */

	int                     drag_slot;            /**< Slot being drag-reordered, or -1. */
	bool                    drag_active;          /**< Pointer moved past the drag threshold. */
	float                   drag_press_x;         /**< Press X in tab-local px. */

	int                     scroll_held;          /**< 0 none, -1 dec held, +1 inc held. */
	DWORD                   scroll_next;          /**< Tick for the next repeat scroll. */
	float                   scroll_from;          /**< Smooth-scroll tween start offset. */
	float                   scroll_target;        /**< Smooth-scroll tween target offset. */
	DWORD                   scroll_start;         /**< Smooth-scroll tween start tick. */
	bool                    scroll_anim;          /**< Smooth scroll running (ChangeView). */

	bool                    anim_active;

	void                    (*on_selection_changed)(void *ctx, int index);
	bool                    (*on_close_requested)(void *ctx, int index); /**< Veto hook; NULL = close. */
	void                    (*on_tab_closed)(void *ctx, int index);
	void                    (*on_add_tab)(void *ctx);
	void                   *cb_ctx;
};

#ifdef __cplusplus
}
#endif

#endif
