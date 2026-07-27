/**
 * @file flux_progress_record.c
 * @brief Status-indicator recorders (ProgressBar / ProgressRing / InfoBadge).
 */
#include "controls/draw/flux_control_record_internal.h"

#include "render/flux_anim.h"

#include <math.h>
#include <stdio.h>

static float progress_unit(float value) {
	if (value <= 0.0f) return 0.0f;
	if (value >= 1.0f) return 1.0f;
	return value;
}

static bool progress_indeterminate(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, FluxColor color) {
	double phase    = rc && rc->now > 0.0 ? fmod(rc->now, 2.0) : 0.0;
	float  t1       = progress_unit(( float ) (phase / 1.5));
	float  t2       = progress_unit(( float ) ((phase - 0.75) / 1.25));
	t1              = flux_cubic_bezier(t1, 0.4f, 0.0f, 0.6f, 1.0f);
	t2              = flux_cubic_bezier(t2, 0.4f, 0.0f, 0.6f, 1.0f);

	float    y      = bounds->y + (bounds->h - 3.0f) * 0.5f;
	FluxRect first  = {bounds->x + bounds->w * (-0.4f + 1.6f * t1), y, bounds->w * 0.4f, 3.0f};
	FluxRect second = {bounds->x + bounds->w * (-0.9f + 1.896f * t2), y, bounds->w * 0.6f, 3.0f};
	if (!xent_dl_pushclip_rect(b, flux_rec_xrect(bounds))) return false;
	bool ok = flux_rec_fill_rounded(b, &first, 1.5f, color);
	ok      = ok && flux_rec_fill_rounded(b, &second, 1.5f, color);
	ok      = xent_dl_popclip(b) && ok;
	if (rc && rc->animations_active) *rc->animations_active = true;
	return ok;
}

bool flux_rec_build_progress(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (snap->u.progress.indeterminate) return progress_indeterminate(b, rc, bounds, t->accent_default);

	float    cy    = bounds->y + bounds->h * 0.5f;
	FluxRect track = {bounds->x, cy - 0.5f, bounds->w, 1.0f};
	if (!flux_rec_fill_rounded(b, &track, 0.5f, t->ctrl_alt_fill_tertiary)) return false;

	float range = snap->u.progress.max_value - snap->u.progress.min_value;
	float pct   = range > 0.0f ? (snap->u.progress.current_value - snap->u.progress.min_value) / range : 0.0f;
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 1.0f) pct = 1.0f;
	if (pct <= 0.0f) return true;
	FluxRect fill = {bounds->x, cy - 1.5f, bounds->w * pct, 3.0f};
	return flux_rec_fill_rounded(b, &fill, 1.5f, t->accent_default);
}

static bool
progress_ring_angles(FluxRenderCtx const *rc, FluxRenderSnap const *snap, float *start_rad, float *sweep_rad) {
	float range = snap->u.progress.max_value - snap->u.progress.min_value;
	if (!snap->u.progress.indeterminate && range > 0.0f) {
		float pct = (snap->u.progress.current_value - snap->u.progress.min_value) / range;
		if (pct < 0.0f) pct = 0.0f;
		if (pct > 1.0f) pct = 1.0f;
		if (pct < 0.001f) return false;

		*start_rad = 0.0f;
		*sweep_rad = pct * 2.0f * 3.14159265f;
		return true;
	}

	double now       = rc ? rc->now : 0.0;
	float  rotation  = ( float ) fmod(now * 2.0 * 3.14159265, 2.0 * 3.14159265);
	float  t_osc     = ( float ) (sin(now * 2.5) * 0.5 + 0.5);
	float  min_sweep = 30.0f * (3.14159265f / 180.0f);
	float  max_sweep = 270.0f * (3.14159265f / 180.0f);

	*start_rad       = rotation;
	*sweep_rad       = min_sweep + t_osc * (max_sweep - min_sweep);
	if (rc && rc->animations_active) *rc->animations_active = true;
	return true;
}

