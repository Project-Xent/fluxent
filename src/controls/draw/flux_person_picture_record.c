#include "controls/draw/flux_control_record_internal.h"

#include "controls/data/flux_person_picture_data.h"

#include "render/flux_resource_intern.h"
#include "render/flux_image_cache.h"

#include <stdio.h>

static bool person_rec_photo(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *box, char const *path) {
	if (!path || !path [0]) return false;
	if (rc && rc->d2d) {
		float nw = 0.0f, nh = 0.0f;
		void *bmp = flux_image_cache_acquire(rc->d2d, path, &nw, &nh);
		if (!bmp || nw <= 0.0f || nh <= 0.0f) return false;
	}
	XentRect xbox = flux_rec_xrect(box);
	if (!xent_dl_pushclipellipse(b, xbox)) return false;
	bool ok = xent_dl_drawimg(b, xbox, flux_resource_intern_path(path), XENT_IMAGE_FIT_UNIFORM_TO_FILL);
	return xent_dl_popclip(b) && ok;
}

static void person_rec_fallback(
  XentDlBuilder *b, FluxRect const *box, bool is_group, char const *initials, float d, FluxThemeColors const *t
) {
	float size = d * FLUX_PP_INITIALS_RATIO;
	if (!is_group && initials && initials [0]) {
		XentTextStyle style
		  = {.font_size = size, .font_weight = ( uint16_t ) FLUX_FONT_SEMI_BOLD, .h_align = 1u, .v_align = 1u};
		flux_rec_text_ex(b, box, initials, &style, t->text_primary);
		return;
	}
	wchar_t const *wc = is_group ? L"\xE716" : L"\xE77B";
	flux_rec_glyph_wc(b, box, wc, size, t->text_primary);
}

static bool
person_rec_badge(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *box, FluxThemeColors const *t) {
	char const *glyph = snap->u.person.badge_glyph;
	int         num   = snap->u.person.badge_number;
	if (num <= 0 && (!glyph || !glyph [0])) return true;

	float    plate = box->w * FLUX_PP_BADGE_RATIO;
	FluxRect pr    = {box->x + box->w - plate + 4.0f, box->y - 4.0f, plate, plate};
	float    pcx   = pr.x + plate * 0.5f;
	float    pcy   = pr.y + plate * 0.5f;

	bool     ok    = flux_rec_fill_circle(b, pcx, pcy, plate, t->accent_default);
	float    font  = plate * FLUX_PP_BADGE_FONT;
	if (num > 0) {
		char buf [8];
		snprintf(buf, sizeof(buf), num > 99 ? "99+" : "%d", num);
		XentTextStyle style
		  = {.font_size = font, .font_weight = ( uint16_t ) FLUX_FONT_SEMI_BOLD, .h_align = 1u, .v_align = 1u};
		ok = ok && flux_rec_text_ex(b, &pr, buf, &style, t->text_on_accent_primary);
	}
	else {
		XentTextStyle style = {
		  .font_size   = font,
		  .font_weight = 400u,
		  .h_align     = 1u,
		  .v_align     = 1u,
		  .flags       = XENT_DL_TEXT_ICON,
		  .font_family = flux_icon_font_family(),
		};
		ok = ok && flux_rec_text_ex(b, &pr, glyph, &style, t->text_on_accent_primary);
	}
	return ok;
}

bool flux_rec_avatar(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	float                  d = flux_minf(bounds->w, bounds->h);
	if (d <= 0.0f) return true;

	float    cx        = bounds->x + bounds->w * 0.5f;
	float    cy        = bounds->y + bounds->h * 0.5f;
	float    r         = d * 0.5f;
	FluxRect box       = {cx - r, cy - r, d, d};

	bool     ok        = flux_rec_fill_circle(b, cx, cy, d, t->ctrl_alt_fill_quarternary);
	bool     has_photo = person_rec_photo(b, rc, &box, snap->u.person.image_path);
	if (!has_photo) person_rec_fallback(b, &box, snap->u.person.is_group, snap->u.person.initials, d, t);

	float stroke_d = d - 1.0f;
	ok             = ok && flux_rec_stroke_circle(b, cx, cy, stroke_d, t->card_stroke_default, 1.0f);
	ok             = ok && person_rec_badge(b, snap, &box, t);
	return ok;
}
