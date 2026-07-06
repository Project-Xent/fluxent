/**
 * @file flux_split_view.c
 * @brief SplitView pane-wrapper renderer. Inline panes share the window
 * background with a 1px divider on the content-facing edge; overlay panes draw
 * an opaque floating surface with a soft shadow along that same edge. The
 * SplitView root and content wrapper use the generic container renderer.
 */
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#include "fluxent/controls/flux_split_view_data.h"

void flux_draw_split_pane(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	if (bounds->w <= 0.5f) return; /* collapsed pane */
	FluxThemeColors const       *t = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxSplitPaneSnapshot const *p = &snap->u.split_pane;
	bool                         right = p->placement == FLUX_SPLITVIEW_RIGHT;

	if (p->overlay) {
		/* Opaque floating surface + a soft shadow on the content-facing edge. */
		flux_fill_rect(rc, bounds, t->solid_background);
		float     ex = right ? bounds->x : bounds->x + bounds->w; /* content-facing edge */
		FluxColor s0 = flux_color_rgba(0, 0, 0, 0x40);
		FluxColor s1 = flux_color_rgba(0, 0, 0, 0);
		FluxRect  shadow = right ? (FluxRect) {ex - 8.0f, bounds->y, 8.0f, bounds->h}
		                         : (FluxRect) {ex, bounds->y, 8.0f, bounds->h};
		flux_fill_h_gradient(rc, &shadow, right ? s1 : s0, right ? s0 : s1);
	}
	else {
		flux_fill_rect(rc, bounds, t->layer_default);
	}

	if (p->divider) {
		float x = right ? bounds->x + 0.5f : bounds->x + bounds->w - 0.5f;
		FluxLineSpec line = {x, bounds->y, x, bounds->y + bounds->h};
		flux_draw_line(rc, &line, t->divider_stroke_default, 1.0f);
	}
}
