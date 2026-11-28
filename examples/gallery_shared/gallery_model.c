#include "gallery_model.h"

#include <fluxent/flux_window.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static bool settings_path(wchar_t *buf, size_t cap) {
	wchar_t const *base = _wgetenv(L"LOCALAPPDATA");
	return base && base [0] && swprintf(buf, cap, L"%ls\\fluxent-gallery.ini", base) > 0;
}

int gallery_backdrop_load(void) {
	wchar_t path [MAX_PATH];
	FILE   *f = settings_path(path, MAX_PATH) ? _wfopen(path, L"rb") : NULL;
	int     v = 1;
	if (!f) return v;
	if (fscanf(f, "backdrop=%d", &v) != 1 || v < 0 || v > 3) v = 1;
	fclose(f);
	return v;
}

void gallery_backdrop_apply(FluxWindow *win, int backdrop) {
	if (win) flux_window_setbackdrop(win, backdrop);
	wchar_t path [MAX_PATH];
	FILE   *f = settings_path(path, MAX_PATH) ? _wfopen(path, L"wb") : NULL;
	if (!f) return;
	fprintf(f, "backdrop=%d\n", backdrop);
	fclose(f);
}

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
  [PAGE_SCROLL_BAR]      = {XTK_NAV_CHILD,     NULL,              "ScrollBar"        },
  [PAGE_LOOPING]         = {XTK_NAV_CHILD,     NULL,              "LoopingSelector"  },
  [PAGE_DATE_PICKER]     = {XTK_NAV_CHILD,     NULL,              "DatePicker"       },
  [PAGE_TIME_PICKER]     = {XTK_NAV_CHILD,     NULL,              "TimePicker"       },
  [PAGE_CALENDAR]        = {XTK_NAV_CHILD,     NULL,              "CalendarView"     },
  [PAGE_COLOR_PICKER]    = {XTK_NAV_CHILD,     NULL,              "ColorPicker"      },
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
  [PAGE_COMMANDBAR]      = {XTK_NAV_CHILD,     NULL,              "CommandBar"       },
  [PAGE_TABVIEW]         = {XTK_NAV_CHILD,     NULL,              "TabView"          },
  [PAGE_EXPANDER]        = {XTK_NAV_CHILD,     NULL,              "Expander"         },
  [PAGE_BREADCRUMB]      = {XTK_NAV_CHILD,     NULL,              "BreadcrumbBar"    },
  [PAGE_SELECTOR_BAR]    = {XTK_NAV_CHILD,     NULL,              "SelectorBar"      },
  [PAGE_CAT_COLLECTIONS] = {XTK_NAV_ITEM,      "List",            "Collections"      },
  [PAGE_LISTVIEW]        = {XTK_NAV_CHILD,     NULL,              "ListView"         },
  [PAGE_GRIDVIEW]        = {XTK_NAV_CHILD,     NULL,              "GridView"         },
  [PAGE_LISTBOX]         = {XTK_NAV_CHILD,     NULL,              "ListBox"          },
  [PAGE_DETAILS]         = {XTK_NAV_CHILD,     NULL,              "Details header"   },
  [PAGE_TREE_VIEW]       = {XTK_NAV_CHILD,     NULL,              "TreeView"         },
  [PAGE_ITEMS_VIEW]      = {XTK_NAV_CHILD,     NULL,              "ItemsView"        },
  [PAGE_PULL_REFRESH]    = {XTK_NAV_CHILD,     NULL,              "PullToRefresh"    },
  [PAGE_CAT_SHELL]       = {XTK_NAV_ITEM,      "GlobalNavButton", "Layout & shell"   },
  [PAGE_SPLIT_VIEW]      = {XTK_NAV_CHILD,     NULL,              "SplitView"        },
  [PAGE_NAVIGATION_VIEW] = {XTK_NAV_CHILD,     NULL,              "NavigationView"   },
  [PAGE_TITLE_BAR]       = {XTK_NAV_CHILD,     NULL,              "TitleBar"         },
  [PAGE_DRAG_DROP]       = {XTK_NAV_CHILD,     NULL,              "Drag and drop"    },
  [PAGE_MATERIALS]       = {XTK_NAV_CHILD,     NULL,              "Materials"        },
  [PAGE_MOTION]          = {XTK_NAV_CHILD,     NULL,              "Motion"           },
  [PAGE_CAT_DIALOGS]     = {XTK_NAV_ITEM,      "Flag",            "Dialogs & flyouts"},
  [PAGE_DIALOG]          = {XTK_NAV_CHILD,     NULL,              "ContentDialog"    },
  [PAGE_TEACHING_TIP]    = {XTK_NAV_CHILD,     NULL,              "TeachingTip"      },
  [PAGE_SETTINGS]        = {XTK_NAV_FOOTER,    "Settings",        "Settings"         },
};

