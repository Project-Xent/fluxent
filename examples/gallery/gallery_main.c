#include "gallery.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static void tab_close(Model *m, int index) {
	if (index < 0 || index >= m->tab_count || m->tab_count <= 1) return;
	memmove(&m->tab_ids [index], &m->tab_ids [index + 1], sizeof(int) * ( size_t ) (m->tab_count - index - 1));
	m->tab_count--;
	if (m->tab_sel >= m->tab_count) m->tab_sel = m->tab_count - 1;
	m->tab_gen++;
}

static void tab_add(Model *m) {
	if (m->tab_count >= 8) return;
	m->tab_ids [m->tab_count] = m->next_tab_id++;
	m->tab_sel                = m->tab_count;
	m->tab_count++;
	m->tab_gen++;
}

static void update(void *model, XtkMsg msg) {
	Model *m = ( Model * ) model;
	switch (msg.tag) {
	case MSG_NAV            : m->page = msg.i; break;
	case MSG_CLICK          : m->clicks++; break;
	case MSG_BUTTON_DISABLE : m->button_disabled = msg.b; break;
	case MSG_CHECK_A        : m->check_a = msg.b; break;
	case MSG_CHECK_B        : m->check_b = msg.b; break;
	case MSG_RADIO_0 :
	case MSG_RADIO_1 :
	case MSG_RADIO_2 :
		if (msg.b) m->radio = msg.tag - MSG_RADIO_0;
		break;
	case MSG_COMBO        : m->combo_sel = msg.i; break;
	case MSG_COMBO_FONT   : m->combo_font = msg.i; break;
	case MSG_SLIDER_BASIC : m->slider_basic = msg.f; break;
	case MSG_SLIDER_STEP  : m->slider_step = msg.f; break;
	case MSG_SLIDER_TICK  : m->slider_tick = msg.f; break;
	case MSG_LIGHT        : m->light_on = msg.b; break;
	case MSG_NAME :
		free(m->name);
		m->name = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
		break;
	case MSG_PASSWORD : m->pass_chars = msg.i; break;
	case MSG_QUANTITY     : m->quantity = msg.d; break;
	case MSG_BAR_CLOSE :
		if (msg.i >= 0 && msg.i < BAR_COUNT) m->bar_open [msg.i] = false;
		break;
	case MSG_BARS_RESET :
		for (int i = 0; i < BAR_COUNT; i++) m->bar_open [i] = true;
		break;
	case MSG_BADGE_DELTA :
		m->badge_value += msg.i;
		if (m->badge_value < 0) m->badge_value = 0;
		break;
	case MSG_PROGRESS      : m->progress = msg.f; break;
	case MSG_INDETERMINATE : m->indeterminate = msg.b; break;
	case MSG_MENU          : m->menu_choice = msg.i; break;
	case MSG_REPEAT        : m->repeats++; break;
	case MSG_EXPAND        : m->expanded = msg.b; break;
	case MSG_SPLIT_PRIMARY : m->split_action = 1; break;
	case MSG_SPLIT_MENU    : m->split_action = msg.i + 2; break;
	case MSG_TAB_SELECT    : m->tab_sel = msg.i; break;
	case MSG_TAB_CLOSE : tab_close(m, msg.i); break;
	case MSG_TAB_ADD   : tab_add(m); break;
	case MSG_DIALOG_OPEN   : m->dialog_open = true; break;
	case MSG_DIALOG_RESULT :
		m->dialog_open   = false;
		m->dialog_result = msg.i;
		break;
	case MSG_LIST_SELECT    : m->list_sel = msg.i; break;
	case MSG_MULTI_SELECT   : m->multi_lead = msg.i; break;
	case MSG_GRID_SELECT    : m->grid_sel = msg.i; break;
	case MSG_LISTBOX_SELECT : m->listbox_sel = msg.i; break;
	case MSG_FLIP_SELECT    : m->flip_page = msg.i; break;
	case MSG_ASB_TEXT :
		free(m->asb_text);
		m->asb_text   = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
		m->asb_chosen = -1;
		break;
	case MSG_ASB_QUERY :
		free(m->asb_query);
		m->asb_query = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
		break;
	case MSG_ASB_CHOSEN : m->asb_chosen = msg.i; break;
	case MSG_RADIO_BUTTONS : m->radio_buttons_sel = msg.i; break;
	case MSG_RATING :
		m->rating_value = msg.d;
		break;
	case MSG_TOGGLE_SPLIT      : m->toggle_split_checked = msg.b; break;
	case MSG_TOGGLE_SPLIT_CLICK: m->toggle_split_menu    = 0; break;
	case MSG_TOGGLE_SPLIT_MENU : m->toggle_split_menu    = msg.i + 1; break;
	case MSG_BREADCRUMB   : m->breadcrumb_click = msg.i; break;
	case MSG_SELECTOR_BAR : m->selector_sel     = msg.i; break;
	case MSG_TIP_OPEN     : m->tip_open = !m->tip_open; break;
	case MSG_TIP_CLOSE :
		m->tip_open         = false;
		m->tip_close_reason = msg.i;
		break;
	case MSG_TIP_ACTION : m->tip_actions++; break;
	case MSG_TREE_SELECT :
		m->tree_sel = msg.i >= 0 ? msg.i : -1;
		break;
	case MSG_TREE_EXPAND : break;
	case MSG_TREE_INVOKE : m->tree_invoke = msg.i; break;
	case MSG_ITEMS_SELECT: m->items_sel   = msg.i; break;
	case MSG_ITEMS_INVOKE: m->items_invoke = msg.i; break;
	case MSG_REFRESH :
		m->refresh_busy  = true;
		m->refresh_count++;
		break;
	case MSG_REFRESH_DONE : m->refresh_busy = false; break;
	case MSG_PP_BADGE :
		m->pp_badge += msg.i;
		if (m->pp_badge < 0) m->pp_badge = 0;
		break;
	case MSG_PP_GROUP : m->pp_group = msg.b; break;
	case MSG_PAGER_SELECT : m->pager_page = msg.i; break;
	case MSG_SPLIT_TOGGLE : m->split_open = !m->split_open; break;
	case MSG_SPLIT_MODE   : m->split_mode = msg.i; break;
	case MSG_SPLIT_PLACE  : m->split_right = msg.b; break;
	default : break;
	}
}

