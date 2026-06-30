#include "render/flux_fluent.h"
#include "controls/draw/flux_button_internal.h"
#include "controls/draw/flux_control_draw.h"

/* WinUI Expander renderers (Expander.xaml / Expander_themeresources.xaml).
 *
 * The control is three nodes mirroring the XAML tree: a layout-only root grid,
 * an EXPANDER_HEADER (the ToggleButton) drawn here, and an EXPANDER_CONTENT
 * panel drawn here. The header background (CardBackgroundFillColorDefault) and
 * border (CardStrokeColorDefault) are constant across states — only the 32x32
 * chevron box's subtle fill and the chevron rotation/glyph change on hover /
 * press / expand, exactly as in the ExpanderHeaderDownStyle CommonStates.
 *
 * When expanded the header keeps its top corners rounded and its bottom edge
 * square (TopCornerRadiusFilter), leaving a 1px seam; the content panel rounds
 * only its bottom corners (BottomCornerRadiusFilter) with a 1,0,1,1 border so
 * the header's bottom edge is the only divider between them. */
#define EXP_GLYPH_DOWN L"\xE70D" /* ExpanderChevronDownGlyph (collapsed) */
#define EXP_GLYPH_UP   L"\xE70E" /* ExpanderChevronUpGlyph (expanded) */

static FluxColor exp_content_bg(bool dark) {
	/* CardBackgroundFillColorSecondary. */
	return dark ? flux_color_rgba(255, 255, 255, 0x08) : flux_color_rgba(0xf6, 0xf6, 0xf6, 0x80);
}

/* Fill @p r with one corner pair rounded and the opposite pair square, by
 * extending the rounded rect past the square edge and clipping it back. */
static void
exp_fill_split(FluxRenderContext const *rc, FluxRect const *r, float radius, FluxColor fill, bool round_top) {
	D2D1_RECT_F clip = flux_d2d_rect(r);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	FluxRect ext = round_top ? (FluxRect) {r->x, r->y, r->w, r->h + radius}
	                         : (FluxRect) {r->x, r->y - radius, r->w, r->h + radius};
	flux_fill_rounded_rect(rc, &ext, radius, fill);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

/* Stroke @p r with one corner pair rounded; the far (square) edge is clipped
 * away, so callers draw it explicitly when a straight edge is wanted there. */
static void
exp_stroke_split(FluxRenderContext const *rc, FluxRect const *r, float radius, FluxColor color, bool round_top) {
	float       half = FLUX_STROKE_WIDTH * 0.5f;
	D2D1_RECT_F clip = flux_d2d_rect(r);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	FluxRect ext = round_top ? (FluxRect) {r->x + half, r->y + half, r->w - half * 2.0f, r->h - half + radius}
	                         : (FluxRect) {r->x + half, r->y - radius, r->w - half * 2.0f, r->h - half + radius};
	flux_draw_rounded_rect(rc, &ext, radius, color, FLUX_STROKE_WIDTH);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

void flux_draw_expander_header(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxRect               hb       = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float                  radius   = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxThemeColors const *t        = rc->theme ? rc->theme : flux_theme_default_colors();
	bool                   expanded = snap->u.expander.is_checked;

	/* Header background + border are state-invariant CardBackgroundFillColorDefault
	 * / CardStrokeColorDefault. Expanded: top-rounded, square bottom + seam line. */
	if (expanded) {
		exp_fill_split(rc, &hb, radius, t->card_bg_default, true);
		exp_stroke_split(rc, &hb, radius, t->card_stroke_default, true);
		float by = hb.y + hb.h - FLUX_STROKE_WIDTH * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {hb.x, by, hb.x + hb.w, by}, t->card_stroke_default, FLUX_STROKE_WIDTH);
	}
	else {
		float    half   = FLUX_STROKE_WIDTH * 0.5f;
		FluxRect stroke = {hb.x + half, hb.y + half, hb.w - half * 2.0f, hb.h - half * 2.0f};
		flux_fill_rounded_rect(rc, &hb, radius, t->card_bg_default);
		flux_draw_rounded_rect(rc, &stroke, radius, t->card_stroke_default, FLUX_STROKE_WIDTH);
	}

	/* Chevron box: 32x32, right margin 8, vertically centered. Only its subtle
	 * fill reacts to pointer state (ExpanderChevron[PointerOver|Pressed]Background). */
	FluxRect chev = {
	  hb.x + hb.w - FLUX_EXPANDER_CHEVRON_MR - FLUX_EXPANDER_CHEVRON_BOX,
	  hb.y + (hb.h - FLUX_EXPANDER_CHEVRON_BOX) * 0.5f, FLUX_EXPANDER_CHEVRON_BOX, FLUX_EXPANDER_CHEVRON_BOX};
	if (state->enabled && state->pressed)
		flux_fill_rounded_rect(rc, &chev, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	else if (state->enabled && state->hovered)
		flux_fill_rounded_rect(rc, &chev, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);

	FluxColor fg = state->enabled ? t->text_primary : t->text_disabled;
	/* ExpanderChevronGlyphSize 12, centered in the 32x32 box (FLUX_CHEVRON_FONT). */
	flux_button_draw_chevron(rc, &chev, expanded ? EXP_GLYPH_UP : EXP_GLYPH_DOWN, FLUX_CHEVRON_FONT, fg);

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &hb, radius);
}

void flux_draw_expander_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxRect               cb     = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxThemeColors const *t      = rc->theme ? rc->theme : flux_theme_default_colors();

	/* CardBackgroundFillColorSecondary fill, bottom corners rounded; the 1,0,1,1
	 * border (left/right/bottom, no top) falls out of the split stroke because its
	 * top edge is clipped away — the header's bottom border is the seam. */
	exp_fill_split(rc, &cb, radius, exp_content_bg(rc->is_dark), false);
	exp_stroke_split(rc, &cb, radius, t->card_stroke_default, false);
}
