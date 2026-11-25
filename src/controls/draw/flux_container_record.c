#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/behavior/flux_refresh_internal.h"

#include <math.h>

static bool expander_panel_edge(
  XentDlBuilder *b, FluxRect const *bounds, float radius, float width, FluxColor fill, FluxColor stroke, bool top
) {
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(bounds))) return false;
	FluxRect ext  = *bounds;
	ext.h        += radius;
	if (!top) ext.y -= radius;
	bool ok = flux_rec_fill_rounded(b, &ext, radius, fill) && flux_rec_stroke_rounded(b, &ext, radius, stroke, width);
	return xent_dl_popclip(b) && ok;
}

static FluxColor expander_header_border(FluxThemeColors const *t, FluxControlState const *state) {
	if (!t->contrast.active) return t->card_stroke_default;
	if (!state->enabled) return t->contrast.gray_text;
	return state->hovered || state->pressed ? t->contrast.highlight : t->contrast.button_text;
}

static bool expander_header_chrome(
  XentDlBuilder *b, FluxRect const *bounds, float radius, bool expanded, FluxThemeColors const *t,
  FluxControlState const *state
) {
	FluxColor fill   = t->contrast.active ? t->contrast.button_face : t->card_bg_default;
	FluxColor border = expander_header_border(t, state);
	float     width  = t->contrast.active ? 2.0f : FLUX_STROKE_WIDTH;
	if (!expanded)
		return flux_rec_fill_rounded(b, bounds, radius, fill)
		    && flux_rec_stroke_rounded(b, bounds, radius, border, width);
	return expander_panel_edge(b, bounds, radius, width, fill, border, true)
	    && flux_rec_line_h(b, bounds->x, bounds->x + bounds->w, bounds->y + bounds->h - width, width, border);
}

static bool expander_chevron_chrome(
  XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t, FluxColor *fg
) {
	bool hot = state->enabled && (state->pressed || state->hovered);
	if (t->contrast.active) {
		FluxContrastColors const *p = &t->contrast;
		*fg                         = state->enabled ? (hot ? p->highlight : p->button_text) : p->gray_text;
		return (hot || flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, p->button_face))
		    && flux_rec_stroke_rounded(b, box, FLUX_CORNER_RADIUS, *fg, 2.0f);
	}
	*fg            = state->enabled ? t->text_primary : t->text_disabled;
	FluxColor fill = state->pressed ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	return !hot || flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, fill);
}

static bool expander_chevron(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxRect chev = {
	  bounds->x + bounds->w - FLUX_EXPANDER_CHEVRON_MR - FLUX_EXPANDER_CHEVRON_BOX,
	  bounds->y + (bounds->h - FLUX_EXPANDER_CHEVRON_BOX) * 0.5f, FLUX_EXPANDER_CHEVRON_BOX, FLUX_EXPANDER_CHEVRON_BOX};
	FluxColor fg;
	return expander_chevron_chrome(b, &chev, state, flux_rec_theme(rc), &fg)
	    && flux_rec_up_down_chevron(b, rc, snap, &chev, state, snap->u.expander.is_checked, fg);
}

bool flux_rec_exphead(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t        = flux_rec_theme(rc);
	float                  radius   = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   expanded = snap->u.expander.is_checked;
	if (!expander_header_chrome(b, bounds, radius, expanded, t, state)) return false;
	return expander_chevron(b, rc, snap, bounds, state);
}

bool flux_rec_expbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxColor bg = rc && rc->is_dark ? flux_color_rgba(255, 255, 255, 0x08) : flux_color_rgba(0xf6, 0xf6, 0xf6, 0x80);
	if (t->contrast.active)
		return expander_panel_edge(b, bounds, radius, 1.0f, t->contrast.window, t->contrast.window_text, false);
	return expander_panel_edge(b, bounds, radius, 1.0f, bg, t->card_stroke_default, false);
}

bool flux_rec_dialogbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds) {
	FluxRect               sb = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t  = flux_rec_theme(rc);
	bool                   ok = t->contrast.active || flux_rec_filltop(b, &sb, 8.0f, t->layer_alt);
	if (ok) ok = flux_rec_linehs(b, rc, sb.x, sb.x + sb.w, sb.y + sb.h - 0.5f, 1.0f, t->card_stroke_default);
	return ok;
}

static bool splitpane_divider(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, FluxSplitPaneSnapshot const *p, FluxColor color
) {
	if (!p->divider) return true;
	float x = p->placement == FLUX_SPLITVIEW_RIGHT ? bounds->x + 0.5f : bounds->x + bounds->w - 0.5f;
	return flux_rec_strokevs(b, rc, x, bounds->y, bounds->y + bounds->h, 1.0f, color);
}

