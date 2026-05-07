#include "render/flux_fluent.h"
#include <string.h>

typedef struct HyperlinkBackground {
	FluxRenderContext const *rc;
	FluxRect const          *bounds;
	FluxControlState const  *state;
	float                    radius;
	float                    hover_t;
	float                    press_t;
} HyperlinkBackground;

static void hyperlink_update_animation(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, float *out_hover_t,
  float *out_press_t
) {
	FluxCacheEntry *ce      = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float           hover_t = state->hovered ? 1.0f : 0.0f;
	float           press_t = state->pressed ? 1.0f : 0.0f;

	if (ce) {
		FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
		FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, false};
		bool              animating = flux_tween_update_states(&channels, targets, rc->now);
		if (animating && rc->animations_active) *rc->animations_active = true;
		hover_t = ce->hover_anim.current;
		press_t = ce->press_anim.current;
	}

	*out_hover_t = hover_t;
	*out_press_t = press_t;
}

static FluxColor hyperlink_text_color(
  FluxRenderSnapshot const *snap, FluxControlState const *state, FluxThemeColors const *t, float hover_t, float press_t
) {
	if (!state->enabled) return t ? t->text_disabled : ft_text_disabled();

	if (flux_color_af(snap->text_color) > 0.0f) return snap->text_color;

	FluxColor acc_n = t ? t->accent_default : ft_accent_default();
	FluxColor acc_h = t ? t->accent_secondary : ft_accent_secondary();
	FluxColor acc_p = t ? t->accent_tertiary : ft_accent_tertiary();
	return flux_anim_lerp_color(flux_anim_lerp_color(acc_n, acc_h, hover_t), acc_p, press_t);
}

static void hyperlink_draw_background(HyperlinkBackground const *bg) {
	if (!bg->state->enabled || (bg->hover_t <= 0.001f && bg->press_t <= 0.001f)) return;

	FluxThemeColors const *t              = bg->rc->theme;
	FluxColor              bg_transparent = flux_color_rgba(0, 0, 0, 0);
	FluxColor              bg_hover       = t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0x09);
	FluxColor              bg_press       = t ? t->subtle_fill_tertiary : flux_color_rgba(0, 0, 0, 0x06);
	FluxColor              hover_bg
	  = flux_anim_lerp_color(flux_anim_lerp_color(bg_transparent, bg_hover, bg->hover_t), bg_press, bg->press_t);
	flux_fill_rounded_rect(bg->rc, bg->bounds, bg->radius, hover_bg);
}

void flux_draw_hyperlink(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	char const *label = snap->label ? snap->label : snap->text_content;
	if (!label || !label [0] || !rc->text) return;

	FluxRect sb      = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float    hover_t = 0.0f;
	float    press_t = 0.0f;
	hyperlink_update_animation(rc, snap, state, &hover_t, &press_t);

	float               font_size  = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	float               radius     = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxColor           text_color = hyperlink_text_color(snap, state, rc->theme, hover_t, press_t);

	HyperlinkBackground bg         = {rc, &sb, state, radius, hover_t, press_t};
	hyperlink_draw_background(&bg);
	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &sb, radius);

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size         = font_size;
	ts.font_weight       = FLUX_FONT_REGULAR;
	ts.text_align        = FLUX_TEXT_CENTER;
	ts.vert_align        = FLUX_TEXT_VCENTER;
	ts.color             = text_color;
	ts.word_wrap         = false;

	FluxRect text_bounds = {
	  sb.x + FLUX_BTN_PAD_LEFT, sb.y + FLUX_BTN_PAD_TOP, sb.w - FLUX_BTN_PAD_LEFT - FLUX_BTN_PAD_RIGHT,
	  sb.h - FLUX_BTN_PAD_TOP - FLUX_BTN_PAD_BOTTOM};
	text_bounds.w = flux_maxf(0.0f, text_bounds.w);
	text_bounds.h = flux_maxf(0.0f, text_bounds.h);

	flux_text_draw(rc->text, FLUX_RT(rc), label, &text_bounds, &ts);

	if (!state->enabled) return;

	FluxSize text_size    = flux_text_measure(rc->text, label, &ts, 0);
	float    text_start_x = text_bounds.x + (text_bounds.w - text_size.w) * 0.5f;
	float    underline_y  = sb.y + (sb.h + text_size.h) * 0.5f;
	flux_draw_line(
	  rc, &(FluxLineSpec) {text_start_x, underline_y, text_start_x + text_size.w, underline_y}, text_color, 1.0f
	);
}
