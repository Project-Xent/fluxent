#include "hello_fluxent_demo.h"
#include <stdio.h>

/* Phase 3 showcase: MenuBar, TabView, NavigationView. Persistent demo state
 * lives in file statics since the showcase drives a single window. */
typedef struct Phase3 {
	Demo      *d;
	XentNodeId nav_label; /**< Text in the nav content host, updated on selection. */
	XentNodeId nav_view;  /**< NavigationView root (PaneDisplayMode switcher target). */
	XentNodeId tab_view;
	int        tab_seq;   /**< Next "New Tab N" number. */
} Phase3;

static Phase3            g_p3;


static XentNodeId        p3_wide_panel(Demo *d, uint32_t row) {
	XentNodeId panel = demo_panel(d, d->dashboard, row, 0, 1);
	xent_set_grid_column_span(d->ctx, panel, 2);
	return panel;
}

static void p3_title(Demo *d, XentNodeId panel, char const *text) {
	XentNodeId t = demo_create_text(d->ctx, d->store, panel, text, 16.0f);
	xent_set_size(d->ctx, t, (XentSize) {NAN, 24});
	flux_text_set_weight(d->store, t, FLUX_FONT_SEMI_BOLD);
}

/* --- MenuBar --- */

static void p3_menu_item(FluxMenuFlyout *menu, char const *label) {
	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_NORMAL;
	item.label           = label;
	item.enabled         = true;
	flux_menu_flyout_add_item(menu, &item);
}

static void p3_add_menu_bar(Demo *d) {
	XentNodeId panel = p3_wide_panel(d, 9);
	p3_title(d, panel, "MenuBar");

	FluxWindow           *win = flux_app_get_window(d->app);
	FluxMenuBarCreateInfo mbi
	  = {d->ctx, d->store, panel, win, flux_app_get_text_renderer(d->app), flux_app_get_theme(d->app)};
	XentNodeId bar = flux_create_menu_bar(&mbi);
	xent_set_size(d->ctx, bar, (XentSize) {NAN, 40});

	FluxMenuFlyout *file = flux_menu_bar_add_menu(d->store, bar, "File");
	p3_menu_item(file, "New");
	p3_menu_item(file, "Open...");
	p3_menu_item(file, "Save");
	flux_menu_flyout_add_separator(file);
	p3_menu_item(file, "Exit");

	FluxMenuFlyout *edit = flux_menu_bar_add_menu(d->store, bar, "Edit");
	p3_menu_item(edit, "Undo");
	p3_menu_item(edit, "Redo");
	flux_menu_flyout_add_separator(edit);
	p3_menu_item(edit, "Cut");
	p3_menu_item(edit, "Copy");
	p3_menu_item(edit, "Paste");

	FluxMenuFlyout *view = flux_menu_bar_add_menu(d->store, bar, "View");
	p3_menu_item(view, "Zoom In");
	p3_menu_item(view, "Zoom Out");
	flux_menu_flyout_add_separator(view);
	p3_menu_item(view, "Full Screen");
}

/* --- TabView --- */

static void p3_fill_tab(Demo *d, XentNodeId content, char const *body) {
	xent_set_protocol(d->ctx, content, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, content, XENT_FLEX_COLUMN);
	xent_set_padding(d->ctx, content, (XentInsets) {16, 16, 16, 16});
	XentNodeId t = demo_create_text(d->ctx, d->store, content, body, 14.0f);
	xent_set_size(d->ctx, t, (XentSize) {NAN, 24});
}

static void p3_on_add_tab(void *ctx) {
	Phase3 *p = ( Phase3 * ) ctx;
	char    label [32];
	snprintf(label, sizeof(label), "New Tab %d", ++p->tab_seq);
	XentNodeId  content = XENT_NODE_INVALID;
	/* Borrowed label must outlive the tab; keep a small ring of stored names. */
	static char names [16][32];
	int         slot = p->tab_seq % 16;
	snprintf(names [slot], sizeof(names [slot]), "%s", label);
	int idx = flux_tab_view_add_tab(p->d->store, p->tab_view, "Document", names [slot], &content);
	if (idx >= 0 && content != XENT_NODE_INVALID) p3_fill_tab(p->d, content, "A freshly created tab.");
}

