/**
 * @file flux_dispatch_props.c
 * @brief Table-driven per-type property application, binding wiring,
 * and interactive-type lookup for the fluxent backend.
 *
 * Owns kPropsTable, kInteractive, flux_be_apply_props, and flux_be_is_interactive.
 */
#include "flux_internal.h"

#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_CONTROL_TYPE_MAX (FLUX_CONTROL_CUSTOM + 1)

void flux_tramp_click(void *ud) {
	FluxBinding *b = ( FluxBinding * ) ud;
	xtk_runtime_post(b->rt, b->on_click);
}

void flux_tramp_toggle(void *ud, FluxCheckState state) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.b          = state == FLUX_CHECK_CHECKED;
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_slider(void *ud, float value) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.f          = value;
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_text(void *ud, char const *text) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.ptr        = ( void * ) text;
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_password(void *ud, char const *text) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.i          = xtk_utf8_len(text);
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_select(void *ud, int index) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.i          = index;
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_expand(void *ud, bool expanded) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.b          = expanded;
	xtk_runtime_post(b->rt, m);
}

void flux_tramp_double(void *ud, double value) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.d          = value;
	xtk_runtime_post(b->rt, m);
}

bool flux_tramp_tab_close_req(void *ud, int index) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_close;
	m.i          = index;
	xtk_runtime_post(b->rt, m);
	return false;
}

void flux_tramp_dialog_result(void *ud, FluxDialogResult result) {
	FluxBinding *b = ( FluxBinding * ) ud;
	XtkMsg      m = b->on_change;
	m.i          = ( int32_t ) result;
	xtk_runtime_post(b->rt, m);
}

FluxWindow       *flux_be_window(FluxBackendCtx *rt) { return rt->app ? flux_app_get_window(rt->app) : NULL; }
FluxTextRenderer *flux_be_text(FluxBackendCtx *rt) { return rt->app ? flux_app_get_text_renderer(rt->app) : NULL; }
FluxThemeManager *flux_be_theme(FluxBackendCtx *rt) { return rt->app ? flux_app_get_theme(rt->app) : NULL; }

static bool flux_feq(float a, float b) { return (isnan(a) && isnan(b)) || a == b; }

static bool flux_streq(char const *a, char const *b) {
	if (a == b) return true;
	if (!a || !b) return false;
	return strcmp(a, b) == 0;
}

static bool flux_insets_eq(XentInsets a, XentInsets b) { return memcmp(&a, &b, sizeof(a)) == 0; }

/* Grow semantics: positive grow implies flex-basis: 0 (fill mode). */
static void flux_apply_grow(XentContext *ctx, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->grow != el->grow) {
		xent_set_flex_grow(ctx, id, el->grow);
		xent_set_flex_basis(ctx, id, el->grow > 0.0f ? 0.0f : NAN);
	}
}

/* Diff width/height/tooltip/grow/align/margin against prev; skip unchanged. */
static void flux_apply_common(FluxBackendCtx *rt, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	XentContext *ctx = rt->ctx;
	if (prev ? !flux_feq(prev->width, el->width) : !isnan(el->width)) xent_set_width(ctx, id, el->width);
	if (prev ? !flux_feq(prev->height, el->height) : !isnan(el->height)) xent_set_height(ctx, id, el->height);
	if (prev ? !flux_streq(prev->tooltip, el->tooltip) : el->tooltip != NULL)
		flux_node_set_tooltip(rt->store, id, el->tooltip);
	flux_apply_grow(ctx, id, prev, el);
	if (!prev || prev->align_self != el->align_self) xent_set_flex_align_self(ctx, id, el->align_self);
	if (!prev || !flux_insets_eq(prev->margin, el->margin)) xent_set_margin(ctx, id, el->margin);
}

/* Rows wrap height, columns wrap width; fill disables the primary axis. */
static void flux_apply_stack_wrap(XentContext *ctx, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->stack.fill != el->stack.fill || prev->stack_row != el->stack_row) {
		bool wrap_w = el->stack_row ? !el->stack.fill : true;
		bool wrap_h = el->stack_row ? true : !el->stack.fill;
		xent_set_wrap_content(ctx, id, wrap_w, wrap_h);
	}
}

