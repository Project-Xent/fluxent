/**
 * @file gallery.h
 * @brief Shared definitions for the FluXent Gallery: model, messages,
 * navigation spine, and page/helper function declarations.
 */
#ifndef GALLERY_H
#define GALLERY_H

#include <fluxent/fluxent.h>
#include <fluxent/fx.h>
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

extern FxNavItemDesc const kNavItems [PAGE_COUNT];

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
	char   name [256];
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

FluxEl *page_header(FxUi *ui, char const *title, char const *blurb);
FluxEl *example(FxUi *ui, char const *caption, FluxEl *content);
FluxEl *output_col(FxUi *ui, char const *value);
FluxEl *demo_card(FxUi *ui, FluxEl *control, char const *output, FluxEl *extra);
FluxEl *page_category(FxUi *ui, char const *title, char const *blurb);

/* -------------------------------------------------------------------------
 * Pages
 * ---------------------------------------------------------------------- */

FluxEl *page_home(FxUi *ui, Model const *m);
FluxEl *page_button(FxUi *ui, Model const *m);
FluxEl *page_split(FxUi *ui, Model const *m);
FluxEl *page_checkbox(FxUi *ui, Model const *m);
FluxEl *page_radio(FxUi *ui, Model const *m);
FluxEl *page_combo(FxUi *ui, Model const *m);
FluxEl *page_slider(FxUi *ui, Model const *m);
FluxEl *page_toggle(FxUi *ui, Model const *m);
FluxEl *page_textbox(FxUi *ui, Model const *m);
FluxEl *page_numberbox(FxUi *ui, Model const *m);
FluxEl *page_typography(FxUi *ui, Model const *m);
FluxEl *page_infobar(FxUi *ui, Model const *m);
FluxEl *page_badge(FxUi *ui, Model const *m);
FluxEl *page_progress(FxUi *ui, Model const *m);
FluxEl *page_tooltip(FxUi *ui, Model const *m);
FluxEl *page_image(FxUi *ui, Model const *m);
FluxEl *page_menus(FxUi *ui, Model const *m);
FluxEl *page_tabview(FxUi *ui, Model const *m);
FluxEl *page_expander(FxUi *ui, Model const *m);
FluxEl *page_dialog(FxUi *ui, Model const *m);
FluxEl *page_settings(FxUi *ui, Model const *m);

#endif
