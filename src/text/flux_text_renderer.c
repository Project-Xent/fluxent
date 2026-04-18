#include "flux_render_internal.h"
#include <string.h>

void flux_draw_text(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;

	/* 1. Background + border */
	flux_fill_background(rc, snap, bounds);
	flux_stroke_border(rc, snap, bounds);

	/* 2. Text content */
	if (!rc->text) return;

	char const *content = snap->text_content;
	if (!content || !content [0]) return;

	FluxThemeColors const *t = rc->theme;

	/* Text color: user override or theme primary */
	FluxColor              text_color;
	if (flux_color_af(snap->text_color) > 0.0f) text_color = snap->text_color;
	else text_color = t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xe4);

	/* Disabled text */
	if (!state->enabled) text_color = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c);

	float         font_size = snap->font_size > 0.0f ? snap->font_size : 14.0f;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = snap->font_family;
	ts.font_size   = font_size;
	ts.font_weight = snap->font_weight > 0 ? snap->font_weight : FLUX_FONT_REGULAR;
	ts.text_align  = snap->text_alignment;
	ts.vert_align  = snap->text_vert_alignment;
	ts.color       = text_color;
	ts.word_wrap   = snap->word_wrap;

	flux_text_draw(rc->text, ( ID2D1RenderTarget * ) rc->d2d, content, bounds, &ts);
}
