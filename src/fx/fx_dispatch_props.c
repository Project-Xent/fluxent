/**
 * @file fx_dispatch_props.c
 * @brief Table-driven per-type property application, binding wiring,
 * and interactive-type lookup for the FX reconciler.
 *
 * Owns kPropsTable, kInteractive, fx_apply_props, and fx_el_interactive.
 */
#include "fx_internal.h"

#include "controls/factory/flux_factory.h"
#include "render/flux_icon.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FX_CONTROL_TYPE_MAX (FLUX_CONTROL_CUSTOM + 1)

void fx_tramp_click(void *ud) {
	FxBinding *b = ( FxBinding * ) ud;
	fx_runtime_post(b->rt, b->on_click);
}

void fx_tramp_toggle(void *ud, FluxCheckState state) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.b          = state == FLUX_CHECK_CHECKED;
	fx_runtime_post(b->rt, m);
}

void fx_tramp_slider(void *ud, float value) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.f          = value;
	fx_runtime_post(b->rt, m);
}

void fx_tramp_text(void *ud, char const *text) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.ptr        = ( void * ) text;
	fx_runtime_post(b->rt, m);
}

void fx_tramp_select(void *ud, int index) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.i          = index;
	fx_runtime_post(b->rt, m);
}

void fx_tramp_expand(void *ud, bool expanded) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.b          = expanded;
	fx_runtime_post(b->rt, m);
}

void fx_tramp_double(void *ud, double value) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.d          = value;
	fx_runtime_post(b->rt, m);
}

bool fx_tramp_tab_close_req(void *ud, int index) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_close;
	m.i          = index;
	fx_runtime_post(b->rt, m);
	return false;
}

void fx_tramp_dialog_result(void *ud, FluxDialogResult result) {
	FxBinding *b = ( FxBinding * ) ud;
	FxMsg      m = b->on_change;
	m.i          = ( int32_t ) result;
	fx_runtime_post(b->rt, m);
}

FluxWindow       *fx_rt_window(FxRuntime *rt) { return rt->app ? flux_app_get_window(rt->app) : NULL; }
FluxTextRenderer *fx_rt_text(FxRuntime *rt) { return rt->app ? flux_app_get_text_renderer(rt->app) : NULL; }
FluxThemeManager *fx_rt_theme(FxRuntime *rt) { return rt->app ? flux_app_get_theme(rt->app) : NULL; }

static bool fx_feq(float a, float b) { return (isnan(a) && isnan(b)) || a == b; }

static bool fx_streq(char const *a, char const *b) {
	if (a == b) return true;
	if (!a || !b) return false;
	return strcmp(a, b) == 0;
}

static bool fx_insets_eq(XentInsets a, XentInsets b) { return memcmp(&a, &b, sizeof(a)) == 0; }

/* Grow semantics: positive grow implies flex-basis: 0 (fill mode). */
static void fx_apply_grow(XentContext *ctx, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->grow != el->grow) {
		xent_set_flex_grow(ctx, id, el->grow);
		xent_set_flex_basis(ctx, id, el->grow > 0.0f ? 0.0f : NAN);
	}
}

/* Diff width/height/tooltip/grow/align/margin against prev; skip unchanged. */
static void fx_apply_common(FxRuntime *rt, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	XentContext *ctx = rt->ctx;
	if (prev ? !fx_feq(prev->width, el->width) : !isnan(el->width)) xent_set_width(ctx, id, el->width);
	if (prev ? !fx_feq(prev->height, el->height) : !isnan(el->height)) xent_set_height(ctx, id, el->height);
	if (prev ? !fx_streq(prev->tooltip, el->tooltip) : el->tooltip != NULL)
		flux_node_set_tooltip(rt->store, id, el->tooltip);
	fx_apply_grow(ctx, id, prev, el);
	if (!prev || prev->align_self != el->align_self) xent_set_flex_align_self(ctx, id, el->align_self);
	if (!prev || !fx_insets_eq(prev->margin, el->margin)) xent_set_margin(ctx, id, el->margin);
}

