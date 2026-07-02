/**
 * @file flux_dispatch_create.c
 * @brief Table-driven per-type control creation and mount setup
 * for the fluxent backend.
 *
 * Owns kCreateTable, flux_be_create, and the mount helpers
 * (flux_be_mount_setup etc.).
 */
#include "flux_internal.h"

#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"

#include <math.h>
#include <stdlib.h>

#define FLUX_CONTROL_TYPE_MAX (FLUX_CONTROL_CUSTOM + 1)

typedef XentNodeId (*FluxCreateFn)(FluxBackendCtx *, XentNodeId, XtkEl const *, FluxBinding *);

static XentNodeId flux_cr_container(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el; ( void ) b;
	return flux_factory_create_node(rt->ctx, rt->store, p, FLUX_CONTROL_CONTAINER);
}

static XentNodeId flux_cr_card(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_card(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId flux_cr_scroll(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_scroll(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId flux_cr_divider(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_divider(&(FluxContainerCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId flux_cr_text(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_text(&(FluxTextCreateInfo) {rt->ctx, rt->store, p, el->text, el->text_props.size});
}

static XentNodeId flux_cr_button(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_button(&(FluxButtonCreateInfo) {rt->ctx, rt->store, p, el->text, flux_tramp_click, b});
}

static XentNodeId flux_cr_checkbox(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_checkbox(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, flux_tramp_toggle, b});
}

static XentNodeId flux_cr_radio(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_radio(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, flux_tramp_toggle, b});
}

static XentNodeId flux_cr_switch(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_switch(&(FluxToggleCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->toggle.checked, flux_tramp_toggle, b});
}

static XentNodeId flux_cr_slider(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_slider(&(FluxSliderCreateInfo) {
	  rt->ctx, rt->store, p, el->slider.min, el->slider.max, el->slider.value, flux_tramp_slider, b});
}

static XentNodeId flux_cr_textbox(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	if (rt->app)
		return flux_app_create_textbox(&(FluxAppTextBoxCreateInfo) {rt->app, p, el->text, flux_tramp_text, b});
	return flux_create_textbox(&(FluxTextBoxCreateInfo) {rt->ctx, rt->store, p, el->text, flux_tramp_text, b});
}

static XentNodeId flux_cr_password(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	if (rt->app)
		return flux_app_create_password_box(&(FluxAppTextBoxCreateInfo) {rt->app, p, el->text, flux_tramp_password, b});
	return flux_create_password_box(&(FluxTextBoxCreateInfo) {rt->ctx, rt->store, p, el->text, flux_tramp_password, b});
}

static XentNodeId flux_cr_progress(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_progress(&(FluxProgressCreateInfo) {rt->ctx, rt->store, p, el->progress.value, el->progress.max});
}

static XentNodeId flux_cr_progress_ring(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_progress_ring(&(FluxProgressCreateInfo) {
	  rt->ctx, rt->store, p, el->progress.value, el->progress.max});
}

static XentNodeId flux_cr_badge(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_info_badge(&(FluxInfoBadgeCreateInfo) {
	  rt->ctx, rt->store, p, el->badge.mode, el->badge.value});
}

static XentNodeId flux_cr_image(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_image(&(FluxImageCreateInfo) {rt->ctx, rt->store, p, el->text, el->image.stretch});
}

static XentNodeId flux_cr_hyperlink(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_hyperlink(&(FluxHyperlinkCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->hyperlink.url, flux_tramp_click, b});
}

static XentNodeId flux_cr_repeat_button(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_repeat_button(&(FluxButtonCreateInfo) {rt->ctx, rt->store, p, el->text, flux_tramp_click, b});
}

static XentNodeId flux_cr_combo(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_combo_box(&(FluxComboBoxCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .parent         = p,
	  .window         = flux_be_window(rt),
	  .text           = flux_be_text(rt),
	  .theme          = flux_be_theme(rt),
	  .items          = el->combo.items,
	  .item_count     = el->combo.count,
	  .selected_index = el->combo.selected,
	  .placeholder    = el->text,
	  .on_select      = flux_tramp_select,
	  .userdata       = b});
}

static XentNodeId flux_cr_expander(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_expander(&(FluxExpanderCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .parent         = p,
	  .window         = flux_be_window(rt),
	  .header         = el->text,
	  .expanded       = el->expander.expanded,
	  .width          = el->expander.width,
	  .content_height = el->expander.content_height,
	  .on_toggle      = flux_tramp_expand,
	  .userdata       = b});
}

static XentNodeId flux_cr_infobar(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_info_bar(&(FluxInfoBarCreateInfo) {
	  rt->ctx, rt->store, p, el->info_bar.severity, el->text, el->info_bar.message, el->info_bar.closable,
	  flux_tramp_click, b});
}

static XentNodeId flux_cr_numberbox(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	if (rt->app)
		return flux_app_create_number_box(&(FluxAppNumberBoxCreateInfo) {
		  rt->app, p, el->number.min, el->number.max, el->number.step, flux_tramp_double, b});
	return flux_create_number_box(&(FluxNumberBoxCreateInfo) {
	  rt->ctx, rt->store, p, el->number.min, el->number.max, el->number.step, flux_tramp_double, b});
}

static XentNodeId flux_cr_dropdown(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) b;
	return flux_create_dropdown_button(&(FluxDropDownButtonCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->dropdown.icon});
}

static XentNodeId flux_cr_split(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_split_button(&(FluxSplitButtonCreateInfo) {
	  rt->ctx, rt->store, p, el->text, el->split.icon, flux_tramp_click, b});
}

static XentNodeId flux_cr_menubar(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el; ( void ) b;
	return flux_create_menu_bar(&(FluxMenuBarCreateInfo) {
	  rt->ctx, rt->store, p, flux_be_window(rt), flux_be_text(rt), flux_be_theme(rt)});
}

static XentNodeId flux_cr_tabview(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	return flux_create_tab_view(&(FluxTabViewCreateInfo) {
	  .ctx                  = rt->ctx,
	  .store                = rt->store,
	  .parent               = p,
	  .window               = flux_be_window(rt),
	  .text                 = flux_be_text(rt),
	  .input                = rt->app ? flux_app_get_input(rt->app) : NULL,
	  .on_selection_changed = flux_tramp_select,
	  .on_close_requested   = flux_tramp_tab_close_req,
	  .on_add_tab           = el->tab_view.on_add.tag ? flux_tramp_click : NULL,
	  .userdata             = b});
}

static XentNodeId flux_cr_dialog(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	if (!rt->app) return XENT_NODE_INVALID;
	return flux_create_content_dialog(&(FluxContentDialogCreateInfo) {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .overlay_parent = rt->runtime->host,
	  .input          = flux_app_get_input(rt->app),
	  .window         = flux_be_window(rt),
	  .theme          = flux_be_theme(rt),
	  .title          = el->text,
	  .primary_text   = el->dialog.primary,
	  .secondary_text = el->dialog.secondary,
	  .close_text     = el->dialog.close,
	  .width          = el->dialog.width,
	  .content_height = el->dialog.content_height,
	  .on_result      = flux_tramp_dialog_result,
	  .userdata       = b});
}

static int flux_nav_add_child(FluxNodeStore *store, XentNodeId nav, int group, XtkNavItemDesc const *it) {
	if (group >= 0) {
		flux_nav_view_add_child_item(store, nav, group, it->icon, it->label);
		return group;
	}
	return flux_nav_view_add_item(store, nav, it->icon, it->label);
}

static void flux_create_nav_items(FluxNodeStore *store, XentNodeId nav, XtkEl const *el) {
	int group = -1;
	for (int i = 0; i < el->nav.item_count; i++) {
		XtkNavItemDesc const *it = &el->nav.items [i];
		switch (it->kind) {
		case XTK_NAV_ITEM      : group = flux_nav_view_add_item(store, nav, it->icon, it->label); break;
		case XTK_NAV_HEADER    : flux_nav_view_add_header(store, nav, it->label); break;
		case XTK_NAV_SEPARATOR : flux_nav_view_add_separator(store, nav); break;
		case XTK_NAV_FOOTER    : flux_nav_view_add_footer_item(store, nav, it->icon, it->label); break;
		case XTK_NAV_CHILD     : group = flux_nav_add_child(store, nav, group, it); break;
		}
	}
}

static XtkListKind flux_list_kind_for_type(XtkControlType type) {
	switch (type) {
	case XTK_CONTROL_LIST_BOX       : return XTK_LIST_KIND_LIST_BOX;
	case XTK_CONTROL_GRID_VIEW      : return XTK_LIST_KIND_GRID;
	case XTK_CONTROL_ITEMS_REPEATER : return XTK_LIST_KIND_REPEATER;
	default                         : return XTK_LIST_KIND_LIST;
	}
}

static XentNodeId flux_cr_list(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	XentNodeId list = flux_create_list_view(&(FluxListViewCreateInfo) {
	  .ctx       = rt->ctx,
	  .store     = rt->store,
	  .parent    = p,
	  .kind      = flux_list_kind_for_type(el->type),
	  .input     = rt->app ? flux_app_get_input(rt->app) : NULL,
	  .on_select = b ? flux_tramp_select : NULL,
	  .userdata  = b});
	if (list == XENT_NODE_INVALID) return list;
	flux_list_view_set_extent(
	  rt->store, list, el->list.count, el->list.item_height, el->list.item_width, el->list.columns
	);
	flux_list_view_set_sel_mode(rt->store, list, el->list.sel_mode);
	flux_list_view_set_selected(rt->store, list, el->list.selected);
	return list;
}

static XentNodeId flux_cr_list_item(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	( void ) el;
	( void ) b;
	return flux_create_list_item(&(FluxListItemCreateInfo) {rt->ctx, rt->store, p});
}

static XentNodeId flux_cr_flip(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	XentNodeId flip = flux_create_flip_view(&(FluxFlipViewCreateInfo) {
	  .ctx       = rt->ctx,
	  .store     = rt->store,
	  .parent    = p,
	  .window    = flux_be_window(rt),
	  .vertical  = el->flip.vertical,
	  .on_select = b ? flux_tramp_select : NULL,
	  .userdata  = b});
	if (flip != XENT_NODE_INVALID && el->flip.selected > 0)
		flux_flip_view_select(rt->store, flip, el->flip.selected);
	return flip;
}

static XentNodeId flux_cr_suggest(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	XentNodeId asb = flux_create_auto_suggest(&(FluxAutoSuggestCreateInfo) {
	  .ctx         = rt->ctx,
	  .store       = rt->store,
	  .parent      = p,
	  .app         = rt->app,
	  .window      = flux_be_window(rt),
	  .text        = flux_be_text(rt),
	  .theme       = flux_be_theme(rt),
	  .placeholder = el->text,
	  .query_icon  = el->suggest.query_icon,
	  .on_text     = flux_tramp_text,
	  .on_query    = flux_tramp_query,
	  .on_chosen   = flux_tramp_chosen,
	  .userdata    = b});
	if (asb == XENT_NODE_INVALID) return asb;
	if (el->suggest.content) flux_auto_suggest_set_content(rt->store, asb, el->suggest.content);
	flux_auto_suggest_set_suggestions(rt->store, asb, el->suggest.suggestions, el->suggest.count);
	return asb;
}

static XentNodeId flux_cr_pips(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	XentNodeId pips = flux_create_pips_pager(&(FluxPipsPagerCreateInfo) {
	  .ctx       = rt->ctx,
	  .store     = rt->store,
	  .parent    = p,
	  .vertical  = el->pips.vertical,
	  .on_select = b ? flux_tramp_select : NULL,
	  .userdata  = b});
	if (pips != XENT_NODE_INVALID)
		flux_pips_pager_configure(
		  rt->store, pips, el->pips.count, el->pips.selected, el->pips.max_visible, ( int ) el->pips.nav
		);
	return pips;
}

static XentNodeId flux_cr_nav(FluxBackendCtx *rt, XentNodeId p, XtkEl const *el, FluxBinding *b) {
	XentNodeId nav = flux_create_nav_view(&(FluxNavViewCreateInfo) {
	  .ctx                  = rt->ctx,
	  .store                = rt->store,
	  .parent               = p,
	  .window               = flux_be_window(rt),
	  .text                 = flux_be_text(rt),
	  .theme                = flux_be_theme(rt),
	  .width                = isnan(el->width) ? 0.0f : el->width,
	  .height               = isnan(el->height) ? 0.0f : el->height,
	  .adaptive             = true,
	  .on_selection_changed = flux_tramp_select,
	  .userdata             = b});
	if (nav == XENT_NODE_INVALID) return nav;
	flux_create_nav_items(rt->store, nav, el);
	if (el->nav.selected > 0) flux_nav_view_select(rt->store, nav, el->nav.selected);
	return nav;
}

static FluxCreateFn const kCreateTable [FLUX_CONTROL_TYPE_MAX] = {
	[FLUX_CONTROL_CONTAINER]       = flux_cr_container,
	[FLUX_CONTROL_CARD]            = flux_cr_card,
	[FLUX_CONTROL_SCROLL]          = flux_cr_scroll,
	[FLUX_CONTROL_DIVIDER]         = flux_cr_divider,
	[FLUX_CONTROL_TEXT]            = flux_cr_text,
	[FLUX_CONTROL_BUTTON]          = flux_cr_button,
	[FLUX_CONTROL_CHECKBOX]        = flux_cr_checkbox,
	[FLUX_CONTROL_RADIO]           = flux_cr_radio,
	[FLUX_CONTROL_SWITCH]          = flux_cr_switch,
	[FLUX_CONTROL_SLIDER]          = flux_cr_slider,
	[FLUX_CONTROL_TEXT_INPUT]      = flux_cr_textbox,
	[FLUX_CONTROL_PASSWORD_BOX]    = flux_cr_password,
	[FLUX_CONTROL_PROGRESS]        = flux_cr_progress,
	[FLUX_CONTROL_PROGRESS_RING]   = flux_cr_progress_ring,
	[FLUX_CONTROL_INFO_BADGE]      = flux_cr_badge,
	[FLUX_CONTROL_IMAGE]           = flux_cr_image,
	[FLUX_CONTROL_HYPERLINK]       = flux_cr_hyperlink,
	[FLUX_CONTROL_REPEAT_BUTTON]   = flux_cr_repeat_button,
	[FLUX_CONTROL_COMBO_BOX]       = flux_cr_combo,
	[FLUX_CONTROL_EXPANDER]        = flux_cr_expander,
	[FLUX_CONTROL_INFO_BAR]        = flux_cr_infobar,
	[FLUX_CONTROL_NUMBER_BOX]      = flux_cr_numberbox,
	[FLUX_CONTROL_DROPDOWN_BUTTON] = flux_cr_dropdown,
	[FLUX_CONTROL_SPLIT_BUTTON]    = flux_cr_split,
	[FLUX_CONTROL_MENU_BAR]        = flux_cr_menubar,
	[FLUX_CONTROL_TAB_VIEW]        = flux_cr_tabview,
	[FLUX_CONTROL_CONTENT_DIALOG]  = flux_cr_dialog,
	[FLUX_CONTROL_NAV_VIEW]        = flux_cr_nav,
	[FLUX_CONTROL_LIST]            = flux_cr_list,
	[FLUX_CONTROL_LIST_BOX]        = flux_cr_list,
	[FLUX_CONTROL_GRID_VIEW]       = flux_cr_list,
	[FLUX_CONTROL_ITEMS_REPEATER]  = flux_cr_list,
	[FLUX_CONTROL_LIST_ITEM]       = flux_cr_list_item,
	[FLUX_CONTROL_FLIP_VIEW]       = flux_cr_flip,
	[FLUX_CONTROL_PIPS_PAGER]      = flux_cr_pips,
	[FLUX_CONTROL_AUTO_SUGGEST]    = flux_cr_suggest,
};

XentNodeId flux_create_control(FluxBackendCtx *rt, XentNodeId parent, XtkEl const *el, FluxBinding *b) {
	if (el->type >= FLUX_CONTROL_TYPE_MAX) return XENT_NODE_INVALID;
	FluxCreateFn fn = kCreateTable [el->type];
	return fn ? fn(rt, parent, el, b) : XENT_NODE_INVALID;
}

static XentNodeId flux_content_host_for(FluxNodeStore *store, XentNodeId node, FluxControlType type) {
	switch (type) {
	case FLUX_CONTROL_NAV_VIEW:        return flux_nav_view_content_node(store, node);
	case FLUX_CONTROL_EXPANDER:        return flux_expander_content_node(store, node);
	case FLUX_CONTROL_CONTENT_DIALOG:  return flux_content_dialog_content_node(store, node);
	case FLUX_CONTROL_LIST:
	case FLUX_CONTROL_LIST_BOX:
	case FLUX_CONTROL_GRID_VIEW:
	case FLUX_CONTROL_ITEMS_REPEATER:  return flux_list_view_content_node(store, node);
	case FLUX_CONTROL_FLIP_VIEW:       return flux_flip_view_content_node(store, node);
	default:                           return XENT_NODE_INVALID;
	}
}

static void flux_mount_content_host(FluxBackendCtx *rt, XtkNode *n, XtkEl const *el) {
	XentNodeId host = flux_content_host_for(rt->store, n->node, el->type);
	if (host == XENT_NODE_INVALID) return;
	n->host = host;
	/* Items/pages hosts keep their ABSOLUTE protocol. */
	if (xtk_is_list_type(el->type) || el->type == FLUX_CONTROL_FLIP_VIEW) return;
	xent_set_protocol(rt->ctx, host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(rt->ctx, host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(rt->ctx, host, XENT_FLEX_ALIGN_STRETCH);
	if (el->type == FLUX_CONTROL_CONTENT_DIALOG && el->dialog.open)
		flux_content_dialog_show(rt->store, n->node);
}

static FluxNodeExt *flux_be_ext_ensure(XtkNode *n) {
	if (!n->ext) n->ext = calloc(1, sizeof(FluxNodeExt));
	return ( FluxNodeExt * ) n->ext;
}

static void flux_mount_tab_view(FluxBackendCtx *rt, XtkNode *n, XentNodeId id, XtkEl const *el) {
	if (el->type != FLUX_CONTROL_TAB_VIEW || el->tab_view.tab_count <= 0) return;
	FluxNodeExt *ext = flux_be_ext_ensure(n);
	if (!ext) return;
	ext->tab_hosts = ( XentNodeId * ) calloc(( size_t ) el->tab_view.tab_count, sizeof(XentNodeId));
	if (!ext->tab_hosts) return;
	ext->tab_host_count = el->tab_view.tab_count;
	for (int i = 0; i < el->tab_view.tab_count; i++) {
		XentNodeId content = XENT_NODE_INVALID;
		flux_tab_view_add_tab(rt->store, id, el->tab_view.tabs [i].icon, el->tab_view.tabs [i].label, &content);
		ext->tab_hosts [i] = content;
		if (content == XENT_NODE_INVALID) continue;
		xent_set_protocol(rt->ctx, content, XENT_PROTOCOL_FLEX);
		xent_set_flex_direction(rt->ctx, content, XENT_FLEX_COLUMN);
		xent_set_flex_align_items(rt->ctx, content, XENT_FLEX_ALIGN_STRETCH);
	}
	int sel = el->tab_view.selected;
	if (sel < 0 || sel >= ext->tab_host_count) sel = 0;
	if (sel > 0) flux_tab_view_select(rt->store, id, sel);
	if (ext->tab_hosts [sel] != XENT_NODE_INVALID) n->host = ext->tab_hosts [sel];
}

static void flux_mount_menu_bar(FluxBackendCtx *rt, XtkNode *n, XentNodeId id, XtkEl const *el) {
	if (el->type != FLUX_CONTROL_MENU_BAR || el->menu_bar.menu_count <= 0) return;
	FluxNodeExt *ext = flux_be_ext_ensure(n);
	if (!ext) return;
	int total = 0;
	for (int i = 0; i < el->menu_bar.menu_count; i++) total += el->menu_bar.menus [i].item_count;
	ext->menu_bindings = total ? ( FluxBinding * ) calloc(( size_t ) total, sizeof(FluxBinding)) : NULL;
	if (!ext->menu_bindings && total) return;
	ext->menu_binding_count = total;
	int slot                = 0;
	for (int i = 0; i < el->menu_bar.menu_count; i++) {
		XtkMenuDesc const *m    = &el->menu_bar.menus [i];
		FluxMenuFlyout   *menu = flux_menu_bar_add_menu(rt->store, id, m->title);
		if (menu) flux_be_fill_menu(rt, menu, m->items, m->item_count, &ext->menu_bindings [slot]);
		slot += m->item_count;
	}
}

/* Scrolling outside the realized row window re-runs view + reconcile. */
static void flux_be_list_stale(void *ctx) {
	FluxBackendCtx *rt = ( FluxBackendCtx * ) ctx;
	if (rt->runtime) xtk_runtime_invalidate(rt->runtime);
}

void flux_mount_setup(FluxBackendCtx *rt, XtkNode *n, XentNodeId id, XtkEl const *el) {
	flux_mount_content_host(rt, n, el);
	flux_mount_tab_view(rt, n, id, el);
	flux_mount_menu_bar(rt, n, id, el);
	if (xtk_is_list_type(el->type)) flux_list_view_set_stale_callback(rt->store, id, flux_be_list_stale, rt);
	flux_apply_props(rt, n, NULL, el);
	if (el->type == FLUX_CONTROL_INFO_BADGE && el->badge.icon)
		flux_info_badge_set_icon(rt->store, id, el->badge.icon);
}
