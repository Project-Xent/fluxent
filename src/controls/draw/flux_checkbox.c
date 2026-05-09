#include "render/flux_fluent.h"
#include "render/flux_icon.h"

static char const kCheckGlyphUtf8 [] = "\xEE\x9C\xBE";

typedef struct {
	FluxColor unchecked_fill;
	FluxColor checked_fill;
	FluxColor unchecked_stroke;
	FluxColor glyph_color;
} CheckboxColors;

typedef enum
{
	CHECKBOX_VISUAL_DEFAULT,
	CHECKBOX_VISUAL_HOVERED,
	CHECKBOX_VISUAL_PRESSED,
	CHECKBOX_VISUAL_DISABLED,
} CheckboxVisualState;

static CheckboxVisualState checkbox_visual_state(FluxControlState const *state) {
	if (!state->enabled) return CHECKBOX_VISUAL_DISABLED;
	if (state->pressed) return CHECKBOX_VISUAL_PRESSED;
	if (state->hovered) return CHECKBOX_VISUAL_HOVERED;
	return CHECKBOX_VISUAL_DEFAULT;
}

static CheckboxColors checkbox_fallback_colors(CheckboxVisualState visual) {
	static CheckboxColors const fallback [] = {
	  [CHECKBOX_VISUAL_DEFAULT]  = {{0xffffffffu}, {0x0078d4ffu}, {0x0000008cu}, {0xffffffffu}},
	  [CHECKBOX_VISUAL_HOVERED]  = {{0xf9f9f980u}, {0x0078d4e6u}, {0x0000008cu}, {0xffffffffu}},
	  [CHECKBOX_VISUAL_PRESSED]  = {{0xf9f9f94du}, {0x0078d4ccu}, {0x0000008cu}, {0xffffffffu}},
	  [CHECKBOX_VISUAL_DISABLED] = {{0xf9f9f94du}, {0x00000037u}, {0x00000037u}, {0xffffffffu}},
	};
	return fallback [visual];
}

static CheckboxColors checkbox_theme_colors(FluxThemeColors const *t, CheckboxVisualState visual) {
	if (!t) return checkbox_fallback_colors(visual);

	switch (visual) {
	case CHECKBOX_VISUAL_DISABLED :
		return (CheckboxColors) {
		  t->ctrl_fill_disabled, t->accent_disabled, t->ctrl_strong_stroke_disabled, t->text_on_accent_disabled};
	case CHECKBOX_VISUAL_PRESSED :
		return (CheckboxColors) {
		  t->ctrl_fill_tertiary, t->accent_tertiary, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
	case CHECKBOX_VISUAL_HOVERED :
		return (CheckboxColors) {
		  t->ctrl_fill_secondary, t->accent_secondary, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
	case CHECKBOX_VISUAL_DEFAULT :
	default :
		return (CheckboxColors) {
		  t->ctrl_fill_default, t->accent_default, t->ctrl_strong_stroke_default, t->text_on_accent_primary};
	}
}

static CheckboxColors checkbox_colors(FluxControlState const *state, FluxThemeColors const *t) {
	return checkbox_theme_colors(t, checkbox_visual_state(state));
}

static float checkbox_update_anim(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, bool checked
) {
	FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (!ce) return checked ? 1.0f : 0.0f;

	FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
	FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, checked};
	bool              animating = flux_tween_update_states(&channels, targets, rc->now);
	if (animating && rc->animations_active) *rc->animations_active = true;
	return ce->check_anim.current;
}

static void draw_checkbox_box(
  FluxRenderContext const *rc, FluxRect const *box, CheckboxColors const *colors, float check_progress
) {
	FluxColor fill         = flux_anim_lerp_color(colors->unchecked_fill, colors->checked_fill, check_progress);
	float     stroke_alpha = flux_color_af(colors->unchecked_stroke) * (1.0f - check_progress);
	FluxColor stroke       = flux_color_rgba(
	  (colors->unchecked_stroke.rgba >> 24) & 0xff, (colors->unchecked_stroke.rgba >> 16) & 0xff,
	  (colors->unchecked_stroke.rgba >> 8) & 0xff, ( uint8_t ) (stroke_alpha * 255.0f + 0.5f)
	);

	flux_fill_rounded_rect(rc, box, FLUX_CHECKBOX_CORNER, fill);
	if (flux_color_af(stroke) > 0.0f) flux_draw_rounded_rect(rc, box, FLUX_CHECKBOX_CORNER, stroke, 1.0f);
}

static void
draw_checkbox_glyph(FluxRenderContext const *rc, FluxRect const *box, FluxColor glyph_color, float check_progress) {
	if (check_progress <= 0.001f || !rc->text) return;

	FluxTextStyle icon_style;
	memset(&icon_style, 0, sizeof(icon_style));
	icon_style.font_family = "Segoe Fluent Icons";
	icon_style.font_size   = 12.0f * flux_ease_out_quad(check_progress);
	icon_style.font_weight = FLUX_FONT_REGULAR;
	icon_style.text_align  = FLUX_TEXT_CENTER;
	icon_style.vert_align  = FLUX_TEXT_VCENTER;
	icon_style.color       = glyph_color;
	icon_style.word_wrap   = false;

	flux_text_draw(rc->text, ( ID2D1RenderTarget * ) rc->d2d, kCheckGlyphUtf8, box, &icon_style);
}

static void draw_checkbox_label(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (!snap->label || !snap->label [0] || !rc->text) return;

	FluxThemeColors const *t         = rc->theme;
	float                  gap       = FLUX_CHECKBOX_GAP;
	FluxRect               text_rect = {
	  bounds->x + FLUX_CHECKBOX_SIZE + gap, bounds->y, flux_maxf(0.0f, bounds->w - FLUX_CHECKBOX_SIZE - gap),
	  bounds->h};
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

void flux_draw_checkbox(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t              = rc->theme;

	float                  box_size       = FLUX_CHECKBOX_SIZE;
	float                  box_radius     = FLUX_CHECKBOX_CORNER;
	float                  cy             = bounds->y + bounds->h * 0.5f;
	FluxRect               box            = {bounds->x, cy - box_size * 0.5f, box_size, box_size};

	bool                   checked        = (snap->check_state == FLUX_CHECK_CHECKED);
	float                  check_progress = checkbox_update_anim(rc, snap, state, checked);
	CheckboxColors         colors         = checkbox_colors(state, t);

	draw_checkbox_box(rc, &box, &colors, check_progress);
	draw_checkbox_glyph(rc, &box, colors.glyph_color, check_progress);
	draw_checkbox_label(rc, snap, bounds, state);

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &box, box_radius);
}