static XtkEl *page_view(XtkUi *ui, Model const *m) {
	switch (m->page) {
	case PAGE_CAT_BASIC :
		return page_category(ui, "Basic input", "Buttons, checkboxes, radios, combos, sliders, and switches.");
	case PAGE_CAT_TEXT : return page_category(ui, "Text", "Text entry and display: TextBox, NumberBox, Typography.");
	case PAGE_CAT_STATUS : return page_category(ui, "Status & info", "InfoBar, InfoBadge, progress, and tooltips.");
	case PAGE_CAT_MEDIA  : return page_category(ui, "Media", "Images decoded through WIC.");
	case PAGE_CAT_MENUS  : return page_category(ui, "Menus & layout", "Menus, tabs, links, and layout containers.");
	case PAGE_CAT_DIALOGS : return page_category(ui, "Dialogs & flyouts", "Modal dialogs and popup surfaces.");
	case PAGE_BUTTON      : return page_button(ui, m);
	case PAGE_SPLIT       : return page_split(ui, m);
	case PAGE_CHECKBOX    : return page_checkbox(ui, m);
	case PAGE_RADIO       : return page_radio(ui, m);
	case PAGE_COMBO       : return page_combo(ui, m);
	case PAGE_SLIDER      : return page_slider(ui, m);
	case PAGE_TOGGLE      : return page_toggle(ui, m);
	case PAGE_RADIO_BUTTONS : return page_radio_buttons(ui, m);
	case PAGE_RATING      : return page_rating(ui, m);
	case PAGE_TOGGLE_SPLIT: return page_toggle_split(ui, m);
	case PAGE_TEXTBOX     : return page_textbox(ui, m);
	case PAGE_NUMBERBOX   : return page_numberbox(ui, m);
	case PAGE_TYPOGRAPHY  : return page_typography(ui, m);
	case PAGE_INFOBAR     : return page_infobar(ui, m);
	case PAGE_BADGE       : return page_badge(ui, m);
	case PAGE_PROGRESS    : return page_progress(ui, m);
	case PAGE_TOOLTIP     : return page_tooltip(ui, m);
	case PAGE_IMAGE       : return page_image(ui, m);
	case PAGE_PERSON_PICTURE : return page_person_picture(ui, m);
	case PAGE_PAGER       : return page_pager(ui, m);
	case PAGE_CAT_SHELL   : return page_category(ui, "Layout & shell", "SplitView and window scaffolding.");
	case PAGE_SPLIT_VIEW  : return page_split_view(ui, m);
	case PAGE_MENUS       : return page_menus(ui, m);
	case PAGE_TABVIEW     : return page_tabview(ui, m);
	case PAGE_EXPANDER    : return page_expander(ui, m);
	case PAGE_BREADCRUMB  : return page_breadcrumb(ui, m);
	case PAGE_SELECTOR_BAR: return page_selector_bar(ui, m);
	case PAGE_CAT_COLLECTIONS :
		return page_category(ui, "Collections", "ListView, GridView, ListBox, TreeView, ItemsView, and PullToRefresh.");
	case PAGE_LISTVIEW    : return page_listview(ui, m);
	case PAGE_GRIDVIEW    : return page_gridview(ui, m);
	case PAGE_LISTBOX     : return page_listbox(ui, m);
	case PAGE_TREE_VIEW   : return page_tree_view(ui, m);
	case PAGE_ITEMS_VIEW  : return page_items_view(ui, m);
	case PAGE_PULL_REFRESH: return page_pull_refresh(ui, m);
	case PAGE_FLIPVIEW    : return page_flipview(ui, m);
	case PAGE_AUTOSUGGEST : return page_autosuggest(ui, m);
	case PAGE_DIALOG      : return page_dialog(ui, m);
	case PAGE_TEACHING_TIP: return page_teaching_tip(ui, m);
	case PAGE_SETTINGS    : return page_settings(ui, m);
	default              : return page_home(ui, m);
	}
}

