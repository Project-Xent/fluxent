/**
 * @file flux_control_record_helpers.c
 * @brief Shared drawing primitives for the Flux MAIN paint → XentDl
 * recorders: theme/color/rect conversion, DPI snapping, fill/stroke/text/glyph
 * builders. Control-specific recorders live in their own *_record.c files.
 */
#include "controls/draw/flux_control_record_internal.h"

#include <math.h>

FluxThemeColors const *flux_rec_theme(FluxRenderCtx const *rc) {
	return rc && rc->theme ? rc->theme : flux_theme_defaults();
}

XentColor flux_rec_xcolor(FluxColor c) {
	return xent_color_rgba(
	  ( uint8_t ) ((c.rgba >> 24) & 0xffu), ( uint8_t ) ((c.rgba >> 16) & 0xffu), ( uint8_t ) ((c.rgba >> 8) & 0xffu),
	  ( uint8_t ) (c.rgba & 0xffu)
	);
}

XentRect     flux_rec_xrect(FluxRect const *r) { return (XentRect) {r->x, r->y, r->w, r->h}; }

/* WinUI UseLayoutRounding aligns edges to device pixels so fractional-DPI
 * hairlines stay sharp. */
static float rec_dpi_scale_x(FluxRenderCtx const *rc) {
	float sx = rc && rc->dpi.dpi_x > 0.0f ? rc->dpi.dpi_x / FLUX_DPI_BASE : 1.0f;
	return sx > 0.0f ? sx : 1.0f;
}

static float rec_dpi_scale_y(FluxRenderCtx const *rc) {
	float sy = rc && rc->dpi.dpi_y > 0.0f ? rc->dpi.dpi_y / FLUX_DPI_BASE : 1.0f;
	return sy > 0.0f ? sy : 1.0f;
}

FluxRect flux_rec_snap_bounds(FluxRenderCtx const *rc, FluxRect const *bounds) {
	if (!bounds) return (FluxRect) {0};
	return flux_snap_bounds(bounds, rec_dpi_scale_x(rc), rec_dpi_scale_y(rc));
}

float flux_rec_snapx(FluxRenderCtx const *rc, float x) {
	float sx = rec_dpi_scale_x(rc);
	return (floorf(x * sx) + 0.5f) / sx;
}

float flux_rec_snapy(FluxRenderCtx const *rc, float y) {
	float sy = rec_dpi_scale_y(rc);
	return (floorf(y * sy) + 0.5f) / sy;
}

XentDl *flux_rec_finish(XentDlBuilder *b, bool ok) {
	if (!ok) {
		xent_dl_abort(b);
		return NULL;
	}
	return xent_dl_end(b);
}

bool flux_rec_fill_rect(XentDlBuilder *b, FluxRect const *r, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	return xent_dl_fillrect(b, flux_rec_xrect(r), flux_rec_xcolor(c));
}

bool flux_rec_fill_rounded(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	return xent_dl_fillrrect(b, flux_rec_xrect(r), radius, flux_rec_xcolor(c));
}

bool flux_rec_stroke_rounded(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c, float w) {
	if (w <= 0.0f || flux_color_af(c) <= 0.0f) return true;
	float    half  = w * 0.5f;
	FluxRect inset = {r->x + half, r->y + half, r->w - half * 2.0f, r->h - half * 2.0f};
	return xent_dl_strokerrect(b, flux_rec_xrect(&inset), radius, flux_rec_xcolor(c), w);
}

bool flux_rec_fill_circle(XentDlBuilder *b, float cx, float cy, float d, FluxColor c) {
	if (d <= 0.0f || flux_color_af(c) <= 0.0f) return true;
	float    r   = d * 0.5f;
	XentRect box = {cx - r, cy - r, d, d};
	return xent_dl_fillellipse(b, box, flux_rec_xcolor(c));
}

bool flux_rec_stroke_circle(XentDlBuilder *b, float cx, float cy, float d, FluxColor c, float w) {
	if (d <= 0.0f || w <= 0.0f || flux_color_af(c) <= 0.0f) return true;
	float    r   = d * 0.5f;
	XentRect box = {cx - r, cy - r, d, d};
	return xent_dl_strokeellipse(b, box, flux_rec_xcolor(c), w, XENT_STROKE_CAP_FLAT);
}

bool flux_rec_stroke_arc(
  XentDlBuilder *b, FluxRect const *bounds, float start_deg, float sweep_deg, FluxColor c, float w, XentStrokeCap cap
) {
	if (w <= 0.0f || sweep_deg == 0.0f || flux_color_af(c) <= 0.0f) return true;
	return xent_dl_strokearc(b, flux_rec_xrect(bounds), start_deg, sweep_deg, flux_rec_xcolor(c), w, cap);
}

