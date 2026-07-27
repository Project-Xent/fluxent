#include "gallery_model.h"

#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
  #define strdup _strdup
#endif

XtkNavItemDesc const kNavItems [PAGE_COUNT] = {
  [PAGE_HOME]            = {XTK_NAV_ITEM,      "Home",            "Home"             },
  [PAGE_SEP_1]           = {XTK_NAV_SEPARATOR, NULL,              NULL               },
  [PAGE_CAT_BASIC]       = {XTK_NAV_ITEM,      "Keyboard",        "Basic input"      },
  [PAGE_BUTTON]          = {XTK_NAV_CHILD,     NULL,              "Button"           },
  [PAGE_SPLIT]           = {XTK_NAV_CHILD,     NULL,              "SplitButton"      },
  [PAGE_CHECKBOX]        = {XTK_NAV_CHILD,     NULL,              "CheckBox"         },
  [PAGE_RADIO]           = {XTK_NAV_CHILD,     NULL,              "RadioButton"      },
  [PAGE_COMBO]           = {XTK_NAV_CHILD,     NULL,              "ComboBox"         },
  [PAGE_SLIDER]          = {XTK_NAV_CHILD,     NULL,              "Slider"           },
  [PAGE_TOGGLE]          = {XTK_NAV_CHILD,     NULL,              "ToggleSwitch"     },
  [PAGE_RADIO_BUTTONS]   = {XTK_NAV_CHILD,     NULL,              "RadioButtons"     },
  [PAGE_RATING]          = {XTK_NAV_CHILD,     NULL,              "RatingControl"    },
  [PAGE_TOGGLE_SPLIT]    = {XTK_NAV_CHILD,     NULL,              "ToggleSplitButton"},
  [PAGE_CAT_TEXT]        = {XTK_NAV_ITEM,      "Font",            "Text"             },
  [PAGE_TEXTBOX]         = {XTK_NAV_CHILD,     NULL,              "TextBox"          },
  [PAGE_AUTOSUGGEST]     = {XTK_NAV_CHILD,     NULL,              "AutoSuggestBox"   },
  [PAGE_NUMBERBOX]       = {XTK_NAV_CHILD,     NULL,              "NumberBox"        },
  [PAGE_TYPOGRAPHY]      = {XTK_NAV_CHILD,     NULL,              "Typography"       },
  [PAGE_CAT_STATUS]      = {XTK_NAV_ITEM,      "View",            "Status & info"    },
  [PAGE_INFOBAR]         = {XTK_NAV_CHILD,     NULL,              "InfoBar"          },
  [PAGE_BADGE]           = {XTK_NAV_CHILD,     NULL,              "InfoBadge"        },
  [PAGE_PROGRESS]        = {XTK_NAV_CHILD,     NULL,              "Progress"         },
  [PAGE_TOOLTIP]         = {XTK_NAV_CHILD,     NULL,              "ToolTip"          },
  [PAGE_CAT_MEDIA]       = {XTK_NAV_ITEM,      "Camera",          "Media"            },
  [PAGE_IMAGE]           = {XTK_NAV_CHILD,     NULL,              "Image"            },
  [PAGE_PERSON_PICTURE]  = {XTK_NAV_CHILD,     NULL,              "PersonPicture"    },
  [PAGE_FLIPVIEW]        = {XTK_NAV_CHILD,     NULL,              "FlipView"         },
  [PAGE_PAGER]           = {XTK_NAV_CHILD,     NULL,              "PagerControl"     },
  [PAGE_CAT_MENUS]       = {XTK_NAV_ITEM,      "More",            "Menus & layout"   },
  [PAGE_MENUS]           = {XTK_NAV_CHILD,     NULL,              "Menus & links"    },
  [PAGE_TABVIEW]         = {XTK_NAV_CHILD,     NULL,              "TabView"          },
  [PAGE_EXPANDER]        = {XTK_NAV_CHILD,     NULL,              "Expander"         },
  [PAGE_BREADCRUMB]      = {XTK_NAV_CHILD,     NULL,              "BreadcrumbBar"    },
  [PAGE_SELECTOR_BAR]    = {XTK_NAV_CHILD,     NULL,              "SelectorBar"      },
  [PAGE_CAT_COLLECTIONS] = {XTK_NAV_ITEM,      "List",            "Collections"      },
  [PAGE_LISTVIEW]        = {XTK_NAV_CHILD,     NULL,              "ListView"         },
  [PAGE_GRIDVIEW]        = {XTK_NAV_CHILD,     NULL,              "GridView"         },
  [PAGE_LISTBOX]         = {XTK_NAV_CHILD,     NULL,              "ListBox"          },
  [PAGE_TREE_VIEW]       = {XTK_NAV_CHILD,     NULL,              "TreeView"         },
  [PAGE_ITEMS_VIEW]      = {XTK_NAV_CHILD,     NULL,              "ItemsView"        },
  [PAGE_PULL_REFRESH]    = {XTK_NAV_CHILD,     NULL,              "PullToRefresh"    },
  [PAGE_CAT_SHELL]       = {XTK_NAV_ITEM,      "GlobalNavButton", "Layout & shell"   },
  [PAGE_SPLIT_VIEW]      = {XTK_NAV_CHILD,     NULL,              "SplitView"        },
  [PAGE_TITLE_BAR]       = {XTK_NAV_CHILD,     NULL,              "TitleBar"         },
  [PAGE_CAT_DIALOGS]     = {XTK_NAV_ITEM,      "Flag",            "Dialogs & flyouts"},
  [PAGE_DIALOG]          = {XTK_NAV_CHILD,     NULL,              "ContentDialog"    },
  [PAGE_TEACHING_TIP]    = {XTK_NAV_CHILD,     NULL,              "TeachingTip"      },
  [PAGE_SETTINGS]        = {XTK_NAV_FOOTER,    "Settings",        "Settings"         },
};