/* Rows wrap height, columns wrap width; fill disables the primary axis. */
static void fx_apply_stack_wrap(XentContext *ctx, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->stack.fill != el->stack.fill || prev->stack_row != el->stack_row) {
		bool wrap_w = el->stack_row ? !el->stack.fill : true;
		bool wrap_h = el->stack_row ? true : !el->stack.fill;
		xent_set_wrap_content(ctx, id, wrap_w, wrap_h);
	}
}

/* Set flex protocol, direction, gap, padding, alignment on stack containers. */
static void fx_apply_stack(FxRuntime *rt, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	XentContext *ctx = rt->ctx;
	if (!prev) xent_set_protocol(ctx, id, XENT_PROTOCOL_FLEX);
	if (!prev || prev->stack_row != el->stack_row)
		xent_set_flex_direction(ctx, id, el->stack_row ? XENT_FLEX_ROW : XENT_FLEX_COLUMN);
	if (!prev || prev->stack.gap != el->stack.gap) xent_set_gap(ctx, id, el->stack.gap);
	if (!prev || !fx_insets_eq(prev->stack.padding, el->stack.padding)) xent_set_padding(ctx, id, el->stack.padding);
	XentFlexAlign align = el->stack.align ? el->stack.align : XENT_FLEX_ALIGN_STRETCH;
	if (!prev || prev->stack.align != el->stack.align) xent_set_flex_align_items(ctx, id, align);
	if (!prev || prev->stack.justify != el->stack.justify) xent_set_flex_justify_content(ctx, id, el->stack.justify);
	fx_apply_stack_wrap(ctx, id, prev, el);
}

/* WinUI badge sizing: dot=8, icon/single-digit=16, two-digit=24, 100+=30. */
static XentSize fx_badge_size(FxBadgeDesc const *b) {
	if (b->mode == FLUX_BADGE_DOT) return (XentSize) {8.0f, 8.0f};
	if (b->mode == FLUX_BADGE_ICON || b->value < 10) return (XentSize) {16.0f, 16.0f};
	return b->value < 100 ? (XentSize) {24.0f, 16.0f} : (XentSize) {30.0f, 16.0f};
}

/* Deep-compare combo item arrays to avoid unnecessary flux_combo_box_set_items. */
static bool fx_combo_items_eq(FxComboDesc const *a, FxComboDesc const *b) {
	if (a->count != b->count) return false;
	for (int i = 0; i < a->count; i++)
		if (!fx_streq(a->items [i], b->items [i])) return false;
	return true;
}

/* Deep-compare menu items (label + icon + disabled) to skip menu rebuilds. */
static bool fx_menu_items_eq(FxMenuItemDesc const *a, int ac, FxMenuItemDesc const *b, int bc) {
	if (ac != bc) return false;
	for (int i = 0; i < ac; i++) {
		if (!fx_streq(a [i].label, b [i].label)) return false;
		if (!fx_streq(a [i].icon, b [i].icon)) return false;
		if (a [i].disabled != b [i].disabled) return false;
	}
	return true;
}

void fx_fill_menu(FxRuntime *rt, FluxMenuFlyout *menu, FxMenuItemDesc const *items, int count, FxBinding *slots) {
	for (int i = 0; i < count; i++) {
		FxMenuItemDesc const *it = &items [i];
		slots [i]                = (FxBinding) {.rt = rt, .on_click = it->on_click};
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
				  .on_click     = fx_tramp_click,
				  .on_click_ctx = &slots [i],
				}
		);
	}
}

