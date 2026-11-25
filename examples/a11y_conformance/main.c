#include <fluxent/fluxent.h>
#include <fluxent/flux_app.h>
#include <xtk/xtk.h>

#include <math.h>
#include <string.h>
#include <windows.h>

typedef enum Msg
{
	MSG_NONE,
	MSG_INFOBAR,
	MSG_INFOBAR_CLOSE,
	MSG_TIP,
	MSG_TIP_CLOSE,
	MSG_FLIP,
	MSG_PIPS,
	MSG_PAGER,
	MSG_RATING,
	MSG_SLIDER,
	MSG_NUMBER,
	MSG_EXPAND,
	MSG_BAR,
	MSG_LIST,
	MSG_LIVE,
} Msg;

typedef struct Model {
	bool     infobar;
	bool     tip;
	int      flip;
	int      pips;
	int      pager;
	double   rating;
	float    slider;
	double   number;
	bool     expanded;
	bool     bar;
	int      list;
	int      live;
	FluxApp *app;
} Model;

static char const *const     kFruit []   = {"Apple", "Banana", "Cherry", "Grape", "Lemon", "Mango", "Peach", "Pear"};
static char const *const     kSuggest [] = {"Alpha", "Beta", "Gamma", "Delta"};
static XtkTreeNodeDesc const kTreeChildren [] = {{.text = "Proposal"}, {.text = "Budget"}};
static XtkTreeNodeDesc const kTreeRoots []    = {
  {.text = "Projects", .expanded = true, .children = kTreeChildren, .child_count = 2},
  {.text = "Archive"},
};

static XentNodeId find_live(XentCtx const *ctx, XentNodeId node) {
	char const *text = xent_node_text(ctx, node);
	if (text && strncmp(text, "Live region count", 17) == 0) return node;
	for (XentNodeId c = xent_node_first(ctx, node); c != XENT_NODE_INVALID; c = xent_node_next(ctx, c)) {
		XentNodeId hit = find_live(ctx, c);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return XENT_NODE_INVALID;
}

static void bump_live(Model *m) {
	m->live++;
	XentNodeId node = m->app ? find_live(flux_app_ctx(m->app), flux_app_root(m->app)) : XENT_NODE_INVALID;
	if (node != XENT_NODE_INVALID) flux_app_raiseevent(m->app, ( XtkCtrl ) node, FLUX_AUTOMATION_LIVE_REGION_CHANGED);
}

static bool update_visibility(Model *m, XtkMsg msg) {
	switch (( Msg ) msg.tag) {
	case MSG_INFOBAR       : m->infobar = !m->infobar; return true;
	case MSG_INFOBAR_CLOSE : m->infobar = false; return true;
	case MSG_TIP           : m->tip = !m->tip; return true;
	case MSG_TIP_CLOSE     : m->tip = false; return true;
	case MSG_EXPAND        : m->expanded = !m->expanded; return true;
	case MSG_BAR           : m->bar = msg.b; return true;
	default                : return false;
	}
}

static void update_selection(Model *m, XtkMsg msg) {
	switch (( Msg ) msg.tag) {
	case MSG_FLIP  : m->flip = msg.i; break;
	case MSG_PIPS  : m->pips = msg.i; break;
	case MSG_PAGER : m->pager = msg.i; break;
	case MSG_LIST  : m->list = msg.i; break;
	default        : break;
	}
}

static void update(void *model, XtkMsg msg) {
	Model *m = model;
	if (update_visibility(m, msg)) return;
	switch (( Msg ) msg.tag) {
	case MSG_LIVE   : bump_live(m); break;
	case MSG_RATING : m->rating = msg.d; break;
	case MSG_SLIDER : m->slider = msg.f; break;
	case MSG_NUMBER : m->number = msg.d; break;
	default         : update_selection(m, msg); break;
	}
}

static XtkEl *labeled(XtkUi *ui, char const *id, char const *name, XtkEl *el) {
	return xtk_automation(ui, el, &(XtkAutomation) {.id = id, .name = name});
}

static XtkEl *section(XtkUi *ui, char const *title, XtkEl *kids []) {
	XtkEl *head = xtk_automation(
	  ui, xtk_text(ui, title, (XtkTextDesc) {.size = 20, .weight = XTK_FONT_SEMI_BOLD}),
	  &(XtkAutomation) {.heading = XTK_HEADING_LEVEL2}
	);
	XtkEl *body = xtk_column(ui, (XtkStackDesc) {.gap = 8}, kids);
	return xtk_card(
	  ui,
	  (XtkStackDesc) {
		.gap = 10, .padding = {16, 16, 16, 16}
    },
	  xtk_kids(head, body)
	);
}

static XtkEl *fruit_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, kFruit [index % 8], (XtkTextDesc) {0});
}

