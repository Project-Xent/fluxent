#include "controls/factory/flux_init_nav.h"
#include "render/flux_render_snapshot.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"
#include "runtime/flux_anim_driver.h"
#include "controls/textbox/tb_internal.h"

#include <windows.h>
#include "fluxent/flux_window.h"
#include "window/flux_window_internal.h"
#include "controls/data/flux_menu_bar_data.h"
#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_tab_view_data.h"
#include "controls/behavior/flux_breadcrumb_internal.h"
#include "controls/behavior/flux_expander_internal.h"
#include "controls/behavior/flux_flip_internal.h"
#include "controls/behavior/flux_refresh_internal.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/behavior/flux_nav_internal.h"
#include "render/flux_looping_geom.h"
#include "controls/behavior/flux_picker_internal.h"
#include "render/flux_render_internal.h"
#include "motion/flux_motion_internal.h"

#include <string.h>

typedef struct SnapshotButtonFields {
	char const     *label;
	char const     *icon_name;
	FluxColor       label_color;
	float           font_size;
	FluxButtonStyle style;
} SnapshotButtonFields;

static void snapshot_base(FluxRenderSnap *snap, XentCtx const *ctx, XentNodeId node, FluxNodeData const *nd) {
	memset(snap, 0, sizeof(*snap));
	snap->id   = ( uint64_t ) node;
	snap->type = nd ? nd->component_type : FLUX_CONTROL_CONTAINER;
	snap->rtl  = xent_resolved_dir(ctx, node) == XENT_DIRECTION_RTL;
	if (!nd) return;

	snap->background     = nd->visuals.background;
	snap->border_color   = nd->visuals.border_color;
	snap->corner_radius  = nd->visuals.corner_radius;
	snap->border_width   = nd->visuals.border_width;
	snap->opacity        = nd->visuals.opacity;
	snap->dialog_surface = nd->visuals.dialog_surface;
	snap->hover_local_x  = nd->hover_local_x;
	snap->hover_local_y  = nd->hover_local_y;
}

static void snapshot_text(FluxRenderSnap *snap, FluxTextData const *t) {
	snap->font_size                  = t->font_size;
	snap->u.text.text_content        = t->content;
	snap->u.text.font_family         = t->font_family;
	snap->u.text.text_color          = t->text_color;
	snap->u.text.font_weight         = t->font_weight;
	snap->u.text.text_alignment      = t->alignment;
	snap->u.text.text_vert_alignment = t->vertical_alignment;
	snap->u.text.max_lines           = t->max_lines;
	snap->u.text.trim                = t->trim;
	snap->u.text.word_wrap           = t->wrap;
	snap->u.text.selectable          = t->selectable;
	snap->u.text.selection_start     = t->selection_start;
	snap->u.text.selection_end       = t->selection_end;
}

static void snapshot_button_fields(FluxRenderSnap *snap, SnapshotButtonFields fields) {
	snap->font_size             = fields.font_size;
	snap->u.button.label        = fields.label;
	snap->u.button.icon_name    = fields.icon_name;
	snap->u.button.text_color   = fields.label_color;
	snap->u.button.button_style = fields.style;
}

static void snapshot_button(FluxRenderSnap *snap, FluxButtonData const *b) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {b->label, b->icon_name, b->label_color, b->font_size, b->style}
	);
	snap->u.button.picker_flyout_button = b->picker_flyout_button;
	snap->u.button.teaching_tip_close   = b->teaching_tip_close;
	snap->u.button.suggest_query        = b->suggest_query;
	snap->u.button.navigation_back      = b->navigation_back;
	snap->u.button.is_checked           = b->is_checked;
}

static void snapshot_textbox(FluxRenderSnap *snap, FluxTextBoxData const *tb) {
	snap->font_size                         = tb->font_size;
	snap->u.textbox.text_content            = tb->content;
	snap->u.textbox.placeholder             = tb->placeholder;
	snap->u.textbox.font_family             = tb->font_family;
	snap->u.textbox.text_color              = tb->text_color;
	snap->u.textbox.edit.cursor_position    = tb->cursor_position;
	snap->u.textbox.edit.selection_start    = tb->selection_start;
	snap->u.textbox.edit.selection_end      = tb->selection_end;
	snap->u.textbox.edit.scroll_offset_x    = tb->scroll_offset_x;
	snap->u.textbox.edit.scroll_offset_y    = tb->scroll_offset_y;
	snap->u.textbox.edit.composition_text   = tb->composition_text;
	snap->u.textbox.edit.composition_length = tb->composition_length;
	snap->u.textbox.edit.composition_cursor = tb->composition_cursor;
	snap->u.textbox.edit.selection_color    = tb->selection_color;
	snap->u.textbox.edit.readonly           = tb->readonly;
	snap->u.textbox.edit.multiline          = tb->multiline;
	snap->u.textbox.combo_editor            = tb->combo_editor;
	snap->u.textbox.suggest_editor          = tb->suggest_editor;
	snap->u.textbox.query_reserve           = tb->query_reserve;
}

static void snapshot_checkbox_like(FluxRenderSnap *snap, FluxCheckboxData const *c) {
	snap->u.check.label       = c->label;
	snap->u.check.check_state = c->state;
}