/* Set flex protocol, direction, gap, padding, alignment on stack containers. */
static void flux_apply_stack(FluxBackendCtx *rt, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	XentContext *ctx = rt->ctx;
	if (!prev) xent_set_protocol(ctx, id, XENT_PROTOCOL_FLEX);
	if (!prev || prev->stack_row != el->stack_row)
		xent_set_flex_direction(ctx, id, el->stack_row ? XENT_FLEX_ROW : XENT_FLEX_COLUMN);
	if (!prev || prev->stack.gap != el->stack.gap) xent_set_gap(ctx, id, el->stack.gap);
	if (!prev || !flux_insets_eq(prev->stack.padding, el->stack.padding)) xent_set_padding(ctx, id, el->stack.padding);
	XentFlexAlign align = el->stack.align ? el->stack.align : XENT_FLEX_ALIGN_STRETCH;
	if (!prev || prev->stack.align != el->stack.align) xent_set_flex_align_items(ctx, id, align);
	if (!prev || prev->stack.justify != el->stack.justify) xent_set_flex_justify_content(ctx, id, el->stack.justify);
	flux_apply_stack_wrap(ctx, id, prev, el);
}

/* WinUI badge sizing: dot=8, icon/single-digit=16, two-digit=24, 100+=30. */
static XentSize flux_badge_size(XtkBadgeDesc const *b) {
	if (b->mode == FLUX_BADGE_DOT) return (XentSize) {8.0f, 8.0f};
	if (b->mode == FLUX_BADGE_ICON || b->value < 10) return (XentSize) {16.0f, 16.0f};
	return b->value < 100 ? (XentSize) {24.0f, 16.0f} : (XentSize) {30.0f, 16.0f};
}

/* Deep-compare combo item arrays to avoid unnecessary flux_combo_box_set_items. */
static bool flux_combo_items_eq(XtkComboDesc const *a, XtkComboDesc const *b) {
	if (a->count != b->count) return false;
	for (int i = 0; i < a->count; i++)
		if (!flux_streq(a->items [i], b->items [i])) return false;
	return true;
}

/* Deep-compare menu items (label + icon + disabled) to skip menu rebuilds. */
static bool flux_menu_items_eq(XtkMenuItemDesc const *a, int ac, XtkMenuItemDesc const *b, int bc) {
	if (ac != bc) return false;
	for (int i = 0; i < ac; i++) {
		if (!flux_streq(a [i].label, b [i].label)) return false;
		if (!flux_streq(a [i].icon, b [i].icon)) return false;
		if (a [i].disabled != b [i].disabled) return false;
	}
	return true;
}

void flux_be_fill_menu(FluxBackendCtx *rt, FluxMenuFlyout *menu, XtkMenuItemDesc const *items, int count, FluxBinding *slots) {
	for (int i = 0; i < count; i++) {
		XtkMenuItemDesc const *it = &items [i];
		slots [i]                = (FluxBinding) {.rt = rt->runtime, .on_click = it->on_click};
		if (!it->label) {
			flux_menu_flyout_add_separator(menu);
			continue;
		}
		char glyph [8] = {0};
		if (it->icon) flux_icon_to_utf8(flux_icon_lookup(it->icon), glyph, sizeof(glyph));
		flux_menu_flyout_add_item(
		  menu, &(FluxMenuItemDef) {
				  .type         = FLUX_MENU_ITEM_NORMAL,
				  .label        = it->label,
				  .icon_glyph   = glyph [0] ? glyph : NULL,
				  .enabled      = !it->disabled,
				  .on_click     = flux_tramp_click,
				  .on_click_ctx = &slots [i],
				}
		);
	}
}

static FluxNodeExt *flux_be_ext(XtkNode *n) {
	if (!n->ext) n->ext = calloc(1, sizeof(FluxNodeExt));
	return ( FluxNodeExt * ) n->ext;
}

