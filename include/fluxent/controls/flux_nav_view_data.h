/**
 * @file flux_nav_view_data.h
 * @brief Shared runtime for the FluxNavView control (WinUI 3 NavigationView, left nav).
 */
#ifndef FLUX_NAV_VIEW_DATA_H
#define FLUX_NAV_VIEW_DATA_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Minimal scalar tween (current eases start->target over a duration). */
typedef struct FluxNavTween {
	float current;
	float start;
	float target;
	DWORD start_ms; /**< GetTickCount at the last target change. */
	float duration; /**< ms. */
	bool  active;
	bool  lead;     /**< Indicator edge uses the fast (leading) spline this run. */
} FluxNavTween;

typedef struct FluxNodeStore    FluxNodeStore;
typedef struct FluxMenuFlyout   FluxMenuFlyout;
typedef struct FluxThemeManager FluxThemeManager;
typedef struct FluxWindow       FluxWindow;
typedef struct FluxTextRenderer FluxTextRenderer;
typedef struct FluxNavViewData  FluxNavViewData;
struct FluxScrollData;

#define FLUX_NAV_VIEW_MAX_ITEMS 32

/** @brief Left-nav display mode (WinUI NavigationViewDisplayMode). */
typedef enum FluxNavDisplayMode
{
	FLUX_NAV_EXPANDED = 0, /**< Pane open (320px), icon + label. */
	FLUX_NAV_COMPACT  = 1, /**< Pane 48px, icons only, inline. */
	FLUX_NAV_MINIMAL  = 2, /**< Pane hidden; hamburger opens a floating overlay pane. */
	FLUX_NAV_TOP      = 3, /**< Horizontal top bar; never adaptive. */
} FluxNavDisplayMode;

/** @brief NavigationView.PaneDisplayMode: Auto adapts to the window width via the
 * 641/1008 thresholds; every other value is sticky (resizing never changes it). */
typedef enum FluxNavPaneDisplayMode
{
	FLUX_NAV_PANE_AUTO         = 0,
	FLUX_NAV_PANE_LEFT         = 1,
	FLUX_NAV_PANE_LEFT_COMPACT = 2,
	FLUX_NAV_PANE_LEFT_MINIMAL = 3,
	FLUX_NAV_PANE_TOP          = 4,
} FluxNavPaneDisplayMode;

/** @brief Item role within the pane. */
typedef enum FluxNavItemKind
{
	FLUX_NAV_ITEM_TOGGLE    = 0, /**< Hamburger pane-toggle button. */
	FLUX_NAV_ITEM_MENU      = 1, /**< Selectable menu item (top list). */
	FLUX_NAV_ITEM_FOOTER    = 2, /**< Selectable footer item (e.g. Settings). */
	FLUX_NAV_ITEM_SEPARATOR = 3, /**< NavigationViewItemSeparator (1px rule). */
	FLUX_NAV_ITEM_HEADER    = 4, /**< NavigationViewItemHeader (section caption). */
} FluxNavItemKind;

/**
 * @brief One pane entry. Stored inline in FluxNavViewData so the item node's
 * borrowed component_data points straight at it (and back at the owner via
 * @ref nav). Label and icon name are owned copies of the caller's strings.
 */
typedef struct FluxNavViewItem {
	FluxNavViewData *nav;
	XentNodeId       node;             /**< FLUX_CONTROL_NAV_VIEW_ITEM node. */
	char const      *label;            /**< Owned copy of the display text. */
	char const      *icon_name;        /**< Owned copy of the Segoe Fluent Icons name (flux_icon_lookup). */
	FluxNavItemKind  kind;
	int              index;            /**< Stable selection index (menu then footer, in add order). */
	int              parent;           /**< Parent item index, or -1 for roots. */
	int              depth;            /**< 0 for roots; children indent 31px per level. */
	bool             has_children;
	bool             expanded;         /**< Inline children revealed (Expanded / Minimal pane). */
	bool             press_on_chevron; /**< The active press landed on the expand chevron. */
	XentNodeId       children_host;    /**< Container for child item nodes (parents only). */
	FluxNavTween     expand_t;         /**< Children reveal 0..1. */
} FluxNavViewItem;