static void snapshot_switch(FluxRenderSnap *snap, FluxCheckboxData const *c) {
	snap->u.sw.label       = c->label;
	snap->u.sw.check_state = c->state;
}

static void snapshot_slider(FluxRenderSnap *snap, FluxSliderData const *s) {
	snap->u.slider.min_value             = s->min_value;
	snap->u.slider.max_value             = s->max_value;
	snap->u.slider.current_value         = s->current_value;
	snap->u.slider.step                  = s->step_frequency;
	snap->u.slider.slider_intermediate   = s->intermediate_value;
	snap->u.slider.slider_tick_frequency = s->tick_frequency;
	snap->u.slider.slider_tick_placement = ( uint8_t ) s->tick_placement;
	snap->u.slider.vertical              = s->vertical;
	snap->u.slider.is_range              = s->is_range;
	snap->u.slider.color_picker          = s->color_picker;
	snap->u.slider.thumb_held            = s->thumb_held;
	snap->u.slider.active_thumb          = s->active_thumb;
	for (int i = 0; i < 2; i++) {
		snap->u.slider.range_value [i] = s->range_value [i];
		snap->u.slider.range_live [i]  = s->range_live [i];
	}
}

static void snapshot_scroll(FluxRenderSnap *snap, FluxScrollData const *sc) {
	snap->u.scroll.x                  = sc->scroll_x;
	snap->u.scroll.y                  = sc->scroll_y;
	snap->u.scroll.origin_x           = sc->origin_x;
	snap->u.scroll.origin_y           = sc->origin_y;
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

static void
snapshot_progress_fields(FluxRenderSnap *snap, float value, float min_value, float max_value, bool indeterminate) {
	snap->u.progress.min_value     = min_value;
	snap->u.progress.current_value = value;
	snap->u.progress.max_value     = max_value;
	snap->u.progress.indeterminate = indeterminate;
}

static void snapshot_progress(FluxRenderSnap *snap, FluxProgressData const *p) {
	snapshot_progress_fields(snap, p->value, p->min_value, p->max_value, p->indeterminate);
	snap->u.progress.state   = p->state;
	snap->u.progress.padding = p->padding;
}

static void snapshot_hyperlink(FluxRenderSnap *snap, FluxHyperlinkData const *hl) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {hl->label, hl->icon_name, hl->label_color, hl->font_size, FLUX_BUTTON_TEXT}
	);
}

static void snapshot_repeat_button(FluxRenderSnap *snap, FluxRepeatButtonData const *rb) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {rb->label, rb->icon_name, rb->label_color, rb->font_size, rb->style}
	);
}

static void snapshot_progress_ring(FluxRenderSnap *snap, FluxProgressRingData const *pr) {
	snapshot_progress_fields(snap, pr->value, pr->min_value, pr->max_value, pr->indeterminate);
	snap->u.progress.inactive = pr->inactive;
}

static void snapshot_info_badge(FluxRenderSnap *snap, FluxInfoBadgeData const *ib) {
	snap->background             = ib->background;
	snap->u.info_badge.value     = ib->value;
	snap->u.info_badge.icon_name = ib->icon_name;
	snap->u.info_badge.mode      = ib->mode;
}

static void snapshot_info_bar(FluxRenderSnap *snap, FluxInfoBarData const *ib) {
	snap->u.info_bar.label        = ib->title;
	snap->u.info_bar.text_content = ib->message;
	snap->u.info_bar.severity     = ib->severity;
	snap->u.info_bar.is_closable  = ib->is_closable;
}

void flux_snap_info_bar(FluxSnapCtx const *ctx) { snapshot_info_bar(ctx->snap, ( FluxInfoBarData const * ) ctx->data); }

void flux_snap_expander_header(FluxSnapCtx const *ctx) {
	ctx->snap->u.expander.is_checked = (( FluxExpanderData const * ) ctx->data)->expanded;
}

static void snapshot_image(FluxRenderSnap *snap, FluxImageData const *im) {
	snap->u.image.text_content = im->source;
	snap->u.image.stretch      = im->stretch;
	snap->u.image.frame        = im->frame;
	snap->u.image.nine         = im->nine;
}

void        flux_snap_image(FluxSnapCtx const *ctx) { snapshot_image(ctx->snap, ( FluxImageData const * ) ctx->data); }

static void snapshot_combo_box(FluxRenderSnap *snap, FluxComboBoxData const *cb) {
	bool valid                 = cb->selected_index >= 0 && cb->selected_index < cb->item_count;
	snap->u.combo.text_content = valid ? cb->items [cb->selected_index] : NULL;
	if (cb->editable) snap->u.combo.text_content = cb->edit_text ? *cb->edit_text : NULL;
	snap->u.combo.placeholder = cb->placeholder;
	snap->u.combo.is_checked  = cb->open;
	snap->u.combo.editable    = cb->editable;
	snap->u.combo.edit_active = cb->editable && cb->edit_active;
}

void flux_snap_combo_box(FluxSnapCtx const *ctx) {
	snapshot_combo_box(ctx->snap, ( FluxComboBoxData const * ) ctx->data);
}

void flux_snap_menu_bar_item(FluxSnapCtx const *ctx) {
	FluxMenuItemVisual const *item = ( FluxMenuItemVisual const * ) ctx->data;
	ctx->snap->u.menu.label        = item->label;
	ctx->snap->u.menu.is_checked   = item->open;
}

