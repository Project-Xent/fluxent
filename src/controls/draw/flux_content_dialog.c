#include "render/flux_fluent.h"

/* ContentDialogSmokeFill = SmokeFillColorDefault (#4D000000): a ~30% black scrim
 * over the whole window. The centered card and its children are real nodes drawn
 * by their own renderers on top of this. */
#define FLUX_DIALOG_SCRIM_ALPHA 0x4d
#define FLUX_DIALOG_CORNER      8.0f /* OverlayCornerRadius */

void flux_draw_content_dialog(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	flux_fill_rect(rc, bounds, flux_color_rgba(0, 0, 0, FLUX_DIALOG_SCRIM_ALPHA));
}

/* The content region (title + body) sits on ContentDialogTopOverlay (LayerFillColorAlt),
 * a hair lighter than the dialog's SolidBackgroundFillColorBase command bar below it,
 * with a 1px ContentDialogSeparator (CardStrokeColorDefault) hairline between the two
 * (ContentDialogSeparatorThickness 0,0,0,1). Top corners follow the card's
 * OverlayCornerRadius; the bottom edge is square so it meets the command bar flush. */
void flux_draw_dialog_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FluxThemeColors const *t    = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               cb   = flux_snap_bounds(bounds, 1.0f, 1.0f);

	/* layer_alt fill with only the top corners rounded: draw a rounded rect extended
	 * past the bottom edge and clip it back so the bottom corners come out square. */
	D2D1_RECT_F            clip = flux_d2d_rect(&cb);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	FluxRect ext = {cb.x, cb.y, cb.w, cb.h + FLUX_DIALOG_CORNER};
	flux_fill_rounded_rect(rc, &ext, FLUX_DIALOG_CORNER, t->layer_alt);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));

	float sy = cb.y + cb.h - FLUX_STROKE_WIDTH * 0.5f;
	flux_draw_line(rc, &(FluxLineSpec) {cb.x, sy, cb.x + cb.w, sy}, t->card_stroke_default, FLUX_STROKE_WIDTH);
}
