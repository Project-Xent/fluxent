/**
 * @file flux_person_picture.c
 * @brief PersonPicture renderer: a bordered circle showing a photo (as a
 * circular image brush), the person's initials, or a placeholder glyph
 * (Contact / People), with an optional accent badge at the top-right.
 *
 * Colors follow PersonPicture_themeresources: ControlAltFillColorQuarternary
 * fill, CardStrokeColorDefault border, TextFillColorPrimary foreground; the
 * badge uses AccentFillColorDefault with TextOnAccentFillColorPrimary. Initials
 * render at 42% of the diameter (SemiBold); the badge plate is 50% of the
 * diameter with its text/glyph at 60% of the plate.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "render/flux_image_cache.h"
#include "render/flux_render_internal.h"

#include "fluxent/controls/flux_person_picture_data.h"

#include <stdio.h>

/* Draw the profile photo clipped to the circle via a UniformToFill bitmap
 * brush filling the ellipse. */
static bool person_draw_photo(FluxRenderContext const *rc, FluxRect const *box, char const *path, float cx, float cy, float r) {
	if (!path || !rc->d2d) return false;
	float nw = 0.0f, nh = 0.0f;
	void *bmp = flux_image_cache_acquire(rc->d2d, path, &nw, &nh);
	if (!bmp || nw <= 0.0f || nh <= 0.0f) return false;

	float s  = flux_maxf(box->w / nw, box->h / nh);
	float tx = box->x + (box->w - nw * s) * 0.5f;
	float ty = box->y + (box->h - nh * s) * 0.5f;

	D2D1_BITMAP_BRUSH_PROPERTIES bbp = {
	  D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR};
	D2D1_BRUSH_PROPERTIES bp = {1.0f, {s, 0.0f, 0.0f, s, tx, ty}};

	ID2D1BitmapBrush *brush = NULL;
	ID2D1RenderTarget_CreateBitmapBrush(FLUX_RT(rc), ( ID2D1Bitmap * ) bmp, &bbp, &bp, &brush);
	if (!brush) return false;

	FluxEllipseSpec spec = {cx, cy, r, r};
	D2D1_ELLIPSE    e    = flux_ellipse(&spec);
	ID2D1RenderTarget_FillEllipse(FLUX_RT(rc), &e, ( ID2D1Brush * ) brush);
	ID2D1BitmapBrush_Release(brush);
	return true;
}

static void person_draw_centered_text(
  FluxRenderContext const *rc, FluxRect const *box, char const *utf8, char const *font, float size,
  FluxFontWeight weight, FluxColor color
) {
	if (!rc->text || !utf8 || !utf8 [0]) return;
	FluxTextStyle ts = {0};
	ts.font_family = font;
	ts.font_size   = size;
	ts.font_weight = weight;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = color;
	flux_text_draw(rc->text, FLUX_RT(rc), utf8, box, &ts);
}

static void person_draw_badge(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *box, FluxThemeColors const *t
) {
	char const *glyph = snap->u.person.badge_glyph;
	int         num   = snap->u.person.badge_number;
	if (num <= 0 && (!glyph || !glyph [0])) return;

	float plate = box->w * FLUX_PP_BADGE_RATIO;
	/* Top-right, nudged 4px outward (PersonPictureBadgeGridMargin 0,-4,-4,0). */
	FluxRect pr = {box->x + box->w - plate + 4.0f, box->y - 4.0f, plate, plate};
	float    pcx = pr.x + plate * 0.5f, pcy = pr.y + plate * 0.5f, prd = plate * 0.5f;

	FluxEllipseSpec spec = {pcx, pcy, prd, prd};
	flux_fill_ellipse(rc, &spec, t->accent_default);

	float font = plate * FLUX_PP_BADGE_FONT;
	if (num > 0) {
		char buf [8];
		snprintf(buf, sizeof(buf), num > 99 ? "99+" : "%d", num);
		person_draw_centered_text(rc, &pr, buf, NULL, font, FLUX_FONT_SEMI_BOLD, t->text_on_accent_primary);
	}
	else {
		person_draw_centered_text(rc, &pr, glyph, flux_icon_font_family(), font, FLUX_FONT_REGULAR, t->text_on_accent_primary);
	}
}

/* No photo: initials (single person only), else a group or generic-person
 * placeholder glyph — the WinUI PersonPicture fallback order. */
static void person_draw_fallback(
  FluxRenderContext const *rc, FluxRect const *box, bool is_group, char const *initials, float d,
  FluxThemeColors const *t
) {
	float size = d * FLUX_PP_INITIALS_RATIO;
	if (!is_group && initials && initials [0]) {
		person_draw_centered_text(rc, box, initials, NULL, size, FLUX_FONT_SEMI_BOLD, t->text_primary);
		return;
	}
	char g [8];
	flux_icon_to_utf8(is_group ? L"\xE716" : L"\xE77B", g, sizeof(g));
	person_draw_centered_text(rc, box, g, flux_icon_font_family(), size, FLUX_FONT_REGULAR, t->text_primary);
}

void flux_draw_person_picture(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();

	float d  = flux_minf(bounds->w, bounds->h);
	if (d <= 0.0f) return;
	float cx = bounds->x + bounds->w * 0.5f;
	float cy = bounds->y + bounds->h * 0.5f;
	float r  = d * 0.5f;
	FluxRect box = {cx - r, cy - r, d, d};

	/* Fill + 1px stroke circle (the fill also backs initials/placeholder). */
	FluxEllipseSpec spec = {cx, cy, r, r};
	flux_fill_ellipse(rc, &spec, t->ctrl_alt_fill_quarternary);

	bool has_photo = person_draw_photo(rc, &box, snap->u.person.image_path, cx, cy, r);
	if (!has_photo) person_draw_fallback(rc, &box, snap->u.person.is_group, snap->u.person.initials, d, t);

	{
		FluxEllipseSpec stroke = {cx, cy, r - 0.5f, r - 0.5f};
		flux_stroke_ellipse(rc, &stroke, t->card_stroke_default, 1.0f);
	}

	person_draw_badge(rc, snap, &box, t);
}