static void snap_nav_content(FluxNavViewData const *d, FluxNavSnapshot *snap) {
	XentRect root = {0}, content = {0};
	if (!xent_layout_rect(d->ctx, d->root, &root) || !xent_layout_rect(d->ctx, d->content_grid, &content)) return;
	float left          = content.x - root.x;
	bool  rtl           = xent_resolved_dir(d->ctx, d->root) == XENT_DIRECTION_RTL;
	snap->nav_content_x = rtl ? root.w - left - content.w : left;
	snap->nav_content_y = content.y - root.y;
}

void flux_snap_nav_view(FluxSnapCtx const *ctx) {
	FluxNavViewData const *d = ( FluxNavViewData const * ) ctx->data;
	ctx->snap->u.nav.nav_top = d->mode == FLUX_NAV_TOP;
	if (ctx->node != d->pane_clip) {
		snap_nav_content(d, &ctx->snap->u.nav);
		return;
	}
	ctx->snap->u.nav.nav_is_pane        = true;
	bool overlay                        = flux_nav_pane_overlay(d);
	ctx->snap->u.nav.nav_pane_solid     = overlay && d->pane_progress.current > 0.0f;
	ctx->snap->u.nav.nav_shadow_opacity = overlay ? d->pane_progress.current : 0.0f;
}

void flux_snap_nav_view_item(FluxSnapCtx const *ctx) {
	FluxNavViewItem const *it       = ( FluxNavViewItem const * ) ctx->data;
	FluxNavViewData const *d        = it->nav;
	bool                   item     = it->index >= 0;
	FluxNavSnapshot       *s        = &ctx->snap->u.nav;
	s->nav_item_kind                = ( uint8_t ) it->kind;
	s->nav_place                    = it->place;
	s->nav_top                      = it->place == FLUX_NAV_PLACE_TOP;
	s->nav_depth                    = item ? flux_nav_draw_depth(d, it) : 0;
	s->nav_has_children             = it->has_children;
	s->nav_expanded                 = it->expanded;
	s->icon_name                    = it->icon_name;
	s->label                        = flux_nav_shows_label(d, it) ? it->label : NULL;
	s->is_checked                   = (item || it->kind == FLUX_NAV_ITEM_SETTINGS) && d->selected == it->index;
	FluxNavIndicatorFrame indicator = flux_nav_indicator_frame(it, ctx->snap->rtl);
	s->nav_ind_scale                = indicator.scale;
	s->nav_ind_offset               = indicator.offset;
	s->nav_ind_opacity              = indicator.opacity;
	s->nav_header_opacity           = it->place == FLUX_NAV_PLACE_LEFT ? d->header_opacity.current : 1.0f;
	s->nav_ind_horizontal           = indicator.horizontal;
	s->nav_in_flyout                = indicator.in_flyout;
}

static bool snapshot_tab_node_hot(FluxStore *store, XentNodeId node) {
	FluxNodeData const *nd = flux_store_find(store, node);
	return nd && (nd->state.hovered || nd->state.pressed);
}

static bool snapshot_tab_slot_active(FluxTabViewData const *tv, int slot) {
	if (tv->selected == slot) return true;
	return snapshot_tab_node_hot(tv->store, tv->tabs [slot].tab_node)
	    || snapshot_tab_node_hot(tv->store, tv->tabs [slot].close_node);
}

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
	return true;
}

static void snapshot_tab_close(FluxSnapCtx const *ctx, FluxTabViewItem const *it) {
	FluxTabViewData const *tv           = it->tv;
	XentNodeId             owner        = tv->tabs [it->index].tab_node;
	ctx->snap->u.tab.is_checked         = tv->selected == it->index;
	ctx->snap->u.tab.tab_owner_hot      = snapshot_tab_node_hot(tv->store, owner);
	ctx->snap->u.tab.tab_owner_disabled = !xent_sem_enabled(ctx->ctx, owner);
}

void flux_snap_tab_view_item(FluxSnapCtx const *ctx) {
	FluxTabViewItem const *it  = ( FluxTabViewItem const * ) ctx->data;
	ctx->snap->u.tab.tab_kind  = ( uint8_t ) it->kind;
	ctx->snap->u.tab.icon_name = it->icon_name;
	if (it->kind == FLUX_TAB_KIND_CLOSE) snapshot_tab_close(ctx, it);
	if (it->kind != FLUX_TAB_KIND_TAB) return;

	FluxTabViewData const *tv       = it->tv;
	bool                   selected = (tv->selected == it->index);
	ctx->snap->u.tab.is_checked     = selected;
	bool compact                    = tv->width_mode == FLUX_TAB_WIDTH_COMPACT && !selected;
	ctx->snap->u.tab.label          = compact ? NULL : it->label;
	ctx->snap->u.tab.tab_dragging   = tv->drag_active && tv->drag_slot == it->index;
	ctx->snap->u.tab.tab_closable   = it->closable;
	ctx->snap->u.tab.tab_separator  = snapshot_tab_separator(it);
}

void flux_snap_text(FluxSnapCtx const *ctx) {
	snapshot_text(ctx->snap, ( FluxTextData const * ) ctx->data);
	ctx->snap->u.text.text_content = xent_node_text(ctx->ctx, ctx->node);
	ctx->snap->font_size           = xent_fontsize(ctx->ctx, ctx->node);
	ctx->snap->u.text.font_weight  = ( FluxFontWeight ) xent_fontweight(ctx->ctx, ctx->node);
}