/* Create or rebuild the owned flyout menu attached to a dropdown/split button. */
static void fx_sync_owned_menu(FxRuntime *rt, FxNode *n, FxMenuItemDesc const *items, int count, bool split) {
	FluxWindow *window = fx_rt_window(rt);
	if (!window) return;

	if (!n->menu) {
		n->menu = flux_menu_flyout_create(window);
		if (!n->menu) return;
		flux_menu_flyout_set_theme_manager(n->menu, fx_rt_theme(rt));
		flux_menu_flyout_set_text_renderer(n->menu, fx_rt_text(rt));
		FluxContextFlyoutBindingInfo bind = {rt->store, n->node, n->menu, rt->ctx, window};
		if (split) flux_split_button_set_menu_flyout_ex(&bind);
		else flux_node_set_menu_flyout_ex(&bind);
	}

	if (count != n->menu_binding_count) {
		FxBinding *bindings
		  = ( FxBinding * ) realloc(n->menu_bindings, sizeof(FxBinding) * ( size_t ) (count ? count : 1));
		if (!bindings) return;
		n->menu_bindings      = bindings;
		n->menu_binding_count = count;
	}

	flux_menu_flyout_clear(n->menu);
	fx_fill_menu(rt, n->menu, items, count, n->menu_bindings);
}

/* Reparent children into the newly-selected tab's content host. */
static void fx_tab_view_home_children(FxRuntime *rt, FxNode *n, int selected) {
	if (!n->tab_hosts || selected < 0 || selected >= n->tab_host_count) return;
	XentNodeId host = n->tab_hosts [selected];
	if (host == XENT_NODE_INVALID || host == n->host) return;
	n->host = host;
	for (int i = 0; i < n->child_count; i++)
		if (n->children [i]) xent_append_child(rt->ctx, host, n->children [i]->node);
}

/* Resolve the on_click message for el's type (hyperlink, info_bar, etc.). */
static FxMsg fx_binding_click(FluxEl const *el) {
	switch (el->type) {
	case FLUX_CONTROL_HYPERLINK    : return el->hyperlink.on_click;
	case FLUX_CONTROL_INFO_BAR     : return el->info_bar.on_close;
	case FLUX_CONTROL_SPLIT_BUTTON : return el->split.on_click;
	case FLUX_CONTROL_TAB_VIEW     : return el->tab_view.on_add;
	default                        : return el->button.on_click;
	}
}

/* Controls whose on_change comes from a non-toggle union member. */
static bool fx_binding_change_special(FluxEl const *el, FxMsg *out) {
	switch (el->type) {
	case FLUX_CONTROL_SLIDER       : *out = el->slider.on_change; return true;
	case FLUX_CONTROL_TEXT_INPUT    :
	case FLUX_CONTROL_PASSWORD_BOX  : *out = el->textbox.on_change; return true;
	case FLUX_CONTROL_COMBO_BOX    : *out = el->combo.on_select; return true;
	case FLUX_CONTROL_NAV_VIEW     : *out = el->nav.on_select; return true;
	default                        : return false;
	}
}

static FxMsg fx_binding_change(FluxEl const *el) {
	FxMsg m;
	if (fx_binding_change_special(el, &m)) return m;
	switch (el->type) {
	case FLUX_CONTROL_TAB_VIEW       : return el->tab_view.on_select;
	case FLUX_CONTROL_CONTENT_DIALOG : return el->dialog.on_result;
	case FLUX_CONTROL_EXPANDER       : return el->expander.on_toggle;
	case FLUX_CONTROL_NUMBER_BOX     : return el->number.on_change;
	default                          : return el->toggle.on_change;
	}
}

/* Rewire the stable binding slots with this frame's message values. */
static void fx_apply_bindings(FxNode *n, FluxEl const *el) {
	if (!n->binding) return;
	n->binding->on_click  = fx_binding_click(el);
	n->binding->on_change = fx_binding_change(el);
	n->binding->on_close  = el->type == FLUX_CONTROL_TAB_VIEW ? el->tab_view.on_close : ( FxMsg ) {0};
}

