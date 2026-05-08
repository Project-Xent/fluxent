#include "hello_fluxent_demo.h"

static void on_textbox_change(void *ctx, char const *t) {
	( void ) ctx;
	( void ) t;
}

static void on_password_change(void *ctx, char const *t) {
	( void ) ctx;
	( void ) t;
}

static void on_number_change(void *ctx, double v) {
	( void ) ctx;
	( void ) v;
}

static void demo_move_footer_after_inputs(Demo *d) {
	xent_remove_child(d->ctx, d->root, d->footer_div);
	xent_remove_child(d->ctx, d->root, d->footer);
	xent_remove_child(d->ctx, d->root, d->spacer);
}

static void demo_restore_footer(Demo *d) {
	xent_append_child(d->ctx, d->root, d->footer_div);
	xent_append_child(d->ctx, d->root, d->footer);
	xent_append_child(d->ctx, d->root, d->spacer);
}

static void demo_add_text_inputs(Demo *d) {
	demo_move_footer_after_inputs(d);
	FluxAppTextBoxCreateInfo text_info = {d->app, d->root, "Type something here...", on_textbox_change, NULL};
	XentNodeId               textbox   = flux_app_create_textbox(&text_info);
	xent_set_size(d->ctx, textbox, (XentSize) {480, 32});
	FluxAppTextBoxCreateInfo pass_info = {d->app, d->root, "Enter password...", on_password_change, NULL};
	XentNodeId               passbox   = flux_app_create_password_box(&pass_info);
	xent_set_size(d->ctx, passbox, (XentSize) {480, 32});
	FluxAppNumberBoxCreateInfo num_info = {d->app, d->root, -100.0, 100.0, 1.0, on_number_change, NULL};
	XentNodeId                 numbox   = flux_app_create_number_box(&num_info);
	xent_set_size(d->ctx, numbox, (XentSize) {480, 32});
}

static void
menu_add_item(FluxMenuFlyout *menu, FluxMenuItemType type, char const *label, char const *accel, bool enabled) {
	FluxMenuItemDef item  = {0};
	item.type             = type;
	item.label            = label;
	item.accelerator_text = accel;
	item.enabled          = enabled;
	flux_menu_flyout_add_item(menu, &item);
}

static FluxMenuFlyout *demo_create_theme_menu(FluxWindow *win, FluxThemeManager *tmgr, FluxTextRenderer *tr) {
	FluxMenuFlyout *menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(menu, tmgr);
	flux_menu_flyout_set_text_renderer(menu, tr);
	for (int i = 0; i < 3; i++) {
		FluxMenuItemDef item = {0};
		item.type            = FLUX_MENU_ITEM_RADIO;
		item.label           = i == 0 ? "System" : (i == 1 ? "Light" : "Dark");
		item.radio_group     = "theme";
		item.checked         = i == 0;
		item.enabled         = true;
		flux_menu_flyout_add_item(menu, &item);
	}
	return menu;
}

static FluxMenuFlyout *
demo_create_view_menu(FluxWindow *win, FluxThemeManager *tmgr, FluxTextRenderer *tr, FluxMenuFlyout *theme) {
	FluxMenuFlyout *menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(menu, tmgr);
	flux_menu_flyout_set_text_renderer(menu, tr);

	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_TOGGLE;
	item.label           = "Word Wrap";
	item.checked         = true;
	item.enabled         = true;
	flux_menu_flyout_add_item(menu, &item);
	item.checked = false;
	item.label   = "Line Numbers";
	flux_menu_flyout_add_item(menu, &item);
	flux_menu_flyout_add_separator(menu);
	item         = (FluxMenuItemDef) {0};
	item.type    = FLUX_MENU_ITEM_SUBMENU;
	item.label   = "Theme";
	item.enabled = true;
	item.submenu = theme;
	flux_menu_flyout_add_item(menu, &item);
	return menu;
}

static void demo_fill_context_menu(FluxMenuFlyout *menu, FluxMenuFlyout *view) {
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Cut", "Ctrl+X", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Copy", "Ctrl+C", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Paste", "Ctrl+V", true);
	flux_menu_flyout_add_separator(menu);
	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_SUBMENU;
	item.label           = "View";
	item.enabled         = true;
	item.submenu         = view;
	flux_menu_flyout_add_item(menu, &item);
	flux_menu_flyout_add_separator(menu);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Select All", "Ctrl+A", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Delete", NULL, false);
}

static void demo_add_flyout_menu(Demo *d) {
	add_divider(d);
	make_section(d->ctx, d->store, d->root, "Flyout & MenuFlyout");
	FluxWindow       *win  = flux_app_get_window(d->app);
	FluxThemeManager *tmgr = flux_app_get_theme(d->app);
	FluxTextRenderer *tr   = flux_app_get_text_renderer(d->app);

	d->flyout              = flux_flyout_create(win);
	flux_flyout_set_theme_manager(d->flyout, tmgr);
	flux_flyout_set_text_renderer(d->flyout, tr);
	flux_flyout_set_content_size(d->flyout, 200, 40);

	d->menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(d->menu, tmgr);
	flux_menu_flyout_set_text_renderer(d->menu, tr);
	FluxMenuFlyout *theme = demo_create_theme_menu(win, tmgr, tr);
	FluxMenuFlyout *view  = demo_create_view_menu(win, tmgr, tr, theme);
	demo_fill_context_menu(d->menu, view);
}

static void demo_add_popup_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId fly = demo_button(d, row, "Click me (Flyout)", NULL, NULL);
	xent_set_size(d->ctx, fly, (XentSize) {200, 32});
	flux_button_set_style(d->store, fly, FLUX_BUTTON_ACCENT);
	FluxFlyoutBindingInfo flyout_info
	  = {d->store, fly, d->flyout, FLUX_PLACEMENT_BOTTOM, d->ctx, flux_app_get_window(d->app)};
	flux_node_set_flyout_ex(&flyout_info);
	flux_node_set_tooltip(d->store, fly, "Click to show a Flyout popup below this button");

	XentNodeId ctx_btn = demo_button(d, row, "Right-click (Menu)", NULL, NULL);
	xent_set_size(d->ctx, ctx_btn, (XentSize) {200, 32});
	FluxContextFlyoutBindingInfo context_info = {d->store, ctx_btn, d->menu, d->ctx, flux_app_get_window(d->app)};
	flux_node_set_context_flyout_ex(&context_info);
	flux_node_set_tooltip(d->store, ctx_btn, "Right-click to show a MenuFlyout context menu");
}

void demo_add_dynamic_content(Demo *d) {
	demo_add_text_inputs(d);
	demo_add_flyout_menu(d);
	demo_add_popup_buttons(d);
	demo_restore_footer(d);
}
