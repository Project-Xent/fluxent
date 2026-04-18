#include "../flux_fluent.h"

void flux_draw_hyperlink(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t       = rc->theme;
	FluxRect               sb      = flux_snap_bounds(bounds, 1.0f, 1.0f);

	/* ── Hover/press tweens (WinUI ColorsFastAnimationDuration ≈ 83 ms) ── */
	FluxCacheEntry        *ce      = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	float                  hover_t = state->hovered ? 1.0f : 0.0f;
	float                  press_t = state->pressed ? 1.0f : 0.0f;
		if (ce) {
			bool animating = flux_tween_update_states(
			  &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim, state->hovered, state->pressed,
			  state->focused, false, rc->now
			);
			if (animating && rc->animations_active) *rc->animations_active = true;
			hover_t = ce->hover_anim.current;
			press_t = ce->press_anim.current;
		}

	/* ── Resolve text color based on state ──
	   Legacy uses AccentTextFillColor* tokens (SystemAccentColorDark2/Dark3/Dark1).
	   We approximate with accent_* for now; the real fix comes with system accent
	   color integration.  Disabled uses TextFillColorDisabled (not accent). */
	FluxColor text_color;
		if (!state->enabled) {
			/* Legacy: AccentTextFillColorDisabledBrush = #5C000000 (Light)
			   which is identical to TextFillColorDisabled. */
			text_color = t ? t->text_disabled : ft_text_disabled();
		}
		else {
			FluxColor acc_n = t ? t->accent_default : ft_accent_default();
			FluxColor acc_h = t ? t->accent_secondary : ft_accent_secondary();
			FluxColor acc_p = t ? t->accent_tertiary : ft_accent_tertiary();
			text_color      = flux_anim_lerp_color(flux_anim_lerp_color(acc_n, acc_h, hover_t), acc_p, press_t);
		}

	/* Override with user-specified color if set */
	if (flux_color_af(snap->text_color) > 0.0f && state->enabled) text_color = snap->text_color;

	char const *label = snap->label ? snap->label : snap->text_content;
	if (!label || !label [0] || !rc->text) return;

	float font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	float radius    = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;

		/* ── Background ──
	       Legacy: SubtleFillColor tokens — far more transparent than ControlFillColor.
	       Normal & Disabled = Transparent. Lerp transparent → secondary (hover) → tertiary (press). */
		if (state->enabled && (hover_t > 0.001f || press_t > 0.001f)) {
			FluxColor bg_transparent = flux_color_rgba(0, 0, 0, 0);
			FluxColor bg_hover       = t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0x09);
			FluxColor bg_press       = t ? t->subtle_fill_tertiary : flux_color_rgba(0, 0, 0, 0x06);
			FluxColor hover_bg
			  = flux_anim_lerp_color(flux_anim_lerp_color(bg_transparent, bg_hover, hover_t), bg_press, press_t);
			flux_fill_rounded_rect(rc, &sb, radius, hover_bg);
		}

	/* Focus rect */
	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &sb, radius);

	/* Text style */
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size         = font_size;
	ts.font_weight       = FLUX_FONT_REGULAR;
	ts.text_align        = FLUX_TEXT_CENTER;
	ts.vert_align        = FLUX_TEXT_VCENTER;
	ts.color             = text_color;
	ts.word_wrap         = false;

	/* ── Padding ──
	   Legacy: Padding="{StaticResource ButtonPadding}" = 11,5,11,6  (L,T,R,B).
	   Same as the standard Button padding. */
	FluxRect text_bounds = {
	  sb.x + FLUX_BTN_PAD_LEFT, sb.y + FLUX_BTN_PAD_TOP, sb.w - FLUX_BTN_PAD_LEFT - FLUX_BTN_PAD_RIGHT,
	  sb.h - FLUX_BTN_PAD_TOP - FLUX_BTN_PAD_BOTTOM};
	if (text_bounds.w < 0.0f) text_bounds.w = 0.0f;
	if (text_bounds.h < 0.0f) text_bounds.h = 0.0f;

	/* Draw text */
	flux_text_draw(rc->text, FLUX_RT(rc), label, &text_bounds, &ts);

		/* ── Underline ──
	       WinUI HyperlinkButton adds TextDecorations.Underline in code-behind
	       (OnApplyTemplate), not in the XAML template.  We replicate that here
	       for all enabled states. */
		if (state->enabled) {
			FluxSize text_size    = flux_text_measure(rc->text, label, &ts, 0);
			float    text_start_x = text_bounds.x + (text_bounds.w - text_size.w) * 0.5f;
			float    underline_y  = sb.y + (sb.h + text_size.h) * 0.5f;
			flux_draw_line(rc, text_start_x, underline_y, text_start_x + text_size.w, underline_y, text_color, 1.0f);
		}
}