/**
 * @brief NavigationView runtime, owned by the root FLUX_CONTROL_NAV_VIEW node.
 *
 * Layout is an ABSOLUTE root hosting two children: the content host (caller
 * fills it; the control swaps nothing — selection raises a callback, matching
 * WinUI's Frame model) and the pane (FLUX_CONTROL_NAV_VIEW, draws its own
 * background + the moving selection indicator in its overlay pass). In Expanded
 * and Compact the pane occupies flow width (320/48) and the content sits beside
 * it; in Minimal the content is full width and the pane slides in over it as an
 * overlay (render_translate_x) with a dimming scrim.
 *
 * All scalar transitions run on FluxTween over WinUI durations, driven by a
 * shared 60fps timer while @ref anim_active. The selection indicator stretches
 * between items by easing its top and bottom edges with different splines (the
 * edge leading the travel direction settles faster), reproducing the WinUI
 * offset+scale indicator animation.
 */
struct FluxNavViewData {
	XentContext           *ctx;
	FluxNodeStore         *store;
	FluxWindow            *window;
	FluxTextRenderer      *text;

	XentNodeId             root;              /**< FLUX_CONTROL_NAV_VIEW root (ABSOLUTE). */
	XentNodeId             pane;              /**< FLUX_CONTROL_NAV_VIEW pane (bg + indicator). */
	XentNodeId             toggle;            /**< Hamburger item node. */
	XentNodeId             items_scroll;      /**< FLUX_CONTROL_SCROLL viewport around the menu list; its grow=1
	                                               also pins the footer to the pane's trailing edge. */
	XentNodeId             items_host;        /**< Flex column of menu items (inside items_scroll). */
	XentNodeId             footer_host;       /**< Flex column of footer items. */
	XentNodeId             content;           /**< Content host (caller fills). */
	XentNodeId             scrim;             /**< Minimal-mode dimming overlay (FLUX_CONTROL_CONTAINER). */

	struct FluxScrollData *items_scroll_data; /**< Borrowed scroll state of items_scroll. */
	bool                   ind_in_scroll;     /**< Indicator tracks a menu item (shifts with the
	                                               viewport scroll), not a footer item. */

	FluxNavViewItem        toggle_item;       /**< Hamburger pane-toggle (kind TOGGLE). */
	FluxNavViewItem        items [FLUX_NAV_VIEW_MAX_ITEMS];
	int                    count;
	int                    selected;  /**< Selected item index, or -1. */
	int                    ind_index; /**< Item the indicator is currently tracking, or -1. */

	FluxNavDisplayMode     mode;
	FluxMenuFlyout        *child_flyout;  /**< Children flyout for Compact/Top (lazily created). */
	int                    flyout_parent; /**< Item index whose child flyout is open, or -1. */
	FluxThemeManager      *theme;         /**< For the children flyout chrome. */
	bool                   adaptive;      /**< Auto-switch mode by window width (641/1008). */
	bool                   minimal_open;  /**< Overlay pane shown (Minimal mode only). */
	bool                   fill_window;   /**< Created with width <= 0: track the window client size on both axes. */

	float                  width;         /**< Current root width (DIPs); tracks the window when adaptive. */
	float                  max_width;     /**< Width cap from creation (the host panel width). */
	float                  height;        /**< Root height (DIPs). */

	/* Animations. */
	FluxNavTween           pane_w;         /**< Inline pane flow width (Expanded<->Compact). */
	FluxNavTween           minimal_t;      /**< Minimal overlay open fraction 0..1. */
	FluxNavTween           ind_top;        /**< Indicator pill top edge (pane-local px). */
	FluxNavTween           ind_bot;        /**< Indicator pill bottom edge (pane-local px). */
	FluxNavTween           ind_op;         /**< Indicator opacity 0..1. */
	bool                   anim_active;
	bool                   ind_primed;     /**< Indicator has a valid first target (fade in vs slide). */
	bool                   labels_visible; /**< Pane wide enough for item labels; tracks the animated
	                                        * pane_w so labels and the pane collapse in lockstep (not the
	                                        * instantaneous mode, which would blank labels a frame early). */

	void                   (*on_selection_changed)(void *ctx, int index);
	void                  *on_sel_ctx;
};

#ifdef __cplusplus
}
#endif

#endif