/* Signature for per-type prop handlers; NULL in kPropsTable means no-op. */
typedef void (*FxPropsFn)(FxRuntime *, FxNode *, FluxEl const *, FluxEl const *);

static void fx_pp_stack(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	fx_apply_stack(rt, n->node, prev, el);
}

static float fx_font_size(float size) { return size > 0.0f ? size : 14.0f; }

static void fx_pp_text_style(FluxNodeStore *store, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->text_props.size != el->text_props.size)
		flux_text_set_font_size(store, id, fx_font_size(el->text_props.size));
	if (!prev || prev->text_props.weight != el->text_props.weight)
		flux_text_set_weight(store, id, el->text_props.weight);
	if (!prev || prev->text_props.align != el->text_props.align)
		flux_text_set_alignment(store, id, el->text_props.align);
	if (!prev || prev->text_props.color.rgba != el->text_props.color.rgba)
		flux_text_set_color(store, id, el->text_props.color);
}

static void fx_pp_text(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && !fx_streq(prev->text, el->text)) flux_text_set_content(rt->store, n->node, el->text);
	fx_pp_text_style(rt->store, n->node, prev, el);
}

static void fx_pp_button(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev ? !fx_streq(prev->button.icon, el->button.icon) : el->button.icon != NULL)
		flux_button_set_icon(store, id, el->button.icon);
	if (!prev || prev->button.style != el->button.style) flux_button_set_style(store, id, el->button.style);
	if (!prev || prev->button.disabled != el->button.disabled)
		flux_button_set_enabled(store, id, !el->button.disabled);
}

static void fx_pp_toggle(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_checkbox_set_label(store, id, el->text);
	if (prev && prev->toggle.checked != el->toggle.checked)
		flux_checkbox_set_state(store, id, el->toggle.checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
	if (!prev || prev->toggle.disabled != el->toggle.disabled)
		flux_checkbox_set_enabled(store, id, !el->toggle.disabled);
}

static void fx_pp_slider_range(FluxNodeStore *store, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->slider.min != el->slider.min || prev->slider.max != el->slider.max)
		flux_slider_set_range(store, id, el->slider.min, el->slider.max);
}

static void fx_pp_slider_ticks(FluxNodeStore *store, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->slider.tick != el->slider.tick) flux_slider_set_tick_frequency(store, id, el->slider.tick);
	if (!prev || prev->slider.tick_placement != el->slider.tick_placement)
		flux_slider_set_tick_placement(store, id, el->slider.tick_placement);
	if (!prev || prev->slider.snap_to_ticks != el->slider.snap_to_ticks)
		flux_slider_set_snaps_to(
		  store, id, el->slider.snap_to_ticks ? FLUX_SLIDER_SNAPS_TO_TICKS : FLUX_SLIDER_SNAPS_TO_STEP_VALUES
		);
}

static void fx_pp_slider_changes(FluxNodeStore *store, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->slider.small_change != el->slider.small_change)
		flux_slider_set_small_change(store, id, el->slider.small_change);
	if (!prev || prev->slider.large_change != el->slider.large_change)
		flux_slider_set_large_change(store, id, el->slider.large_change);
}

static void fx_pp_slider(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	fx_pp_slider_range(store, id, prev, el);
	if (prev && prev->slider.value != el->slider.value) flux_slider_set_value(store, id, el->slider.value);
	if (!prev || prev->slider.step != el->slider.step) flux_slider_set_step(store, id, el->slider.step);
	if (!prev || prev->slider.disabled != el->slider.disabled)
		flux_slider_set_enabled(store, id, !el->slider.disabled);
	fx_pp_slider_ticks(store, id, prev, el);
	fx_pp_slider_changes(store, id, prev, el);
}

