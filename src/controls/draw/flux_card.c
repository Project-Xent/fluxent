#include "render/flux_render_internal.h"

void flux_draw_card(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t      = rc->theme;

	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : 8.0f;
	FluxColor              bg     = snap->background;
	if (bg.rgba == 0) bg = t ? t->card_bg_default : flux_color_rgba(255, 255, 255, 230);

	FluxColor stroke = t ? t->card_stroke_default : flux_color_rgba(0, 0, 0, 20);

	flux_fill_rounded_rect(rc, bounds, radius, bg);
	flux_draw_rounded_rect(rc, bounds, radius, stroke, 1.0f);
}
