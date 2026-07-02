#include "fluxent/flux_render_snapshot.h"
#include "controls/textbox/tb_internal.h"
#include "fluxent/controls/flux_menu_bar_data.h"
#include "fluxent/controls/flux_nav_view_data.h"
#include "fluxent/controls/flux_tab_view_data.h"

#include <string.h>

typedef struct SnapshotContext {
	FluxRenderSnapshot *snap;
	XentContext const  *ctx;
	XentNodeId          node;
	void const         *data;
} SnapshotContext;

typedef void (*SnapshotHandler)(SnapshotContext const *ctx);

typedef struct SnapshotButtonFields {
	char const     *label;
	char const     *icon_name;
	FluxColor       label_color;
	float           font_size;
	FluxButtonStyle style;
} SnapshotButtonFields;

static void snapshot_base(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd) {
	memset(snap, 0, sizeof(*snap));
	snap->id   = ( uint64_t ) node;
	snap->type = flux_get_control_type(ctx, node);
	if (!nd) return;

	snap->background    = nd->visuals.background;
	snap->border_color  = nd->visuals.border_color;
	snap->corner_radius = nd->visuals.corner_radius;
	snap->border_width  = nd->visuals.border_width;
	snap->opacity       = nd->visuals.opacity;
	snap->hover_local_x = nd->hover_local_x;
	snap->hover_local_y = nd->hover_local_y;
}

static void snapshot_text(FluxRenderSnapshot *snap, FluxTextData const *t) {
	snap->font_size                = t->font_size;
	snap->u.text.text_content        = t->content;
	snap->u.text.font_family         = t->font_family;
	snap->u.text.text_color          = t->text_color;
	snap->u.text.font_weight         = t->font_weight;
	snap->u.text.text_alignment      = t->alignment;
	snap->u.text.text_vert_alignment = t->vertical_alignment;
	snap->u.text.max_lines           = t->max_lines;
	snap->u.text.word_wrap           = t->wrap;
}

static void snapshot_button_fields(FluxRenderSnapshot *snap, SnapshotButtonFields fields) {
	snap->font_size           = fields.font_size;
	snap->u.button.label        = fields.label;
	snap->u.button.icon_name    = fields.icon_name;
	snap->u.button.text_color   = fields.label_color;
	snap->u.button.button_style = fields.style;
}

static void snapshot_button(FluxRenderSnapshot *snap, FluxButtonData const *b) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {b->label, b->icon_name, b->label_color, b->font_size, b->style}
	);
	snap->u.button.is_checked = b->is_checked;
}

static void snapshot_textbox(FluxRenderSnapshot *snap, FluxTextBoxData const *tb) {
	snap->font_size                    = tb->font_size;
	snap->u.textbox.text_content            = tb->content;
	snap->u.textbox.placeholder             = tb->placeholder;
	snap->u.textbox.font_family             = tb->font_family;
	snap->u.textbox.text_color              = tb->text_color;
	snap->u.textbox.edit.cursor_position    = tb->cursor_position;
	snap->u.textbox.edit.selection_start    = tb->selection_start;
	snap->u.textbox.edit.selection_end      = tb->selection_end;
	snap->u.textbox.edit.scroll_offset_x    = tb->scroll_offset_x;
	snap->u.textbox.edit.composition_text   = tb->composition_text;
	snap->u.textbox.edit.composition_length = tb->composition_length;
	snap->u.textbox.edit.composition_cursor = tb->composition_cursor;
	snap->u.textbox.edit.selection_color    = tb->selection_color;
	snap->u.textbox.edit.readonly           = tb->readonly;
}

static void snapshot_checkbox_like(FluxRenderSnapshot *snap, FluxCheckboxData const *c) {
	snap->u.check.label       = c->label;
	snap->u.check.check_state = c->state;
}

static void snapshot_switch(FluxRenderSnapshot *snap, FluxCheckboxData const *c) {
	snap->u.sw.label       = c->label;
	snap->u.sw.check_state = c->state;
}

static void snapshot_slider(FluxRenderSnapshot *snap, FluxSliderData const *s) {
	snap->u.slider.min_value             = s->min_value;
	snap->u.slider.max_value             = s->max_value;
	snap->u.slider.current_value         = s->current_value;
	snap->u.slider.step                  = s->step_frequency;
	snap->u.slider.slider_intermediate   = s->intermediate_value;
	snap->u.slider.slider_tick_frequency = s->tick_frequency;
	snap->u.slider.slider_tick_placement = ( uint8_t ) s->tick_placement;
}