static bool splitpane_shadow(XentDlBuilder *b, FluxRect const *bounds, bool right) {
	float          ex     = right ? bounds->x : bounds->x + bounds->w;
	FluxColor      inner  = flux_color_rgba(0, 0, 0, 0x40);
	FluxColor      outer  = flux_color_rgba(0, 0, 0, 0);
	FluxRect       shadow = {right ? ex - 8.0f : ex, bounds->y, 8.0f, bounds->h};
	XentLinearGrad grad   = {
	  shadow.x, shadow.y, shadow.x + shadow.w, shadow.y, 0.0f, 1.0f, flux_rec_xcolor(right ? outer : inner),
	  flux_rec_xcolor(right ? inner : outer)};
	return xent_dl_fillrectgrad(b, flux_rec_xrect(&shadow), &grad);
}

bool flux_rec_splitpane(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	if (bounds->w <= 0.5f) return true;
	FluxThemeColors const       *t = flux_rec_theme(rc);
	FluxSplitPaneSnapshot const *p = &snap->u.split_pane;
	if (t->contrast.active)
		return flux_rec_fill_rect(b, bounds, t->contrast.window)
		    && splitpane_divider(b, rc, bounds, p, t->contrast.button_text);
	FluxColor fill = p->overlay ? t->solid_background : t->layer_default;
	return flux_rec_fill_rect(b, bounds, fill)
	    && (!p->overlay || splitpane_shadow(b, bounds, p->placement == FLUX_SPLITVIEW_RIGHT))
	    && splitpane_divider(b, rc, bounds, p, t->divider_stroke_default);
}

bool flux_rec_build_refresh(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds) {
	return flux_rec_fill_bg(b, snap, bounds);
}

static float refresh_reveal_fraction(FluxRefreshSnapshot const *r) {
	if (r->state == FLUX_REFRESH_REFRESHING) return r->threshold_ratio;
	return flux_clamp01(r->interaction_ratio);
}

static void
refresh_glyph_center(FluxRefreshSnapshot const *r, FluxRect const *bounds, float revealed, float *cx, float *cy) {
	float mid_x  = bounds->x + bounds->w * 0.5f;
	float mid_y  = bounds->y + bounds->h * 0.5f;
	float travel = revealed * FLUX_REFRESH_PARALLAX_RATIO;
	switch (r->direction) {
	case FLUX_PULL_TOP_TO_BOTTOM :
		*cx = mid_x;
		*cy = bounds->y + travel;
		break;
	case FLUX_PULL_BOTTOM_TO_TOP :
		*cx = mid_x;
		*cy = bounds->y + bounds->h - travel;
		break;
	case FLUX_PULL_LEFT_TO_RIGHT :
		*cx = bounds->x + travel;
		*cy = mid_y;
		break;
	case FLUX_PULL_RIGHT_TO_LEFT :
		*cx = bounds->x + bounds->w - travel;
		*cy = mid_y;
		break;
	default :
		*cx = mid_x;
		*cy = bounds->y + travel;
		break;
	}
}

static XentAffine2 refresh_glyph_affine(float angle, float scale, float cx, float cy) {
	float c = scale * cosf(angle);
	float s = scale * sinf(angle);
	return (XentAffine2) {c, -s, cx - cx * c + cy * s, s, c, cy - cx * s - cy * c};
}

bool flux_rec_refresh_overlay(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	FluxRefreshSnapshot const *r        = &snap->u.refresh;
	float                      size     = r->visualizer_size > 0.0f ? r->visualizer_size : FLUX_REFRESH_PULL_DIMENSION;
	float                      revealed = refresh_reveal_fraction(r) * size;
	if (revealed <= FLUX_PIXEL_SNAP) return true;

	float cx = 0.0f, cy = 0.0f;
	refresh_glyph_center(r, bounds, revealed, &cx, &cy);

	wchar_t const *wc = flux_icon_lookup("Refresh");
	if (!wc) return true;

	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxColor              c  = t->contrast.active ? t->contrast.highlight_text : t->text_primary;
	uint8_t                a  = ( uint8_t ) (( float ) (c.rgba & 0xffu) * flux_clamp01(r->glyph_opacity));
	FluxColor              gc = (FluxColor) {(c.rgba & 0xffffff00u) | a};

	bool                   ok = xent_dl_pushtransform(b, refresh_glyph_affine(r->glyph_angle, r->glyph_scale, cx, cy));
	float                  half = FLUX_REFRESH_INDICATOR_SIZE * 0.5f;
	FluxRect               box  = {cx - half, cy - half, FLUX_REFRESH_INDICATOR_SIZE, FLUX_REFRESH_INDICATOR_SIZE};
	ok                          = ok && flux_rec_glyph_wc(b, &box, wc, 20.0f, gc);
	ok                          = ok && xent_dl_poptransform(b);
	return ok;
}
