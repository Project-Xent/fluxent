/**
 * @file flux_init_nav.h
 * @brief Private Desc + init prototypes for navigation & bar controls:
 * NavView / TabView / TitleBar / MenuBar / BreadcrumbBar / SelectorBar /
 * PagerControl / PipsPager.
 */
#ifndef FLUX_INIT_NAV_H
#define FLUX_INIT_NAV_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * NavView
 * ---------------------------------------------------------------------- */

typedef struct FluxNavViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;           /**< For repaint, adaptive width, coordinate mapping. */
	FluxText   *text;             /**< Measures/draws item labels. */
	FluxTheme  *theme;            /**< Children-flyout chrome (Compact/Top hierarchical items). */
	float       width;            /**< Initial control size (DIPs); tracks the window when it fills it.
	                               *   <= 0 fills the window and tracks the client size on both axes. */
	float       height;
	int         mode;             /**< Initial FluxNavDisplayMode (ignored when adaptive). */
	bool        adaptive;         /**< Auto-switch Expanded/Compact/Minimal by window width. */
	bool        window_title_bar; /**< Extend into the OS title bar; the nav top strip owns it. */
	char const *app_title;        /**< App title for the integrated title bar, or NULL. */
	void        (*on_selection_changed)(void *ctx, int index);
	void       *userdata;
	XentNodeId *out_content; /**< Optional: receives the content host node to fill. */
} FluxNavViewDesc;

/** @brief Create a left-nav NavigationView; out_content receives the content host to fill. */
XentNodeId  flux_nav_init(FluxNavViewDesc const *info);

/** @brief Apply mount-time title-bar chrome to an initialized NavigationView. */
bool        flux_nav_set_chrome(FluxStore *store, XentNodeId nav, bool window_title_bar, char const *app_title);

/** @brief Remove all public model items while preserving the NavigationView shell. */
bool        flux_nav_reset_items(FluxStore *store, XentNodeId nav);

/**
 * @brief Append a menu item (icon name + label) and return its selection index.
 * @p icon_name and @p label are copied. The first menu item is auto-selected.
 */
int         flux_nav_add_item(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label);

/** @brief Append a footer item (e.g. Settings); returns its selection index. */
int         flux_nav_addfooter(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label);

/** @brief Programmatically select an item by index (fires on_selection_changed). */
void        flux_nav_select(FluxStore *store, XentNodeId nav, int index);

/** @brief Project retained selected state without raising a user event. */
void        flux_nav_set_selected(FluxStore *store, XentNodeId nav, int index);

/** @brief Get the content host node (the child the caller fills / swaps). */
XentNodeId  flux_nav_content_node(FluxStore *store, XentNodeId nav);

/**
 * @brief Set PaneDisplayMode (FluxNavPaneDisplayMode). Auto re-enables adaptive
 * threshold behavior; Left/LeftCompact/LeftMinimal/Top are sticky.
 */
void        flux_nav_setmode(FluxStore *store, XentNodeId nav, int mode);

/** @brief Append a NavigationViewItemSeparator (1px rule) to the menu list. */
int         flux_nav_add_separator(FluxStore *store, XentNodeId nav);

/** @brief Append a NavigationViewItemHeader (section caption) to the menu list. */
int         flux_nav_add_header(FluxStore *store, XentNodeId nav, char const *label);

/** @brief Item label by index, owned by the control (NULL for separators/invalid). */
char const *flux_nav_item_label(FluxStore *store, XentNodeId nav, int index);

/**
 * @brief Append a child under @p parent_index (hierarchical item). Children
 * expand inline in Expanded/Minimal (31px indent per depth) and open in a
 * flyout from Compact/Top, like WinUI's hierarchical NavigationView.
 */
int flux_nav_addchild(FluxStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label);

/* -------------------------------------------------------------------------
 * TabView
 * ---------------------------------------------------------------------- */

typedef struct FluxTabViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;          /**< For repaint + coordinate mapping. */
	FluxText   *text;            /**< Measures/draws tab headers. */
	FluxInput  *input;           /**< Arrow-key focus moves between tab headers. */
	int         width_mode;      /**< FluxTabWidthMode; 0 = Equal (WinUI default). */
	int         close_overlay;   /**< FluxTabCloseOverlayMode; 0 = Auto (always visible). */
	bool        disable_reorder; /**< Opt out of drag-reorder (CanReorderTabs defaults true). */
	void        (*on_selection_changed)(void *ctx, int index);
	void       *selection_ctx;
	bool        (*on_close_requested)(void *ctx, int index); /**< Veto hook; NULL = always close. */
	void       *close_requested_ctx;
	void        (*on_tab_closed)(void *ctx, int index);
	void       *tab_closed_ctx;
	void        (*on_add_tab)(void *ctx); /**< NULL hides the (+) button. */
	void       *add_tab_ctx;
} FluxTabViewDesc;

/** @brief Create a TabView (tab strip over a content host). */
XentNodeId flux_tabs_init(FluxTabViewDesc const *info);

