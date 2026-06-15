#include <math.h>
#include "hello_fluxent_demo.h"

static char const *const kComboItems [] = {
  "Alabama",
  "Alaska",
  "Arizona",
  "Arkansas",
  "California",
  "Colorado",
  "Connecticut",
  "Delaware",
  "Florida",
  "Georgia",
  "Hawaii",
  "Idaho",
  "Illinois",
  "Indiana",
  "Iowa",
  "Kansas",
  "Kentucky",
  "Louisiana",
  "Maine",
  "Maryland",
};
#define COMBO_ITEM_COUNT ( int ) (sizeof(kComboItems) / sizeof(kComboItems [0]))

static void demo_phase2_add_menu_item(FluxMenuFlyout *menu, char const *label) {
	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_NORMAL;
	item.label           = label;
	item.enabled         = true;
	flux_menu_flyout_add_item(menu, &item);
}

static void on_split_primary(void *ctx) { ( void ) ctx; }

static void on_combo_select(void *ctx, int index) {
	( void ) ctx;
	( void ) index;
}

static void on_expander_toggle(void *ctx, bool expanded) {
	( void ) ctx;
	( void ) expanded;
}

static void       on_infobar_close(void *ctx) { ( void ) ctx; }

static XentNodeId demo_wide_panel(Demo *d, uint32_t row) {
	XentNodeId panel = demo_panel(d, d->dashboard, row, 0, 1);
	xent_set_grid_column_span(d->ctx, panel, 2);
	return panel;
}

static XentNodeId demo_panel_title(Demo *d, XentNodeId panel, char const *text) {
	XentNodeId t = demo_create_text(d->ctx, d->store, panel, text, 16.0f);
	xent_set_size(d->ctx, t, (XentSize) {NAN, 24});
	flux_text_set_weight(d->store, t, FLUX_FONT_SEMI_BOLD);
	return t;
}

static void demo_add_phase2_buttons(Demo *d) {
	XentNodeId panel = demo_wide_panel(d, 5);
	demo_panel_title(d, panel, "DropDownButton / SplitButton / ComboBox");

	/* Width is fixed; height sizes to the row's controls via xent-core wrap-content
	 * (fit-content) instead of a hand-matched literal. */
	XentNodeId row = make_row(d->ctx, panel, 16, 0);
	xent_set_wrap_content(d->ctx, row, false, true);
	FluxWindow                  *win = flux_app_get_window(d->app);

	FluxDropDownButtonCreateInfo ddb = {d->ctx, d->store, row, "Options", "More"};
	XentNodeId                   dd  = flux_create_dropdown_button(&ddb);
	xent_set_size(d->ctx, dd, (XentSize) {160, 32});
	/* WinUI DropDownButton hosts a MenuFlyout: click or Space/Enter opens it; a keyboard
	 * invoke highlights the first item so arrows/Enter work at once. */
	d->dd_menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(d->dd_menu, flux_app_get_theme(d->app));
	flux_menu_flyout_set_text_renderer(d->dd_menu, flux_app_get_text_renderer(d->app));
	demo_phase2_add_menu_item(d->dd_menu, "Open");
	demo_phase2_add_menu_item(d->dd_menu, "Save as...");
	demo_phase2_add_menu_item(d->dd_menu, "Export");
	FluxContextFlyoutBindingInfo dd_menu_info = {d->store, dd, d->dd_menu, d->ctx, win};
	flux_node_set_menu_flyout_ex(&dd_menu_info);

	FluxSplitButtonCreateInfo sbi = {d->ctx, d->store, row, "Save", "Save", on_split_primary, NULL};
	XentNodeId                sb  = flux_create_split_button(&sbi);
	xent_set_size(d->ctx, sb, (XentSize) {160, 32});
	/* WinUI SplitButton hosts a MenuFlyout in its secondary zone (Alt+Down / F4 / chevron). */
	d->sb_menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(d->sb_menu, flux_app_get_theme(d->app));
	flux_menu_flyout_set_text_renderer(d->sb_menu, flux_app_get_text_renderer(d->app));
	demo_phase2_add_menu_item(d->sb_menu, "Save");
	demo_phase2_add_menu_item(d->sb_menu, "Save as...");
	demo_phase2_add_menu_item(d->sb_menu, "Save all");
	FluxContextFlyoutBindingInfo sb_menu_info = {d->store, sb, d->sb_menu, d->ctx, win};
	flux_split_button_set_menu_flyout_ex(&sb_menu_info);

	FluxComboBoxCreateInfo cbi = {
	  d->ctx, d->store, row, win, flux_app_get_text_renderer(d->app), flux_app_get_theme(d->app), kComboItems,
	  COMBO_ITEM_COUNT, 0, "Pick a state", on_combo_select, NULL};
	XentNodeId cb = flux_create_combo_box(&cbi);
	xent_set_size(d->ctx, cb, (XentSize) {220, 32});
}