void gallery_model_init(Model *m) {
	memset(m, 0, sizeof(*m));
	m->combo_sel    = -1;
	m->combo_font   = -1;
	m->slider_basic = 50.0f;
	m->slider_step  = 800.0f;
	m->slider_tick  = 40.0f;
	m->quantity     = 1.0;
	m->bar_open [0] = m->bar_open [1] = m->bar_open [2] = m->bar_open [3] = true;
	m->tab_ids [0]                                                        = 1;
	m->tab_ids [1]                                                        = 2;
	m->tab_ids [2]                                                        = 3;
	m->ntab                                                               = 3;
	m->next_tab_id                                                        = 4;
	m->dialog_result                                                      = -1;
	m->list_sel                                                           = -1;
	m->multi_lead                                                         = -1;
	m->grid_sel                                                           = -1;
	m->listbox_sel                                                        = -1;
	m->asb_chosen                                                         = -1;
	m->badge_value                                                        = 5;
	m->progress                                                           = 60.0f;
	m->radio_buttons_sel                                                  = 0;
	m->rating_value                                                       = -1.0;
	m->selector_sel                                                       = 0;
	m->tree_sel                                                           = -1;
	m->tree_invoke                                                        = -1;
	m->tree_docs_expanded                                                 = true;
	m->items_sel                                                          = -1;
	m->items_invoke                                                       = -1;
	m->breadcrumb_click                                                   = -1;
	m->tip_close_reason                                                   = -1;
}

static int tab_selection_after_close(Model const *m, int index) {
	if (m->ntab == 1) return -1;
	if (m->tab_sel > index) return m->tab_sel - 1;
	if (m->tab_sel != index) return m->tab_sel;
	return index < m->ntab - 1 ? index : m->ntab - 2;
}

static void tab_close(Model *m, int index) {
	if (index < 0 || index >= m->ntab) return;
	int selection = tab_selection_after_close(m, index);
	memmove(&m->tab_ids [index], &m->tab_ids [index + 1], sizeof(int) * ( size_t ) (m->ntab - index - 1));
	m->ntab--;
	m->tab_sel = selection;
	m->tab_gen++;
}