/**
 * @brief Append a tab (icon name + label) and return its index; out_content
 * receives the content subtree to fill. @p icon_name / @p label are copied.
 * The first tab is auto-selected.
 */
int        flux_tabs_add_tab(
  FluxStore *store, XentNodeId tabview, char const *icon_name, char const *label, XentNodeId *out_content
);

bool flux_tabs_set_items(
  FluxStore *store, XentNodeId tabview, char const *const *icons, char const *const *labels, int count
);
bool flux_tabs_mount_items(
  FluxStore *store, XentNodeId tabview, char const *const *icons, char const *const *labels, int count, int selected,
  bool add_visible
);

XentNodeId flux_tabs_content_node(FluxStore *store, XentNodeId tabview);

/** @brief Publish tab widths from the current laid-out strip before painting. */
void       flux_tabs_sync_layout(FluxStore *store, XentNodeId tabview);

/** @brief Select a tab by index (fires on_selection_changed, swaps content). */
void       flux_tabs_select(FluxStore *store, XentNodeId tabview, int index);

/** @brief Close a tab by index (collapse animation, then detach; fires on_tab_closed). */
void       flux_tabs_close(FluxStore *store, XentNodeId tabview, int index);

/** @brief Per-tab TabViewItem.IsClosable: hides the close glyph and blocks Ctrl+F4/middle-click. */
void       flux_tabs_set_closable(FluxStore *store, XentNodeId tabview, int index, bool closable);

/** @brief Switch TabWidthMode (FluxTabWidthMode) and re-lay the strip. */
void       flux_tabs_setmode(FluxStore *store, XentNodeId tabview, int width_mode);
void       flux_tabs_setaddvisible(FluxStore *store, XentNodeId tabview, bool visible);

/** @brief Place a caller-built node before the tabs (TabStripHeader). */
void       flux_tabs_setheader(FluxStore *store, XentNodeId tabview, XentNodeId node);

/** @brief Place a caller-built node after the (+) button (TabStripFooter). */
void       flux_tabs_setfooter(FluxStore *store, XentNodeId tabview, XentNodeId node);

/* -------------------------------------------------------------------------
 * TitleBar (WinUI 3 TitleBar band)
 * ---------------------------------------------------------------------- */

typedef struct FluxTitleBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window; /**< Required for window integration. */
	char const *title;
	char const *subtitle;
	char const *icon; /**< Segoe Fluent Icons name, or NULL. */
	bool        show_back;
	bool        back_disabled;
	bool        show_pane_toggle;
	bool        integrate_window; /**< Report drag/passthrough regions to the window. */
	void        (*on_back)(void *ctx);
	void       *back_ctx;
	void        (*on_pane_toggle)(void *ctx);
	void       *pane_toggle_ctx;
} FluxTitleBarDesc;

/** @brief Create a TitleBar band leaf. */
XentNodeId flux_titlebar_init(FluxTitleBarDesc const *info);

/** @brief Engine per-frame hook: report drag/passthrough regions to the window. */
void       flux_titlebar_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd);

/** @brief Replace the title and subtitle (re-measures the title). */
void       flux_titlebar_set_title(FluxStore *store, XentNodeId id, char const *title, char const *subtitle);

/** @brief Show/hide the back button and set its disabled state. */
void       flux_titlebar_set_back(FluxStore *store, XentNodeId id, bool show, bool disabled);

/** @brief Show/hide the pane-toggle button. */
void       flux_titlebar_settoggle(FluxStore *store, XentNodeId id, bool show);

/** @brief Replace the leading icon (Segoe Fluent Icons name, or NULL). */
void       flux_titlebar_set_icon(FluxStore *store, XentNodeId id, char const *icon);
void       flux_titlebar_set_window_integration(FluxStore *store, XentNodeId id, bool integrate);

/* -------------------------------------------------------------------------
 * MenuBar
 * ---------------------------------------------------------------------- */

typedef struct FluxMenuBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window; /**< Owner for the drop-down flyouts + coordinate mapping. */
	FluxText   *text;   /**< Measures item titles and draws the flyouts. */
	FluxTheme  *theme;  /**< Theme source for the flyouts. */
} FluxMenuBarDesc;

/** @brief Create a menu bar (horizontal row of top-level menus). */
XentNodeId flux_menubar_init(FluxMenuBarDesc const *info);
XentNodeId flux_menubar_content_node(FluxStore *store, XentNodeId bar);

/**
 * @brief Append a top-level menu titled @p title and return its (empty) flyout.
 * Populate the returned flyout with flux_menu_additem / _add_separator.
 * @p title is copied. Returns NULL if the bar is full/invalid.
 */
FluxMenu  *flux_menubar_add_menu(FluxStore *store, XentNodeId bar, char const *title);
void       flux_menubar_clear_menus(FluxStore *store, XentNodeId bar);
bool       flux_menubar_add_menu_ref(FluxStore *store, XentNodeId bar, char const *title, FluxMenu *flyout);

/* -------------------------------------------------------------------------
 * BreadcrumbBar
 * ---------------------------------------------------------------------- */