/* Sync textbox content only when it differs from the live control state. */
static void fx_sync_textbox_content(FluxNodeStore *store, XentNodeId id, FluxEl const *el, bool password) {
	if (!el->textbox.content) return;
	FluxNodeData *nd    = flux_node_store_get(store, id);
	char const *current = nd && nd->component_data ? (( FluxTextBoxData * ) nd->component_data)->content : NULL;
	if (fx_streq(current, el->textbox.content)) return;
	if (password) flux_password_set_content(store, id, el->textbox.content);
	else flux_textbox_set_content(store, id, el->textbox.content);
}

static void fx_pp_textbox(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store    = rt->store;
	XentNodeId     id       = n->node;
	bool           password = el->type == FLUX_CONTROL_PASSWORD_BOX;
	if (prev && !fx_streq(prev->text, el->text)) {
		if (password) flux_password_set_placeholder(store, id, el->text);
		else flux_textbox_set_placeholder(store, id, el->text);
	}
	fx_sync_textbox_content(store, id, el, password);
	if (!prev || prev->textbox.disabled != el->textbox.disabled) {
		if (password) flux_password_set_enabled(store, id, !el->textbox.disabled);
		else flux_textbox_set_enabled(store, id, !el->textbox.disabled);
	}
}

static void fx_pp_progress(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->progress.value != el->progress.value) flux_progress_set_value(store, id, el->progress.value);
	if (!prev || prev->progress.max != el->progress.max) flux_progress_set_max(store, id, el->progress.max);
	if (!prev || prev->progress.indeterminate != el->progress.indeterminate)
		flux_progress_set_indeterminate(store, id, el->progress.indeterminate);
}

static void fx_pp_progress_ring(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->progress.value != el->progress.value) flux_progress_ring_set_value(store, id, el->progress.value);
	if (!prev || prev->progress.max != el->progress.max) flux_progress_ring_set_max(store, id, el->progress.max);
	if (!prev || prev->progress.indeterminate != el->progress.indeterminate)
		flux_progress_ring_set_indeterminate(store, id, el->progress.indeterminate);
}

static bool fx_badge_resized(FluxEl const *prev, FluxEl const *el) {
	return prev && (prev->badge.mode != el->badge.mode || prev->badge.value != el->badge.value) && isnan(el->width);
}

static void fx_pp_badge(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && prev->badge.mode != el->badge.mode) flux_info_badge_set_mode(store, id, el->badge.mode);
	if (prev && prev->badge.value != el->badge.value) flux_info_badge_set_value(store, id, el->badge.value);
	if (prev && !fx_streq(prev->badge.icon, el->badge.icon)) flux_info_badge_set_icon(store, id, el->badge.icon);
	if (fx_badge_resized(prev, el)) xent_set_size(rt->ctx, id, fx_badge_size(&el->badge));
}

static void fx_pp_image(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && !fx_streq(prev->text, el->text)) flux_image_set_source(rt->store, n->node, el->text);
	if (!prev || prev->image.stretch != el->image.stretch) flux_image_set_stretch(rt->store, n->node, el->image.stretch);
}

static void fx_pp_hyperlink(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_hyperlink_set_label(store, id, el->text);
	if (prev && !fx_streq(prev->hyperlink.url, el->hyperlink.url))
		flux_hyperlink_set_url(store, id, el->hyperlink.url);
	if (!prev || prev->hyperlink.disabled != el->hyperlink.disabled)
		flux_hyperlink_set_enabled(store, id, !el->hyperlink.disabled);
}

static void fx_pp_repeat_button(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_repeat_button_set_label(store, id, el->text);
	if (prev ? !fx_streq(prev->button.icon, el->button.icon) : el->button.icon != NULL)
		flux_repeat_button_set_icon(store, id, el->button.icon);
	if (!prev || prev->button.style != el->button.style) flux_repeat_button_set_style(store, id, el->button.style);
	if (!prev || prev->button.disabled != el->button.disabled)
		flux_repeat_button_set_enabled(store, id, !el->button.disabled);
}