/* Create or rebuild the owned flyout menu attached to a dropdown/split button. */
static void flux_sync_owned_menu(FluxBackendCtx *rt, XtkNode *n, XtkMenuItemDesc const *items, int count, bool split) {
	FluxWindow *window = flux_be_window(rt);
	if (!window) return;
	FluxNodeExt *ext = flux_be_ext(n);
	if (!ext) return;

	if (!ext->menu) {
		ext->menu = flux_menu_flyout_create(window);
		if (!ext->menu) return;
		flux_menu_flyout_set_theme_manager(ext->menu, flux_be_theme(rt));
		flux_menu_flyout_set_text_renderer(ext->menu, flux_be_text(rt));
		FluxContextFlyoutBindingInfo bind = {rt->store, n->node, ext->menu, rt->ctx, window};
		if (split) flux_split_button_set_menu_flyout_ex(&bind);
		else flux_node_set_menu_flyout_ex(&bind);
	}

	if (count != ext->menu_binding_count) {
		FluxBinding *bindings
		  = ( FluxBinding * ) realloc(ext->menu_bindings, sizeof(FluxBinding) * ( size_t ) (count ? count : 1));
		if (!bindings) return;
		ext->menu_bindings      = bindings;
		ext->menu_binding_count = count;
	}

	flux_menu_flyout_clear(ext->menu);
	flux_be_fill_menu(rt, ext->menu, items, count, ext->menu_bindings);
}

/* Reparent children into the newly-selected tab's content host. */
static void flux_tab_view_home_children(FluxBackendCtx *rt, XtkNode *n, int selected) {
	FluxNodeExt *ext = ( FluxNodeExt * ) n->ext;
	if (!ext || !ext->tab_hosts || selected < 0 || selected >= ext->tab_host_count) return;
	XentNodeId host = ext->tab_hosts [selected];
	if (host == XENT_NODE_INVALID || host == n->host) return;
	n->host = host;
	for (int i = 0; i < n->child_count; i++)
		if (n->children [i]) xent_append_child(rt->ctx, host, n->children [i]->node);
}

/* Resolve the on_click message for el's type (hyperlink, info_bar, etc.). */
static XtkMsg flux_binding_click(XtkEl const *el) {
	switch (el->type) {
	case FLUX_CONTROL_HYPERLINK    : return el->hyperlink.on_click;
	case FLUX_CONTROL_INFO_BAR     : return el->info_bar.on_close;
	case FLUX_CONTROL_SPLIT_BUTTON : return el->split.on_click;
	case FLUX_CONTROL_TAB_VIEW     : return el->tab_view.on_add;
	default                        : return el->button.on_click;
	}
}

/* Controls whose on_change comes from a non-toggle union member. */
static bool flux_binding_change_special(XtkEl const *el, XtkMsg *out) {
	switch (el->type) {
	case FLUX_CONTROL_SLIDER       : *out = el->slider.on_change; return true;
	case FLUX_CONTROL_TEXT_INPUT    :
	case FLUX_CONTROL_PASSWORD_BOX  : *out = el->textbox.on_change; return true;
	case FLUX_CONTROL_COMBO_BOX    : *out = el->combo.on_select; return true;
	case FLUX_CONTROL_NAV_VIEW     : *out = el->nav.on_select; return true;
	case FLUX_CONTROL_LIST         :
	case FLUX_CONTROL_LIST_BOX     :
	case FLUX_CONTROL_GRID_VIEW    : *out = el->list.on_select; return true;
	case FLUX_CONTROL_FLIP_VIEW    : *out = el->flip.on_select; return true;
	case FLUX_CONTROL_PIPS_PAGER   : *out = el->pips.on_select; return true;
	default                        : return false;
	}
}