static void p3_add_tab_view(Demo *d) {
	XentNodeId panel = p3_wide_panel(d, 10);
	p3_title(d, panel, "TabView");

	FluxTabViewCreateInfo tvi = {
	  .ctx        = d->ctx,
	  .store      = d->store,
	  .parent     = panel,
	  .window     = flux_app_get_window(d->app),
	  .text       = flux_app_get_text_renderer(d->app),
	  .input      = flux_app_get_input(d->app),
	  .on_add_tab = p3_on_add_tab,
	  .userdata   = &g_p3};
	XentNodeId tv = flux_create_tab_view(&tvi);
	xent_set_size(d->ctx, tv, (XentSize) {NAN, 260});
	g_p3.tab_view = tv;
	g_p3.tab_seq  = 3;

	XentNodeId c0 = XENT_NODE_INVALID, c1 = XENT_NODE_INVALID, c2 = XENT_NODE_INVALID;
	flux_tab_view_add_tab(d->store, tv, "Home", "Welcome", &c0);
	flux_tab_view_add_tab(d->store, tv, "Document", "Report.docx", &c1);
	flux_tab_view_add_tab(d->store, tv, "Picture", "Photo.png", &c2);
	if (c0 != XENT_NODE_INVALID) p3_fill_tab(d, c0, "Home tab content.");
	if (c1 != XENT_NODE_INVALID) p3_fill_tab(d, c1, "Editing Report.docx.");
	if (c2 != XENT_NODE_INVALID) p3_fill_tab(d, c2, "Viewing Photo.png.");
}

/* --- NavigationView --- */

static void p3_on_nav_select(void *ctx, int index) {
	Phase3 *p = ( Phase3 * ) ctx;
	if (p->nav_label == XENT_NODE_INVALID) return;
	static char buf [64];
	/* Separators/headers occupy item indices too; resolve the label by index
	 * instead of assuming a dense table of selectable pages. */
	char const *name = flux_nav_view_item_label(p->d->store, p->nav_view, index);
	snprintf(buf, sizeof(buf), "Selected page: %s", name ? name : "?");
	flux_text_set_content(p->d->store, p->nav_label, buf);
}

static void p3_on_nav_pane_mode(void *ctx, int index) {
	Phase3 *p = ( Phase3 * ) ctx;
	flux_nav_view_set_pane_display_mode(p->d->store, p->nav_view, index);
}

static char const *const kNavPaneModes [] = {"Auto", "Left", "LeftCompact", "LeftMinimal", "Top"};

static void p3_add_nav_view(Demo *d) {
	XentNodeId panel = p3_wide_panel(d, 11);
	p3_title(d, panel, "NavigationView (PaneDisplayMode + adaptive resize)");

	FluxComboBoxCreateInfo cbi = {
	  .ctx            = d->ctx,
	  .store          = d->store,
	  .parent         = panel,
	  .window         = flux_app_get_window(d->app),
	  .text           = flux_app_get_text_renderer(d->app),
	  .theme          = flux_app_get_theme(d->app),
	  .items          = kNavPaneModes,
	  .item_count     = 5,
	  .selected_index = 0,
	  .on_select      = p3_on_nav_pane_mode,
	  .userdata       = &g_p3};
	XentNodeId cb = flux_create_combo_box(&cbi);
	xent_set_size(d->ctx, cb, (XentSize) {180, 32});

	XentNodeId            content = XENT_NODE_INVALID;
	FluxNavViewCreateInfo nvi     = {
	  .ctx                  = d->ctx,
	  .store                = d->store,
	  .parent               = panel,
	  .window               = flux_app_get_window(d->app),
	  .text                 = flux_app_get_text_renderer(d->app),
	  .theme                = flux_app_get_theme(d->app),
	  .width                = 950,
	  .height               = 340,
	  .mode                 = 0 /* Expanded */,
	  .adaptive             = true,
	  .on_selection_changed = p3_on_nav_select,
	  .userdata             = &g_p3,
	  .out_content          = &content};
	XentNodeId nav = flux_create_nav_view(&nvi);
	g_p3.nav_view  = nav;

	flux_nav_view_add_item(d->store, nav, "Home", "Home");
	flux_nav_view_add_separator(d->store, nav);
	flux_nav_view_add_header(d->store, nav, "Library");
	int doc = flux_nav_view_add_item(d->store, nav, "Folder", "Documents");
	flux_nav_view_add_child_item(d->store, nav, doc, NULL, "Recent");
	flux_nav_view_add_child_item(d->store, nav, doc, NULL, "Shared with me");
	flux_nav_view_add_item(d->store, nav, "Download", "Downloads");
	flux_nav_view_add_item(d->store, nav, "Camera", "Pictures");
	flux_nav_view_add_footer_item(d->store, nav, "Settings", "Settings");

	if (content != XENT_NODE_INVALID) {
		xent_set_protocol(d->ctx, content, XENT_PROTOCOL_FLEX);
		xent_set_flex_direction(d->ctx, content, XENT_FLEX_COLUMN);
		xent_set_padding(d->ctx, content, (XentInsets) {24, 24, 24, 24});
		g_p3.nav_label = demo_create_text(d->ctx, d->store, content, "Selected page: Home", 14.0f);
		xent_set_size(d->ctx, g_p3.nav_label, (XentSize) {560, 24});
	}
}

void demo_add_phase3_content(Demo *d) {
	g_p3           = (Phase3) {0};
	g_p3.d         = d;
	g_p3.nav_label = XENT_NODE_INVALID;
	p3_add_menu_bar(d);
	p3_add_tab_view(d);
	p3_add_nav_view(d);
}
