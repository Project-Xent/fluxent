#ifndef GALLERY_MODEL_H
#define GALLERY_MODEL_H

#include <xtk/xtk.h>
#include <stdbool.h>

#define GALLERY_MODEL_VERSION 1u

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
	PAGE_SCROLL_BAR,
	PAGE_LOOPING,
	PAGE_DATE_PICKER,
	PAGE_TIME_PICKER,
	PAGE_CALENDAR,
	PAGE_COLOR_PICKER,
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
	PAGE_COMMANDBAR,
	PAGE_TABVIEW,
	PAGE_EXPANDER,
	PAGE_BREADCRUMB,
	PAGE_SELECTOR_BAR,
	PAGE_CAT_COLLECTIONS,
	PAGE_LISTVIEW,
	PAGE_GRIDVIEW,
	PAGE_LISTBOX,
	PAGE_DETAILS,
	PAGE_TREE_VIEW,
	PAGE_ITEMS_VIEW,
	PAGE_PULL_REFRESH,
	PAGE_CAT_SHELL,
	PAGE_SPLIT_VIEW,
	PAGE_NAVIGATION_VIEW,
	PAGE_TITLE_BAR,
	PAGE_DRAG_DROP,
	PAGE_MATERIALS,
	PAGE_MOTION,
	PAGE_CAT_DIALOGS,
	PAGE_DIALOG,
	PAGE_TEACHING_TIP,
	PAGE_SETTINGS,
	PAGE_COUNT,
};

extern XtkNavItemDesc const kNavItems [PAGE_COUNT];

enum
{
	GALLERY_NAV_DEMO_MODES = 5,
	GALLERY_NAV_DEMO_ITEMS = 6,
	GALLERY_NAV_DEMO_HOME  = 1,
};

extern char const *const    kNavDemoModes [GALLERY_NAV_DEMO_MODES];
extern XtkNavPaneMode const kNavDemoPaneModes [GALLERY_NAV_DEMO_MODES];
extern XtkNavItemDesc const kNavDemoItems [GALLERY_NAV_DEMO_ITEMS];
extern char const *const    kNavDemoSuggestions [2];
char const                 *gallery_nav_demo_title(int selected);

typedef struct FluxWindow   FluxWindow;
int                         gallery_backdrop_load(void);
void                        gallery_backdrop_apply(FluxWindow *win, int backdrop);

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
	MSG_SCROLLBAR_V,
	MSG_SCROLLBAR_H,
	MSG_LOOPING,
	MSG_DATE_PICKER,
	MSG_TIME_PICKER,
	MSG_TIME_PICKER_24,
	MSG_CALENDAR,
	MSG_CALENDAR_MULTI,
	MSG_COLOR_PICKER,
	MSG_COLOR_ALPHA,
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
	MSG_SPLIT_OPEN,
	MSG_SPLIT_MODE,
	MSG_SPLIT_PLACE,
	MSG_TB_BACK,
	MSG_TB_PANE,
	MSG_CHECK_TRI,
	MSG_SLIDER_VERT,
	MSG_SLIDER_RANGE,
	MSG_COMBO_EDIT,
	MSG_COMBO_SUBMIT,
	MSG_DET_SORT,
	MSG_DET_RESIZE,
	MSG_DET_FIT,
	MSG_DET_REORDER,
	MSG_CB_ACTION,
	MSG_CB_OPEN,
	MSG_CB_FAV,
	MSG_CB_BOLD,
	MSG_DD_HOVER,
	MSG_DD_FILES,
	MSG_DD_TEXT,
	MSG_DD_DONE,
	MSG_BACKDROP,
	MSG_RTL,
	MSG_NAV_STYLE,
	MSG_NAV_DEMO,
	MSG_NAV_DEMO_HIDE,
	MSG_NAV_DEMO_PICK,
	MSG_NAV_DEMO_MODE,
	MSG_NAV_DEMO_HIDE_HEADER,
	MSG_CONTROL_SEARCH_TEXT,
	MSG_CONTROL_SEARCH_QUERY,
	MSG_MOTION_IMP_STATE,
	MSG_MOTION_IMP_MASK,
	MSG_MOTION_ENTER_KIND,
	MSG_MOTION_ITEM_ADD,
	MSG_MOTION_ITEM_REMOVE,
	MSG_MOTION_ITEM_SHUFFLE,
	MSG_MOTION_FRAME_KIND,
	MSG_MOTION_FRAME_STEP,
	MSG_MOTION_CONN_KIND,
	MSG_MOTION_CONN_TOGGLE,
	MSG_MOTION_REDUCED,
	MSG_COUNT,
};