void flux_snap_fonticon(FluxSnapCtx const *ctx) {
	FluxFontIconData const *d                = ( FluxFontIconData const * ) ctx->data;
	ctx->snap->font_size                     = d->font_size > 0.0f ? d->font_size : 20.0f;
	ctx->snap->u.font_icon.glyph             = d->glyph;
	ctx->snap->u.font_icon.font_family       = d->font_family;
	ctx->snap->u.font_icon.color             = d->color;
	ctx->snap->u.font_icon.font_weight       = d->font_weight;
	ctx->snap->u.font_icon.mirrored_when_rtl = d->mirrored_when_rtl;
}

void flux_snap_button(FluxSnapCtx const *ctx) { snapshot_button(ctx->snap, ( FluxButtonData const * ) ctx->data); }

void flux_snap_checkbox_like(FluxSnapCtx const *ctx) {
	snapshot_checkbox_like(ctx->snap, ( FluxCheckboxData const * ) ctx->data);
}

void flux_snap_switch(FluxSnapCtx const *ctx) { snapshot_switch(ctx->snap, ( FluxCheckboxData const * ) ctx->data); }

void flux_snap_slider(FluxSnapCtx const *ctx) { snapshot_slider(ctx->snap, ( FluxSliderData const * ) ctx->data); }

void flux_snap_textbox(FluxSnapCtx const *ctx) {
	tb_sync_content(( FluxTextBoxInputData * ) ctx->data);
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
	if (ctx->snap->u.textbox.combo_editor) ctx->snap->u.textbox.combo_owner = xent_node_parent(ctx->ctx, ctx->node);
}

static void snapshot_scroll_bar(FluxRenderSnap *snap, FluxScrollBarData const *sb) {
	snap->u.scroll_bar.value             = ( float ) sb->value;
	snap->u.scroll_bar.minimum           = ( float ) sb->minimum;
	snap->u.scroll_bar.maximum           = ( float ) sb->maximum;
	snap->u.scroll_bar.viewport          = ( float ) sb->viewport_size;
	snap->u.scroll_bar.orientation       = ( uint8_t ) sb->orientation;
	snap->u.scroll_bar.indicator_mode    = ( uint8_t ) sb->indicator_mode;
	snap->u.scroll_bar.dragging          = sb->dragging ? 1u : 0u;
	snap->u.scroll_bar.dec_pressed       = sb->dec_pressed;
	snap->u.scroll_bar.inc_pressed       = sb->inc_pressed;
	snap->u.scroll_bar.large_dec_pressed = sb->large_dec_pressed;
	snap->u.scroll_bar.large_inc_pressed = sb->large_inc_pressed;
}

void flux_snap_scroll_bar(FluxSnapCtx const *ctx) {
	snapshot_scroll_bar(ctx->snap, ( FluxScrollBarData const * ) ctx->data);
}

void flux_snap_looping(FluxSnapCtx const *ctx) {
	FluxLoopingData const *ld          = ( FluxLoopingData const * ) ctx->data;
	bool                   shown       = ld->enabled && ld->pointer_inside && !ld->pointer_touch;
	ctx->snap->u.looping.buttons_shown = shown ? 1u : 0u;
	ctx->snap->u.looping.up_pressed    = ld->up_pressed ? 1u : 0u;
	ctx->snap->u.looping.down_pressed  = ld->down_pressed ? 1u : 0u;
}

void flux_snap_looping_item(FluxSnapCtx const *ctx) {
	FluxLoopingItemData const *cell = ( FluxLoopingItemData const * ) ctx->data;
	FluxLoopingData const     *ld   = cell->owner;
	bool                       live = ld && ld->item_count > 0 && cell->visual >= 0;
	ctx->snap->u.loop_item.label    = live ? ld->items [flux_looping_mod(cell->visual, ld->item_count)] : NULL;
	ctx->snap->u.loop_item.selected = cell->selected;
	ctx->snap->u.loop_item.accent   = ld && ld->accent;
	ctx->snap->u.loop_item.left     = ld && ld->left;
	ctx->snap->u.loop_item.band_y   = cell->band_y;
	ctx->snap->u.loop_item.band_h   = ld ? ld->item_height : 0.0f;
}

void flux_snap_picker(FluxSnapCtx const *ctx) {
	flux_picker_snapshot(&ctx->snap->u.picker, ( FluxPickerData const * ) ctx->data);
}

void flux_snap_picker_card(FluxSnapCtx const *ctx) {
	ctx->snap->u.picker_card = *( FluxPickerCardData const * ) ctx->data;
}

void flux_snap_scroll(FluxSnapCtx const *ctx) { snapshot_scroll(ctx->snap, ( FluxScrollData const * ) ctx->data); }

void flux_snap_progress(FluxSnapCtx const *ctx) {
	snapshot_progress(ctx->snap, ( FluxProgressData const * ) ctx->data);
}

void flux_snap_password_box(FluxSnapCtx const *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx->data;
	tb_sync_content(tb);
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
	ctx->snap->u.textbox.is_checked = tb->password_show_plain || xent_sem_checked(ctx->ctx, ctx->node) != 0;
}