static XtkMsg flux_binding_change(XtkEl const *el) {
	XtkMsg m;
	if (flux_binding_change_special(el, &m)) return m;
	switch (el->type) {
	case FLUX_CONTROL_TAB_VIEW       : return el->tab_view.on_select;
	case FLUX_CONTROL_CONTENT_DIALOG : return el->dialog.on_result;
	case FLUX_CONTROL_EXPANDER       : return el->expander.on_toggle;
	case FLUX_CONTROL_NUMBER_BOX     : return el->number.on_change;
	default                          : return el->toggle.on_change;
	}
}

/* Rewire the stable binding slots with this frame's message values. */
static void flux_apply_bindings(XtkNode *n, XtkEl const *el) {
	FluxBinding *b = ( FluxBinding * ) n->binding;
	if (!b) return;
	b->on_click  = flux_binding_click(el);
	b->on_change = flux_binding_change(el);
	b->on_close  = el->type == FLUX_CONTROL_TAB_VIEW ? el->tab_view.on_close : ( XtkMsg ) {0};
}

/* Signature for per-type prop handlers; NULL in kPropsTable means no-op. */
typedef void (*FluxPropsFn)(FluxBackendCtx *, XtkNode *, XtkEl const *, XtkEl const *);

static void flux_pp_stack(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	flux_apply_stack(rt, n->node, prev, el);
}

static float flux_font_size(float size) { return size > 0.0f ? size : 14.0f; }

static void flux_pp_text_style(FluxNodeStore *store, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->text_props.size != el->text_props.size)
		flux_text_set_font_size(store, id, flux_font_size(el->text_props.size));
	if (!prev || prev->text_props.weight != el->text_props.weight)
		flux_text_set_weight(store, id, el->text_props.weight);
	if (!prev || prev->text_props.align != el->text_props.align)
		flux_text_set_alignment(store, id, el->text_props.align);
	if (!prev || prev->text_props.color.rgba != el->text_props.color.rgba)
		flux_text_set_color(store, id, el->text_props.color);
}

static void flux_pp_text(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && !flux_streq(prev->text, el->text)) flux_text_set_content(rt->store, n->node, el->text);
	flux_pp_text_style(rt->store, n->node, prev, el);
}

static void flux_pp_button(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev ? !flux_streq(prev->button.icon, el->button.icon) : el->button.icon != NULL)
		flux_button_set_icon(store, id, el->button.icon);
	if (!prev || prev->button.style != el->button.style) flux_button_set_style(store, id, el->button.style);
	if (!prev || prev->button.disabled != el->button.disabled)
		flux_button_set_enabled(store, id, !el->button.disabled);
}

static void flux_pp_toggle(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_checkbox_set_label(store, id, el->text);
	if (prev && prev->toggle.checked != el->toggle.checked)
		flux_checkbox_set_state(store, id, el->toggle.checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
	if (!prev || prev->toggle.disabled != el->toggle.disabled)
		flux_checkbox_set_enabled(store, id, !el->toggle.disabled);
}

static void flux_pp_slider_range(FluxNodeStore *store, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->slider.min != el->slider.min || prev->slider.max != el->slider.max)
		flux_slider_set_range(store, id, el->slider.min, el->slider.max);
}

static void flux_pp_slider_ticks(FluxNodeStore *store, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->slider.tick != el->slider.tick) flux_slider_set_tick_frequency(store, id, el->slider.tick);
	if (!prev || prev->slider.tick_placement != el->slider.tick_placement)
		flux_slider_set_tick_placement(store, id, el->slider.tick_placement);
	if (!prev || prev->slider.snap_to_ticks != el->slider.snap_to_ticks)
		flux_slider_set_snaps_to(
		  store, id, el->slider.snap_to_ticks ? FLUX_SLIDER_SNAPS_TO_TICKS : FLUX_SLIDER_SNAPS_TO_STEP_VALUES
		);
}

static void flux_pp_slider_changes(FluxNodeStore *store, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->slider.small_change != el->slider.small_change)
		flux_slider_set_small_change(store, id, el->slider.small_change);
	if (!prev || prev->slider.large_change != el->slider.large_change)
		flux_slider_set_large_change(store, id, el->slider.large_change);
}