bool flux_rec_progressring(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	float size     = bounds->w < bounds->h ? bounds->w : bounds->h;
	float stroke_w = 4.0f;
	float radius   = (size - stroke_w) * 0.5f;
	if (radius < 2.0f) return true;

	float                  cx           = bounds->x + bounds->w * 0.5f;
	float                  cy           = bounds->y + bounds->h * 0.5f;
	FluxThemeColors const *t            = flux_rec_theme(rc);
	FluxColor              track_color  = t->ctrl_strong_fill_default;
	FluxColor              accent_color = t->accent_default;
	FluxRect               ring         = {cx - radius, cy - radius, radius * 2.0f, radius * 2.0f};

	bool                   ok
	  = xent_dl_strokeellipse(b, flux_rec_xrect(&ring), flux_rec_xcolor(track_color), stroke_w, XENT_STROKE_CAP_FLAT);

	float start_rad = 0.0f;
	float sweep_rad = 0.0f;
	if (!progress_ring_angles(rc, snap, &start_rad, &sweep_rad)) return ok;

	float pi2 = 2.0f * 3.14159265f;
	if (sweep_rad > pi2 - 0.001f) sweep_rad = pi2 - 0.001f;

	/* ProgressRing: 0° = top (12 o'clock), clockwise — matches HEAD path and WinUI. */
	float start_deg = start_rad * (180.0f / 3.14159265f) - 90.0f;
	float sweep_deg = sweep_rad * (180.0f / 3.14159265f);
	if (sweep_deg <= 0.0f) return ok;
	ok = ok && flux_rec_stroke_arc(b, &ring, start_deg, sweep_deg, accent_color, stroke_w, XENT_STROKE_CAP_ROUND);
	return ok;
}

static bool badge_number(XentDlBuilder *b, FluxRect const *bounds, int value, FluxColor accent, FluxColor text) {
	if (value < 1) return true;
	char num [16];
	snprintf(num, sizeof(num), "%d", value);
	float    badge_h = 16.0f;
	float    badge_w = bounds->w > badge_h ? bounds->w : badge_h;
	float    bx      = bounds->x + (bounds->w - badge_w) * 0.5f;
	float    by      = bounds->y + (bounds->h - badge_h) * 0.5f;
	FluxRect badge   = {bx, by, badge_w, badge_h};
	if (!flux_rec_fill_rounded(b, &badge, badge_h * 0.5f, accent)) return false;
	FluxRect      text_box = {badge.x + 4.0f, badge.y, badge.w - 8.0f, badge.h - 2.0f};
	XentTextStyle style    = {.font_size = 11.0f, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, &text_box, num, &style, text);
}

static bool
badge_icon(XentDlBuilder *b, FluxRect const *bounds, char const *icon_name, FluxColor accent, FluxColor text) {
	float d  = 16.0f;
	float cx = bounds->x + bounds->w * 0.5f;
	float cy = bounds->y + bounds->h * 0.5f;
	if (!flux_rec_fill_circle(b, cx, cy, d, accent)) return false;
	if (!icon_name || !icon_name [0]) return true;
	wchar_t const *icon = flux_icon_lookup(icon_name);
	if (!icon) return true;
	FluxRect ir = {cx - d * 0.5f, cy - d * 0.5f, d, d};
	return flux_rec_glyph_wc(b, &ir, icon, 10.0f, text);
}

bool flux_rec_badge(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	FluxColor              accent = t->accent_default;
	FluxColor              text   = t->text_on_accent_primary;

	switch (snap->u.info_badge.mode) {
	case FLUX_BADGE_DOT :
		return flux_rec_fill_circle(b, bounds->x + bounds->w * 0.5f, bounds->y + bounds->h * 0.5f, 8.0f, accent);
	case FLUX_BADGE_NUMBER : return badge_number(b, bounds, snap->u.info_badge.value, accent, text);
	case FLUX_BADGE_ICON   : return badge_icon(b, bounds, snap->u.info_badge.icon_name, accent, text);
	default                : return true;
	}
}
