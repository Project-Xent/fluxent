#include "render/flux_render_internal.h"

void flux_draw_divider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FluxThemeColors const *t     = rc->theme;
	FluxColor              color = t ? t->divider_stroke_default : flux_color_rgba(0, 0, 0, 30);
	if (bounds->w > bounds->h) {
		float y = bounds->y + bounds->h * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {bounds->x, y, bounds->x + bounds->w, y}, color, 1.0f);
	}
	else {
		float x = bounds->x + bounds->w * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {x, bounds->y, x, bounds->y + bounds->h}, color, 1.0f);
	}
}