static void flux_pp_slider(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	flux_pp_slider_range(store, id, prev, el);
	if (prev && prev->slider.value != el->slider.value) flux_slider_set_value(store, id, el->slider.value);
	if (!prev || prev->slider.step != el->slider.step) flux_slider_set_step(store, id, el->slider.step);
	if (!prev || prev->slider.disabled != el->slider.disabled)
		flux_slider_set_enabled(store, id, !el->slider.disabled);
	flux_pp_slider_ticks(store, id, prev, el);
	flux_pp_slider_changes(store, id, prev, el);
}

/* Sync textbox content only when it differs from the live control state. */
static void flux_sync_textbox_content(FluxNodeStore *store, XentNodeId id, XtkEl const *el, bool password) {
	if (!el->textbox.content) return;
	FluxNodeData *nd    = flux_node_store_get(store, id);
	char const *current = nd && nd->component_data ? (( FluxTextBoxData * ) nd->component_data)->content : NULL;
	if (flux_streq(current, el->textbox.content)) return;
	if (password) flux_password_set_content(store, id, el->textbox.content);
	else flux_textbox_set_content(store, id, el->textbox.content);
}

static void flux_pp_textbox(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store    = rt->store;
	XentNodeId     id       = n->node;
	bool           password = el->type == FLUX_CONTROL_PASSWORD_BOX;
	if (prev && !flux_streq(prev->text, el->text)) {
		if (password) flux_password_set_placeholder(store, id, el->text);
		else flux_textbox_set_placeholder(store, id, el->text);
	}
	flux_sync_textbox_content(store, id, el, password);
	if (!prev || prev->textbox.disabled != el->textbox.disabled) {
		if (password) flux_password_set_enabled(store, id, !el->textbox.disabled);
		else flux_textbox_set_enabled(store, id, !el->textbox.disabled);
	}
}

static void flux_pp_progress(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->progress.value != el->progress.value) flux_progress_set_value(store, id, el->progress.value);
	if (!prev || prev->progress.max != el->progress.max) flux_progress_set_max(store, id, el->progress.max);
	if (!prev || prev->progress.indeterminate != el->progress.indeterminate)
		flux_progress_set_indeterminate(store, id, el->progress.indeterminate);
}

static void flux_pp_progress_ring(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->progress.value != el->progress.value) flux_progress_ring_set_value(store, id, el->progress.value);
	if (!prev || prev->progress.max != el->progress.max) flux_progress_ring_set_max(store, id, el->progress.max);
	if (!prev || prev->progress.indeterminate != el->progress.indeterminate)
		flux_progress_ring_set_indeterminate(store, id, el->progress.indeterminate);
}

static bool flux_badge_resized(XtkEl const *prev, XtkEl const *el) {
	return prev && (prev->badge.mode != el->badge.mode || prev->badge.value != el->badge.value) && isnan(el->width);
}

static void flux_pp_badge(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->badge.mode != el->badge.mode) flux_info_badge_set_mode(store, id, el->badge.mode);
	if (prev && prev->badge.value != el->badge.value) flux_info_badge_set_value(store, id, el->badge.value);
	if (prev && !flux_streq(prev->badge.icon, el->badge.icon)) flux_info_badge_set_icon(store, id, el->badge.icon);
	if (flux_badge_resized(prev, el)) xent_set_size(rt->ctx, id, flux_badge_size(&el->badge));
}

static void flux_pp_image(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && !flux_streq(prev->text, el->text)) flux_image_set_source(rt->store, n->node, el->text);
	if (!prev || prev->image.stretch != el->image.stretch) flux_image_set_stretch(rt->store, n->node, el->image.stretch);
}

static void flux_pp_hyperlink(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_hyperlink_set_label(store, id, el->text);
	if (prev && !flux_streq(prev->hyperlink.url, el->hyperlink.url))
		flux_hyperlink_set_url(store, id, el->hyperlink.url);
	if (!prev || prev->hyperlink.disabled != el->hyperlink.disabled)
		flux_hyperlink_set_enabled(store, id, !el->hyperlink.disabled);
}

