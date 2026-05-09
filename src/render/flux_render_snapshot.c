#include "fluxent/flux_render_snapshot.h"
#include "controls/textbox/tb_internal.h"

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
	bool            enabled;
} SnapshotButtonFields;

static void snapshot_base(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd) {
	memset(snap, 0, sizeof(*snap));
	snap->id      = ( uint64_t ) node;
	snap->type    = xent_get_control_type(ctx, node);
	snap->enabled = xent_get_semantic_enabled(ctx, node);
	if (!nd) return;

	snap->background    = nd->visuals.background;
	snap->border_color  = nd->visuals.border_color;
	snap->corner_radius = nd->visuals.corner_radius;
	snap->border_width  = nd->visuals.border_width;
	snap->opacity       = nd->visuals.opacity;
	snap->hover_local_x = nd->hover_local_x;
}

static void snapshot_text(FluxRenderSnapshot *snap, FluxTextData const *t) {
	snap->text_content        = t->content;
	snap->font_family         = t->font_family;
	snap->text_color          = t->text_color;
	snap->font_size           = t->font_size;
	snap->font_weight         = t->font_weight;
	snap->text_alignment      = t->alignment;
	snap->text_vert_alignment = t->vertical_alignment;
	snap->max_lines           = t->max_lines;
	snap->word_wrap           = t->wrap;
}

static void snapshot_button_fields(FluxRenderSnapshot *snap, SnapshotButtonFields fields) {
	snap->label        = fields.label;
	snap->icon_name    = fields.icon_name;
	snap->text_color   = fields.label_color;
	snap->font_size    = fields.font_size;
	snap->button_style = fields.style;
	snap->enabled      = fields.enabled;
}

static void snapshot_button(FluxRenderSnapshot *snap, FluxButtonData const *b) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {b->label, b->icon_name, b->label_color, b->font_size, b->style, b->enabled}
	);
	snap->is_checked = b->is_checked;
}

static void snapshot_textbox(FluxRenderSnapshot *snap, FluxTextBoxData const *tb) {
	snap->text_content       = tb->content;
	snap->placeholder        = tb->placeholder;
	snap->font_family        = tb->font_family;
	snap->font_size          = tb->font_size;
	snap->text_color         = tb->text_color;
	snap->cursor_position    = tb->cursor_position;
	snap->selection_start    = tb->selection_start;
	snap->selection_end      = tb->selection_end;
	snap->scroll_offset_x    = tb->scroll_offset_x;
	snap->enabled            = tb->enabled;
	snap->composition_text   = tb->composition_text;
	snap->composition_length = tb->composition_length;
	snap->composition_cursor = tb->composition_cursor;
	snap->selection_color    = tb->selection_color;
	snap->readonly           = tb->readonly;
}

static void snapshot_checkbox_like(FluxRenderSnapshot *snap, FluxCheckboxData const *c) {
	snap->label       = c->label;
	snap->check_state = c->state;
	snap->enabled     = c->enabled;
}

static void snapshot_slider(FluxRenderSnapshot *snap, FluxSliderData const *s) {
	snap->min_value     = s->min_value;
	snap->max_value     = s->max_value;
	snap->current_value = s->current_value;
	snap->step          = s->step;
	snap->enabled       = s->enabled;
}

static void snapshot_scroll(FluxRenderSnapshot *snap, FluxScrollData const *sc) {
	snap->scroll_x                  = sc->scroll_x;
	snap->scroll_y                  = sc->scroll_y;
	snap->scroll_content_w          = sc->content_w;
	snap->scroll_content_h          = sc->content_h;
	snap->scroll_h_vis              = sc->h_vis;
	snap->scroll_v_vis              = sc->v_vis;
	snap->scroll_mouse_over         = sc->mouse_over;
	snap->scroll_mouse_local_x      = sc->mouse_local_x;
	snap->scroll_mouse_local_y      = sc->mouse_local_y;
	snap->scroll_drag_axis          = sc->drag_axis;
	snap->scroll_v_up_pressed       = sc->v_up_pressed;
	snap->scroll_v_dn_pressed       = sc->v_dn_pressed;
	snap->scroll_h_lf_pressed       = sc->h_lf_pressed;
	snap->scroll_h_rg_pressed       = sc->h_rg_pressed;
	snap->scroll_last_activity_time = sc->last_activity_time;
}

static void snapshot_progress_fields(FluxRenderSnapshot *snap, float value, float max_value, bool indeterminate) {
	snap->current_value = value;
	snap->max_value     = max_value;
	snap->indeterminate = indeterminate;
}

static void snapshot_progress(FluxRenderSnapshot *snap, FluxProgressData const *p) {
	snapshot_progress_fields(snap, p->value, p->max_value, p->indeterminate);
}