static void tab_add(Model *m) {
	if (m->ntab >= 8) return;
	m->tab_ids [m->ntab] = m->next_tab_id++;
	m->tab_sel           = m->ntab;
	m->ntab++;
	m->tab_gen++;
}

typedef void (*GalleryMsgFn)(Model *m, XtkMsg msg);

static void  msg_nav(Model *m, XtkMsg msg) { m->page = msg.i; }

static void  msg_click(Model *m, XtkMsg msg) {
	( void ) msg;
	m->clicks++;
}

static void msg_button_disable(Model *m, XtkMsg msg) { m->button_disabled = msg.b; }

static void msg_check_a(Model *m, XtkMsg msg) { m->check_a = msg.b; }

static void msg_check_b(Model *m, XtkMsg msg) { m->check_b = msg.b; }

static void msg_radio(Model *m, XtkMsg msg) {
	if (msg.b) m->radio = msg.tag - MSG_RADIO_0;
}

static void msg_combo(Model *m, XtkMsg msg) { m->combo_sel = msg.i; }

static void msg_combo_font(Model *m, XtkMsg msg) { m->combo_font = msg.i; }

static void msg_slider_basic(Model *m, XtkMsg msg) { m->slider_basic = msg.f; }

static void msg_slider_step(Model *m, XtkMsg msg) { m->slider_step = msg.f; }

static void msg_slider_tick(Model *m, XtkMsg msg) { m->slider_tick = msg.f; }

static void msg_light(Model *m, XtkMsg msg) { m->light_on = msg.b; }

static void msg_name(Model *m, XtkMsg msg) {
	free(m->name);
	m->name = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
}

static void msg_password(Model *m, XtkMsg msg) {
	free(m->password);
	m->password   = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
	m->pass_chars = m->password ? ( int ) strlen(m->password) : 0;
}

static void msg_quantity(Model *m, XtkMsg msg) { m->quantity = msg.d; }

static void msg_bar_close(Model *m, XtkMsg msg) {
	if (msg.i >= 0 && msg.i < BAR_COUNT) m->bar_open [msg.i] = false;
}

static void msg_bars_reset(Model *m, XtkMsg msg) {
	( void ) msg;
	for (int i = 0; i < BAR_COUNT; i++) m->bar_open [i] = true;
}

static void msg_badge_delta(Model *m, XtkMsg msg) {
	m->badge_value += msg.i;
	if (m->badge_value < 0) m->badge_value = 0;
}

static void msg_progress(Model *m, XtkMsg msg) { m->progress = msg.f; }

static void msg_indeterminate(Model *m, XtkMsg msg) { m->indeterminate = msg.b; }

static void msg_menu(Model *m, XtkMsg msg) { m->menu_choice = msg.i; }

static void msg_repeat(Model *m, XtkMsg msg) {
	( void ) msg;
	m->repeats++;
}

static void msg_expand(Model *m, XtkMsg msg) { m->expanded = msg.b; }

static void msg_split_primary(Model *m, XtkMsg msg) {
	( void ) msg;
	m->split_action = 1;
}

static void msg_split_menu(Model *m, XtkMsg msg) { m->split_action = msg.i + 2; }

static void msg_tab_select(Model *m, XtkMsg msg) { m->tab_sel = msg.i; }

static void msg_tab_close(Model *m, XtkMsg msg) { tab_close(m, msg.i); }

static void msg_tab_add(Model *m, XtkMsg msg) {
	( void ) msg;
	tab_add(m);
}

static void msg_dialog_open(Model *m, XtkMsg msg) {
	( void ) msg;
	m->dialog_open = true;
}

static void msg_dialog_result(Model *m, XtkMsg msg) {
	m->dialog_open   = false;
	m->dialog_result = msg.i;
}

static void msg_list_select(Model *m, XtkMsg msg) { m->list_sel = msg.i; }

static void msg_multi_select(Model *m, XtkMsg msg) { m->multi_lead = msg.i; }

static void msg_grid_select(Model *m, XtkMsg msg) { m->grid_sel = msg.i; }