static void flux_pp_repeat_button(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_repeat_button_set_label(store, id, el->text);
	if (prev ? !flux_streq(prev->button.icon, el->button.icon) : el->button.icon != NULL)
		flux_repeat_button_set_icon(store, id, el->button.icon);
	if (!prev || prev->button.style != el->button.style) flux_repeat_button_set_style(store, id, el->button.style);
	if (!prev || prev->button.disabled != el->button.disabled)
		flux_repeat_button_set_enabled(store, id, !el->button.disabled);
}

static void flux_pp_combo(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && !flux_combo_items_eq(&prev->combo, &el->combo))
		flux_combo_box_set_items(rt->store, n->node, el->combo.items, el->combo.count);
	if (prev && prev->combo.selected != el->combo.selected)
		flux_combo_box_set_selected(rt->store, n->node, el->combo.selected);
}

static void flux_pp_expander(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && prev->expander.expanded != el->expander.expanded)
		flux_expander_set_expanded(rt->store, n->node, rt->ctx, el->expander.expanded);
}

static void flux_pp_infobar(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_info_bar_set_title(store, id, el->text);
	if (prev && !flux_streq(prev->info_bar.message, el->info_bar.message))
		flux_info_bar_set_message(store, id, el->info_bar.message);
	if (prev && prev->info_bar.severity != el->info_bar.severity)
		flux_info_bar_set_severity(store, id, el->info_bar.severity);
}

static void flux_pp_number_range(FluxNodeStore *store, XentNodeId id, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->number.min != el->number.min || prev->number.max != el->number.max)
		flux_numberbox_set_range(store, id, el->number.min, el->number.max);
	if (!prev || prev->number.step != el->number.step) flux_numberbox_set_step(store, id, el->number.step);
	if (!prev || prev->number.value != el->number.value) flux_numberbox_set_value(store, id, el->number.value);
}

static void flux_pp_number(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	flux_pp_number_range(rt->store, n->node, prev, el);
	if (!prev || prev->number.hide_spin != el->number.hide_spin)
		flux_numberbox_set_spin_enabled(rt->store, n->node, !el->number.hide_spin);
	if (!prev || prev->number.disabled != el->number.disabled)
		flux_numberbox_set_enabled(rt->store, n->node, !el->number.disabled);
}

static void flux_pp_nav(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && prev->nav.selected != el->nav.selected) flux_nav_view_select(rt->store, n->node, el->nav.selected);
}

static void flux_pp_list(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->list.count != el->list.count || prev->list.item_height != el->list.item_height
	    || prev->list.item_width != el->list.item_width || prev->list.columns != el->list.columns)
		flux_list_view_set_extent(
		  rt->store, n->node, el->list.count, el->list.item_height, el->list.item_width, el->list.columns
		);
	if (!prev || prev->list.sel_mode != el->list.sel_mode)
		flux_list_view_set_sel_mode(rt->store, n->node, el->list.sel_mode);
	if (prev && prev->list.selected != el->list.selected)
		flux_list_view_set_selected(rt->store, n->node, el->list.selected);
}

/* Recycled cells: an index change repositions the retained node to its new
 * content-space slot; content underneath was already patched by the regular
 * child diff. */
static void flux_pp_list_item(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->item.index != el->item.index || prev->item.x != el->item.x || prev->item.y != el->item.y)
		flux_list_item_set_place(rt->store, n->node, el->item.index, el->item.x, el->item.y);
	if (!prev || prev->item.selected != el->item.selected || prev->item.multi != el->item.multi)
		flux_list_item_set_state(rt->store, n->node, el->item.selected, el->item.multi);
}

static void flux_rewire_menu_bindings(XtkNode *n, XtkMenuItemDesc const *items, int count) {
	FluxNodeExt *ext = ( FluxNodeExt * ) n->ext;
	if (!ext) return;
	for (int i = 0; i < ext->menu_binding_count && i < count; i++)
		ext->menu_bindings [i].on_click = items [i].on_click;
}