static XtkEl *page_basic(XtkUi *ui, Model const *m) {
	return section(
	  ui, "Invoke, Toggle, SelectionItem, RangeValue, Value, Text",
	  xtk_kids(
		labeled(ui, "invoke", NULL, xtk_button(ui, "Invoke", (XtkButtonDesc) {0})),
		labeled(ui, "repeat", NULL, xtk_repeat(ui, "Repeat", (XtkButtonDesc) {0})),
		labeled(ui, "toggle", NULL, xtk_checkbox(ui, "Toggle", (XtkToggleDesc) {0})),
		labeled(ui, "switch", "Switch", xtk_switch(ui, (XtkToggleDesc) {0})),
		labeled(ui, "radio", NULL, xtk_radio(ui, "Selection item", (XtkToggleDesc) {0})),
		labeled(
		  ui, "slider", "Range value",
		  xtk_sized(xtk_slider(ui, (XtkSliderDesc) {.value = m->slider, .change = xtk_msg(MSG_SLIDER)}), 320.0f, NAN)
		),
		labeled(
		  ui, "number", "Number",
		  xtk_number(
			ui, (XtkNumberDesc) {.min = 0, .max = 100, .step = 1, .value = m->number, .change = xtk_msg(MSG_NUMBER)}
		  )
		),
		labeled(
		  ui, "textbox", "Text",
		  xtk_sized(xtk_textbox(ui, "Value and Text patterns", (XtkTextBoxDesc) {0}), 320.0f, NAN)
		),
		labeled(
		  ui, "password", "Password", xtk_sized(xtk_password(ui, "IsPassword", (XtkTextBoxDesc) {0}), 320.0f, NAN)
		),
		labeled(
		  ui, "rating", "Rating", xtk_rating(ui, (XtkRatingDesc) {.value = m->rating, .change = xtk_msg(MSG_RATING)})
		),
		labeled(ui, "progress", "Progress", xtk_sized(xtk_progress(ui, (XtkProgressDesc) {.value = 40}), 320.0f, NAN)),
		labeled(ui, "link", NULL, xtk_link(ui, "Hyperlink", (XtkHyperlinkDesc) {.url = "https://example.com"}))
	  )
	);
}

static XtkEl *page_items(XtkUi *ui, Model const *m) {
	return section(
	  ui, "Selection, Scroll, ItemContainer, VirtualizedItem, ExpandCollapse",
	  xtk_kids(
		labeled(
		  ui, "multi_tree", "Multiple selection tree",
		  xtk_sized(
			xtk_tree(ui, (XtkTreeDesc) {.roots = kTreeRoots, .nroot = 2, .sel_mode = XTK_TREE_SELECT_MULTIPLE}), 320.0f,
			160.0f
		  )
		),
		labeled(
		  ui, "list", "List",
		  xtk_sized(
			xtk_list(ui, (XtkListDesc) {.nitem = 500, .item = fruit_row, .sel = m->list, .select = xtk_msg(MSG_LIST)}),
			320.0f, 200.0f
		  )
		),
		labeled(
		  ui, "combo", "Combo",
		  xtk_sized(xtk_combo(ui, "Pick", (XtkComboDesc) {.items = kFruit, .nitem = 8, .sel = -1}), 320.0f, NAN)
		),
		labeled(
		  ui, "suggest", "Search",
		  xtk_sized(
			xtk_suggest(ui, "AutoSuggestBox", (XtkAutoSuggestDesc) {.suggestions = kSuggest, .nitem = 4}), 320.0f, NAN
		  )
		),
		labeled(
		  ui, "expander", NULL,
		  xtk_expander(
			ui, "Expander", (XtkExpanderDesc) {.expanded = m->expanded, .toggle = xtk_msg(MSG_EXPAND)},
			xtk_kids(xtk_text(ui, "Expanded content", (XtkTextDesc) {0}))
		  )
		)
	  )
	);
}