static void msg_listbox_select(Model *m, XtkMsg msg) { m->listbox_sel = msg.i; }

static void msg_flip_select(Model *m, XtkMsg msg) { m->flip_page = msg.i; }

static void msg_asb_text(Model *m, XtkMsg msg) {
	free(m->asb_text);
	m->asb_text   = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
	m->asb_chosen = -1;
}

static void msg_asb_query(Model *m, XtkMsg msg) {
	free(m->asb_query);
	m->asb_query = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
}

static void msg_asb_chosen(Model *m, XtkMsg msg) { m->asb_chosen = msg.i; }

static void msg_radio_buttons(Model *m, XtkMsg msg) { m->radio_buttons_sel = msg.i; }

static void msg_rating(Model *m, XtkMsg msg) { m->rating_value = msg.d; }

static void msg_toggle_split(Model *m, XtkMsg msg) { m->toggle_split_checked = msg.b; }

static void msg_toggle_split_click(Model *m, XtkMsg msg) {
	( void ) msg;
	m->toggle_split_menu = 0;
}

static void msg_toggle_split_menu(Model *m, XtkMsg msg) { m->toggle_split_menu = msg.i + 1; }

static void msg_breadcrumb(Model *m, XtkMsg msg) { m->breadcrumb_click = msg.i; }

static void msg_selector_bar(Model *m, XtkMsg msg) { m->selector_sel = msg.i; }

static void msg_tip_open(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tip_open = !m->tip_open;
}

static void msg_tip_close(Model *m, XtkMsg msg) {
	m->tip_open         = false;
	m->tip_close_reason = msg.i;
}

static void msg_tip_action(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tip_actions++;
}

static void msg_tree_select(Model *m, XtkMsg msg) { m->tree_sel = msg.i >= 0 ? msg.i : -1; }

static void msg_tree_expand(Model *m, XtkMsg msg) {
	if (msg.i == 0) {
		m->tree_docs_expanded = msg.b;
		return;
	}
	int pictures = m->tree_docs_expanded ? 3 : 1;
	if (msg.i == pictures) m->tree_pictures_expanded = msg.b;
}

static void msg_tree_invoke(Model *m, XtkMsg msg) { m->tree_invoke = msg.i; }

static void msg_items_select(Model *m, XtkMsg msg) { m->items_sel = msg.i; }

static void msg_items_invoke(Model *m, XtkMsg msg) { m->items_invoke = msg.i; }

static void msg_refresh(Model *m, XtkMsg msg) {
	( void ) msg;
	m->refresh_busy = true;
	m->refresh_count++;
}

static void msg_refresh_done(Model *m, XtkMsg msg) {
	( void ) msg;
	m->refresh_busy = false;
}

static void msg_pp_badge(Model *m, XtkMsg msg) {
	m->pp_badge += msg.i;
	if (m->pp_badge < 0) m->pp_badge = 0;
}

static void msg_pp_group(Model *m, XtkMsg msg) { m->pp_group = msg.b; }

static void msg_pager_select(Model *m, XtkMsg msg) { m->pager_page = msg.i; }

static void msg_split_toggle(Model *m, XtkMsg msg) {
	( void ) msg;
	m->split_open = !m->split_open;
}

static void msg_split_mode(Model *m, XtkMsg msg) { m->split_mode = msg.i; }

static void msg_split_place(Model *m, XtkMsg msg) { m->split_right = msg.b; }

static void msg_tb_back(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tb_back_clicks++;
}

static void msg_tb_pane(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tb_pane_clicks++;
}