#define BAR_COUNT            4
#define DET_COLS             4
#define DET_ROWS             12
#define CB_COUNT             10
#define CONTROL_SEARCH_LIMIT 8

typedef struct Model {
	int      page;
	char     control_query [128];
	int      backdrop;
	bool     rtl;
	int      nav_style;
	int      clicks;
	bool     button_disabled;
	bool     check_a;
	bool     check_b;
	int      radio;
	int      combo_sel;
	int      combo_font;
	float    slider_basic;
	float    slider_step;
	float    slider_tick;
	double   scrollbar_v;
	double   scrollbar_h;
	int      looping_sel;
	int      date_pick;
	int      time_pick;
	int      time_pick_24;
	int      cal_date;
	int      cal_multi [16];
	int      cal_multi_count;
	unsigned color_pick;
	unsigned color_alpha;
	bool     light_on;
	char    *name;
	char    *password;
	int      pass_chars;
	double   quantity;
	bool     bar_open [BAR_COUNT];
	int      badge_value;
	float    progress;
	bool     indeterminate;
	int      menu_choice;
	int      repeats;
	bool     expanded;
	int      split_action;
	int      tab_ids [8];
	int      ntab;
	int      next_tab_id;
	int      tab_sel;
	int      tab_gen;
	bool     dialog_open;
	int      dialog_result;
	int      list_sel;
	int      multi_lead;
	int      grid_sel;
	int      listbox_sel;
	int      flip_page;
	char    *asb_text;
	int      asb_chosen;
	char    *asb_query;
	int      radio_buttons_sel;
	double   rating_value;
	bool     toggle_split_checked;
	int      toggle_split_menu;
	int      breadcrumb_click;
	int      selector_sel;
	bool     tip_open;
	int      tip_actions;
	int      tip_close_reason;
	int      tree_sel;
	int      tree_invoke;
	bool     tree_docs_expanded;
	bool     tree_pictures_expanded;
	int      items_sel;
	int      items_invoke;
	bool     refresh_busy;
	int      refresh_count;
	int      pp_badge;
	bool     pp_group;
	int      pager_page;
	bool     split_open;
	int      split_mode;
	bool     split_right;
	int      tb_back_clicks;
	int      tb_pane_clicks;
	int      check_state;
	float    slider_vert;
	float    slider_range_lo;
	float    slider_range_hi;
	int      combo_edit_sel;
	char    *combo_submit;
	int      det_order [DET_COLS];
	float    det_width [DET_COLS];
	int      det_sort;
	bool     det_desc;
	int      cb_last;
	bool     cb_open;
	bool     cb_fav;
	bool     cb_bold;
	bool     dd_hover;
	bool     dd_files;
	char    *dd_drop;
	int      dd_done;
	char     dd_exe [1024];
	int      nav_demo_sel;
	bool     nav_demo_hide;
	int      nav_demo_mode;
	bool     nav_demo_hide_header;
	int      prev_page;
	int      motion_imp_step;
	uint32_t motion_imp_mask;
	int      motion_theme_kind;
	int      motion_item_ids [8];
	int      motion_item_count;
	int      motion_item_next;
	int      motion_frame_page;
	int      motion_frame_kind;
	bool     motion_frame_back;
	int      motion_conn_kind;
	bool     motion_conn_detail;
	int      motion_reduced_override;
} Model;

void                     gallery_model_init(Model *m);

extern char const *const kDetHeaders [DET_COLS];
extern char const *const kCbCommands [CB_COUNT];

char const              *gallery_dd_prompt(Model const *m);
char const              *gallery_dd_result(Model const *m);
char const              *gallery_dd_completed(Model const *m);
char const              *gallery_det_cell(int row, int col);
void                     gallery_det_rows(Model const *m, int out [DET_ROWS]);
float                    gallery_det_fit(int col);
void                     gallery_model_update(Model *m, XtkMsg msg);
int                      gallery_search_items(char const *query, char const *out [CONTROL_SEARCH_LIMIT]);

#endif