static void snapshot_scroll(FluxRenderSnapshot *snap, FluxScrollData const *sc) {
	snap->u.scroll.x                  = sc->scroll_x;
	snap->u.scroll.y                  = sc->scroll_y;
	snap->u.scroll.content_w          = sc->content_w;
	snap->u.scroll.content_h          = sc->content_h;
	snap->u.scroll.h_vis              = sc->h_vis;
	snap->u.scroll.v_vis              = sc->v_vis;
	snap->u.scroll.mouse_over         = sc->mouse_over;
	snap->u.scroll.mouse_local_x      = sc->mouse_local_x;
	snap->u.scroll.mouse_local_y      = sc->mouse_local_y;
	snap->u.scroll.drag_axis          = sc->drag_axis;
	snap->u.scroll.v_up_pressed       = sc->v_up_pressed;
	snap->u.scroll.v_dn_pressed       = sc->v_dn_pressed;
	snap->u.scroll.h_lf_pressed       = sc->h_lf_pressed;
	snap->u.scroll.h_rg_pressed       = sc->h_rg_pressed;
	snap->u.scroll.last_activity_time = sc->last_activity_time;
}

static void snapshot_progress_fields(FluxRenderSnapshot *snap, float value, float max_value, bool indeterminate) {
	snap->u.progress.min_value     = 0.0f;
	snap->u.progress.current_value = value;
	snap->u.progress.max_value     = max_value;
	snap->u.progress.indeterminate = indeterminate;
}

static void snapshot_progress(FluxRenderSnapshot *snap, FluxProgressData const *p) {
	snapshot_progress_fields(snap, p->value, p->max_value, p->indeterminate);
}

static void snapshot_number_box_spin(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node) {
	snap->u.textbox.nb_spin_placement = ( uint8_t ) (xent_get_semantic_expanded(ctx, node) ? 2 : 0);

	float sv                = 0.0f;
	float smin              = 0.0f;
	float smax              = 0.0f;
	if (!xent_get_semantic_value(ctx, node, &sv, &smin, &smax)) {
		snap->u.textbox.nb_up_enabled   = true;
		snap->u.textbox.nb_down_enabled = true;
		return;
	}
	snap->u.textbox.nb_up_enabled   = (sv < smax) || (smin == smax);
	snap->u.textbox.nb_down_enabled = (sv > smin) || (smin == smax);
}

static void snapshot_hyperlink(FluxRenderSnapshot *snap, FluxHyperlinkData const *hl) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {hl->label, hl->icon_name, hl->label_color, hl->font_size, FLUX_BUTTON_TEXT}
	);
}

static void snapshot_repeat_button(FluxRenderSnapshot *snap, FluxRepeatButtonData const *rb) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {rb->label, rb->icon_name, rb->label_color, rb->font_size, rb->style}
	);
}

static void snapshot_progress_ring(FluxRenderSnapshot *snap, FluxProgressRingData const *pr) {
	snapshot_progress_fields(snap, pr->value, pr->max_value, pr->indeterminate);
}

static void snapshot_info_badge(FluxRenderSnapshot *snap, FluxInfoBadgeData const *ib) {
	snap->background           = ib->background;
	snap->u.info_badge.value     = ib->value;
	snap->u.info_badge.icon_name = ib->icon_name;
	snap->u.info_badge.mode      = ib->mode;
}

static void snapshot_info_bar(FluxRenderSnapshot *snap, FluxInfoBarData const *ib) {
	snap->u.info_bar.label        = ib->title;
	snap->u.info_bar.text_content = ib->message;
	snap->u.info_bar.severity     = ib->severity;
	snap->u.info_bar.is_closable  = ib->is_closable;
}

static void snapshot_handle_info_bar(SnapshotContext const *ctx) {
	snapshot_info_bar(ctx->snap, ( FluxInfoBarData const * ) ctx->data);
}

static void snapshot_handle_expander_header(SnapshotContext const *ctx) {
	ctx->snap->u.expander.is_checked = (( FluxExpanderData const * ) ctx->data)->expanded;
}

