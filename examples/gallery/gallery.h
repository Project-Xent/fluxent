/**
 * @file gallery.h
 * @brief Shared definitions for the FluXent Gallery: model, messages,
 * navigation spine, and page/helper function declarations.
 */
#ifndef GALLERY_H
#define GALLERY_H

#include <fluxent/fluxent.h>
#include <fluxent/flux_app.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * Navigation spine
 * ---------------------------------------------------------------------- */

enum
{
	PAGE_HOME = 0,
	PAGE_SEP_1,
	PAGE_CAT_BASIC,
	PAGE_BUTTON,
	PAGE_SPLIT,
	PAGE_CHECKBOX,
	PAGE_RADIO,
	PAGE_COMBO,
	PAGE_SLIDER,
	PAGE_TOGGLE,
	PAGE_RADIO_BUTTONS,
	PAGE_RATING,
	PAGE_TOGGLE_SPLIT,
	PAGE_CAT_TEXT,
	PAGE_TEXTBOX,
	PAGE_AUTOSUGGEST,
	PAGE_NUMBERBOX,
	PAGE_TYPOGRAPHY,
	PAGE_CAT_STATUS,
	PAGE_INFOBAR,
	PAGE_BADGE,
	PAGE_PROGRESS,
	PAGE_TOOLTIP,
	PAGE_CAT_MEDIA,
	PAGE_IMAGE,
	PAGE_FLIPVIEW,
	PAGE_CAT_MENUS,
	PAGE_MENUS,
	PAGE_TABVIEW,
	PAGE_EXPANDER,
	PAGE_BREADCRUMB,
	PAGE_SELECTOR_BAR,
	PAGE_CAT_COLLECTIONS,
	PAGE_LISTVIEW,
	PAGE_GRIDVIEW,
	PAGE_LISTBOX,
	PAGE_TREE_VIEW,
	PAGE_ITEMS_VIEW,
	PAGE_PULL_REFRESH,
	PAGE_CAT_DIALOGS,
	PAGE_DIALOG,
	PAGE_TEACHING_TIP,
	PAGE_SETTINGS,
	PAGE_COUNT,
};

extern XtkNavItemDesc const kNavItems [PAGE_COUNT];

/* -------------------------------------------------------------------------
 * Model + messages
 * ---------------------------------------------------------------------- */

enum
{
	MSG_NONE = 0,
	MSG_NAV,            /**< .i = nav item index. */
	MSG_CLICK,
	MSG_BUTTON_DISABLE, /**< .b */
	MSG_CHECK_A,        /**< .b */
	MSG_CHECK_B,        /**< .b */
	MSG_RADIO_0,        /**< .b; tags 0..2 must stay consecutive. */
	MSG_RADIO_1,
	MSG_RADIO_2,
	MSG_COMBO,         /**< .i = selected index. */
	MSG_COMBO_FONT,    /**< .i = selected index. */
	MSG_SLIDER_BASIC,  /**< .f */
	MSG_SLIDER_STEP,   /**< .f */
	MSG_SLIDER_TICK,   /**< .f */
	MSG_LIGHT,         /**< .b */
	MSG_NAME,          /**< .ptr = transient UTF-8. */
	MSG_PASSWORD,      /**< .ptr = transient UTF-8. */
	MSG_QUANTITY,      /**< .d */
	MSG_BAR_CLOSE,     /**< .i = severity row. */
	MSG_BARS_RESET,
	MSG_BADGE_DELTA,   /**< .i = +1 / -1. */
	MSG_PROGRESS,      /**< .f */
	MSG_INDETERMINATE, /**< .b */
	MSG_MENU,          /**< .i = menu item ordinal. */
	MSG_REPEAT,
	MSG_EXPAND,        /**< .b */
	MSG_SPLIT_PRIMARY,
	MSG_SPLIT_MENU,    /**< .i = menu item ordinal. */
	MSG_TAB_SELECT,    /**< .i = tab index. */
	MSG_TAB_CLOSE,     /**< .i = tab index. */
	MSG_TAB_ADD,
	MSG_DIALOG_OPEN,
	MSG_DIALOG_RESULT, /**< .i = FluxDialogResult. */
	MSG_LIST_SELECT,   /**< .i = row index. */
	MSG_MULTI_SELECT,  /**< .i = lead index of the Extended-mode list. */
	MSG_GRID_SELECT,   /**< .i = cell index. */
	MSG_LISTBOX_SELECT,/**< .i = row index. */
	MSG_FLIP_SELECT,   /**< .i = page index (FlipView + PipsPager). */
	MSG_ASB_TEXT,      /**< .ptr = transient query text. */
	MSG_ASB_QUERY,     /**< .ptr = transient query text. */
	MSG_ASB_CHOSEN,    /**< .i = previewed suggestion index. */
	MSG_RADIO_BUTTONS, /**< .i = selected index. */
	MSG_RATING,        /**< .d = value; .i = star count. */
	MSG_TOGGLE_SPLIT,  /**< .b = checked (ToggleSplitButton). */
	MSG_TOGGLE_SPLIT_CLICK,
	MSG_TOGGLE_SPLIT_MENU, /**< .i = menu item ordinal. */
	MSG_BREADCRUMB,    /**< .i = clicked crumb index. */
	MSG_SELECTOR_BAR,  /**< .i = selected index. */
	MSG_TIP_OPEN,
	MSG_TIP_CLOSE,     /**< .i = close reason. */
	MSG_TIP_ACTION,
	MSG_TREE_SELECT,   /**< .i = flat index when selected, else -(flat+1). */
	MSG_TREE_EXPAND,   /**< .i = flat when expanded, else -(flat+1). */
	MSG_TREE_INVOKE,   /**< .i = flat index. */
	MSG_ITEMS_SELECT,  /**< .i = selected index. */
	MSG_ITEMS_INVOKE,  /**< .i = invoked index. */
	MSG_REFRESH,       /**< .i = pull direction. */
	MSG_REFRESH_DONE,
};