void flux_snap_number_box(FluxSnapCtx const *ctx) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) ctx->data;
	tb_sync_content(tb);
	snapshot_textbox(ctx->snap, &tb->base);
	ctx->snap->u.textbox.nb_spin_placement = tb->nb->spin_placement;
	ctx->snap->u.textbox.nb_up_enabled     = nb_spin_enabled(tb->nb, true);
	ctx->snap->u.textbox.nb_down_enabled   = nb_spin_enabled(tb->nb, false);
	ctx->snap->u.textbox.nb_spin_direction = tb->nb->spin_direction;
}

void flux_snap_hyperlink(FluxSnapCtx const *ctx) {
	snapshot_hyperlink(ctx->snap, ( FluxHyperlinkData const * ) ctx->data);
}

void flux_snap_repeat_button(FluxSnapCtx const *ctx) {
	snapshot_repeat_button(ctx->snap, ( FluxRepeatButtonData const * ) ctx->data);
}

void flux_snap_progress_ring(FluxSnapCtx const *ctx) {
	snapshot_progress_ring(ctx->snap, ( FluxProgressRingData const * ) ctx->data);
}

void flux_snap_info_badge(FluxSnapCtx const *ctx) {
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

void flux_snap_breadcrumb_item(FluxSnapCtx const *ctx) {
	FluxBreadcrumbItem const    *it      = ( FluxBreadcrumbItem const * ) ctx->data;
	FluxBreadcrumbBarData const *d       = it->bar;
	bool                         is_last = it->elem == d->count;
	ctx->snap->u.breadcrumb.label        = it->elem > 0 ? d->labels [it->elem - 1] : NULL;
	ctx->snap->u.breadcrumb.is_ellipsis  = it->elem == 0;
	ctx->snap->u.breadcrumb.is_last      = is_last;
	ctx->snap->u.breadcrumb.show_chevron = !is_last;
	ctx->snap->u.breadcrumb.content_w    = d->widths [it->elem] - (is_last ? 0.0f : d->chevron_cell_w);
}

void flux_snap_selector_item(FluxSnapCtx const *ctx) {
	FluxSelectorBarItemData const *it = ( FluxSelectorBarItemData const * ) ctx->data;
	ctx->snap->u.selector.text        = it->text;
	ctx->snap->u.selector.icon_glyph  = it->icon_glyph;
	ctx->snap->u.selector.selected    = it->selected;
	ctx->snap->u.selector.pill_t      = it->pill_t;
}

void flux_snap_rating(FluxSnapCtx const *ctx) {
	FluxRatingData const *r               = ( FluxRatingData const * ) ctx->data;
	ctx->snap->u.rating.value             = r->value;
	ctx->snap->u.rating.hover_value       = r->hover_value;
	ctx->snap->u.rating.placeholder_value = r->placeholder_value;
	ctx->snap->u.rating.max_rating        = r->max_rating;
	ctx->snap->u.rating.star_size         = ( float ) r->star_size;
	ctx->snap->u.rating.item_spacing      = ( float ) r->item_spacing;
	ctx->snap->u.rating.set_glyph         = r->set_glyph;
	ctx->snap->u.rating.unset_glyph       = r->unset_glyph;
	ctx->snap->u.rating.caption           = r->caption;
	ctx->snap->u.rating.is_read_only      = r->is_read_only;
}

void flux_snap_list_item(FluxSnapCtx const *ctx) {
	FluxListItemData const *it         = ( FluxListItemData const * ) ctx->data;
	ctx->snap->u.list_item.kind        = ( uint8_t ) (it->owner ? it->owner->kind : XTK_LIST_KIND_LIST);
	ctx->snap->u.list_item.is_selected = snapshot_list_item_selected(it);
	ctx->snap->u.list_item.multi       = it->multi;
}

void flux_snap_tree_item(FluxSnapCtx const *ctx) {
	FluxTreeItemData const *it = ( FluxTreeItemData const * ) ctx->data;
	FluxTreeViewData const *d  = it->owner;
	if (!d || it->flat_index < 0 || it->flat_index >= d->flat_count) return;
	int                 h            = d->flat [it->flat_index];
	FluxTreeNode const *n            = &d->nodes [h];
	ctx->snap->u.tree_item.label     = n->text;
	ctx->snap->u.tree_item.glyph     = n->glyph;
	ctx->snap->u.tree_item.depth     = n->depth;
	ctx->snap->u.tree_item.sel_state = d->sel_mode == XTK_TREE_SELECT_SINGLE
	                                   ? ( uint8_t ) (d->selected == h ? FLUX_TREE_SEL_SELECTED : FLUX_TREE_SEL_NONE)
	                                   : n->sel_state;
	ctx->snap->u.tree_item.flags = ( uint8_t ) ((n->first_child >= 0 ? FLUX_TREE_ROW_HAS_CHILDREN : 0)
	                                            | (n->expanded ? FLUX_TREE_ROW_EXPANDED : 0)
	                                            | (d->sel_mode == XTK_TREE_SELECT_MULTIPLE ? FLUX_TREE_ROW_MULTI : 0)
	                                            | (n->disabled ? FLUX_TREE_ROW_DISABLED : 0));
}

void flux_snap_flip_view(FluxSnapCtx const *ctx) {
	FluxFlipViewData const *fv    = ( FluxFlipViewData const * ) ctx->data;
	int                     count = 0;
	for (XentNodeId c = xent_node_first(ctx->ctx, fv->host); c != XENT_NODE_INVALID; c = xent_node_next(ctx->ctx, c))
		count++;
	ctx->snap->u.flip.vertical      = fv->vertical;
	ctx->snap->u.flip.prev_enabled  = fv->selected > 0 && count > 1;
	ctx->snap->u.flip.next_enabled  = fv->selected < count - 1 && count > 1;
	ctx->snap->u.flip.buttons_alive = flux_anim_now() - fv->pointer_activity < 3000;
	ctx->snap->u.flip.pressed_btn   = fv->pressed_btn;
}

void flux_snap_pips_pager(FluxSnapCtx const *ctx) {
	FluxPipsPagerData const *pd      = ( FluxPipsPagerData const * ) ctx->data;
	int                      visible = pd->max_visible < pd->count ? pd->max_visible : pd->count;
	ctx->snap->u.pips.count          = pd->count;
	ctx->snap->u.pips.selected       = pd->selected;
	ctx->snap->u.pips.window_start   = pd->window_start;
	ctx->snap->u.pips.visible        = visible > 0 ? visible : 0;
	ctx->snap->u.pips.vertical       = pd->vertical;
	ctx->snap->u.pips.nav_vis        = pd->nav_vis;
	ctx->snap->u.pips.pressed_pip    = pd->pressed_pip;
	ctx->snap->u.pips.pressed_nav    = pd->pressed_nav;
}

void flux_snap_refresh(FluxSnapCtx const *ctx) {
	FluxRefreshData const *d      = ( FluxRefreshData const * ) ctx->data;
	FluxRefreshSnapshot   *r      = &ctx->snap->u.refresh;
	unsigned long          now    = flux_anim_now();
	float const            two_pi = 6.28318530718f;
	float                  thr    = d->threshold_ratio > 0.0f ? d->threshold_ratio : FLUX_REFRESH_EXECUTION_RATIO;

	r->state                      = d->state;
	r->direction                  = ( int ) d->direction;
	r->interaction_ratio          = d->interaction_ratio;
	r->threshold_ratio            = thr;
	r->visualizer_size            = d->visualizer_size;

	if (d->state == FLUX_REFRESH_REFRESHING) {
		unsigned long e    = now - d->spin_start_tick;
		float         frac = ( float ) (e % ( unsigned long ) FLUX_REFRESH_SPIN_MS) / FLUX_REFRESH_SPIN_MS;
		r->glyph_angle     = d->start_angle + frac * two_pi;
	}
	else {
		float ratio    = d->interaction_ratio < 0.0f ? 0.0f : (d->interaction_ratio > thr ? thr : d->interaction_ratio);
		r->glyph_angle = d->start_angle + two_pi * (ratio / thr);
	}

	bool bright
	  = d->state == FLUX_REFRESH_PEEKING || d->state == FLUX_REFRESH_PENDING || d->state == FLUX_REFRESH_REFRESHING;
	r->glyph_opacity = bright ? 1.0f : d->min_opacity;

	float scale      = 1.0f;
	if (d->state == FLUX_REFRESH_PENDING && d->pop_start_tick) {
		float t = ( float ) (now - d->pop_start_tick) / FLUX_REFRESH_POP_MS;
		if (t < 1.0f) scale = (t < 0.5f) ? 1.0f + 0.5f * (t / 0.5f) : 1.5f - 0.5f * ((t - 0.5f) / 0.5f);
	}
	r->glyph_scale = scale;
}

void flux_snap_pager(FluxSnapCtx const *ctx) {
	FluxPagerData const *d   = ( FluxPagerData const * ) ctx->data;
	FluxPagerSnapshot   *p   = &ctx->snap->u.pager;
	int                  sel = d->selected;

	p->count                 = d->count;
	p->selected              = sel;
	p->display_mode          = d->display_mode;
	p->first_state           = ( uint8_t ) flux_pager_nav_state(0, d->first_vis, sel, d->count);
	p->prev_state            = ( uint8_t ) flux_pager_nav_state(1, d->prev_vis, sel, d->count);
	p->next_state            = ( uint8_t ) flux_pager_nav_state(2, d->next_vis, sel, d->count);
	p->last_state            = ( uint8_t ) flux_pager_nav_state(3, d->last_vis, sel, d->count);
	p->cell_count            = ( uint8_t ) flux_pager_build_cells(d->count, sel, !d->hide_ends, p->cells);
	p->pressed_elem          = ( int16_t ) d->pressed_elem;
	p->box_w                 = d->box_w;
	int page                 = 0;
	p->focus_kind            = ( int8_t ) flux_pager_focus(d, &page);
	p->focus_page            = page;
}

void flux_snap_colorpicker(FluxSnapCtx const *ctx) { flux_colorpicker_snapshot(&ctx->snap->u.color_picker, ctx->data); }

void flux_snap_calendar(FluxSnapCtx const *ctx) { flux_calendar_snapshot(&ctx->snap->u.calendar, ctx->data); }

void flux_snap_title_bar(FluxSnapCtx const *ctx) {
	FluxTitleBarData const *d = ( FluxTitleBarData const * ) ctx->data;
	FluxTitleBarSnapshot   *t = &ctx->snap->u.title_bar;
	t->title                  = d->title;
	t->subtitle               = d->subtitle;
	t->icon_glyph             = d->icon_glyph [0] ? d->icon_glyph : NULL;
	t->title_w                = d->title_w;
	t->subtitle_w             = d->subtitle_w;
	t->pressed                = d->pressed;
	t->focus_part             = d->focus_part;
	if (d->activate_key != FLUX_KEY_NONE) t->pressed = d->focus_part;
	t->show_back        = d->show_back;
	t->back_disabled    = d->back_disabled;
	t->show_pane_toggle = d->show_pane_toggle;
	t->frame            = d->frame;
	t->maximized        = d->maximized;
	HWND hwnd           = d->window ? flux_window_hwnd(d->window) : NULL;
	t->inactive         = hwnd && GetActiveWindow() != hwnd;
	if (d->integrate_window && d->window && d->window->title_bar_max_pressed) t->pressed = FLUX_TB_REGION_MAX;
}

void flux_snap_split_pane(FluxSnapCtx const *ctx) {
	FluxSplitPaneData const *d        = ( FluxSplitPaneData const * ) ctx->data;
	ctx->snap->u.split_pane.overlay   = d->overlay;
	ctx->snap->u.split_pane.placement = d->placement;
	ctx->snap->u.split_pane.divider   = d->divider;
}

void flux_snap_person_picture(FluxSnapCtx const *ctx) {
	FluxPersonPictureData const *d = ( FluxPersonPictureData const * ) ctx->data;
	FluxPersonSnapshot          *p = &ctx->snap->u.person;
	p->initials                    = d->resolved;
	p->image_path                  = d->image_path;
	p->badge_glyph                 = d->badge_glyph;
	p->badge_image_path            = d->badge_image_path;
	p->badge_number                = d->badge_number;
	p->diameter                    = d->diameter;
	p->is_group                    = d->is_group;
}

void flux_snapshot_build(FluxRenderSnap *snap, XentCtx const *ctx, XentNodeId node, FluxNodeData const *nd) {
	snapshot_base(snap, ctx, node, nd);
	if (nd && nd->component_data) {
		FluxControlModule const *m = flux_module_get(snap->type);
		if (m && m->snapshot) {
			FluxSnapCtx build = {snap, ctx, node, nd->component_data};
			m->snapshot(&build);
		}
	}
	if (nd && nd->motion) flux_motion_patch_snap(nd, snap);
}

static uint64_t snap_fnv(uint64_t h, void const *data, size_t len) {
	uint8_t const *p = ( uint8_t const * ) data;
	for (size_t i = 0; i < len; i++) {
		h ^= p [i];
		h *= UINT64_C(0x100000001b3);
	}
	return h;
}

static uint64_t snap_hash_str(uint64_t h, char const *s) {
	if (!s) return snap_fnv(h, "", 1u);
	return snap_fnv(h, s, strlen(s) + 1u);
}

static uint64_t snap_hash_strings_at(uint64_t h, FluxRenderSnap const *snap) {
#define STR_OFF(field) (( uint16_t ) (offsetof(FluxRenderSnap, u.field) + 1u))
	static uint16_t const kStrOff [XTK_CTRL_TYPE_COUNT][4] = {
	  [FLUX_CONTROL_TEXT]            = {STR_OFF(text.text_content), STR_OFF(text.font_family)},
	  [FLUX_CONTROL_FONT_ICON]       = {STR_OFF(font_icon.glyph), STR_OFF(font_icon.font_family)},
	  [FLUX_CONTROL_BUTTON]          = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_TOGGLE_BUTTON]   = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_REPEAT_BUTTON]   = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_HYPERLINK]       = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_DROPDOWN_BUTTON] = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_SPLIT_BUTTON]    = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_TOGGLE_SPLIT_BUTTON]
	  = {STR_OFF(button.label), STR_OFF(button.text_content), STR_OFF(button.icon_name)},
	  [FLUX_CONTROL_CHECKBOX] = {STR_OFF(check.label)},
	  [FLUX_CONTROL_RADIO]    = {STR_OFF(check.label)},
	  [FLUX_CONTROL_SWITCH]   = {STR_OFF(sw.label)},
	  [FLUX_CONTROL_TEXT_INPUT]
	  = {STR_OFF(textbox.text_content), STR_OFF(textbox.placeholder), STR_OFF(textbox.font_family)},
	  [FLUX_CONTROL_PASSWORD_BOX]
	  = {STR_OFF(textbox.text_content), STR_OFF(textbox.placeholder), STR_OFF(textbox.font_family)},
	  [FLUX_CONTROL_NUMBER_BOX]
	  = {STR_OFF(textbox.text_content), STR_OFF(textbox.placeholder), STR_OFF(textbox.font_family)},
	  [FLUX_CONTROL_INFO_BADGE]        = {STR_OFF(info_badge.icon_name)},
	  [FLUX_CONTROL_INFO_BAR]          = {STR_OFF(info_bar.label), STR_OFF(info_bar.text_content)},
	  [FLUX_CONTROL_COMBO_BOX]         = {STR_OFF(combo.text_content), STR_OFF(combo.placeholder)},
	  [FLUX_CONTROL_LOOPING_ITEM]      = {STR_OFF(loop_item.label)},
	  [FLUX_CONTROL_TAB_VIEW_ITEM]     = {STR_OFF(tab.label), STR_OFF(tab.icon_name)},
	  [FLUX_CONTROL_NAV_VIEW_ITEM]     = {STR_OFF(nav.label), STR_OFF(nav.icon_name)},
	  [FLUX_CONTROL_MENU_BAR_ITEM]     = {STR_OFF(menu.label)},
	  [FLUX_CONTROL_SELECTOR_BAR_ITEM] = {STR_OFF(selector.text)},
	  [FLUX_CONTROL_BREADCRUMB_ITEM]   = {STR_OFF(breadcrumb.label)},
	  [FLUX_CONTROL_RATING]            = {STR_OFF(rating.caption)},
	  [FLUX_CONTROL_TREE_ITEM]         = {STR_OFF(tree_item.label)},
	  [FLUX_CONTROL_TITLE_BAR] = {STR_OFF(title_bar.title), STR_OFF(title_bar.subtitle), STR_OFF(title_bar.icon_glyph)},
	  [FLUX_CONTROL_PERSON_PICTURE]
	  = {STR_OFF(person.initials), STR_OFF(person.image_path), STR_OFF(person.badge_glyph), STR_OFF(person.badge_image_path)},
	  [FLUX_CONTROL_COLITEM]      = {STR_OFF(col_item.label)},
	  [FLUX_CONTROL_COLHEADER]    = {STR_OFF(col_header.item.label)},
	  [FLUX_CONTROL_APPBARBUTTON] = {STR_OFF(app_bar.label), STR_OFF(app_bar.icon)},
	  [FLUX_CONTROL_APPBARTOGGLE] = {STR_OFF(app_bar.label), STR_OFF(app_bar.icon)},
	  [FLUX_CONTROL_APPBARSEP]    = {STR_OFF(app_bar.label), STR_OFF(app_bar.icon)},
	  [FLUX_CONTROL_COMBO_ITEM]   = {STR_OFF(combo_item.label)},
	  [FLUX_CONTROL_SUGGEST_ITEM] = {STR_OFF(suggest_item.label)},
	};