static void flux_sync_dropdown_menu(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (!prev
		|| !flux_menu_items_eq(prev->dropdown.items, prev->dropdown.item_count, el->dropdown.items, el->dropdown.item_count))
		flux_sync_owned_menu(rt, n, el->dropdown.items, el->dropdown.item_count, false);
	else
		flux_rewire_menu_bindings(n, el->dropdown.items, el->dropdown.item_count);
}

static void flux_pp_dropdown(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev && !flux_streq(prev->dropdown.icon, el->dropdown.icon))
		flux_button_set_icon(store, id, el->dropdown.icon);
	if (!prev || prev->dropdown.style != el->dropdown.style) flux_button_set_style(store, id, el->dropdown.style);
	if (!prev || prev->dropdown.disabled != el->dropdown.disabled)
		flux_button_set_enabled(store, id, !el->dropdown.disabled);
	flux_sync_dropdown_menu(rt, n, prev, el);
}

static void flux_sync_split_menu(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (!prev
		|| !flux_menu_items_eq(prev->split.items, prev->split.item_count, el->split.items, el->split.item_count))
		flux_sync_owned_menu(rt, n, el->split.items, el->split.item_count, true);
	else
		flux_rewire_menu_bindings(n, el->split.items, el->split.item_count);
}

static void flux_pp_split(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !flux_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev ? !flux_streq(prev->split.icon, el->split.icon) : el->split.icon != NULL)
		flux_button_set_icon(store, id, el->split.icon);
	if (!prev || prev->split.disabled != el->split.disabled)
		flux_button_set_enabled(store, id, !el->split.disabled);
	flux_sync_split_menu(rt, n, prev, el);
}

static void flux_pp_tab(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && prev->tab_view.selected != el->tab_view.selected) {
		flux_tab_view_select(rt->store, n->node, el->tab_view.selected);
		flux_tab_view_home_children(rt, n, el->tab_view.selected);
	}
}

static void flux_pp_flip(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && prev->flip.selected != el->flip.selected)
		flux_flip_view_select(rt->store, n->node, el->flip.selected);
}

static void flux_pp_pips(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (!prev || prev->pips.count != el->pips.count || prev->pips.selected != el->pips.selected
	    || prev->pips.max_visible != el->pips.max_visible || prev->pips.nav != el->pips.nav)
		flux_pips_pager_configure(
		  rt->store, n->node, el->pips.count, el->pips.selected, el->pips.max_visible, ( int ) el->pips.nav
		);
}

static void flux_pp_dialog(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	if (prev && prev->dialog.open != el->dialog.open) {
		if (el->dialog.open) flux_content_dialog_show(rt->store, n->node);
		else flux_content_dialog_hide(rt->store, n->node);
	}
}

