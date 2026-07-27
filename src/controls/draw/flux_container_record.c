/**
 * @file flux_container_record.c
 * @brief Container chrome recorders (Expander / ContentDialog content region /
 * SplitView pane / RefreshContainer overlay).
 */
#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/behavior/flux_refresh_internal.h"

#include <math.h>

static bool expander_header_chrome(
  XentDlBuilder *b, FluxRect const *bounds, float radius, bool expanded, FluxThemeColors const *t
) {
	bool ok = expanded ? flux_rec_filltop(b, bounds, radius, t->card_bg_default)
	                   : flux_rec_fill_rounded(b, bounds, radius, t->card_bg_default);
	if (!ok) return false;
	if (!flux_rec_stroke_rounded(b, bounds, radius, t->card_stroke_default, FLUX_STROKE_WIDTH)) return false;
	if (!expanded) return true;
	return flux_rec_line_h(
	  b, bounds->x, bounds->x + bounds->w, bounds->y + bounds->h - 0.5f, 1.0f, t->card_stroke_default
	);
}

static FluxColor subtle_press_hover(FluxControlState const *state, FluxThemeColors const *t, bool *draw) {
	*draw = state->enabled && (state->pressed || state->hovered);
	if (!*draw) return flux_color_rgba(0, 0, 0, 0);
	return state->pressed ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
}

static bool expander_chevron(
  XentDlBuilder *b, FluxRect const *bounds, bool expanded, FluxControlState const *state, FluxThemeColors const *t
) {
	FluxRect chev = {
	  bounds->x + bounds->w - FLUX_EXPANDER_CHEVRON_MR - FLUX_EXPANDER_CHEVRON_BOX,
	  bounds->y + (bounds->h - FLUX_EXPANDER_CHEVRON_BOX) * 0.5f, FLUX_EXPANDER_CHEVRON_BOX, FLUX_EXPANDER_CHEVRON_BOX};
	bool      paint = false;
	FluxColor fill  = subtle_press_hover(state, t, &paint);
	if (paint && !flux_rec_fill_rounded(b, &chev, FLUX_CORNER_RADIUS, fill)) return false;
	return flux_rec_glyph_wc(
	  b, &chev, expanded ? L"\xE70E" : L"\xE70D", 12.0f, state->enabled ? t->text_primary : t->text_disabled
	);
}

bool flux_rec_exphead(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t        = flux_rec_theme(rc);
	float                  radius   = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   expanded = snap->u.expander.is_checked;
	if (!expander_header_chrome(b, bounds, radius, expanded, t)) return false;
	return expander_chevron(b, bounds, expanded, state, t);
}

bool flux_rec_expbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxColor bg = rc && rc->is_dark ? flux_color_rgba(255, 255, 255, 0x08) : flux_color_rgba(0xf6, 0xf6, 0xf6, 0x80);
	bool      ok = flux_rec_fillbot(b, bounds, radius, bg);
	if (ok) ok = flux_rec_stroke_rounded(b, bounds, radius, t->card_stroke_default, FLUX_STROKE_WIDTH);
	return ok;
}

bool flux_rec_dialogbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds) {
	FluxRect               sb = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t  = flux_rec_theme(rc);
	bool                   ok = flux_rec_filltop(b, &sb, 8.0f, t->layer_alt);
	if (ok) ok = flux_rec_linehs(b, rc, sb.x, sb.x + sb.w, sb.y + sb.h - 0.5f, 1.0f, t->card_stroke_default);
	return ok;
}

bool flux_rec_splitpane(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	if (bounds->w <= 0.5f) return true;
	FluxThemeColors const       *t     = flux_rec_theme(rc);
	FluxSplitPaneSnapshot const *p     = &snap->u.split_pane;
	bool                         right = p->placement == FLUX_SPLITVIEW_RIGHT;

	bool                         ok;
	if (p->overlay) {
		ok = flux_rec_fill_rect(b, bounds, t->solid_background);
		if (ok) {
			float          ex     = right ? bounds->x : bounds->x + bounds->w;
			FluxColor      s0     = flux_color_rgba(0, 0, 0, 0x40);
			FluxColor      s1     = flux_color_rgba(0, 0, 0, 0);
			FluxRect       shadow = right ? (FluxRect) {ex - 8.0f, bounds->y, 8.0f, bounds->h}
			                              : (FluxRect) {ex, bounds->y, 8.0f, bounds->h};
			XentRect       xr     = flux_rec_xrect(&shadow);
			XentLinearGrad grad   = {
			  shadow.x, shadow.y, shadow.x + shadow.w, shadow.y, 0.0f, 1.0f, flux_rec_xcolor(right ? s1 : s0),
			  flux_rec_xcolor(right ? s0 : s1)};
			ok = xent_dl_fillrect_grad(b, xr, &grad);
		}
	}
	else { ok = flux_rec_fill_rect(b, bounds, t->layer_default); }

	if (ok && p->divider) {
		float x = right ? bounds->x + 0.5f : bounds->x + bounds->w - 0.5f;
		ok      = flux_rec_strokevs(b, rc, x, bounds->y, bounds->y + bounds->h, 1.0f, t->divider_stroke_default);
	}
	return ok;
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
	FluxColor              c  = t->text_primary;
	uint8_t                a  = ( uint8_t ) (( float ) (c.rgba & 0xffu) * flux_clamp01(r->glyph_opacity));
	FluxColor              gc = (FluxColor) {(c.rgba & 0xffffff00u) | a};

	bool                   ok = xent_dl_pushtransform(b, refresh_glyph_affine(r->glyph_angle, r->glyph_scale, cx, cy));
	float                  half = FLUX_REFRESH_INDICATOR_SIZE * 0.5f;
	FluxRect               box  = {cx - half, cy - half, FLUX_REFRESH_INDICATOR_SIZE, FLUX_REFRESH_INDICATOR_SIZE};
	ok                          = ok && flux_rec_glyph_wc(b, &box, wc, 20.0f, gc);
	ok                          = ok && xent_dl_poptransform(b);
	return ok;
}
