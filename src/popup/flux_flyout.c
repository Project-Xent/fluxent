/* ══════════════════════════════════════════════════════════════════════════
 *  flux_flyout.c — WinUI 3 FlyoutPresenter, 1:1 replica in C+D2D
 *  ------------------------------------------------------------------------
 *  Source of truth:
 *      fluxent-legacy/dev/CommonStyles/FlyoutPresenter_themeresources.xaml
 *
 *  <Style x:Key="DefaultFlyoutPresenterStyle" TargetType="FlyoutPresenter">
 *      Background      = AcrylicInAppFillColorDefaultBrush
 *      BorderBrush     = SurfaceStrokeColorFlyoutBrush
 *      BorderThickness = 1   (FlyoutBorderThemeThickness)
 *      Padding         = 16,15,16,17   (FlyoutContentPadding)
 *      MinWidth/Height = FlyoutThemeMinWidth / FlyoutThemeMinHeight
 *      CornerRadius    = OverlayCornerRadius  (8)
 *      Template:
 *          <Border ... BackgroundSizing="InnerBorderEdge">
 *              <ScrollViewer><ContentPresenter Margin="{Padding}"/></ScrollViewer>
 *          </Border>
 *
 *  Entrance transition is supplied by the FluxPopup layer (PopupTheme-
 *  Transition: 150 ms fade + 4 DIP Y-translate, ease-out cubic).
 * ══════════════════════════════════════════════════════════════════════════ */

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_flyout.h"
#include "fluxent/flux_graphics.h"
#include "flux_render_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

/* ── Private struct ──────────────────────────────────────────────── */

struct FluxFlyout {
	FluxPopup               *popup;
	FluxWindow              *owner;

	FluxTextRenderer        *text;  /* not owned */
	FluxThemeColors const   *theme; /* not owned */
	bool                     is_dark;

	/* D2D brush created on the popup's device context (owned) */
	ID2D1SolidColorBrush    *brush;

	/* Desired content (interior) size — padding & border added on top */
	float                    content_w;
	float                    content_h;

	/* Custom interior painter */
	FluxFlyoutPaintCallback  paint_cb;
	void                    *paint_ctx;

	/* Dismiss handler mirrored from popup */
	FluxPopupDismissCallback dismiss_cb;
	void                    *dismiss_ctx;
};

/* ── Popup paint thunk ───────────────────────────────────────────── */

static void ensure_brush(FluxFlyout *fly, ID2D1DeviceContext *d2d) {
	if (fly->brush || !d2d) return;
	D2D1_COLOR_F          black = {0.0f, 0.0f, 0.0f, 1.0f};
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &fly->brush);
}

/* Resolve FlyoutPresenter brushes exactly as per WinUI theme. */
static void resolve_card_colors(FluxFlyout const *fly, FluxColor *out_bg, FluxColor *out_border) {
	/* FlyoutPresenterBackground = AcrylicInAppFillColorDefaultBrush.
	 * Its exact FallbackColor (Materials/Acrylic/AcrylicBrush_themeresources.xaml):
	 *   Light : #F9F9F9    Dark : #2C2C2C   (both opaque).
	 *
	 * When the Win11 DWM transient-acrylic backdrop is active on the
	 * popup HWND we emit a fully-transparent fill so the system material
	 * shows through — matching the acrylic look in the WinUI Gallery.  */
	FluxColor bg;
	if (flux_popup_has_system_backdrop(fly->popup)) bg = flux_color_rgba(0, 0, 0, 0);
	else bg = fly->is_dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9);

	/* SurfaceStrokeColorFlyoutBrush (exact WinUI fallback — this token
	 * does NOT alias ControlStrokeColorDefault):
	 *   Dark:  #33000000    Light: #0F000000                             */
	FluxColor border = fly->is_dark ? flux_color_rgba(0, 0, 0, 0x33) : flux_color_rgba(0, 0, 0, 0x0f);

	*out_bg          = bg;
	*out_border      = border;
}

static void flyout_paint_thunk(void *ctx, FluxPopup *popup) {
	FluxFlyout *fly = ( FluxFlyout * ) ctx;
	if (!fly) return;

	FluxGraphics *gfx = flux_popup_get_graphics(popup);
	if (!gfx) return;

	ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
	if (!d2d) return;

	ensure_brush(fly, d2d);
	if (!fly->brush) return;

	/* ---- Card geometry ---- */
	float     outer_w = fly->content_w + FLUX_FLYOUT_PAD_LEFT + FLUX_FLYOUT_PAD_RIGHT + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	float     outer_h = fly->content_h + FLUX_FLYOUT_PAD_TOP + FLUX_FLYOUT_PAD_BOTTOM + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;

	FluxRect  bounds  = {0.0f, 0.0f, outer_w, outer_h};

	FluxColor bg, border;
	resolve_card_colors(fly, &bg, &border);

	/* Build a render context so we can reuse the standard rounded-rect
	 * helpers from flux_render_internal.h.                              */
	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = fly->brush;
	rc.text    = fly->text;
	rc.theme   = fly->theme;
	rc.dpi     = flux_window_dpi(fly->owner);
	rc.is_dark = fly->is_dark;

	/* 1. Card background — fills the full rounded rect (BackgroundSizing=
	 *    InnerBorderEdge means the bg still reaches the outer radius,
	 *    while the border is drawn *inside* at half-width inset).       */
	flux_fill_rounded_rect(&rc, &bounds, FLUX_FLYOUT_CORNER_RADIUS, bg);

	/* 2. 1-px hairline border, inset by half the border width.          */
	{
		float    half  = FLUX_FLYOUT_BORDER_WIDTH * 0.5f;
		FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - half * 2.0f, bounds.h - half * 2.0f};
		flux_draw_rounded_rect(&rc, &inset, FLUX_FLYOUT_CORNER_RADIUS, border, FLUX_FLYOUT_BORDER_WIDTH);
	}

		/* 3. Interior (ContentPresenter with Margin = Padding).             */
		if (fly->paint_cb) {
			FluxRect content_rect = {
			  FLUX_FLYOUT_BORDER_WIDTH + FLUX_FLYOUT_PAD_LEFT, FLUX_FLYOUT_BORDER_WIDTH + FLUX_FLYOUT_PAD_TOP,
			  fly->content_w, fly->content_h};
			fly->paint_cb(fly->paint_ctx, fly, &content_rect);
		}
}

