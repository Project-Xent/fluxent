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
	PAGE_CAT_TEXT,
	PAGE_TEXTBOX,
	PAGE_NUMBERBOX,
	PAGE_TYPOGRAPHY,
	PAGE_CAT_STATUS,
	PAGE_INFOBAR,
	PAGE_BADGE,
	PAGE_PROGRESS,
	PAGE_TOOLTIP,
	PAGE_CAT_MEDIA,
	PAGE_IMAGE,
	PAGE_CAT_MENUS,
	PAGE_MENUS,
	PAGE_TABVIEW,
	PAGE_EXPANDER,
	PAGE_CAT_DIALOGS,
	PAGE_DIALOG,
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
XtkEl *page_dialog(XtkUi *ui, Model const *m);
XtkEl *page_settings(XtkUi *ui, Model const *m);

#endif