#define BAR_COUNT 4

typedef struct Model {
	int    page;
	int    clicks;
	bool   button_disabled;
	bool   check_a;
	bool   check_b;
	int    radio;
	int    combo_sel;
	int    combo_font;
	float  slider_basic;
	float  slider_step;
	float  slider_tick;
	bool   light_on;
	char  *name;
	int    pass_chars;
	double quantity;
	bool   bar_open [BAR_COUNT];
	int    badge_value;
	float  progress;
	bool   indeterminate;
	int    menu_choice;
	int    repeats;
	bool   expanded;
	int    split_action;
	int    tab_ids [8];
	int    tab_count;
	int    next_tab_id;
	int    tab_sel;
	int    tab_gen;
	bool   dialog_open;
	int    dialog_result;
	int    list_sel;
	int    multi_lead;
	int    grid_sel;
	int    listbox_sel;
	int    flip_page;
	char  *asb_text;
	int    asb_chosen;
	char  *asb_query;
	int    radio_buttons_sel;
	double rating_value;
	bool   toggle_split_checked;
	int    toggle_split_menu;
	int    breadcrumb_click;
	int    selector_sel;
	bool   tip_open;
	int    tip_actions;
	int    tip_close_reason;
	int    tree_sel;
	int    tree_invoke;
	int    items_sel;
	int    items_invoke;
	bool   refresh_busy;
	int    refresh_count;
} Model;

/* -------------------------------------------------------------------------
 * Helpers (gallery_helpers.c)
 * ---------------------------------------------------------------------- */

XtkEl *page_header(XtkUi *ui, char const *title, char const *blurb);
XtkEl *example(XtkUi *ui, char const *caption, XtkEl *content);
XtkEl *output_col(XtkUi *ui, char const *value);
XtkEl *demo_card(XtkUi *ui, XtkEl *control, char const *output, XtkEl *extra);
XtkEl *page_category(XtkUi *ui, char const *title, char const *blurb);

/* -------------------------------------------------------------------------
 * Pages
 * ---------------------------------------------------------------------- */

XtkEl *page_home(XtkUi *ui, Model const *m);
XtkEl *page_button(XtkUi *ui, Model const *m);
XtkEl *page_split(XtkUi *ui, Model const *m);
XtkEl *page_checkbox(XtkUi *ui, Model const *m);
XtkEl *page_radio(XtkUi *ui, Model const *m);
XtkEl *page_combo(XtkUi *ui, Model const *m);
XtkEl *page_slider(XtkUi *ui, Model const *m);
XtkEl *page_toggle(XtkUi *ui, Model const *m);
XtkEl *page_radio_buttons(XtkUi *ui, Model const *m);
XtkEl *page_rating(XtkUi *ui, Model const *m);
XtkEl *page_toggle_split(XtkUi *ui, Model const *m);
XtkEl *page_breadcrumb(XtkUi *ui, Model const *m);
XtkEl *page_selector_bar(XtkUi *ui, Model const *m);
XtkEl *page_teaching_tip(XtkUi *ui, Model const *m);
XtkEl *page_textbox(XtkUi *ui, Model const *m);
XtkEl *page_numberbox(XtkUi *ui, Model const *m);
XtkEl *page_typography(XtkUi *ui, Model const *m);
XtkEl *page_infobar(XtkUi *ui, Model const *m);
XtkEl *page_badge(XtkUi *ui, Model const *m);
XtkEl *page_progress(XtkUi *ui, Model const *m);
XtkEl *page_tooltip(XtkUi *ui, Model const *m);
XtkEl *page_image(XtkUi *ui, Model const *m);
XtkEl *page_menus(XtkUi *ui, Model const *m);
XtkEl *page_tabview(XtkUi *ui, Model const *m);
XtkEl *page_expander(XtkUi *ui, Model const *m);
XtkEl *page_listview(XtkUi *ui, Model const *m);
XtkEl *page_gridview(XtkUi *ui, Model const *m);
XtkEl *page_listbox(XtkUi *ui, Model const *m);
XtkEl *page_tree_view(XtkUi *ui, Model const *m);
XtkEl *page_items_view(XtkUi *ui, Model const *m);
XtkEl *page_pull_refresh(XtkUi *ui, Model const *m);
XtkEl *page_flipview(XtkUi *ui, Model const *m);
XtkEl *page_autosuggest(XtkUi *ui, Model const *m);
XtkEl *page_dialog(XtkUi *ui, Model const *m);
XtkEl *page_settings(XtkUi *ui, Model const *m);

#endif