static void snapshot_image(FluxRenderSnapshot *snap, FluxImageData const *im) {
	snap->u.image.text_content = im->source;
	snap->u.image.stretch      = im->stretch;
}

static void snapshot_handle_image(SnapshotContext const *ctx) {
	snapshot_image(ctx->snap, ( FluxImageData const * ) ctx->data);
}

static void snapshot_combo_box(FluxRenderSnapshot *snap, FluxComboBoxData const *cb) {
	bool valid              = cb->selected_index >= 0 && cb->selected_index < cb->item_count;
	snap->u.combo.text_content = valid ? cb->items [cb->selected_index] : NULL;
	snap->u.combo.placeholder  = cb->placeholder;
	snap->u.combo.is_checked   = cb->open;
}

static void snapshot_handle_combo_box(SnapshotContext const *ctx) {
	snapshot_combo_box(ctx->snap, ( FluxComboBoxData const * ) ctx->data);
}

static void snapshot_handle_menu_bar_item(SnapshotContext const *ctx) {
	FluxMenuBarItem const *it = ( FluxMenuBarItem const * ) ctx->data;
	ctx->snap->u.menu.label      = it->label;
	ctx->snap->u.menu.is_checked = (it->bar->open_index == it->index);
}

static void snapshot_handle_nav_view(SnapshotContext const *ctx) {
	FluxNavViewData const *d = ( FluxNavViewData const * ) ctx->data;
	/* Root and pane are both NAV_VIEW and share d; only the pane carries the moving
	 * indicator so it slides off-screen in Minimal (the root stays full-width). */
	if (ctx->node != d->pane) return;
	/* The pill is drawn by the pane (outside the items viewport) from layout-space
	 * tween values; shift it by the live scroll offset so it stays glued to the
	 * visually translated item rows. */
	float scroll = 0.0f;
	if (d->ind_in_scroll && d->items_scroll_data)
		scroll = (d->mode == FLUX_NAV_TOP) ? d->items_scroll_data->scroll_x : d->items_scroll_data->scroll_y;
	ctx->snap->u.nav.nav_ind_top        = d->ind_top.current - scroll;
	ctx->snap->u.nav.nav_ind_bottom     = d->ind_bot.current - scroll;
	ctx->snap->u.nav.nav_ind_opacity    = d->ind_op.current;
	/* Drop shadow only while the pane overlays content (Minimal); fades with the slide. */
	ctx->snap->u.nav.nav_shadow_opacity = (d->mode == FLUX_NAV_MINIMAL) ? d->minimal_t.current : 0.0f;
	ctx->snap->u.nav.nav_top            = (d->mode == FLUX_NAV_TOP);
}

static void snapshot_handle_nav_view_item(SnapshotContext const *ctx) {
	FluxNavViewItem const *it        = ( FluxNavViewItem const * ) ctx->data;
	ctx->snap->u.nav.nav_item_kind    = ( uint8_t ) it->kind;
	ctx->snap->u.nav.nav_top          = (it->nav->mode == FLUX_NAV_TOP);
	ctx->snap->u.nav.nav_depth        = ( uint8_t ) it->depth;
	ctx->snap->u.nav.nav_has_children = it->has_children;
	ctx->snap->u.nav.nav_expanded     = it->expanded;
	ctx->snap->u.nav.icon_name        = it->icon_name;
	ctx->snap->u.nav.label            = it->nav->labels_visible ? it->label : NULL;
	ctx->snap->u.nav.is_checked       = (it->kind != FLUX_NAV_ITEM_TOGGLE && it->nav->selected == it->index);
}

/* True when a strip slot is selected, hovered, or pressed — the states that hide
 * the divider on it and on its left-adjacent neighbour (TabViewItem.cpp
 * HideLeftAdjacentTabSeparator). */
static bool snapshot_tab_node_hot(FluxNodeStore *store, XentNodeId node) {
	FluxNodeData const *nd = flux_node_store_get(store, node);
	return nd && (nd->state.hovered || nd->state.pressed);
}

static bool snapshot_tab_slot_active(FluxTabViewData const *tv, int slot) {
	if (tv->selected == slot) return true;
	return snapshot_tab_node_hot(tv->store, tv->tabs [slot].tab_node)
	    || snapshot_tab_node_hot(tv->store, tv->tabs [slot].close_node);
}