typedef struct FluxBreadcrumbBarDesc {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         parent;
	FluxWindow        *window; /**< Ellipsis flyout + repaint. */
	FluxText          *text;   /**< Measures crumb labels for the collapse algorithm. */
	FluxTheme         *theme;  /**< Flyout palette. */
	FluxInput         *input;  /**< Arrow-key focus moves between crumbs. */
	char const *const *items;  /**< Crumb labels, root first; deep-copied. */
	int                item_count;
	bool               disabled;
	void               (*on_item_clicked)(void *ctx, int index);
	void              *userdata;
} FluxBreadcrumbBarDesc;

/** @brief Create a BreadcrumbBar (crumb strip with ellipsis overflow collapse). */
XentNodeId flux_breadcrumb_init(FluxBreadcrumbBarDesc const *info);

/** @brief Replace the crumb labels (deep-copied); dismisses a stale ellipsis flyout. */
void       flux_breadcrumb_set_items(FluxStore *store, XentNodeId bar, char const *const *items, int count);

/** @brief Enable/disable the whole bar. */
void       flux_breadcrumb_set_disabled(FluxStore *store, XentNodeId bar, bool disabled);

/** @brief Engine per-frame hook: re-measure labels + re-run the overflow collapse. */
void       flux_breadcrumb_sync(XentCtx *ctx, XentNodeId bar, struct FluxNodeData *nd);

/* -------------------------------------------------------------------------
 * SelectorBar
 * ---------------------------------------------------------------------- */

typedef struct FluxSelectorBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxInput  *input;
	FluxWindow *window; /**< Repaint target for the pill tween. */
	int         item_count;
	int         selected;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxSelectorBarDesc;

/** @brief Create a SelectorBar (items set via flux_selector_set_item). */
XentNodeId flux_selector_init(FluxSelectorBarDesc const *info);

/** @brief Set item i's content (icon_glyph 0 = none). */
void       flux_selector_set_item(
  FluxStore *store, XentNodeId bar, int i, char const *text, uint32_t icon_glyph, bool disabled
);

/** @brief Rebuild the descriptor-owned item set. */
void flux_selector_set_items(FluxStore *store, XentNodeId bar, XtkSelectorItem const *items, int count, int selected);

/** @brief Select an item (animates the pill). */
void flux_selector_set_selected(FluxStore *store, XentNodeId bar, int index);

/* -------------------------------------------------------------------------
 * PagerControl (numeric/arrow pager; distinct from the dot PipsPager)
 * ---------------------------------------------------------------------- */

typedef struct FluxPagerDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	int         count;        /**< NumberOfPages (>= 1). */
	int         selected;     /**< SelectedPageIndex (0-based). */
	int         display_mode; /**< FluxPagerDisplayMode. */
	int         first_button; /**< FluxPagerButtonVis. */
	int         prev_button;  /**< FluxPagerButtonVis. */
	int         next_button;  /**< FluxPagerButtonVis. */
	int         last_button;  /**< FluxPagerButtonVis. */
	char const *prefix;       /**< NumberText leading label, or NULL. */
	char const *suffix;       /**< NumberText infix before the total, or NULL. */
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxPagerDesc;

/** @brief Create a PagerControl leaf. */
XentNodeId flux_pager_init(FluxPagerDesc const *info);

/** @brief Set page count / selection / display mode (re-measures). */
void       flux_pager_configure(FluxStore *store, XentNodeId id, int count, int selected, int display_mode);

/** @brief Set the selected page index (0-based, clamped). */
void       flux_pager_set_selected(FluxStore *store, XentNodeId id, int index);

/** @brief Set First/Previous/Next/Last button visibility modes (FluxPagerButtonVis). */
void       flux_pager_setbuttons(FluxStore *store, XentNodeId id, int first, int prev, int next, int last);
void       flux_pager_settext(FluxStore *store, XentNodeId id, char const *prefix, char const *suffix);

/* -------------------------------------------------------------------------
 * PipsPager
 * ---------------------------------------------------------------------- */

typedef struct FluxPipsPagerDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	bool       vertical;
	void       (*on_select)(void *ctx, int index);
	void      *userdata;
} FluxPipsPagerDesc;

/** @brief Create a PipsPager (dot pager leaf). */
XentNodeId flux_pips_init(FluxPipsPagerDesc const *info);

/** @brief Set page count / selection / visible cap / nav-button visibility. */
void flux_pips_configure(FluxStore *store, XentNodeId pips, int count, int selected, int max_visible, int nav_vis);
void flux_pips_set_vertical(FluxStore *store, XentNodeId pips, bool vertical);
void flux_pips_set_count(FluxStore *store, XentNodeId pips, int count);
void flux_pips_set_selected(FluxStore *store, XentNodeId pips, int selected);
void flux_pips_set_max_visible(FluxStore *store, XentNodeId pips, int max_visible);
void flux_pips_set_nav_visibility(FluxStore *store, XentNodeId pips, int nav_vis);

#endif
