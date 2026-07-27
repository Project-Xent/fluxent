/**
 * @file gallery_model.h
 * @brief Shared PAGE_/MSG_/Model/kNavItems for declarative + imperative galleries.
 */
#ifndef GALLERY_MODEL_H
#define GALLERY_MODEL_H

#include <xtk/xtk.h>
#include <stdbool.h>

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
	PAGE_PERSON_PICTURE,
	PAGE_FLIPVIEW,
	PAGE_PAGER,
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
	PAGE_CAT_SHELL,
	PAGE_SPLIT_VIEW,
	PAGE_TITLE_BAR,
	PAGE_CAT_DIALOGS,
	PAGE_DIALOG,
	PAGE_TEACHING_TIP,
	PAGE_SETTINGS,
	PAGE_COUNT,
};

extern XtkNavItemDesc const kNavItems [PAGE_COUNT];

enum
{
	MSG_NONE = 0,
	MSG_NAV,
	MSG_CLICK,
	MSG_BUTTON_DISABLE,
	MSG_CHECK_A,
	MSG_CHECK_B,
	MSG_RADIO_0,
	MSG_RADIO_1,
	MSG_RADIO_2,
	MSG_COMBO,
	MSG_COMBO_FONT,
	MSG_SLIDER_BASIC,
	MSG_SLIDER_STEP,
	MSG_SLIDER_TICK,
	MSG_LIGHT,
	MSG_NAME,
	MSG_PASSWORD,
	MSG_QUANTITY,
	MSG_BAR_CLOSE,
	MSG_BARS_RESET,
	MSG_BADGE_DELTA,
	MSG_PROGRESS,
	MSG_INDETERMINATE,
	MSG_MENU,
	MSG_REPEAT,
	MSG_EXPAND,
	MSG_SPLIT_PRIMARY,
	MSG_SPLIT_MENU,
	MSG_TAB_SELECT,
	MSG_TAB_CLOSE,
	MSG_TAB_ADD,
	MSG_DIALOG_OPEN,
	MSG_DIALOG_RESULT,
	MSG_LIST_SELECT,
	MSG_MULTI_SELECT,
	MSG_GRID_SELECT,
	MSG_LISTBOX_SELECT,
	MSG_FLIP_SELECT,
	MSG_ASB_TEXT,
	MSG_ASB_QUERY,
	MSG_ASB_CHOSEN,
	MSG_RADIO_BUTTONS,
	MSG_RATING,
	MSG_TOGGLE_SPLIT,
	MSG_TOGGLE_SPLIT_CLICK,
	MSG_TOGGLE_SPLIT_MENU,
	MSG_BREADCRUMB,
	MSG_SELECTOR_BAR,
	MSG_TIP_OPEN,
	MSG_TIP_CLOSE,
	MSG_TIP_ACTION,
	MSG_TREE_SELECT,
	MSG_TREE_EXPAND,
	MSG_TREE_INVOKE,
	MSG_ITEMS_SELECT,
	MSG_ITEMS_INVOKE,
	MSG_REFRESH,
	MSG_REFRESH_DONE,
	MSG_PP_BADGE,
	MSG_PP_GROUP,
	MSG_PAGER_SELECT,
	MSG_SPLIT_TOGGLE,
	MSG_SPLIT_MODE,
	MSG_SPLIT_PLACE,
	MSG_TB_BACK,
	MSG_TB_PANE,
	MSG_COUNT,
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
	char  *password;
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
	int    ntab;
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
	bool   tree_docs_expanded;
	bool   tree_pictures_expanded;
	int    items_sel;
	int    items_invoke;
	bool   refresh_busy;
	int    refresh_count;
	int    pp_badge;
	bool   pp_group;
	int    pager_page;
	bool   split_open;
	int    split_mode;
	bool   split_right;
	int    tb_back_clicks;
	int    tb_pane_clicks;
} Model;

void gallery_model_init(Model *m);
void gallery_model_update(Model *m, XtkMsg msg);

#endif
