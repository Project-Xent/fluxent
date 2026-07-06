#include "render/flux_render_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "fluxent/flux_text.h"
#include "fluxent/controls/flux_refresh_data.h"

#include <math.h>
#include <string.h>

#ifndef FLUX_PI
  #define FLUX_PI 3.14159265358979323846f
#endif

/* RefreshVisualizer draws exactly one element: the circular-arrow Refresh glyph
 * (U+E72C), 30x30, centered, rotated per state, over a transparent background.
 * "Spinning" is just continuous rotation. There are no per-state brush swaps —
 * only opacity + rotation + a Pending scale pop. The glyph reveals from behind
 * the pull edge (parallax) as the container is over-pulled; the root's inset
 * clip keeps it hidden until then. */

/* The visualizer surface never paints a fill; the glyph floats over content. */
void flux_draw_refresh(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	flux_fill_background(rc, snap, bounds);
}

/* Fraction of the visualizer size currently revealed from behind the pull edge.
 * Refreshing settles at the execution threshold (anim #9 container translation). */
static float refresh_reveal_fraction(FluxRefreshSnapshot const *r) {
	if (r->state == FLUX_REFRESH_REFRESHING) return r->threshold_ratio;
	return flux_clamp01(r->interaction_ratio);
}

/* Glyph center along/across the pull axis: parallax slides it in from the edge. */
static void refresh_glyph_center(
  FluxRefreshSnapshot const *r, FluxRect const *bounds, float revealed, float *cx, float *cy
) {
	float mid_x = bounds->x + bounds->w * 0.5f;
	float mid_y = bounds->y + bounds->h * 0.5f;
	float travel = revealed * FLUX_REFRESH_PARALLAX_RATIO;
	switch (r->direction) {
	case FLUX_PULL_TOP_TO_BOTTOM: *cx = mid_x; *cy = bounds->y + travel; break;
	case FLUX_PULL_BOTTOM_TO_TOP: *cx = mid_x; *cy = bounds->y + bounds->h - travel; break;
	case FLUX_PULL_LEFT_TO_RIGHT: *cx = bounds->x + travel; *cy = mid_y; break;
	case FLUX_PULL_RIGHT_TO_LEFT: *cx = bounds->x + bounds->w - travel; *cy = mid_y; break;
	default:                      *cx = mid_x; *cy = bounds->y + travel; break;
	}
}

/* Rotation (by angle) + uniform scale about (cx,cy), row-vector D2D convention. */
static D2D1_MATRIX_3X2_F refresh_glyph_transform(float angle, float scale, float cx, float cy) {
	float             c = scale * cosf(angle);
	float             s = scale * sinf(angle);
	D2D1_MATRIX_3X2_F m;
	m._11 = c;
	m._12 = s;
	m._21 = -s;
	m._22 = c;
	m._31 = cx - (cx * m._11 + cy * m._21);
	m._32 = cy - (cx * m._12 + cy * m._22);
	return m;
}

static FluxColor refresh_glyph_color(FluxRenderContext const *rc, float opacity) {
	FluxColor c  = rc->theme ? rc->theme->text_primary : ft_text_primary();
	uint8_t   a  = ( uint8_t ) (c.rgba & 0xff);
	uint8_t   na = ( uint8_t ) (( float ) a * flux_clamp01(opacity));
	return (FluxColor) {(c.rgba & 0xffffff00u) | na};
}

static void refresh_draw_glyph(FluxRenderContext const *rc, FluxRefreshSnapshot const *r, float cx, float cy) {
	wchar_t const *wc = flux_icon_lookup("Refresh"); /* U+E72C */
	char           glyph [8];
	if (!flux_icon_to_utf8(wc, glyph, sizeof(glyph))) return;

	ID2D1RenderTarget *rt = FLUX_RT(rc);
	D2D1_MATRIX_3X2_F  prev;
	ID2D1RenderTarget_GetTransform(rt, &prev);
	D2D1_MATRIX_3X2_F  xform = refresh_glyph_transform(r->glyph_angle, r->glyph_scale, cx, cy);
	/* Compose about the existing (scroll/dpi) transform so the glyph rides with it. */
	D2D1_MATRIX_3X2_F  m;
	m._11 = xform._11 * prev._11 + xform._12 * prev._21;
	m._12 = xform._11 * prev._12 + xform._12 * prev._22;
	m._21 = xform._21 * prev._11 + xform._22 * prev._21;
	m._22 = xform._21 * prev._12 + xform._22 * prev._22;
	m._31 = xform._31 * prev._11 + xform._32 * prev._21 + prev._31;
	m._32 = xform._31 * prev._12 + xform._32 * prev._22 + prev._32;
	ID2D1RenderTarget_SetTransform(rt, &m);

	float         half = FLUX_REFRESH_INDICATOR_SIZE * 0.5f;
	FluxRect      box  = {cx - half, cy - half, FLUX_REFRESH_INDICATOR_SIZE, FLUX_REFRESH_INDICATOR_SIZE};
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = flux_icon_font_family();
	ts.font_size   = 20.0f; /* SymbolIcon FontSize within the 30px box */
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = refresh_glyph_color(rc, r->glyph_opacity);
	ts.word_wrap   = false;
	flux_text_draw(rc->text, rt, glyph, &box, &ts);

	ID2D1RenderTarget_SetTransform(rt, &prev);
}

void flux_draw_refresh_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	FluxRefreshSnapshot const *r        = &snap->u.refresh;
	float                      size     = r->visualizer_size > 0.0f ? r->visualizer_size : FLUX_REFRESH_PULL_DIMENSION;
	float                      revealed = refresh_reveal_fraction(r) * size;
	if (revealed <= FLUX_PIXEL_SNAP) return; /* still hidden behind the edge */

	float cx = 0.0f, cy = 0.0f;
	refresh_glyph_center(r, bounds, revealed, &cx, &cy);
	refresh_draw_glyph(rc, r, cx, cy);
}
