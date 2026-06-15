/**
 * @file fx_dispatch_create.c
 * @brief Table-driven per-type control creation and mount setup
 * for the FX reconciler.
 *
 * Owns kCreateTable, fx_create_control, and the mount helpers
 * (fx_mount_setup etc.).
 */
#include "fx_internal.h"

#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"

#include <math.h>
#include <stdlib.h>

#define FX_CONTROL_TYPE_MAX (FLUX_CONTROL_CUSTOM + 1)

typedef XentNodeId (*FxCreateFn)(FxRuntime *, XentNodeId, FluxEl const *, FxBinding *);

static XentNodeId fx_cr_container(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) el; ( void ) b;
	return flux_factory_create_node(rt->ctx, rt->store, p, FLUX_CONTROL_CONTAINER);
}

static XentNodeId fx_cr_card(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_card(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId fx_cr_scroll(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_scroll(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId fx_cr_divider(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_divider(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId fx_cr_text(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_text(&(FluxTextCreateInfo) {rt->ctx, rt->store, p, el->text, el->text_props.size});
}

static XentNodeId fx_cr_button(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_button(&(FluxButtonCreateInfo) {rt->ctx, rt->store, p, el->text, fx_tramp_click, b});
}

static XentNodeId fx_cr_checkbox(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_checkbox(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, fx_tramp_toggle, b});
}

static XentNodeId fx_cr_radio(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_radio(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, fx_tramp_toggle, b});
}

static XentNodeId fx_cr_switch(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_switch(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, fx_tramp_toggle, b});
}

static XentNodeId fx_cr_slider(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_slider(&(FluxSliderCreateInfo) {
	  rt->ctx, rt->store, p, el->slider.min, el->slider.max, el->slider.value, fx_tramp_slider, b});
}

static XentNodeId fx_cr_textbox(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	if (rt->app)
		return flux_app_create_textbox(&(FluxAppTextBoxCreateInfo) {rt->app, p, el->text, fx_tramp_text, b});
	return flux_create_textbox(&(FluxTextBoxCreateInfo) {rt->ctx, rt->store, p, el->text, fx_tramp_text, b});
}

static XentNodeId fx_cr_password(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	if (rt->app)
		return flux_app_create_password_box(&(FluxAppTextBoxCreateInfo) {rt->app, p, el->text, fx_tramp_text, b});
	return flux_create_password_box(&(FluxTextBoxCreateInfo) {rt->ctx, rt->store, p, el->text, fx_tramp_text, b});
}

static XentNodeId fx_cr_progress(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_progress(&(FluxProgressCreateInfo) {rt->ctx, rt->store, p, el->progress.value, el->progress.max});
}

static XentNodeId fx_cr_progress_ring(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_progress_ring(&(FluxProgressCreateInfo) {
	  rt->ctx, rt->store, p, el->progress.value, el->progress.max});
}

static XentNodeId fx_cr_badge(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_info_badge(&(FluxInfoBadgeCreateInfo) {
	  rt->ctx, rt->store, p, el->badge.mode, el->badge.value});
}

static XentNodeId fx_cr_image(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_image(&(FluxImageCreateInfo) {rt->ctx, rt->store, p, el->text, el->image.stretch});
}

static XentNodeId fx_cr_hyperlink(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_hyperlink(&(FluxHyperlinkCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->hyperlink.url, fx_tramp_click, b});
}

static XentNodeId fx_cr_repeat_button(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_repeat_button(&(FluxButtonCreateInfo) {rt->ctx, rt->store, p, el->text, fx_tramp_click, b});
}

static XentNodeId fx_cr_combo(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_combo_box(&(FluxComboBoxCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .parent         = p,
	  .window         = fx_rt_window(rt),
	  .text           = fx_rt_text(rt),
	  .theme          = fx_rt_theme(rt),
	  .items          = el->combo.items,
	  .item_count     = el->combo.count,
	  .selected_index = el->combo.selected,
	  .placeholder    = el->text,
	  .on_select      = fx_tramp_select,
	  .userdata       = b});
}

static XentNodeId fx_cr_expander(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_expander(&(FluxExpanderCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .parent         = p,
	  .window         = fx_rt_window(rt),
	  .header         = el->text,
	  .expanded       = el->expander.expanded,
	  .width          = el->expander.width,
	  .content_height = el->expander.content_height,
	  .on_toggle      = fx_tramp_expand,
	  .userdata       = b});
}

static XentNodeId fx_cr_infobar(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_info_bar(&(FluxInfoBarCreateInfo) {
	  rt->ctx, rt->store, p, el->info_bar.severity, el->text, el->info_bar.message, el->info_bar.closable,
	  fx_tramp_click, b});
}

static XentNodeId fx_cr_numberbox(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	if (rt->app)
		return flux_app_create_number_box(&(FluxAppNumberBoxCreateInfo) {
		  rt->app, p, el->number.min, el->number.max, el->number.step, fx_tramp_double, b});
	return flux_create_number_box(&(FluxNumberBoxCreateInfo) {
	  rt->ctx, rt->store, p, el->number.min, el->number.max, el->number.step, fx_tramp_double, b});
}

static XentNodeId fx_cr_dropdown(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) b;
	return flux_create_dropdown_button(&(FluxDropDownButtonCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->dropdown.icon});
}

static XentNodeId fx_cr_split(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_split_button(&(FluxSplitButtonCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->split.icon, fx_tramp_click, b});
}

static XentNodeId fx_cr_menubar(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_menu_bar(&(FluxMenuBarCreateInfo) {
	  rt->ctx, rt->store, p, fx_rt_window(rt), fx_rt_text(rt), fx_rt_theme(rt)});
}

static XentNodeId fx_cr_tabview(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	return flux_create_tab_view(&(FluxTabViewCreateInfo) {
	  .ctx                  = rt->ctx,
	  .store                = rt->store,
	  .parent               = p,
	  .window               = fx_rt_window(rt),
	  .text                 = fx_rt_text(rt),
	  .input                = rt->app ? flux_app_get_input(rt->app) : NULL,
	  .on_selection_changed = fx_tramp_select,
	  .on_close_requested   = fx_tramp_tab_close_req,
	  .on_add_tab           = el->tab_view.on_add.tag ? fx_tramp_click : NULL,
	  .userdata             = b});
}

static XentNodeId fx_cr_dialog(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	if (!rt->app) return XENT_NODE_INVALID;
	return flux_create_content_dialog(&(FluxContentDialogCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .overlay_parent = rt->host,
	  .input          = flux_app_get_input(rt->app),
	  .window         = fx_rt_window(rt),
	  .theme          = fx_rt_theme(rt),
	  .title          = el->text,
	  .primary_text   = el->dialog.primary,
	  .secondary_text = el->dialog.secondary,
	  .close_text     = el->dialog.close,
	  .width          = el->dialog.width,
	  .content_height = el->dialog.content_height,
	  .on_result      = fx_tramp_dialog_result,
	  .userdata       = b});
}

static int fx_nav_add_child(FluxNodeStore *store, XentNodeId nav, int group, FxNavItemDesc const *it) {
	if (group >= 0) {
		flux_nav_view_add_child_item(store, nav, group, it->icon, it->label);
		return group;
	}
	return flux_nav_view_add_item(store, nav, it->icon, it->label);
}

static void fx_create_nav_items(FluxNodeStore *store, XentNodeId nav, FluxEl const *el) {
	int group = -1;
	for (int i = 0; i < el->nav.item_count; i++) {
		FxNavItemDesc const *it = &el->nav.items [i];
		switch (it->kind) {
		case FX_NAV_ITEM      : group = flux_nav_view_add_item(store, nav, it->icon, it->label); break;
		case FX_NAV_HEADER    : flux_nav_view_add_header(store, nav, it->label); break;
		case FX_NAV_SEPARATOR : flux_nav_view_add_separator(store, nav); break;
		case FX_NAV_FOOTER    : flux_nav_view_add_footer_item(store, nav, it->icon, it->label); break;
		case FX_NAV_CHILD     : group = fx_nav_add_child(store, nav, group, it); break;
		}
	}
}

static XentNodeId fx_cr_nav(FxRuntime *rt, XentNodeId p, FluxEl const *el, FxBinding *b) {
	XentNodeId nav = flux_create_nav_view(&(FluxNavViewCreateInfo) {
	  .ctx                  = rt->ctx,
	  .store                = rt->store,
	  .parent               = p,
	  .window               = fx_rt_window(rt),
	  .text                 = fx_rt_text(rt),
	  .theme                = fx_rt_theme(rt),
	  .width                = isnan(el->width) ? 0.0f : el->width,
	  .height               = isnan(el->height) ? 0.0f : el->height,
	  .adaptive             = true,
	  .on_selection_changed = fx_tramp_select,
	  .userdata             = b});
	if (nav == XENT_NODE_INVALID) return nav;
	fx_create_nav_items(rt->store, nav, el);
	if (el->nav.selected > 0) flux_nav_view_select(rt->store, nav, el->nav.selected);
	return nav;
}

static FxCreateFn const kCreateTable [FX_CONTROL_TYPE_MAX] = {
	[FLUX_CONTROL_CONTAINER]       = fx_cr_container,
	[FLUX_CONTROL_CARD]            = fx_cr_card,
	[FLUX_CONTROL_SCROLL]          = fx_cr_scroll,
	[FLUX_CONTROL_DIVIDER]         = fx_cr_divider,
	[FLUX_CONTROL_TEXT]            = fx_cr_text,
	[FLUX_CONTROL_BUTTON]          = fx_cr_button,
	[FLUX_CONTROL_CHECKBOX]        = fx_cr_checkbox,
	[FLUX_CONTROL_RADIO]           = fx_cr_radio,
	[FLUX_CONTROL_SWITCH]          = fx_cr_switch,
	[FLUX_CONTROL_SLIDER]          = fx_cr_slider,
	[FLUX_CONTROL_TEXT_INPUT]      = fx_cr_textbox,
	[FLUX_CONTROL_PASSWORD_BOX]    = fx_cr_password,
	[FLUX_CONTROL_PROGRESS]        = fx_cr_progress,
	[FLUX_CONTROL_PROGRESS_RING]   = fx_cr_progress_ring,
	[FLUX_CONTROL_INFO_BADGE]      = fx_cr_badge,
	[FLUX_CONTROL_IMAGE]           = fx_cr_image,
	[FLUX_CONTROL_HYPERLINK]       = fx_cr_hyperlink,
	[FLUX_CONTROL_REPEAT_BUTTON]   = fx_cr_repeat_button,
	[FLUX_CONTROL_COMBO_BOX]       = fx_cr_combo,
	[FLUX_CONTROL_EXPANDER]        = fx_cr_expander,
	[FLUX_CONTROL_INFO_BAR]        = fx_cr_infobar,
	[FLUX_CONTROL_NUMBER_BOX]      = fx_cr_numberbox,
	[FLUX_CONTROL_DROPDOWN_BUTTON] = fx_cr_dropdown,
	[FLUX_CONTROL_SPLIT_BUTTON]    = fx_cr_split,
	[FLUX_CONTROL_MENU_BAR]        = fx_cr_menubar,
	[FLUX_CONTROL_TAB_VIEW]        = fx_cr_tabview,
	[FLUX_CONTROL_CONTENT_DIALOG]  = fx_cr_dialog,
	[FLUX_CONTROL_NAV_VIEW]        = fx_cr_nav,
};

XentNodeId fx_create_control(FxRuntime *rt, XentNodeId parent, FluxEl const *el, FxBinding *b) {
	if (el->type >= FX_CONTROL_TYPE_MAX) return XENT_NODE_INVALID;
	FxCreateFn fn = kCreateTable [el->type];
	return fn ? fn(rt, parent, el, b) : XENT_NODE_INVALID;
}

static XentNodeId fx_content_host_for(FluxNodeStore *store, XentNodeId node, FluxControlType type) {
	switch (type) {
	case FLUX_CONTROL_NAV_VIEW:        return flux_nav_view_content_node(store, node);
	case FLUX_CONTROL_EXPANDER:        return flux_expander_content_node(store, node);
	case FLUX_CONTROL_CONTENT_DIALOG:  return flux_content_dialog_content_node(store, node);
	default:                           return XENT_NODE_INVALID;
	}
}

static void fx_mount_content_host(FxRuntime *rt, FxNode *n, FluxEl const *el) {
	XentNodeId host = fx_content_host_for(rt->store, n->node, el->type);
	if (host == XENT_NODE_INVALID) return;
	n->host = host;
	xent_set_protocol(rt->ctx, host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(rt->ctx, host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(rt->ctx, host, XENT_FLEX_ALIGN_STRETCH);
	if (el->type == FLUX_CONTROL_CONTENT_DIALOG && el->dialog.open)
		flux_content_dialog_show(rt->store, n->node);
}

static void fx_mount_tab_view(FxRuntime *rt, FxNode *n, XentNodeId id, FluxEl const *el) {
	if (el->type != FLUX_CONTROL_TAB_VIEW || el->tab_view.tab_count <= 0) return;
	n->tab_hosts = ( XentNodeId * ) calloc(( size_t ) el->tab_view.tab_count, sizeof(XentNodeId));
	if (!n->tab_hosts) return;
	n->tab_host_count = el->tab_view.tab_count;
	for (int i = 0; i < el->tab_view.tab_count; i++) {
		XentNodeId content = XENT_NODE_INVALID;
		flux_tab_view_add_tab(rt->store, id, el->tab_view.tabs [i].icon, el->tab_view.tabs [i].label, &content);
		n->tab_hosts [i] = content;
		if (content == XENT_NODE_INVALID) continue;
		xent_set_protocol(rt->ctx, content, XENT_PROTOCOL_FLEX);
		xent_set_flex_direction(rt->ctx, content, XENT_FLEX_COLUMN);
		xent_set_flex_align_items(rt->ctx, content, XENT_FLEX_ALIGN_STRETCH);
	}
	int sel = el->tab_view.selected;
	if (sel < 0 || sel >= n->tab_host_count) sel = 0;
	if (sel > 0) flux_tab_view_select(rt->store, id, sel);
	if (n->tab_hosts [sel] != XENT_NODE_INVALID) n->host = n->tab_hosts [sel];
}

static void fx_mount_menu_bar(FxRuntime *rt, FxNode *n, XentNodeId id, FluxEl const *el) {
	if (el->type != FLUX_CONTROL_MENU_BAR || el->menu_bar.menu_count <= 0) return;
	int total = 0;
	for (int i = 0; i < el->menu_bar.menu_count; i++) total += el->menu_bar.menus [i].item_count;
	n->menu_bindings = total ? ( FxBinding * ) calloc(( size_t ) total, sizeof(FxBinding)) : NULL;
	if (!n->menu_bindings && total) return;
	n->menu_binding_count = total;
	int slot              = 0;
	for (int i = 0; i < el->menu_bar.menu_count; i++) {
		FxMenuDesc const *m    = &el->menu_bar.menus [i];
		FluxMenuFlyout   *menu = flux_menu_bar_add_menu(rt->store, id, m->title);
		if (menu) fx_fill_menu(rt, menu, m->items, m->item_count, &n->menu_bindings [slot]);
		slot += m->item_count;
	}
}

void fx_mount_setup(FxRuntime *rt, FxNode *n, XentNodeId id, FluxEl const *el) {
	fx_mount_content_host(rt, n, el);
	fx_mount_tab_view(rt, n, id, el);
	fx_mount_menu_bar(rt, n, id, el);
	fx_apply_props(rt, n, NULL, el);
	if (el->type == FLUX_CONTROL_INFO_BADGE && el->badge.icon)
		flux_info_badge_set_icon(rt->store, id, el->badge.icon);
}