/* Dispatch table: FluxControlType → per-type prop update handler. */
static FluxPropsFn const kPropsTable [FLUX_CONTROL_TYPE_MAX] = {
	[FLUX_CONTROL_CONTAINER]       = flux_pp_stack,
	[FLUX_CONTROL_CARD]            = flux_pp_stack,
	[FLUX_CONTROL_SCROLL]          = flux_pp_stack,
	[FLUX_CONTROL_TEXT]            = flux_pp_text,
	[FLUX_CONTROL_BUTTON]          = flux_pp_button,
	[FLUX_CONTROL_CHECKBOX]        = flux_pp_toggle,
	[FLUX_CONTROL_RADIO]           = flux_pp_toggle,
	[FLUX_CONTROL_SWITCH]          = flux_pp_toggle,
	[FLUX_CONTROL_SLIDER]          = flux_pp_slider,
	[FLUX_CONTROL_TEXT_INPUT]      = flux_pp_textbox,
	[FLUX_CONTROL_PASSWORD_BOX]    = flux_pp_textbox,
	[FLUX_CONTROL_PROGRESS]        = flux_pp_progress,
	[FLUX_CONTROL_PROGRESS_RING]   = flux_pp_progress_ring,
	[FLUX_CONTROL_INFO_BADGE]      = flux_pp_badge,
	[FLUX_CONTROL_IMAGE]           = flux_pp_image,
	[FLUX_CONTROL_HYPERLINK]       = flux_pp_hyperlink,
	[FLUX_CONTROL_REPEAT_BUTTON]   = flux_pp_repeat_button,
	[FLUX_CONTROL_COMBO_BOX]       = flux_pp_combo,
	[FLUX_CONTROL_EXPANDER]        = flux_pp_expander,
	[FLUX_CONTROL_INFO_BAR]        = flux_pp_infobar,
	[FLUX_CONTROL_NUMBER_BOX]      = flux_pp_number,
	[FLUX_CONTROL_NAV_VIEW]        = flux_pp_nav,
	[FLUX_CONTROL_LIST]            = flux_pp_list,
	[FLUX_CONTROL_LIST_BOX]        = flux_pp_list,
	[FLUX_CONTROL_GRID_VIEW]       = flux_pp_list,
	[FLUX_CONTROL_ITEMS_REPEATER]  = flux_pp_list,
	[FLUX_CONTROL_LIST_ITEM]       = flux_pp_list_item,
	[FLUX_CONTROL_FLIP_VIEW]       = flux_pp_flip,
	[FLUX_CONTROL_PIPS_PAGER]      = flux_pp_pips,
	[FLUX_CONTROL_DROPDOWN_BUTTON] = flux_pp_dropdown,
	[FLUX_CONTROL_SPLIT_BUTTON]    = flux_pp_split,
	[FLUX_CONTROL_TAB_VIEW]        = flux_pp_tab,
	[FLUX_CONTROL_CONTENT_DIALOG]  = flux_pp_dialog,
};

void flux_apply_props(FluxBackendCtx *rt, XtkNode *n, XtkEl const *prev, XtkEl const *el) {
	flux_apply_bindings(n, el);
	if (el->type < FLUX_CONTROL_TYPE_MAX) {
		FluxPropsFn fn = kPropsTable [el->type];
		if (fn) fn(rt, n, prev, el);
	}
	flux_apply_common(rt, n->node, prev, el);
}

/* Controls that need an FluxBinding (event wiring) during mount. */
static const bool kInteractive [FLUX_CONTROL_TYPE_MAX] = {
	[FLUX_CONTROL_BUTTON]          = true,
	[FLUX_CONTROL_CHECKBOX]        = true,
	[FLUX_CONTROL_RADIO]           = true,
	[FLUX_CONTROL_SWITCH]          = true,
	[FLUX_CONTROL_SLIDER]          = true,
	[FLUX_CONTROL_TEXT_INPUT]      = true,
	[FLUX_CONTROL_PASSWORD_BOX]    = true,
	[FLUX_CONTROL_HYPERLINK]       = true,
	[FLUX_CONTROL_REPEAT_BUTTON]   = true,
	[FLUX_CONTROL_COMBO_BOX]       = true,
	[FLUX_CONTROL_EXPANDER]        = true,
	[FLUX_CONTROL_INFO_BAR]        = true,
	[FLUX_CONTROL_NUMBER_BOX]      = true,
	[FLUX_CONTROL_SPLIT_BUTTON]    = true,
	[FLUX_CONTROL_TAB_VIEW]        = true,
	[FLUX_CONTROL_CONTENT_DIALOG]  = true,
	[FLUX_CONTROL_NAV_VIEW]        = true,
	[FLUX_CONTROL_LIST]            = true,
	[FLUX_CONTROL_LIST_BOX]        = true,
	[FLUX_CONTROL_GRID_VIEW]       = true,
	[FLUX_CONTROL_FLIP_VIEW]       = true,
	[FLUX_CONTROL_PIPS_PAGER]      = true,
};

bool flux_is_interactive(XtkEl const *el) {
	return el->type < FLUX_CONTROL_TYPE_MAX && kInteractive [el->type];
}