enum
{
	SEARCH_SUBSTRING = 1,
	SEARCH_PREFIX,
	SEARCH_EXACT,
};

static struct {
	int         page;
	char const *label;
} const search_aliases [] = {
  {PAGE_TEXTBOX,      "PasswordBox"     },
  {PAGE_PROGRESS,     "ProgressBar"     },
  {PAGE_PROGRESS,     "ProgressRing"    },
  {PAGE_FLIPVIEW,     "PipsPager"       },
  {PAGE_MENUS,        "HyperlinkButton" },
  {PAGE_MENUS,        "MenuBar"         },
  {PAGE_MENUS,        "MenuFlyout"      },
  {PAGE_PULL_REFRESH, "RefreshContainer"},
};

static bool search_key(char out [128], char const *text) {
	if (!text) return false;
	size_t n = 0;
	for (; *text; text++) {
		unsigned char ch = ( unsigned char ) *text;
		if (!isalnum(ch)) continue;
		if (n == 127) return false;
		out [n++] = ( char ) tolower(ch);
	}
	out [n] = '\0';
	return n != 0;
}

static int search_match(char const *query, char const *label) {
	char key [128];
	if (!search_key(key, label)) return 0;
	char const *match = strstr(key, query);
	if (!match) return 0;
	if (match != key) return SEARCH_SUBSTRING;
	return strcmp(key, query) == 0 ? SEARCH_EXACT : SEARCH_PREFIX;
}

static int search_page_rank(char const *query, int page) {
	int best = search_match(query, kNavItems [page].label);
	for (size_t i = 0; i < sizeof(search_aliases) / sizeof(search_aliases [0]); i++) {
		if (search_aliases [i].page != page) continue;
		int rank = search_match(query, search_aliases [i].label);
		if (rank > best) best = rank;
	}
	return best;
}

static int search_append(char const *key, int rank, char const **out, int n) {
	for (int page = 0; page < PAGE_COUNT && n < CONTROL_SEARCH_LIMIT; page++) {
		char const *label = kNavItems [page].label;
		if (search_page_rank(key, page) == rank) out [n++] = label;
	}
	return n;
}

int gallery_search_items(char const *query, char const *out [CONTROL_SEARCH_LIMIT]) {
	char key [128];
	if (!search_key(key, query)) return 0;
	int n = 0;
	for (int rank = SEARCH_EXACT; rank >= SEARCH_SUBSTRING; rank--) n = search_append(key, rank, out, n);
	return n;
}

static int search_page(char const *query) {
	char key [128];
	if (!search_key(key, query)) return -1;
	int found = -1;
	for (int page = 0; page < PAGE_COUNT; page++) {
		int rank = search_page_rank(key, page);
		if (rank == SEARCH_EXACT) return page;
		if (rank) found = found == -1 ? page : -2;
	}
	return found;
}