#undef STR_OFF
	if (( unsigned ) snap->type >= XTK_CTRL_TYPE_COUNT) return h;
	for (int i = 0; i < 4; i++) {
		uint16_t off1 = kStrOff [snap->type][i];
		if (!off1) break;
		char const *s = NULL;
		memcpy(&s, ( char const * ) snap + (off1 - 1u), sizeof(s));
		h = snap_hash_str(h, s);
	}
	return h;
}

static bool snap_type_is_dynamic(FluxRenderSnap const *snap) {
	static bool const kDyn [XTK_CTRL_TYPE_COUNT] = {
	  [FLUX_CONTROL_CANVAS]          = true,
	  [FLUX_CONTROL_IMAGE]           = true,
	  [FLUX_CONTROL_PROGRESS]        = true,
	  [FLUX_CONTROL_PROGRESS_RING]   = true,
	  [FLUX_CONTROL_CALENDAR_VIEW]   = true,
	  [FLUX_CONTROL_COLOR_PICKER]    = true,
	  [FLUX_CONTROL_COLOR_SPECTRUM]  = true,
	  [FLUX_CONTROL_COLOR_SLIDER]    = true,
	  [FLUX_CONTROL_TEACHING_TIP]    = true,
	  [FLUX_CONTROL_MENU_PRESENTER]  = true,
	  [FLUX_CONTROL_MENU_ROW]        = true,
	  [FLUX_CONTROL_CMDBAR_OVERFLOW] = true,
	};
	if (( unsigned ) snap->type < XTK_CTRL_TYPE_COUNT && kDyn [snap->type]) return true;
	bool is_edit = snap->type == FLUX_CONTROL_TEXT_INPUT
	            || snap->type == FLUX_CONTROL_PASSWORD_BOX
	            || snap->type == FLUX_CONTROL_NUMBER_BOX;
	return is_edit && snap->u.textbox.edit.composition_text != NULL;
}

uint64_t flux_snap_hash(
  FluxRenderSnap const *snap, FluxControlState const *state, float w, float h, FluxRenderCtx const *rc,
  bool *out_dynamic
) {
	if (out_dynamic) *out_dynamic = snap_type_is_dynamic(snap);
	uint64_t hash = UINT64_C(0xcbf29ce484222325);
	hash          = snap_fnv(hash, snap, sizeof(*snap));
	hash          = snap_fnv(hash, state, sizeof(*state));
	hash          = snap_fnv(hash, &w, sizeof(w));
	hash          = snap_fnv(hash, &h, sizeof(h));
	if (rc) {
		uint8_t dark     = rc->is_dark ? 1u : 0u;
		uint8_t contrast = (rc->theme && rc->theme->contrast.active) ? 1u : 0u;
		hash             = snap_fnv(hash, &dark, sizeof(dark));
		hash             = snap_fnv(hash, &contrast, sizeof(contrast));
		hash             = snap_fnv(hash, &rc->dpi.dpi_x, sizeof(rc->dpi.dpi_x));
		hash             = snap_fnv(hash, &rc->theme, sizeof(rc->theme));
	}
	return snap_hash_strings_at(hash, snap);
}