static void demo_add_phase2_expander(Demo *d) {
	XentNodeId panel = demo_wide_panel(d, 6);
	demo_panel_title(d, panel, "Expander");

	XentNodeId             content = XENT_NODE_INVALID;
	FluxExpanderCreateInfo exi     = {
	  .ctx            = d->ctx,
	  .store          = d->store,
	  .parent         = panel,
	  .window         = flux_app_get_window(d->app),
	  .header         = "Advanced settings",
	  .expanded       = true,
	  .width          = 950,
	  .content_height = 92, /* padding 16*2 + text 20 + gap 8 + checkbox 32 */
	  .on_toggle      = on_expander_toggle,
	  .out_content    = &content,
	};
	XentNodeId ex = flux_create_expander(&exi);
	( void ) ex;

	if (content != XENT_NODE_INVALID) {
		xent_set_protocol(d->ctx, content, XENT_PROTOCOL_FLEX);
		xent_set_flex_direction(d->ctx, content, XENT_FLEX_COLUMN);
		xent_set_gap(d->ctx, content, 8);
		XentNodeId line
		  = demo_create_text(d->ctx, d->store, content, "Expanders reveal secondary content on demand.", 13.0f);
		xent_set_size(d->ctx, line, (XentSize) {NAN, 20});
		FluxToggleCreateInfo chk = {d->ctx, d->store, content, "Enable telemetry", false, NULL, NULL};
		XentNodeId           cn  = flux_create_checkbox(&chk);
		xent_set_size(d->ctx, cn, (XentSize) {200, 32});
	}
}

static void demo_add_infobar(Demo *d, XentNodeId panel, FluxInfoBarSeverity sev, char const *title, char const *msg) {
	FluxInfoBarCreateInfo ib = {d->ctx, d->store, panel, sev, title, msg, true, on_infobar_close, NULL};
	XentNodeId            n  = flux_create_info_bar(&ib);
	xent_set_size(d->ctx, n, (XentSize) {NAN, 48});
}

static void demo_add_phase2_infobars(Demo *d) {
	XentNodeId panel = demo_wide_panel(d, 7);
	demo_panel_title(d, panel, "InfoBar (severities)");
	demo_add_infobar(d, panel, FLUX_INFOBAR_INFORMATIONAL, "Heads up", "This is an informational message.");
	demo_add_infobar(d, panel, FLUX_INFOBAR_SUCCESS, "Saved", "Your changes were saved successfully.");
	demo_add_infobar(d, panel, FLUX_INFOBAR_WARNING, "Careful", "This action may have side effects.");
	demo_add_infobar(d, panel, FLUX_INFOBAR_ERROR, "Failed", "Something went wrong - please retry.");
}

static void on_dialog_result(void *ctx, FluxDialogResult result) {
	( void ) result;
	( void ) ctx;
}

static void on_show_dialog(void *ctx) {
	Demo *d = ( Demo * ) ctx;
	if (d->dialog == XENT_NODE_INVALID) return;
	flux_content_dialog_show(d->store, d->dialog);
}

static void demo_build_dialog(Demo *d) {
	XentNodeId                  dlg_content = XENT_NODE_INVALID;
	FluxContentDialogCreateInfo cdi         = {
	  .ctx            = d->ctx,
	  .store          = d->store,
	  .overlay_parent = d->scene_root,
	  .input          = flux_app_get_input(d->app),
	  .window         = flux_app_get_window(d->app),
	  .theme          = flux_app_get_theme(d->app),
	  .title          = "Delete file?",
	  .primary_text   = "Delete",
	  .close_text     = "Cancel",
	  .width          = 400,
	  .content_height = 48,
	  .on_result      = on_dialog_result,
	  .userdata       = d,
	  .out_content    = &dlg_content,
	};
	d->dialog = flux_create_content_dialog(&cdi);
	if (dlg_content != XENT_NODE_INVALID) {
		XentNodeId msg = demo_create_text(
		  d->ctx, d->store, dlg_content, "This file will be permanently removed. This cannot be undone.", 14.0f
		);
		xent_set_size(d->ctx, msg, (XentSize) {352, 48}); /* fits the 400 - 2*24 content width */
	}
}

static void demo_add_phase2_image_dialog(Demo *d) {
	XentNodeId panel = demo_wide_panel(d, 8);
	demo_panel_title(d, panel, "Image & ContentDialog");

	XentNodeId row = make_row(d->ctx, panel, 20, 96);

	FluxImageCreateInfo img = {d->ctx, d->store, row, "C:/Windows/Web/Wallpaper/Windows/img0.jpg", FLUX_IMAGE_UNIFORM};
	XentNodeId          im  = flux_create_image(&img);
	xent_set_size(d->ctx, im, (XentSize) {150, 90});

	XentNodeId cap = demo_create_text(d->ctx, d->store, row, "Image (Stretch=Uniform, WIC-decoded)", 13.0f);
	xent_set_size(d->ctx, cap, (XentSize) {340, 20});

	demo_build_dialog(d);
	XentNodeId btn = demo_button(d, row, "Show Dialog", on_show_dialog, d);
	xent_set_size(d->ctx, btn, (XentSize) {140, 32});
	flux_button_set_style(d->store, btn, FLUX_BUTTON_ACCENT);
}

void demo_add_phase2_content(Demo *d) {
	demo_add_phase2_buttons(d);
	demo_add_phase2_expander(d);
	demo_add_phase2_infobars(d);
	demo_add_phase2_image_dialog(d);
}