void gallery_model_init(Model *m) {
	memset(m, 0, sizeof(*m));
	m->combo_sel    = -1;
	m->nav_demo_sel = GALLERY_NAV_DEMO_HOME;
	m->backdrop     = 1;
	m->combo_font   = -1;
	m->slider_basic = 50.0f;
	m->scrollbar_v  = 200.0;
	m->scrollbar_h  = 0.0;
	m->looping_sel  = 3;
	m->date_pick    = 20261012;
	m->time_pick    = 14 * 60 + 30;
	m->time_pick_24 = 9 * 60;
	m->cal_date     = 20261014;
	m->color_pick   = 0x0078d4ffu;
	m->color_alpha  = 0xe81123c0u;
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
	m->tree_sel                                                           = 0;
	m->tree_invoke                                                        = -1;
	m->tree_docs_expanded                                                 = true;
	m->items_sel                                                          = -1;
	m->items_invoke                                                       = -1;
	m->breadcrumb_click                                                   = -1;
	m->tip_close_reason                                                   = -1;
	m->check_state                                                        = XTK_CHECK_INDETERMINATE;
	m->slider_vert                                                        = 40.0f;
	for (int i = 0; i < DET_COLS; i++) m->det_order [i] = i;
	m->cb_last = -1;
	m->dd_done = -1;
	wchar_t exe [512];
	DWORD   n = GetModuleFileNameW(NULL, exe, 512);
	if (n && n < 512) WideCharToMultiByte(CP_UTF8, 0, exe, -1, m->dd_exe, sizeof(m->dd_exe), NULL, NULL);
	m->det_width [0]           = 260.0f;
	m->det_width [1]           = 200.0f;
	m->det_width [2]           = 160.0f;
	m->det_width [3]           = 120.0f;
	m->slider_range_lo         = 30.0f;
	m->slider_range_hi         = 70.0f;
	m->combo_edit_sel          = -1;
	m->prev_page               = PAGE_HOME;
	m->motion_imp_mask         = XTK_MOTION_ALL;
	m->motion_theme_kind       = XTK_THEME_TRANSITION_ENTRANCE;
	m->motion_item_ids [0]     = 1;
	m->motion_item_ids [1]     = 2;
	m->motion_item_ids [2]     = 3;
	m->motion_item_count       = 3;
	m->motion_item_next        = 4;
	m->motion_frame_kind       = XTK_NAV_TRANSITION_SLIDE_RIGHT;
	m->motion_reduced_override = -1;
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

static void  msg_nav(Model *m, XtkMsg msg) {
	if (msg.i == m->page) return;
	m->prev_page = m->page;
	m->page      = msg.i;
}

static void msg_click(Model *m, XtkMsg msg) {
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

static void msg_scrollbar_v(Model *m, XtkMsg msg) { m->scrollbar_v = msg.d; }

static void msg_scrollbar_h(Model *m, XtkMsg msg) { m->scrollbar_h = msg.d; }

static void msg_looping(Model *m, XtkMsg msg) { m->looping_sel = msg.i; }

static void msg_date_picker(Model *m, XtkMsg msg) { m->date_pick = msg.i; }

static void msg_time_picker(Model *m, XtkMsg msg) { m->time_pick = msg.i; }

static void msg_time_picker_24(Model *m, XtkMsg msg) { m->time_pick_24 = msg.i; }

static void msg_calendar(Model *m, XtkMsg msg) {
	if (msg.b) m->cal_date = msg.i;
	else if (m->cal_date == msg.i) m->cal_date = 0;
}

static void msg_calendar_multi(Model *m, XtkMsg msg) {
	int at = -1;
	for (int i = 0; i < m->cal_multi_count; i++)
		if (m->cal_multi [i] == msg.i) at = i;
	if (msg.b && at < 0 && m->cal_multi_count < 16) m->cal_multi [m->cal_multi_count++] = msg.i;
	if (!msg.b && at >= 0) m->cal_multi [at] = m->cal_multi [--m->cal_multi_count];
}

static void msg_color_picker(Model *m, XtkMsg msg) { m->color_pick = ( unsigned ) msg.i; }

static void msg_color_alpha(Model *m, XtkMsg msg) { m->color_alpha = ( unsigned ) msg.i; }

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

static void       msg_list_select(Model *m, XtkMsg msg) { m->list_sel = msg.i; }

static void       msg_multi_select(Model *m, XtkMsg msg) { m->multi_lead = msg.i; }

static void       msg_grid_select(Model *m, XtkMsg msg) { m->grid_sel = msg.i; }

char const *const kCbCommands [CB_COUNT]
  = {"Add", "Edit", "Share", "Save", "Delete", "Copy", "Cut", "Paste", "Settings", "Print"};
char const *const        kDetHeaders [DET_COLS]         = {"Name", "Date modified", "Type", "Size"};

static char const *const kDetCells [DET_ROWS][DET_COLS] = {
  {"Assets",           "2026/10/02 14:21", "File folder",        ""        },
  {"Builds",           "2026/11/12 09:03", "File folder",        ""        },
  {"design-notes.md",  "2026/11/14 22:47", "Markdown Source",    "18 KB"   },
  {"gallery.png",      "2026/09/30 11:15", "PNG File",           "412 KB"  },
  {"installer.exe",    "2026/11/15 08:30", "Application",        "818 KB"  },
  {"LICENSE",          "2026/08/14 10:00", "File",               "1 KB"    },
  {"notes.txt",        "2026/07/21 16:42", "Text Document",      "3 KB"    },
  {"palette.json",     "2026/10/18 13:05", "JSON Source File",   "6 KB"    },
  {"release.zip",      "2026/11/14 23:59", "Compressed Folder",  "1,154 KB"},
  {"roadmap.docx",     "2026/06/02 09:12", "Microsoft Word Doc", "54 KB"   },
  {"screenshot-2.png", "2026/11/01 20:40", "PNG File",           "980 KB"  },
  {"theme.xaml",       "2026/05/11 18:27", "XAML File",          "22 KB"   },
};

static long const kDetSizes [DET_ROWS] = {-1, -1, 18, 412, 818, 1, 3, 6, 1154, 54, 980, 22};

char const       *gallery_det_cell(int row, int col) { return kDetCells [row][col]; }

static int        det_compare(int col, int a, int b) {
	if (col == 3) return (kDetSizes [a] > kDetSizes [b]) - (kDetSizes [a] < kDetSizes [b]);
	return strcmp(kDetCells [a][col], kDetCells [b][col]);
}

static bool det_in_order(Model const *m, int a, int b) {
	int c = det_compare(m->det_sort, a, b);
	return m->det_desc ? c >= 0 : c <= 0;
}

void gallery_det_rows(Model const *m, int out [DET_ROWS]) {
	for (int i = 0; i < DET_ROWS; i++) out [i] = i;
	for (int i = 1; i < DET_ROWS; i++) {
		int v = out [i], j = i;
		for (; j > 0 && !det_in_order(m, out [j - 1], v); j--) out [j] = out [j - 1];
		out [j] = v;
	}
}

float gallery_det_fit(int col) {
	size_t longest = strlen(kDetHeaders [col]);
	for (int r = 0; r < DET_ROWS; r++) {
		size_t n = strlen(kDetCells [r][col]);
		if (n > longest) longest = n;
	}
	return ( float ) longest * 7.0f + 36.0f + 12.0f;
}

static void msg_det_sort(Model *m, XtkMsg msg) {
	m->det_desc = m->det_sort == msg.i ? !m->det_desc : false;
	m->det_sort = msg.i;
}

static void msg_det_resize(Model *m, XtkMsg msg) { m->det_width [msg.i] = msg.f; }

static void msg_det_fit(Model *m, XtkMsg msg) { m->det_width [msg.i] = gallery_det_fit(msg.i); }

static void msg_det_reorder(Model *m, XtkMsg msg) {
	int col = ( int ) msg.d, from = 0;
	while (from < DET_COLS && m->det_order [from] != col) from++;
	if (from == DET_COLS || msg.i < 0 || msg.i >= DET_COLS) return;
	int step = msg.i > from ? 1 : -1;
	for (int i = from; i != msg.i; i += step) m->det_order [i] = m->det_order [i + step];
	m->det_order [msg.i] = col;
}

static void msg_cb_action(Model *m, XtkMsg msg) {
	if (msg.i >= 0 && msg.i < CB_COUNT) m->cb_last = msg.i;
}

static void msg_cb_open(Model *m, XtkMsg msg) { m->cb_open = msg.b; }

static void msg_cb_fav(Model *m, XtkMsg msg) { m->cb_fav = msg.b; }

static void msg_cb_bold(Model *m, XtkMsg msg) { m->cb_bold = msg.b; }

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

static void msg_tree_select(Model *m, XtkMsg msg) { m->tree_sel = msg.i > 0 ? msg.i : 0; }

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

static void msg_split_open(Model *m, XtkMsg msg) { m->split_open = msg.b; }

static void msg_split_place(Model *m, XtkMsg msg) { m->split_right = msg.b; }

static void msg_tb_back(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tb_back_clicks++;
}

static void msg_tb_pane(Model *m, XtkMsg msg) {
	( void ) msg;
	m->tb_pane_clicks++;
}

static void msg_check_tri(Model *m, XtkMsg msg) { m->check_state = msg.i; }

static void msg_slider_vert(Model *m, XtkMsg msg) { m->slider_vert = msg.f; }

static void msg_slider_range(Model *m, XtkMsg msg) {
	m->slider_range_lo = msg.f;
	m->slider_range_hi = ( float ) msg.d;
}

static void msg_combo_edit(Model *m, XtkMsg msg) { m->combo_edit_sel = msg.i; }

static void msg_combo_submit(Model *m, XtkMsg msg) {
	free(m->combo_submit);
	m->combo_submit = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
}

static void msg_dd_hover(Model *m, XtkMsg msg) { m->dd_hover = msg.b; }

static void msg_dd_drop(Model *m, XtkMsg msg) {
	free(m->dd_drop);
	m->dd_drop  = msg.ptr ? strdup(( char const * ) msg.ptr) : NULL;
	m->dd_files = msg.tag == MSG_DD_FILES;
}

static void msg_dd_done(Model *m, XtkMsg msg) { m->dd_done = msg.i; }

static void msg_backdrop(Model *m, XtkMsg msg) {
	if (msg.i < 0 || msg.i > 3) return;
	m->backdrop = msg.i;
}

static void msg_rtl(Model *m, XtkMsg msg) { m->rtl = msg.b; }

static void msg_nav_style(Model *m, XtkMsg msg) { m->nav_style = msg.i > 0 || msg.b; }

static void msg_nav_demo(Model *m, XtkMsg msg) { m->nav_demo_sel = msg.i; }

static void msg_nav_demo_hide(Model *m, XtkMsg msg) { m->nav_demo_hide = msg.b || msg.i > 0; }

static void msg_nav_demo_mode(Model *m, XtkMsg msg) {
	if (( unsigned ) msg.i < GALLERY_NAV_DEMO_MODES) m->nav_demo_mode = msg.i;
}

static void msg_nav_demo_hide_header(Model *m, XtkMsg msg) { m->nav_demo_hide_header = msg.b || msg.i > 0; }

static void msg_nav_demo_pick(Model *m, XtkMsg msg) {
	( void ) msg;
	m->nav_demo_sel = XTK_NAV_SETTINGS;
}

static void msg_control_search_text(Model *m, XtkMsg msg) {
	snprintf(m->control_query, sizeof(m->control_query), "%s", msg.ptr ? ( char const * ) msg.ptr : "");
}

static void msg_control_search_query(Model *m, XtkMsg msg) {
	int page = search_page(msg.ptr);
	if (page < 0) return;
	if (page != m->page) {
		m->prev_page = m->page;
		m->page      = page;
	}
	m->control_query [0] = '\0';
}

static void msg_motion_imp_state(Model *m, XtkMsg msg) { m->motion_imp_step = msg.i; }

static void msg_motion_imp_mask(Model *m, XtkMsg msg) { m->motion_imp_mask ^= ( uint32_t ) msg.i; }

static void msg_motion_enter_kind(Model *m, XtkMsg msg) { m->motion_theme_kind = msg.i; }

static void msg_motion_item_add(Model *m, XtkMsg msg) {
	( void ) msg;
	if (m->motion_item_count < 8) m->motion_item_ids [m->motion_item_count++] = m->motion_item_next++;
}

static void msg_motion_item_remove(Model *m, XtkMsg msg) {
	( void ) msg;
	if (m->motion_item_count > 0) m->motion_item_count--;
}

static void msg_motion_item_shuffle(Model *m, XtkMsg msg) {
	( void ) msg;
	if (m->motion_item_count < 2) return;
	int first = m->motion_item_ids [0];
	for (int i = 0; i < m->motion_item_count - 1; i++) m->motion_item_ids [i] = m->motion_item_ids [i + 1];
	m->motion_item_ids [m->motion_item_count - 1] = first;
}

static void msg_motion_frame_kind(Model *m, XtkMsg msg) { m->motion_frame_kind = msg.i; }

static void msg_motion_frame_step(Model *m, XtkMsg msg) {
	m->motion_frame_back = msg.i < 0;
	m->motion_frame_page = (m->motion_frame_page + (msg.i < 0 ? 3 : 1)) % 4;
}

static void msg_motion_conn_kind(Model *m, XtkMsg msg) { m->motion_conn_kind = msg.i; }

static void msg_motion_conn_toggle(Model *m, XtkMsg msg) {
	( void ) msg;
	m->motion_conn_detail = !m->motion_conn_detail;
}

static void               msg_motion_reduced(Model *m, XtkMsg msg) { m->motion_reduced_override = msg.i; }

static GalleryMsgFn const k_msg_fn [] = {
  [MSG_NAV]                  = msg_nav,
  [MSG_CLICK]                = msg_click,
  [MSG_BUTTON_DISABLE]       = msg_button_disable,
  [MSG_CHECK_A]              = msg_check_a,
  [MSG_CHECK_B]              = msg_check_b,
  [MSG_RADIO_0]              = msg_radio,
  [MSG_RADIO_1]              = msg_radio,
  [MSG_RADIO_2]              = msg_radio,
  [MSG_COMBO]                = msg_combo,
  [MSG_COMBO_FONT]           = msg_combo_font,
  [MSG_SLIDER_BASIC]         = msg_slider_basic,
  [MSG_SLIDER_STEP]          = msg_slider_step,
  [MSG_SLIDER_TICK]          = msg_slider_tick,
  [MSG_SCROLLBAR_V]          = msg_scrollbar_v,
  [MSG_SCROLLBAR_H]          = msg_scrollbar_h,
  [MSG_LOOPING]              = msg_looping,
  [MSG_DATE_PICKER]          = msg_date_picker,
  [MSG_TIME_PICKER]          = msg_time_picker,
  [MSG_TIME_PICKER_24]       = msg_time_picker_24,
  [MSG_CALENDAR]             = msg_calendar,
  [MSG_CALENDAR_MULTI]       = msg_calendar_multi,
  [MSG_COLOR_PICKER]         = msg_color_picker,
  [MSG_COLOR_ALPHA]          = msg_color_alpha,
  [MSG_LIGHT]                = msg_light,
  [MSG_NAME]                 = msg_name,
  [MSG_PASSWORD]             = msg_password,
  [MSG_QUANTITY]             = msg_quantity,
  [MSG_BAR_CLOSE]            = msg_bar_close,
  [MSG_BARS_RESET]           = msg_bars_reset,
  [MSG_BADGE_DELTA]          = msg_badge_delta,
  [MSG_PROGRESS]             = msg_progress,
  [MSG_INDETERMINATE]        = msg_indeterminate,
  [MSG_MENU]                 = msg_menu,
  [MSG_REPEAT]               = msg_repeat,
  [MSG_EXPAND]               = msg_expand,
  [MSG_SPLIT_PRIMARY]        = msg_split_primary,
  [MSG_SPLIT_MENU]           = msg_split_menu,
  [MSG_TAB_SELECT]           = msg_tab_select,
  [MSG_TAB_CLOSE]            = msg_tab_close,
  [MSG_TAB_ADD]              = msg_tab_add,
  [MSG_DIALOG_OPEN]          = msg_dialog_open,
  [MSG_DIALOG_RESULT]        = msg_dialog_result,
  [MSG_LIST_SELECT]          = msg_list_select,
  [MSG_MULTI_SELECT]         = msg_multi_select,
  [MSG_GRID_SELECT]          = msg_grid_select,
  [MSG_LISTBOX_SELECT]       = msg_listbox_select,
  [MSG_DET_SORT]             = msg_det_sort,
  [MSG_DET_RESIZE]           = msg_det_resize,
  [MSG_DET_FIT]              = msg_det_fit,
  [MSG_DET_REORDER]          = msg_det_reorder,
  [MSG_CB_ACTION]            = msg_cb_action,
  [MSG_CB_OPEN]              = msg_cb_open,
  [MSG_CB_FAV]               = msg_cb_fav,
  [MSG_CB_BOLD]              = msg_cb_bold,
  [MSG_FLIP_SELECT]          = msg_flip_select,
  [MSG_ASB_TEXT]             = msg_asb_text,
  [MSG_ASB_QUERY]            = msg_asb_query,
  [MSG_ASB_CHOSEN]           = msg_asb_chosen,
  [MSG_RADIO_BUTTONS]        = msg_radio_buttons,
  [MSG_RATING]               = msg_rating,
  [MSG_TOGGLE_SPLIT]         = msg_toggle_split,
  [MSG_TOGGLE_SPLIT_CLICK]   = msg_toggle_split_click,
  [MSG_TOGGLE_SPLIT_MENU]    = msg_toggle_split_menu,
  [MSG_BREADCRUMB]           = msg_breadcrumb,
  [MSG_SELECTOR_BAR]         = msg_selector_bar,
  [MSG_TIP_OPEN]             = msg_tip_open,
  [MSG_TIP_CLOSE]            = msg_tip_close,
  [MSG_TIP_ACTION]           = msg_tip_action,
  [MSG_TREE_SELECT]          = msg_tree_select,
  [MSG_TREE_EXPAND]          = msg_tree_expand,
  [MSG_TREE_INVOKE]          = msg_tree_invoke,
  [MSG_ITEMS_SELECT]         = msg_items_select,
  [MSG_ITEMS_INVOKE]         = msg_items_invoke,
  [MSG_REFRESH]              = msg_refresh,
  [MSG_REFRESH_DONE]         = msg_refresh_done,
  [MSG_PP_BADGE]             = msg_pp_badge,
  [MSG_PP_GROUP]             = msg_pp_group,
  [MSG_PAGER_SELECT]         = msg_pager_select,
  [MSG_SPLIT_TOGGLE]         = msg_split_toggle,
  [MSG_SPLIT_OPEN]           = msg_split_open,
  [MSG_SPLIT_MODE]           = msg_split_mode,
  [MSG_SPLIT_PLACE]          = msg_split_place,
  [MSG_TB_BACK]              = msg_tb_back,
  [MSG_TB_PANE]              = msg_tb_pane,
  [MSG_CHECK_TRI]            = msg_check_tri,
  [MSG_SLIDER_VERT]          = msg_slider_vert,
  [MSG_SLIDER_RANGE]         = msg_slider_range,
  [MSG_COMBO_EDIT]           = msg_combo_edit,
  [MSG_COMBO_SUBMIT]         = msg_combo_submit,
  [MSG_DD_HOVER]             = msg_dd_hover,
  [MSG_DD_FILES]             = msg_dd_drop,
  [MSG_DD_TEXT]              = msg_dd_drop,
  [MSG_DD_DONE]              = msg_dd_done,
  [MSG_BACKDROP]             = msg_backdrop,
  [MSG_RTL]                  = msg_rtl,
  [MSG_NAV_STYLE]            = msg_nav_style,
  [MSG_NAV_DEMO]             = msg_nav_demo,
  [MSG_NAV_DEMO_HIDE]        = msg_nav_demo_hide,
  [MSG_NAV_DEMO_PICK]        = msg_nav_demo_pick,
  [MSG_NAV_DEMO_MODE]        = msg_nav_demo_mode,
  [MSG_NAV_DEMO_HIDE_HEADER] = msg_nav_demo_hide_header,
  [MSG_CONTROL_SEARCH_TEXT]  = msg_control_search_text,
  [MSG_CONTROL_SEARCH_QUERY] = msg_control_search_query,
  [MSG_MOTION_IMP_STATE]     = msg_motion_imp_state,
  [MSG_MOTION_IMP_MASK]      = msg_motion_imp_mask,
  [MSG_MOTION_ENTER_KIND]    = msg_motion_enter_kind,
  [MSG_MOTION_ITEM_ADD]      = msg_motion_item_add,
  [MSG_MOTION_ITEM_REMOVE]   = msg_motion_item_remove,
  [MSG_MOTION_ITEM_SHUFFLE]  = msg_motion_item_shuffle,
  [MSG_MOTION_FRAME_KIND]    = msg_motion_frame_kind,
  [MSG_MOTION_FRAME_STEP]    = msg_motion_frame_step,
  [MSG_MOTION_CONN_KIND]     = msg_motion_conn_kind,
  [MSG_MOTION_CONN_TOGGLE]   = msg_motion_conn_toggle,
  [MSG_MOTION_REDUCED]       = msg_motion_reduced,
};

void gallery_model_update(Model *m, XtkMsg msg) {
	if (( unsigned ) msg.tag >= sizeof(k_msg_fn) / sizeof(k_msg_fn [0])) return;
	GalleryMsgFn fn = k_msg_fn [msg.tag];
	if (fn) fn(m, msg);
}

char const *gallery_dd_prompt(Model const *m) {
	return m->dd_hover ? "Release to drop." : "Drag files from Explorer or text from another app onto this card.";
}

char const *gallery_dd_result(Model const *m) {
	if (!m->dd_drop) return "Nothing dropped yet.";
	return m->dd_drop;
}

char const *gallery_dd_completed(Model const *m) {
	static char const *const lines []
	  = {"Last drag was cancelled.", "Last drag was copied.", "Last drag was moved.", "Last drag was linked."};
	if (m->dd_done < 0 || m->dd_done > 3) return "Drag a card into Explorer, another app, or the target above.";
	return lines [m->dd_done];
}