static void fx_pp_combo(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && !fx_combo_items_eq(&prev->combo, &el->combo))
		flux_combo_box_set_items(rt->store, n->node, el->combo.items, el->combo.count);
	if (prev && prev->combo.selected != el->combo.selected)
		flux_combo_box_set_selected(rt->store, n->node, el->combo.selected);
}

static void fx_pp_expander(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && prev->expander.expanded != el->expander.expanded)
		flux_expander_set_expanded(rt->store, n->node, rt->ctx, el->expander.expanded);
}

static void fx_pp_infobar(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_info_bar_set_title(store, id, el->text);
	if (prev && !fx_streq(prev->info_bar.message, el->info_bar.message))
		flux_info_bar_set_message(store, id, el->info_bar.message);
	if (prev && prev->info_bar.severity != el->info_bar.severity)
		flux_info_bar_set_severity(store, id, el->info_bar.severity);
}

static void fx_pp_number_range(FluxNodeStore *store, XentNodeId id, FluxEl const *prev, FluxEl const *el) {
	if (!prev || prev->number.min != el->number.min || prev->number.max != el->number.max)
		flux_numberbox_set_range(store, id, el->number.min, el->number.max);
	if (!prev || prev->number.step != el->number.step) flux_numberbox_set_step(store, id, el->number.step);
	if (!prev || prev->number.value != el->number.value) flux_numberbox_set_value(store, id, el->number.value);
}

static void fx_pp_number(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	fx_pp_number_range(rt->store, n->node, prev, el);
	if (!prev || prev->number.hide_spin != el->number.hide_spin)
		flux_numberbox_set_spin_enabled(rt->store, n->node, !el->number.hide_spin);
	if (!prev || prev->number.disabled != el->number.disabled)
		flux_numberbox_set_enabled(rt->store, n->node, !el->number.disabled);
}

static void fx_pp_nav(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && prev->nav.selected != el->nav.selected) flux_nav_view_select(rt->store, n->node, el->nav.selected);
}

/* Rebuild dropdown menu if items changed; else just rewire on_click msgs. */
static void fx_sync_dropdown_menu(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (!prev
		|| !fx_menu_items_eq(prev->dropdown.items, prev->dropdown.item_count, el->dropdown.items, el->dropdown.item_count))
		fx_sync_owned_menu(rt, n, el->dropdown.items, el->dropdown.item_count, false);
	else
		for (int i = 0; i < n->menu_binding_count && i < el->dropdown.item_count; i++)
			n->menu_bindings [i].on_click = el->dropdown.items [i].on_click;
}

static void fx_pp_dropdown(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev && !fx_streq(prev->dropdown.icon, el->dropdown.icon))
		flux_button_set_icon(store, id, el->dropdown.icon);
	if (!prev || prev->dropdown.style != el->dropdown.style) flux_button_set_style(store, id, el->dropdown.style);
	if (!prev || prev->dropdown.disabled != el->dropdown.disabled)
		flux_button_set_enabled(store, id, !el->dropdown.disabled);
	fx_sync_dropdown_menu(rt, n, prev, el);
}

/* Rebuild split-button menu if items changed; else just rewire on_click. */
static void fx_sync_split_menu(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (!prev
		|| !fx_menu_items_eq(prev->split.items, prev->split.item_count, el->split.items, el->split.item_count))
		fx_sync_owned_menu(rt, n, el->split.items, el->split.item_count, true);
	else
		for (int i = 0; i < n->menu_binding_count && i < el->split.item_count; i++)
			n->menu_bindings [i].on_click = el->split.items [i].on_click;
}