static XtkEl *page_paging(XtkUi *ui, Model const *m) {
	return section(
	  ui, "FlipView, PipsPager, PagerControl",
	  xtk_kids(
		labeled(
		  ui, "flip", "Flip",
		  xtk_sized(
			xtk_flip(
			  ui, (XtkFlipDesc) {.sel = m->flip, .select = xtk_msg(MSG_FLIP)},
			  xtk_kids(
				xtk_text(ui, "Page one", (XtkTextDesc) {0}), xtk_text(ui, "Page two", (XtkTextDesc) {0}),
				xtk_text(ui, "Page three", (XtkTextDesc) {0})
			  )
			),
			320.0f, 120.0f
		  )
		),
		labeled(
		  ui, "pips", NULL,
		  xtk_pips(
			ui, (XtkPipsDesc) {.nitem = 8, .sel = m->pips, .nav = XTK_PIPS_NAV_VISIBLE, .select = xtk_msg(MSG_PIPS)}
		  )
		),
		labeled(
		  ui, "pager", "Pager",
		  xtk_pager(ui, (XtkPagerDesc) {.nitem = 20, .sel = m->pager, .select = xtk_msg(MSG_PAGER)})
		)
	  )
	);
}

static XtkEl *page_calendar(XtkUi *ui) {
	return section(
	  ui, "Grid, Table, Calendar, pickers, ColorPicker",
	  xtk_kids(
		labeled(ui, "calendar", "Calendar", xtk_calendar_view(ui, (XtkCalendarDesc) {0})),
		labeled(ui, "date", NULL, xtk_date_picker(ui, (XtkDatePickerDesc) {.header = "Date"})),
		labeled(ui, "time", NULL, xtk_time_picker(ui, (XtkTimePickerDesc) {.header = "Time"})),
		labeled(ui, "color", NULL, xtk_color_picker(ui, (XtkColorPickerDesc) {.color = (XtkColor) {0x0078d4ffu}}))
	  )
	);
}

static XtkEl *page_events(XtkUi *ui, Model const *m) {
	XtkEl *live = xtk_automation(
	  ui, xtk_text(ui, xtk_fmt(ui, "Live region count %d", m->live), (XtkTextDesc) {0}),
	  &(XtkAutomation) {.id = "live", .live = XTK_LIVE_POLITE}
	);
	return section(
	  ui, "Events: Notification, Window, Menu, PropertyChanged",
	  xtk_kids(
		labeled(
		  ui, "infobar_toggle", NULL, xtk_button(ui, "Toggle InfoBar", (XtkButtonDesc) {.click = xtk_msg(MSG_INFOBAR)})
		),
		m->infobar ? labeled(
					   ui, "infobar", NULL,
					   xtk_infobar(
						 ui, "InfoBar", "Notification on open and close",
						 (XtkInfoBarDesc) {
						   .severity = XTK_INFOBAR_WARNING, .closable = true, .close = xtk_msg(MSG_INFOBAR_CLOSE)}
					   )
					 )
		           : xtk_spacer(ui),
		labeled(
		  ui, "tip_target", NULL,
		  xtk_teachtip(
			ui, "Teaching tip",
			(XtkTeachingTipDesc) {
			  .open          = m->tip,
			  .light_dismiss = true,
			  .subtitle      = "WindowOpened and WindowClosed",
			  .close         = xtk_msg(MSG_TIP_CLOSE)},
			xtk_kids(xtk_button(ui, "Toggle TeachingTip", (XtkButtonDesc) {.click = xtk_msg(MSG_TIP)}))
		  )
		),
		labeled(
		  ui, "commandbar", "Command bar",
		  xtk_sized(
			xtk_commandbar(
			  ui, (XtkCommandBarDesc) {.open = m->bar, .change = xtk_msg(MSG_BAR)},
			  xtk_kids(
				xtk_appbarbutton(ui, "Save", (XtkAppBarDesc) {.icon = "Save"}),
				xtk_appbartoggle(ui, "Pin", (XtkAppBarDesc) {.icon = "Pin"}),
				xtk_appbarbutton(ui, "Share", (XtkAppBarDesc) {.icon = "Share", .secondary = true})
			  )
			),
			480.0f, NAN
		  )
		),
		labeled(
		  ui, "live_bump", NULL, xtk_button(ui, "Bump live region", (XtkButtonDesc) {.click = xtk_msg(MSG_LIVE)})
		),
		live
	  )
	);
}