/* The divider sits at each tab's right edge; WinUI hides it while the tab itself
 * or the tab to its right is selected/hovered/pressed. */
static bool snapshot_tab_separator(FluxTabViewItem const *it) {
	FluxTabViewData const *tv  = it->tv;
	int                    pos = -1;
	for (int p = 0; p < tv->count; p++)
		if (tv->order [p] == it->index) pos = p;
	if (pos < 0 || snapshot_tab_slot_active(tv, it->index)) return false;
	for (int p = pos + 1; p < tv->count; p++) {
		int slot = tv->order [p];
		if (tv->tabs [slot].closed || tv->tabs [slot].kind != FLUX_TAB_KIND_TAB) continue;
		return !snapshot_tab_slot_active(tv, slot);
	}
	return true; /* last tab: divider separates it from the (+) button */
}

static void snapshot_handle_tab_view_item(SnapshotContext const *ctx) {
	FluxTabViewItem const *it = ( FluxTabViewItem const * ) ctx->data;
	ctx->snap->u.tab.tab_kind  = ( uint8_t ) it->kind;
	ctx->snap->u.tab.icon_name = it->icon_name;
	if (it->kind != FLUX_TAB_KIND_TAB) return;

	FluxTabViewData const *tv       = it->tv;
	bool                   selected = (tv->selected == it->index);
	ctx->snap->u.tab.is_checked     = selected;
	ctx->snap->u.tab.tab_compact    = (tv->width_mode == FLUX_TAB_WIDTH_COMPACT && !selected);
	ctx->snap->u.tab.label          = ctx->snap->u.tab.tab_compact ? NULL : it->label;
	ctx->snap->u.tab.tab_closable   = it->closable;
	ctx->snap->u.tab.tab_separator  = snapshot_tab_separator(it);
}

static void snapshot_handle_text(SnapshotContext const *ctx) {
	snapshot_text(ctx->snap, ( FluxTextData const * ) ctx->data);
}

static void snapshot_handle_button(SnapshotContext const *ctx) {
	snapshot_button(ctx->snap, ( FluxButtonData const * ) ctx->data);
}

static void snapshot_handle_checkbox_like(SnapshotContext const *ctx) {
	snapshot_checkbox_like(ctx->snap, ( FluxCheckboxData const * ) ctx->data);
}

static void snapshot_handle_switch(SnapshotContext const *ctx) {
	snapshot_switch(ctx->snap, ( FluxCheckboxData const * ) ctx->data);
}

static void snapshot_handle_slider(SnapshotContext const *ctx) {
	snapshot_slider(ctx->snap, ( FluxSliderData const * ) ctx->data);
}

static void snapshot_handle_textbox(SnapshotContext const *ctx) {
	tb_sync_content(( FluxTextBoxInputData * ) ctx->data);
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
}

static void snapshot_handle_scroll(SnapshotContext const *ctx) {
	snapshot_scroll(ctx->snap, ( FluxScrollData const * ) ctx->data);
}

static void snapshot_handle_progress(SnapshotContext const *ctx) {
	snapshot_progress(ctx->snap, ( FluxProgressData const * ) ctx->data);
}

static void snapshot_handle_password_box(SnapshotContext const *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx->data;
	tb_sync_content(tb);
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
	ctx->snap->u.textbox.is_checked = tb->password_show_plain || xent_get_semantic_checked(ctx->ctx, ctx->node) != 0;
}

static void snapshot_handle_number_box(SnapshotContext const *ctx) {
	tb_sync_content(( FluxTextBoxInputData * ) ctx->data);
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
	snapshot_number_box_spin(ctx->snap, ctx->ctx, ctx->node);
}

static void snapshot_handle_hyperlink(SnapshotContext const *ctx) {
	snapshot_hyperlink(ctx->snap, ( FluxHyperlinkData const * ) ctx->data);
}

static void snapshot_handle_repeat_button(SnapshotContext const *ctx) {
	snapshot_repeat_button(ctx->snap, ( FluxRepeatButtonData const * ) ctx->data);
}

static void snapshot_handle_progress_ring(SnapshotContext const *ctx) {
	snapshot_progress_ring(ctx->snap, ( FluxProgressRingData const * ) ctx->data);
}