/* ── Lifecycle ───────────────────────────────────────────────────── */

FluxFlyout *flux_flyout_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxFlyout *fly = ( FluxFlyout * ) calloc(1, sizeof(*fly));
	if (!fly) return NULL;

	fly->owner     = owner;
	fly->content_w = 200.0f;
	fly->content_h = 100.0f;

	fly->popup     = flux_popup_create(owner);
		if (!fly->popup) {
			free(fly);
			return NULL;
		}

	flux_popup_set_dismiss_on_outside(fly->popup, true);
	flux_popup_set_paint_callback(fly->popup, flyout_paint_thunk, fly);
	/* WinUI PopupThemeTransition: translate + fade, ~167 ms. */
	flux_popup_set_anim_style(fly->popup, FLUX_POPUP_ANIM_FLYOUT);

	return fly;
}

void flux_flyout_destroy(FluxFlyout *fly) {
	if (!fly) return;

		if (fly->brush) {
			ID2D1SolidColorBrush_Release(fly->brush);
			fly->brush = NULL;
		}
	if (fly->popup) flux_popup_destroy(fly->popup);

	free(fly);
}

/* ── Configuration ───────────────────────────────────────────────── */

void flux_flyout_set_text_renderer(FluxFlyout *fly, FluxTextRenderer *tr) {
	if (fly) fly->text = tr;
}

void flux_flyout_set_theme(FluxFlyout *fly, FluxThemeColors const *theme, bool is_dark) {
	if (!fly) return;
	fly->theme   = theme;
	fly->is_dark = is_dark;
}

void flux_flyout_set_content_size(FluxFlyout *fly, float width, float height) {
	if (!fly) return;
	fly->content_w = (width > 1.0f) ? width : 1.0f;
	fly->content_h = (height > 1.0f) ? height : 1.0f;

	/* Propagate the *outer* size (incl. padding + border) to the popup.  */
	float outer_w  = fly->content_w + FLUX_FLYOUT_PAD_LEFT + FLUX_FLYOUT_PAD_RIGHT + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	float outer_h  = fly->content_h + FLUX_FLYOUT_PAD_TOP + FLUX_FLYOUT_PAD_BOTTOM + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	flux_popup_set_size(fly->popup, outer_w, outer_h);
}

void flux_flyout_set_paint_callback(FluxFlyout *fly, FluxFlyoutPaintCallback cb, void *user_ctx) {
	if (!fly) return;
	fly->paint_cb  = cb;
	fly->paint_ctx = user_ctx;
}

void flux_flyout_set_dismiss_callback(FluxFlyout *fly, FluxPopupDismissCallback cb, void *user_ctx) {
	if (!fly) return;
	fly->dismiss_cb  = cb;
	fly->dismiss_ctx = user_ctx;
	flux_popup_set_dismiss_callback(fly->popup, cb, user_ctx);
}

/* ── Show / Dismiss ──────────────────────────────────────────────── */

void flux_flyout_show(FluxFlyout *fly, FluxRect anchor, FluxPlacement placement) {
	if (!fly) return;

	/* Make sure the popup is sized to the current content+padding.       */
	flux_flyout_set_content_size(fly, fly->content_w, fly->content_h);

	/* Request the Win11 DWM transient-acrylic backdrop — silently a
	 * no-op on older Windows, in which case we paint the opaque
	 * AcrylicInAppFillColorDefault fallback color instead.              */
	flux_popup_enable_system_backdrop(fly->popup, fly->is_dark);

	flux_popup_show(fly->popup, anchor, placement);
}

void flux_flyout_dismiss(FluxFlyout *fly) {
	if (!fly) return;
	flux_popup_dismiss(fly->popup);
}

bool          flux_flyout_is_visible(FluxFlyout const *fly) { return fly ? flux_popup_is_visible(fly->popup) : false; }

/* ── Accessors ───────────────────────────────────────────────────── */

FluxGraphics *flux_flyout_get_graphics(FluxFlyout *fly) { return fly ? flux_popup_get_graphics(fly->popup) : NULL; }

FluxWindow   *flux_flyout_get_owner(FluxFlyout *fly) { return fly ? fly->owner : NULL; }

FluxPopup    *flux_flyout_get_popup(FluxFlyout *fly) { return fly ? fly->popup : NULL; }

FluxThemeColors const *flux_flyout_get_theme(FluxFlyout const *fly) { return fly ? fly->theme : NULL; }

bool                   flux_flyout_is_dark(FluxFlyout const *fly) { return fly ? fly->is_dark : false; }

FluxTextRenderer      *flux_flyout_get_text_renderer(FluxFlyout *fly) { return fly ? fly->text : NULL; }