static void snapshot_number_box_spin(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node) {
	snap->nb_spin_placement = ( uint8_t ) (xent_get_semantic_expanded(ctx, node) ? 2 : 0);

	float sv                = 0.0f;
	float smin              = 0.0f;
	float smax              = 0.0f;
	if (!xent_get_semantic_value(ctx, node, &sv, &smin, &smax)) {
		snap->nb_up_enabled   = true;
		snap->nb_down_enabled = true;
		return;
	}
	snap->nb_up_enabled   = (sv < smax) || (smin == smax);
	snap->nb_down_enabled = (sv > smin) || (smin == smax);
}

static void snapshot_hyperlink(FluxRenderSnapshot *snap, FluxHyperlinkData const *hl) {
	snapshot_button_fields(
	  snap,
	  (SnapshotButtonFields) {hl->label, hl->icon_name, hl->label_color, hl->font_size, FLUX_BUTTON_TEXT, hl->enabled}
	);
}

static void snapshot_repeat_button(FluxRenderSnapshot *snap, FluxRepeatButtonData const *rb) {
	snapshot_button_fields(
	  snap, (SnapshotButtonFields) {rb->label, rb->icon_name, rb->label_color, rb->font_size, rb->style, rb->enabled}
	);
}

static void snapshot_progress_ring(FluxRenderSnapshot *snap, FluxProgressRingData const *pr) {
	snapshot_progress_fields(snap, pr->value, pr->max_value, pr->indeterminate);
}

static void snapshot_info_badge(FluxRenderSnapshot *snap, FluxInfoBadgeData const *ib) {
	snap->current_value = ( float ) ib->value;
	snap->icon_name     = ib->icon_name;
	snap->background    = ib->background;
	snap->is_checked    = (ib->mode == FLUX_BADGE_DOT);
	snap->indeterminate = (ib->mode == FLUX_BADGE_ICON);
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

static void snapshot_handle_slider(SnapshotContext const *ctx) {
	snapshot_slider(ctx->snap, ( FluxSliderData const * ) ctx->data);
}

static void snapshot_handle_textbox(SnapshotContext const *ctx) {
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
}

static void snapshot_handle_scroll(SnapshotContext const *ctx) {
	snapshot_scroll(ctx->snap, ( FluxScrollData const * ) ctx->data);
}

static void snapshot_handle_progress(SnapshotContext const *ctx) {
	snapshot_progress(ctx->snap, ( FluxProgressData const * ) ctx->data);
}

static void snapshot_handle_password_box(SnapshotContext const *ctx) {
	FluxTextBoxInputData const *tb = ( FluxTextBoxInputData const * ) ctx->data;
	snapshot_textbox(ctx->snap, ( FluxTextBoxData const * ) ctx->data);
	ctx->snap->is_checked = tb->password_show_plain || xent_get_semantic_checked(ctx->ctx, ctx->node) != 0;
}

static void snapshot_handle_number_box(SnapshotContext const *ctx) {
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

static SnapshotHandler const SNAPSHOT_HANDLERS [XENT_CONTROL_CUSTOM + 1] = {
  [XENT_CONTROL_TEXT]          = snapshot_handle_text,
  [XENT_CONTROL_BUTTON]        = snapshot_handle_button,
  [XENT_CONTROL_TOGGLE_BUTTON] = snapshot_handle_button,
  [XENT_CONTROL_CHECKBOX]      = snapshot_handle_checkbox_like,
  [XENT_CONTROL_RADIO]         = snapshot_handle_checkbox_like,
  [XENT_CONTROL_SWITCH]        = snapshot_handle_checkbox_like,
  [XENT_CONTROL_SLIDER]        = snapshot_handle_slider,
  [XENT_CONTROL_TEXT_INPUT]    = snapshot_handle_textbox,
  [XENT_CONTROL_SCROLL]        = snapshot_handle_scroll,
  [XENT_CONTROL_PROGRESS]      = snapshot_handle_progress,
  [XENT_CONTROL_PASSWORD_BOX]  = snapshot_handle_password_box,
  [XENT_CONTROL_NUMBER_BOX]    = snapshot_handle_number_box,
  [XENT_CONTROL_HYPERLINK]     = snapshot_handle_hyperlink,
  [XENT_CONTROL_REPEAT_BUTTON] = snapshot_handle_repeat_button,
  [XENT_CONTROL_PROGRESS_RING] = snapshot_handle_progress_ring,
  [XENT_CONTROL_INFO_BADGE]    = snapshot_handle_info_badge,
};

void flux_snapshot_build(FluxRenderSnapshot *snap, XentContext const *ctx, XentNodeId node, FluxNodeData const *nd) {
	snapshot_base(snap, ctx, node, nd);
	if (!nd || !nd->component_data) return;

	SnapshotHandler handler = snap->type <= XENT_CONTROL_CUSTOM ? SNAPSHOT_HANDLERS [snap->type] : NULL;
	if (!handler) return;

	SnapshotContext build = {snap, ctx, node, nd->component_data};
	handler(&build);
}