static XtkEl *page_props(XtkUi *ui) {
	XtkEl *label
	  = xtk_automation(ui, xtk_text(ui, "Email address", (XtkTextDesc) {0}), &(XtkAutomation) {.id = "email_label"});
	XtkEl *email = xtk_automation(
	  ui, xtk_sized(xtk_textbox(ui, "name@example.com", (XtkTextBoxDesc) {0}), 320.0f, NAN),
	  &(XtkAutomation) {
		.id           = "email",
		.labeled_by   = "email_label",
		.described_by = "email_hint",
		.help         = "Used for sign in",
		.required     = true,
		.invalid      = true,
		.item_status  = "Invalid address"}
	);
	XtkEl *hint
	  = xtk_automation(ui, xtk_text(ui, "We never share it", (XtkTextDesc) {0}), &(XtkAutomation) {.id = "email_hint"});
	XtkEl *icon = xtk_automation(
	  ui, xtk_button(ui, NULL, (XtkButtonDesc) {.icon = "Settings"}),
	  &(XtkAutomation) {.id = "icon_button", .name = "Settings", .accelerator = "Ctrl+,", .access_key = "S"}
	);
	XtkEl *group = xtk_automation(
	  ui, xtk_column(ui, (XtkStackDesc) {.gap = 4}, xtk_kids(xtk_text(ui, "Search landmark", (XtkTextDesc) {0}))),
	  &(XtkAutomation) {.id = "search", .landmark = XTK_LANDMARK_SEARCH}
	);
	XtkEl *raw = xtk_automation(
	  ui, xtk_text(ui, "Raw view text", (XtkTextDesc) {0}), &(XtkAutomation) {.id = "raw", .view = XTK_ACCESS_VIEW_RAW}
	);
	XtkEl *set = xtk_automation(
	  ui, xtk_text(ui, "Item 2 of 5, level 3", (XtkTextDesc) {0}),
	  &(XtkAutomation) {.id = "set", .position = 2, .size = 5, .level = 3}
	);
	return section(ui, "AutomationProperties", xtk_kids(label, email, hint, icon, group, raw, set));
}

static XtkEl *view(XtkUi *ui, void *model) {
	Model const *m = ( Model const * ) model;
	return xtk_grow(
	  xtk_scroll(
		ui,
		(XtkScrollDesc) {
		  0
    },
		xtk_kids(xtk_column(
		  ui, (XtkStackDesc) {.gap = 16, .padding = {24, 24, 24, 24}, .align = XENT_FLEX_ALIGN_STRETCH},
		  xtk_kids(
			page_basic(ui, m), page_items(ui, m), page_paging(ui, m), page_calendar(ui), page_events(ui, m),
			page_props(ui)
		  )
		))
	  ),
	  1.0f
	);
}

static void start(FluxApp *app, void *user) { (( Model * ) user)->app = app; }

int WINAPI  WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;
	flux_controls_all();
	Model      model = {.list = -1, .rating = -1.0, .slider = 30.0f, .number = 5.0};
	FluxAppCfg cfg   = {
	  .title    = L"Xent a11y conformance",
	  .width    = 900,
	  .height   = 760,
	  .backdrop = FLUX_BACKDROP_MICA,
	  .on_start = start,
	  .user     = &model,
	};
	return flux_run(&cfg, &model, update, view);
}
