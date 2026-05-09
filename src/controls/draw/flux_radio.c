#include "render/flux_fluent.h"
#include <string.h>

#define RADIO_OUTER_R       10.0f
#define RADIO_GLYPH_NORMAL  6.0f
#define RADIO_GLYPH_HOVER   7.0f
#define RADIO_GLYPH_PRESS   5.0f
#define RADIO_PRESS_GLYPH_R 5.0f

typedef struct {
	float check;
	float hover;
	float press;
} RadioAnim;

typedef struct {
	FluxColor outer_fill;
	FluxColor outer_stroke;
	FluxColor glyph_fill;
} RadioColors;

typedef struct RadioGlyphDraw {
	FluxRenderContext const *rc;
	FluxControlState const  *state;
	RadioAnim                anim;
	RadioColors              colors;
	float                    cx;
	float                    cy;
	bool                     checked;
} RadioGlyphDraw;

static RadioAnim radio_update_anim(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, bool checked
) {
	FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (!ce) return (RadioAnim) {checked ? 1.0f : 0.0f, state->hovered ? 1.0f : 0.0f, state->pressed ? 1.0f : 0.0f};

	FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
	FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, checked};
	bool              animating = flux_tween_update_states(&channels, targets, rc->now);
	if (animating && rc->animations_active) *rc->animations_active = true;
	return (RadioAnim) {ce->check_anim.current, ce->hover_anim.current, ce->press_anim.current};
}

static RadioColors radio_disabled_colors(bool checked, FluxThemeColors const *t) {
	RadioColors c;
	c.outer_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
	c.outer_fill   = t ? t->ctrl_alt_fill_disabled : flux_color_rgba(0, 0, 0, 0);
	c.glyph_fill
	  = checked ? (t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255)) : flux_color_rgba(0, 0, 0, 0);
	return c;
}

static RadioColors radio_checked_colors(RadioAnim anim, FluxThemeColors const *t) {
	FluxColor   acc_normal = t ? t->accent_default : flux_color_rgb(0, 120, 212);
	FluxColor   acc_hover  = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xe6);
	FluxColor   acc_press  = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xcc);
	RadioColors c;
	c.outer_stroke
	  = flux_anim_lerp_color(flux_anim_lerp_color(acc_normal, acc_hover, anim.hover), acc_press, anim.press);
	c.outer_fill = c.outer_stroke;
	c.glyph_fill = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
	return c;
}

static RadioColors radio_unchecked_colors(RadioAnim anim, FluxThemeColors const *t) {
	FluxColor   stroke_normal = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9c);
	FluxColor   stroke_press  = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
	FluxColor   fill_normal   = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
	FluxColor   fill_hover    = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
	FluxColor   fill_press    = t ? t->ctrl_alt_fill_quarternary : flux_color_rgba(0, 0, 0, 0x12);
	RadioColors c;
	c.outer_stroke = flux_anim_lerp_color(stroke_normal, stroke_press, anim.press);
	c.outer_fill
	  = flux_anim_lerp_color(flux_anim_lerp_color(fill_normal, fill_hover, anim.hover), fill_press, anim.press);
	c.glyph_fill = flux_color_rgba(0, 0, 0, 0);
	return c;
}

static RadioColors radio_colors(bool checked, FluxControlState const *state, RadioAnim anim, FluxThemeColors const *t) {
	if (!state->enabled) return radio_disabled_colors(checked, t);
	if (checked) return radio_checked_colors(anim, t);
	return radio_unchecked_colors(anim, t);
}

static void draw_radio_label(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (!snap->label || !snap->label [0] || !rc->text) return;

	FluxThemeColors const *t        = rc->theme;
	float const            gap      = 8.0f;
	float const            diameter = RADIO_OUTER_R * 2.0f;
	FluxRect               text_rect
	  = {bounds->x + diameter + gap, bounds->y, flux_maxf(0.0f, bounds->w - diameter - gap), bounds->h};
	FluxColor     label_color = state->enabled ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xe4))
	                                           : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c));

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = 14.0f;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = label_color;
	ts.word_wrap   = false;
	flux_text_draw(rc->text, ( ID2D1RenderTarget * ) rc->d2d, snap->label, &text_rect, &ts);
}

static void draw_radio_glyph(RadioGlyphDraw const *draw) {
	if (draw->anim.check > 0.01f) {
		float target_r = flux_anim_mixf(
		  flux_anim_mixf(RADIO_GLYPH_NORMAL, RADIO_GLYPH_HOVER, draw->anim.hover), RADIO_GLYPH_PRESS, draw->anim.press
		);
		float inner_r = target_r * flux_ease_out_quad(draw->anim.check);
		flux_fill_ellipse(draw->rc, &(FluxEllipseSpec) {draw->cx, draw->cy, inner_r, inner_r}, draw->colors.glyph_fill);
	}

	if (draw->anim.press <= 0.01f || draw->checked || !draw->state->enabled) return;

	FluxColor pressed_glyph = draw->rc->theme ? draw->rc->theme->text_on_accent_primary : flux_color_rgb(255, 255, 255);
	float     r             = RADIO_PRESS_GLYPH_R * draw->anim.press;
	flux_fill_ellipse(draw->rc, &(FluxEllipseSpec) {draw->cx, draw->cy, r, r}, pressed_glyph);
}

void flux_draw_radio(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t       = rc->theme;

	float const            outer_r = RADIO_OUTER_R;
	float const            cy      = bounds->y + bounds->h * 0.5f;
	float const            cx      = bounds->x + outer_r;

	bool                   checked = (snap->check_state == FLUX_CHECK_CHECKED);
	RadioAnim              anim    = radio_update_anim(rc, snap, state, checked);
	RadioColors            colors  = radio_colors(checked, state, anim, t);

	flux_fill_ellipse(rc, &(FluxEllipseSpec) {cx, cy, outer_r, outer_r}, colors.outer_fill);
	flux_stroke_ellipse(rc, &(FluxEllipseSpec) {cx, cy, outer_r, outer_r}, colors.outer_stroke, 1.0f);
	RadioGlyphDraw glyph = {rc, state, anim, colors, cx, cy, checked};
	draw_radio_glyph(&glyph);
	draw_radio_label(rc, snap, bounds, state);

	if (state->focused && state->enabled) {
		FluxRect circle_rect = {bounds->x, cy - outer_r, outer_r * 2.0f, outer_r * 2.0f};
		flux_draw_focus_rect(rc, &circle_rect, outer_r);
	}
}