static void fx_pp_split(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	FluxNodeStore *store = rt->store;
	XentNodeId     id    = n->node;
	if (prev && !fx_streq(prev->text, el->text)) flux_button_set_label(store, id, el->text);
	if (prev ? !fx_streq(prev->split.icon, el->split.icon) : el->split.icon != NULL)
		flux_button_set_icon(store, id, el->split.icon);
	if (!prev || prev->split.disabled != el->split.disabled)
		flux_button_set_enabled(store, id, !el->split.disabled);
	fx_sync_split_menu(rt, n, prev, el);
}

static void fx_pp_tab(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && prev->tab_view.selected != el->tab_view.selected) {
		flux_tab_view_select(rt->store, n->node, el->tab_view.selected);
		fx_tab_view_home_children(rt, n, el->tab_view.selected);
	}
}

static void fx_pp_dialog(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	if (prev && prev->dialog.open != el->dialog.open) {
		if (el->dialog.open) flux_content_dialog_show(rt->store, n->node);
		else flux_content_dialog_hide(rt->store, n->node);
	}
}

/* Dispatch table: FluxControlType → per-type prop update handler. */
static FxPropsFn const kPropsTable [FX_CONTROL_TYPE_MAX] = {
	[FLUX_CONTROL_CONTAINER]       = fx_pp_stack,
	[FLUX_CONTROL_CARD]            = fx_pp_stack,
	[FLUX_CONTROL_SCROLL]          = fx_pp_stack,
	[FLUX_CONTROL_TEXT]            = fx_pp_text,
	[FLUX_CONTROL_BUTTON]          = fx_pp_button,
	[FLUX_CONTROL_CHECKBOX]        = fx_pp_toggle,
	[FLUX_CONTROL_RADIO]           = fx_pp_toggle,
	[FLUX_CONTROL_SWITCH]          = fx_pp_toggle,
	[FLUX_CONTROL_SLIDER]          = fx_pp_slider,
	[FLUX_CONTROL_TEXT_INPUT]      = fx_pp_textbox,
	[FLUX_CONTROL_PASSWORD_BOX]    = fx_pp_textbox,
	[FLUX_CONTROL_PROGRESS]        = fx_pp_progress,
	[FLUX_CONTROL_PROGRESS_RING]   = fx_pp_progress_ring,
	[FLUX_CONTROL_INFO_BADGE]      = fx_pp_badge,
	[FLUX_CONTROL_IMAGE]           = fx_pp_image,
	[FLUX_CONTROL_HYPERLINK]       = fx_pp_hyperlink,
	[FLUX_CONTROL_REPEAT_BUTTON]   = fx_pp_repeat_button,
	[FLUX_CONTROL_COMBO_BOX]       = fx_pp_combo,
	[FLUX_CONTROL_EXPANDER]        = fx_pp_expander,
	[FLUX_CONTROL_INFO_BAR]        = fx_pp_infobar,
	[FLUX_CONTROL_NUMBER_BOX]      = fx_pp_number,
	[FLUX_CONTROL_NAV_VIEW]        = fx_pp_nav,
	[FLUX_CONTROL_DROPDOWN_BUTTON] = fx_pp_dropdown,
	[FLUX_CONTROL_SPLIT_BUTTON]    = fx_pp_split,
	[FLUX_CONTROL_TAB_VIEW]        = fx_pp_tab,
	[FLUX_CONTROL_CONTENT_DIALOG]  = fx_pp_dialog,
};

void fx_apply_props(FxRuntime *rt, FxNode *n, FluxEl const *prev, FluxEl const *el) {
	fx_apply_bindings(n, el);
	if (el->type < FX_CONTROL_TYPE_MAX) {
		FxPropsFn fn = kPropsTable [el->type];
		if (fn) fn(rt, n, prev, el);
	}
	fx_apply_common(rt, n->node, prev, el);
}

/* Controls that need an FxBinding (event wiring) during mount. */
static const bool kInteractive [FX_CONTROL_TYPE_MAX] = {
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
};

bool fx_el_interactive(FluxEl const *el) {
	return el->type < FX_CONTROL_TYPE_MAX && kInteractive [el->type];
}