static XtkEl *view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_nav_view(
	  ui,
	  (XtkNavDesc) {
		.items = kNavItems, .item_count = PAGE_COUNT, .selected = m->page, .on_select = xtk_msg(MSG_NAV)
    },
	  (XtkEl *[]) {
		xtk_grow(
		  xtk_scroll(
			ui,
			(XtkEl *[]) {
			  xtk_keyed(
				ui, xtk_fmt(ui, "page-%d", m->page),
				xtk_column(
				  ui, (XtkStackDesc) {.padding = {36, 28, 36, 28}, .align = XENT_FLEX_ALIGN_STRETCH},
				  (XtkEl *[]) {page_view(ui, m), XTK_END}
				)
			  ),
			  XTK_END}
		  ),
		  1.0f
		),
		XTK_END}
	);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	static Model model = {
	  .combo_sel    = -1,
	  .combo_font   = -1,
	  .slider_basic = 50.0f,
	  .slider_step  = 800.0f,
	  .slider_tick  = 40.0f,
	  .quantity     = 1.0,
	  .bar_open     = {true, true, true, true},
	  .tab_ids      = {1, 2, 3},
	  .tab_count    = 3,
	  .next_tab_id  = 4,
	  .dialog_result = -1,
	  .list_sel     = -1,
	  .multi_lead   = -1,
	  .grid_sel     = -1,
	  .listbox_sel  = -1,
	  .asb_chosen   = -1,
	  .badge_value  = 5,
	  .progress     = 60.0f,
	  .radio_buttons_sel = 0,
	  .rating_value      = -1.0,
	  .selector_sel      = 0,
	  .tree_sel          = -1,
	  .tree_invoke       = -1,
	  .items_sel         = -1,
	  .items_invoke      = -1,
	  .breadcrumb_click  = -1,
	  .tip_close_reason  = -1,
	};

	FluxAppConfig cfg = {
	  .title     = L"FluXent Gallery",
	  .width     = 1100,
	  .height    = 760,
	  .dark_mode = flux_theme_system_is_dark(),
	  .backdrop  = FLUX_BACKDROP_MICA,
	};
	return flux_run(&cfg, &model, update, view);
}
