#include "controls/draw/flux_control_draw.h"
#include "controls/textbox/tb_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include <string.h>

#define PB_REVEAL_BTN_W 30.0f

typedef struct PbDrawContext {
	FluxRenderContext const  *rc;
	FluxRenderSnapshot const *snap;
	FluxRect const           *bounds;
	FluxControlState const   *state;
	float                     col0_w;
	float                     radius;
} PbDrawContext;

typedef struct PbRevealState {
	FluxRect rect;
	bool     hovered;
	bool     pressed;
} PbRevealState;

static FluxColor pb_fill_color(FluxControlState const *state, FluxThemeColors const *t) {
	if (!state->enabled) return t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4d);
	if (state->focused) return t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xff);
	if (state->hovered) return t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
	return t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xb3);
}

static void pb_draw_chrome(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state,
  float radius
) {
	FluxThemeColors const *t    = rc->theme;
	FluxColor              fill = pb_fill_color(state, t);
	FluxCacheEntry        *ce   = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (ce) {
		FluxColor tweened;
		bool      a = flux_color_tween_update(&ce->color_anim, fill, FLUX_ANIM_DURATION_FAST, rc->now, &tweened);
		fill        = tweened;
		if (a && rc->animations_active) *rc->animations_active = true;
	}

	flux_fill_rounded_rect(rc, bounds, radius, fill);
	if (!state->enabled) {
		FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
		flux_draw_rounded_rect(rc, bounds, radius, dis_stroke, 1.0f);
		return;
	}

	textbox_draw_elevation_border(rc, bounds, radius, state->focused);
	if (state->focused) textbox_draw_focus_accent(rc, bounds, radius);
}

static FluxRenderSnapshot
pb_masked_snapshot(FluxRenderSnapshot const *snap, bool has_text, bool revealed, char *mask_buf, size_t mask_cap) {
	FluxRenderSnapshot masked = *snap;
	if (!has_text || revealed) return masked;

	uint32_t text_len = ( uint32_t ) strlen(snap->text_content);
	pb_build_mask(snap->text_content, text_len, mask_buf, ( uint32_t ) mask_cap);
	masked.text_content    = mask_buf;
	masked.cursor_position = pb_original_offset_to_mask(snap->text_content, text_len, snap->cursor_position);
	masked.selection_start = pb_original_offset_to_mask(snap->text_content, text_len, snap->selection_start);
	masked.selection_end   = pb_original_offset_to_mask(snap->text_content, text_len, snap->selection_end);
	masked.scroll_offset_x = 0.0f;
	return masked;
}

static FluxRect pb_reveal_rect(FluxRect const *bounds, float col0_w) {
	float btn_margin_l = 0.0f;
	float btn_margin_t = 4.0f;
	float btn_margin_r = 4.0f;
	float btn_margin_b = 4.0f;
	float btn_x        = bounds->x + col0_w + btn_margin_l;
	float btn_y        = bounds->y + btn_margin_t;
	float btn_w        = PB_REVEAL_BTN_W - btn_margin_l - btn_margin_r;
	float btn_h        = bounds->h - btn_margin_t - btn_margin_b;
	if (btn_w < 0.0f) btn_w = 0.0f;
	if (btn_h < 0.0f) btn_h = 0.0f;
	return (FluxRect) {btn_x, btn_y, btn_w, btn_h};
}

static PbRevealState pb_reveal_state(PbDrawContext const *dc) {
	bool hovered = dc->state->hovered && dc->snap->hover_local_x >= dc->col0_w;
	return (PbRevealState) {
	  .rect    = pb_reveal_rect(dc->bounds, dc->col0_w),
	  .hovered = hovered,
	  .pressed = hovered && dc->state->pressed,
	};
}

static void pb_draw_reveal_fill(PbDrawContext const *dc, PbRevealState const *button) {
	FluxThemeColors const *t = dc->rc->theme;
	if (button->pressed)
		flux_fill_rounded_rect(
		  dc->rc, &button->rect, dc->radius, t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f)
		);
	else if (button->hovered)
		flux_fill_rounded_rect(
		  dc->rc, &button->rect, dc->radius, t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06)
		);
}

static void pb_draw_reveal_icon(PbDrawContext const *dc, PbRevealState const *button) {
	FluxThemeColors const *t             = dc->rc->theme;
	FluxColor              icon_color    = button->pressed ? (t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72))
	                                                       : (t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e));
	char                   icon_utf8 [4] = {( char ) 0xef, ( char ) 0x9e, ( char ) 0x8d, '\0'};

	FluxTextStyle          is;
	memset(&is, 0, sizeof(is));
	is.font_family = "Segoe Fluent Icons";
	is.font_size   = 12.0f;
	is.font_weight = FLUX_FONT_REGULAR;
	is.text_align  = FLUX_TEXT_CENTER;
	is.vert_align  = FLUX_TEXT_VCENTER;
	is.color       = icon_color;
	is.word_wrap   = false;
	flux_text_draw(dc->rc->text, FLUX_RT(dc->rc), icon_utf8, &button->rect, &is);
}

static void pb_draw_reveal_button(PbDrawContext const *dc) {
	if (!dc->rc->text) return;

	PbRevealState button = pb_reveal_state(dc);
	pb_draw_reveal_fill(dc, &button);
	pb_draw_reveal_icon(dc, &button);
}

void flux_draw_password_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	float radius      = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool  has_text    = snap->text_content && snap->text_content [0];
	bool  show_reveal = state->enabled && has_text && state->focused;
	bool  revealed    = snap->is_checked;

	float col1_w      = show_reveal ? PB_REVEAL_BTN_W : 0.0f;
	float col0_w      = bounds->w - col1_w;
	if (col0_w < 0.0f) col0_w = 0.0f;

	pb_draw_chrome(rc, snap, bounds, state, radius);

	char               mask_buf [2048];
	FluxRenderSnapshot masked_snap = pb_masked_snapshot(snap, has_text, revealed, mask_buf, sizeof(mask_buf));

	FluxRect           text_area   = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, col0_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};

	flux_draw_textbox_content(rc, &masked_snap, &text_area, state);

	PbDrawContext dc = {rc, snap, bounds, state, col0_w, radius};
	if (show_reveal) pb_draw_reveal_button(&dc);
}
