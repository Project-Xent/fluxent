#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_person_picture_data.h"

#include "render/flux_resource_intern.h"
#include "render/flux_image.h"

#include <stdio.h>

static bool person_rec_photo(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *box, char const *path) {
	if (!path || !path [0]) return false;
	if (rc && rc->d2d) {
		float nw = 0.0f, nh = 0.0f;
		void *bmp = flux_image_bitmap(rc->d2d, path, &nw, &nh);
		if (!bmp || nw <= 0.0f || nh <= 0.0f) return false;
	}
	XentRect xbox = flux_rec_xrect(box);
	if (!xent_dl_pushclipellipse(b, xbox)) return false;
	bool ok = xent_dl_drawimg(b, xbox, flux_resource_intern_path(path), XENT_IMAGE_FIT_UNIFORM_TO_FILL);
	return xent_dl_popclip(b) && ok;
}

static void
person_rec_fallback(XentDlBuilder *b, FluxRect const *box, bool is_group, char const *initials, FluxColor ink) {
	float size = flux_maxf(1.0f, box->w * FLUX_PP_INITIALS_RATIO);
	if (!is_group && initials && initials [0]) {
		XentTextStyle style
		  = {.font_size = size, .font_weight = ( uint16_t ) FLUX_FONT_SEMI_BOLD, .h_align = 1u, .v_align = 1u};
		flux_rec_text_ex(b, box, initials, &style, ink);
		return;
	}
	wchar_t const *wc = is_group ? L"\xE716" : L"\xE77B";
	flux_rec_glyph_wc(b, box, wc, size, ink);
}

static bool person_rec_badge_text(XentDlBuilder *b, FluxRect const *box, char const *glyph, int num, FluxColor ink) {
	XentTextStyle style = {
	  .font_size   = flux_maxf(1.0f, box->w * FLUX_PP_BADGE_FONT),
	  .font_weight = FLUX_FONT_SEMI_BOLD,
	  .h_align     = 1u,
	  .v_align     = 1u};
	if (num > 0) {
		char buf [8];
		snprintf(buf, sizeof(buf), num > 99 ? "99+" : "%d", num);
		return flux_rec_text_ex(b, box, buf, &style, ink);
	}
	style.flags       = XENT_DL_TEXT_ICON;
	style.font_family = flux_icon_font_family();
	return flux_rec_text_ex(b, box, glyph, &style, ink);
}

static bool person_rec_badge(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxPersonSnapshot const *person, FluxRect const *box,
  FluxThemeColors const *t
) {
	char const *image = person->badge_image_path;
	char const *glyph = person->badge_glyph;
	int         num   = person->badge_number;
	if (!image && (num < 0 || (num == 0 && (!glyph || !glyph [0])))) return true;

	float                     plate = box->w * FLUX_PP_BADGE_RATIO;
	FluxRect                  pr    = {box->x + box->w - plate + 4.0f, box->y - 4.0f, plate, plate};
	float                     pcx   = pr.x + plate * 0.5f;
	float                     pcy   = pr.y + plate * 0.5f;

	FluxContrastColors const *p     = &t->contrast;
	FluxColor                 fill  = p->active ? p->button_face : t->accent_default;
	FluxColor                 ink   = p->active ? p->button_text : t->text_on_accent_primary;
	bool                      ok    = flux_rec_fill_circle(b, pcx, pcy, plate, fill);
	if (p->active) ok = ok && flux_rec_stroke_circle(b, pcx, pcy, plate - 2.0f, p->button_text, 2.0f);
	if (!ok) return false;
	if (!image) return person_rec_badge_text(b, &pr, glyph, num, ink);
	( void ) person_rec_photo(b, rc, &pr, image);
	return true;
}

bool flux_rec_avatar(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	float                  d = flux_minf(bounds->w, bounds->h);
	if (d <= 0.0f) return true;

	float                     cx    = bounds->x + bounds->w * 0.5f;
	float                     cy    = bounds->y + bounds->h * 0.5f;
	float                     r     = d * 0.5f;
	FluxRect                  box   = {cx - r, cy - r, d, d};

	FluxContrastColors const *p     = &t->contrast;
	FluxColor                 fill  = p->active ? p->highlight_text : t->ctrl_alt_fill_quarternary;
	FluxColor                 ink   = p->active ? p->button_text : t->text_primary;
	bool                      ok    = flux_rec_fill_circle(b, cx, cy, d, fill)
	                               && flux_rec_stroke_circle(b, cx, cy, d - 1.0f, t->card_stroke_default, 1.0f);
	bool                      group = snap->u.person.is_group;
	if (group || !person_rec_photo(b, rc, &box, snap->u.person.image_path))
		person_rec_fallback(b, &box, group, snap->u.person.initials, ink);
	return ok && person_rec_badge(b, rc, &snap->u.person, &box, t);
}