static GalleryMsgFn const k_msg_fn [] = {
  [MSG_NAV]                = msg_nav,
  [MSG_CLICK]              = msg_click,
  [MSG_BUTTON_DISABLE]     = msg_button_disable,
  [MSG_CHECK_A]            = msg_check_a,
  [MSG_CHECK_B]            = msg_check_b,
  [MSG_RADIO_0]            = msg_radio,
  [MSG_RADIO_1]            = msg_radio,
  [MSG_RADIO_2]            = msg_radio,
  [MSG_COMBO]              = msg_combo,
  [MSG_COMBO_FONT]         = msg_combo_font,
  [MSG_SLIDER_BASIC]       = msg_slider_basic,
  [MSG_SLIDER_STEP]        = msg_slider_step,
  [MSG_SLIDER_TICK]        = msg_slider_tick,
  [MSG_LIGHT]              = msg_light,
  [MSG_NAME]               = msg_name,
  [MSG_PASSWORD]           = msg_password,
  [MSG_QUANTITY]           = msg_quantity,
  [MSG_BAR_CLOSE]          = msg_bar_close,
  [MSG_BARS_RESET]         = msg_bars_reset,
  [MSG_BADGE_DELTA]        = msg_badge_delta,
  [MSG_PROGRESS]           = msg_progress,
  [MSG_INDETERMINATE]      = msg_indeterminate,
  [MSG_MENU]               = msg_menu,
  [MSG_REPEAT]             = msg_repeat,
  [MSG_EXPAND]             = msg_expand,
  [MSG_SPLIT_PRIMARY]      = msg_split_primary,
  [MSG_SPLIT_MENU]         = msg_split_menu,
  [MSG_TAB_SELECT]         = msg_tab_select,
  [MSG_TAB_CLOSE]          = msg_tab_close,
  [MSG_TAB_ADD]            = msg_tab_add,
  [MSG_DIALOG_OPEN]        = msg_dialog_open,
  [MSG_DIALOG_RESULT]      = msg_dialog_result,
  [MSG_LIST_SELECT]        = msg_list_select,
  [MSG_MULTI_SELECT]       = msg_multi_select,
  [MSG_GRID_SELECT]        = msg_grid_select,
  [MSG_LISTBOX_SELECT]     = msg_listbox_select,
  [MSG_FLIP_SELECT]        = msg_flip_select,
  [MSG_ASB_TEXT]           = msg_asb_text,
  [MSG_ASB_QUERY]          = msg_asb_query,
  [MSG_ASB_CHOSEN]         = msg_asb_chosen,
  [MSG_RADIO_BUTTONS]      = msg_radio_buttons,
  [MSG_RATING]             = msg_rating,
  [MSG_TOGGLE_SPLIT]       = msg_toggle_split,
  [MSG_TOGGLE_SPLIT_CLICK] = msg_toggle_split_click,
  [MSG_TOGGLE_SPLIT_MENU]  = msg_toggle_split_menu,
  [MSG_BREADCRUMB]         = msg_breadcrumb,
  [MSG_SELECTOR_BAR]       = msg_selector_bar,
  [MSG_TIP_OPEN]           = msg_tip_open,
  [MSG_TIP_CLOSE]          = msg_tip_close,
  [MSG_TIP_ACTION]         = msg_tip_action,
  [MSG_TREE_SELECT]        = msg_tree_select,
  [MSG_TREE_EXPAND]        = msg_tree_expand,
  [MSG_TREE_INVOKE]        = msg_tree_invoke,
  [MSG_ITEMS_SELECT]       = msg_items_select,
  [MSG_ITEMS_INVOKE]       = msg_items_invoke,
  [MSG_REFRESH]            = msg_refresh,
  [MSG_REFRESH_DONE]       = msg_refresh_done,
  [MSG_PP_BADGE]           = msg_pp_badge,
  [MSG_PP_GROUP]           = msg_pp_group,
  [MSG_PAGER_SELECT]       = msg_pager_select,
  [MSG_SPLIT_TOGGLE]       = msg_split_toggle,
  [MSG_SPLIT_MODE]         = msg_split_mode,
  [MSG_SPLIT_PLACE]        = msg_split_place,
  [MSG_TB_BACK]            = msg_tb_back,
  [MSG_TB_PANE]            = msg_tb_pane,
};

void gallery_model_update(Model *m, XtkMsg msg) {
	if (( unsigned ) msg.tag >= sizeof(k_msg_fn) / sizeof(k_msg_fn [0])) return;
	GalleryMsgFn fn = k_msg_fn [msg.tag];
	if (fn) fn(m, msg);
}