static void snapshot_handle_info_badge(SnapshotContext const *ctx) {
	snapshot_info_badge(ctx->snap, ( FluxInfoBadgeData const * ) ctx->data);
}

static bool snapshot_list_item_selected(FluxListItemData const *it) {
	FluxListViewData const *ld = it->owner;
	if (!ld) return it->selected;
	if (ld->sel_mode == XTK_LIST_SELECT_SINGLE) return it->selected;
	if (ld->sel_mode == XTK_LIST_SELECT_NONE) return false;
	for (int i = 0; i < ld->range_count; i++)
		if (it->index >= ld->ranges [i].first && it->index <= ld->ranges [i].last) return true;
	return false;
}

static void snapshot_handle_list_item(SnapshotContext const *ctx) {
	FluxListItemData const *it        = ( FluxListItemData const * ) ctx->data;
	ctx->snap->u.list_item.kind        = ( uint8_t ) (it->owner ? it->owner->kind : XTK_LIST_KIND_LIST);
	ctx->snap->u.list_item.is_selected = snapshot_list_item_selected(it);
	ctx->snap->u.list_item.multi       = it->multi;
}

static SnapshotHandler const SNAPSHOT_HANDLERS [FLUX_CONTROL_CUSTOM + 1] = {
  [FLUX_CONTROL_TEXT]            = snapshot_handle_text,
  [FLUX_CONTROL_BUTTON]          = snapshot_handle_button,
  [FLUX_CONTROL_TOGGLE_BUTTON]   = snapshot_handle_button,
  [FLUX_CONTROL_DROPDOWN_BUTTON] = snapshot_handle_button,
  [FLUX_CONTROL_SPLIT_BUTTON]    = snapshot_handle_button,
  [FLUX_CONTROL_CHECKBOX]        = snapshot_handle_checkbox_like,
  [FLUX_CONTROL_RADIO]           = snapshot_handle_checkbox_like,
  [FLUX_CONTROL_SWITCH]          = snapshot_handle_switch,
  [FLUX_CONTROL_SLIDER]          = snapshot_handle_slider,
  [FLUX_CONTROL_TEXT_INPUT]      = snapshot_handle_textbox,
  [FLUX_CONTROL_SCROLL]          = snapshot_handle_scroll,
  [FLUX_CONTROL_PROGRESS]        = snapshot_handle_progress,
  [FLUX_CONTROL_PASSWORD_BOX]    = snapshot_handle_password_box,
  [FLUX_CONTROL_NUMBER_BOX]      = snapshot_handle_number_box,
  [FLUX_CONTROL_HYPERLINK]       = snapshot_handle_hyperlink,
  [FLUX_CONTROL_REPEAT_BUTTON]   = snapshot_handle_repeat_button,
  [FLUX_CONTROL_PROGRESS_RING]   = snapshot_handle_progress_ring,
  [FLUX_CONTROL_INFO_BADGE]      = snapshot_handle_info_badge,
  [FLUX_CONTROL_INFO_BAR]        = snapshot_handle_info_bar,
  [FLUX_CONTROL_EXPANDER_HEADER] = snapshot_handle_expander_header,
  [FLUX_CONTROL_IMAGE]           = snapshot_handle_image,
  [FLUX_CONTROL_COMBO_BOX]       = snapshot_handle_combo_box,
  [FLUX_CONTROL_MENU_BAR_ITEM]   = snapshot_handle_menu_bar_item,
  [FLUX_CONTROL_NAV_VIEW]        = snapshot_handle_nav_view,
  [FLUX_CONTROL_NAV_VIEW_ITEM]   = snapshot_handle_nav_view_item,
  [FLUX_CONTROL_TAB_VIEW_ITEM]   = snapshot_handle_tab_view_item,
  [FLUX_CONTROL_LIST_ITEM]       = snapshot_handle_list_item,
};

void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd) {
	snapshot_base(snap, ctx, node, nd);
	if (!nd || !nd->component_data) return;

	SnapshotHandler handler = snap->type <= FLUX_CONTROL_CUSTOM ? SNAPSHOT_HANDLERS [snap->type] : NULL;
	if (!handler) return;

	SnapshotContext build = {snap, ctx, node, nd->component_data};
	handler(&build);
}