bool flux_rec_line_h(XentDlBuilder *b, float x0, float x1, float y, float h, FluxColor c) {
	if (x1 <= x0 || h <= 0.0f || flux_color_af(c) <= 0.0f) return true;
	float mid = y + h * 0.5f;
	return xent_dl_strokeline(b, x0, mid, x1, mid, flux_rec_xcolor(c), h, XENT_STROKE_CAP_FLAT);
}

bool flux_rec_linehs(XentDlBuilder *b, FluxRenderCtx const *rc, float x0, float x1, float y, float h, FluxColor c) {
	return flux_rec_line_h(b, x0, x1, flux_rec_snapy(rc, y), h, c);
}

bool flux_rec_strokev(XentDlBuilder *b, float x, float y0, float y1, float w, FluxColor c) {
	if (y1 <= y0 || w <= 0.0f || flux_color_af(c) <= 0.0f) return true;
	return xent_dl_strokeline(b, x, y0, x, y1, flux_rec_xcolor(c), w, XENT_STROKE_CAP_FLAT);
}

bool flux_rec_strokevs(XentDlBuilder *b, FluxRenderCtx const *rc, float x, float y0, float y1, float w, FluxColor c) {
	return flux_rec_strokev(b, flux_rec_snapx(rc, x), y0, y1, w, c);
}

bool flux_rec_fill_shape(XentDlBuilder *b, XentResourceId shape, FluxRect const *r, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	return xent_dl_fillshape(b, shape, flux_rec_xrect(r), flux_rec_xcolor(c));
}

bool flux_rec_text(XentDlBuilder *b, FluxRect const *r, char const *txt, float size, uint16_t weight, FluxColor c) {
	/* Fluent control labels default to left + vertical center (DWRITE_VCENTER).
	 * xent_dl_drawtext leaves v_align=0 (top), which looks vertically biased. */
	XentTextStyle style = {.font_size = size, .font_weight = weight, .h_align = 0u, .v_align = 1u};
	return flux_rec_text_ex(b, r, txt, &style, c);
}

bool flux_rec_text_ex(XentDlBuilder *b, FluxRect const *r, char const *txt, XentTextStyle const *style, FluxColor c) {
	if (!txt || !txt [0] || !style) return true;
	if (flux_color_af(c) <= 0.0f) return true;
	return xent_dl_drawtext_style(b, flux_rec_xrect(r), txt, style, flux_rec_xcolor(c));
}

bool flux_rec_glyph_wc(XentDlBuilder *b, FluxRect const *r, wchar_t const *wc, float size, FluxColor c) {
	if (!wc || !wc [0]) return true;
	char utf8 [8];
	flux_icon_to_utf8(wc, utf8, sizeof(utf8));
	/* Embed the icon font name in the list — replay must not depend only on the flag. */
	XentTextStyle style = {
	  .font_size   = size,
	  .font_weight = 400u,
	  .h_align     = 1u,
	  .v_align     = 1u,
	  .flags       = XENT_DL_TEXT_ICON,
	  .font_family = flux_icon_font_family(),
	};
	return flux_rec_text_ex(b, r, utf8, &style, c);
}

bool flux_rec_fill_triangle(XentDlBuilder *b, float x0, float y0, float x1, float y1, float x2, float y2, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	return xent_dl_filltri(b, x0, y0, x1, y1, x2, y2, flux_rec_xcolor(c));
}

bool flux_rec_filltop(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	bool ok = xent_dl_pushclip_rect(b, flux_rec_xrect(r));
	if (!ok) return false;
	FluxRect ext = {r->x, r->y, r->w, r->h + radius};
	ok           = flux_rec_fill_rounded(b, &ext, radius, c);
	return xent_dl_popclip(b) && ok;
}

bool flux_rec_fillbot(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c) {
	if (flux_color_af(c) <= 0.0f) return true;
	bool ok = xent_dl_pushclip_rect(b, flux_rec_xrect(r));
	if (!ok) return false;
	FluxRect ext = {r->x, r->y - radius, r->w, r->h + radius};
	ok           = flux_rec_fill_rounded(b, &ext, radius, c);
	return xent_dl_popclip(b) && ok;
}

bool flux_rec_fill_bg(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds) {
	if (flux_color_af(snap->background) <= 0.0f) return true;
	if (snap->corner_radius > 0.0f) return flux_rec_fill_rounded(b, bounds, snap->corner_radius, snap->background);
	return flux_rec_fill_rect(b, bounds, snap->background);
}

bool flux_rec_stroke_border(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds) {
	if (snap->border_width <= 0.0f || flux_color_af(snap->border_color) <= 0.0f) return true;
	return flux_rec_stroke_rounded(b, bounds, snap->corner_radius, snap->border_color, snap->border_width);
}
