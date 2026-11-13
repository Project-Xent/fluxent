#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_flyout.h"
#include "fluxent/flux_gfx.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

typedef struct {
	FluxThemeColors const *colors;
	FluxTheme             *manager;
	bool                   is_dark;
} ThemeRef;

struct FluxFlyout {
	FluxPopup            *popup;
	FluxWindow           *owner;

	FluxText             *text;
	ThemeRef              theme;

	ID2D1SolidColorBrush *brush;

	float                 content_w;
	float                 content_h;

	FluxFlyoutPaintFn     paint_cb;
	void                 *paint_ctx;

	FluxPopupDismissFn    dismiss_cb;
	void                 *dismiss_ctx;
};

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

static void flyout_resolve_theme(FluxFlyout *fly, FluxThemeColors const **out_theme, bool *out_is_dark) {
	if (fly->theme.manager) {
		*out_theme      = flux_theme_colors(fly->theme.manager);
		FluxThemeMode m = flux_theme_mode(fly->theme.manager);
		*out_is_dark    = (m == FLUX_THEME_DARK) || (m == FLUX_THEME_SYSTEM && flux_theme_systemdark());
	}
	else {
		*out_theme   = fly->theme.colors;
		*out_is_dark = fly->theme.is_dark;
	}
}

static void resolve_card_colors(FluxFlyout const *fly, bool is_dark, FluxColor *out_bg, FluxColor *out_border) {
	( void ) fly;

	FluxColor bg = is_dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9);

	FluxColor border;
	if (is_dark) border = flux_color_rgba(0, 0, 0, 0x33);
	else border = flux_color_rgba(0, 0, 0, 0x0f);

	*out_bg     = bg;
	*out_border = border;
}

static FluxRenderCtx
flyout_render_context(FluxFlyout *fly, ID2D1DeviceContext *d2d, FluxThemeColors const *theme, bool is_dark) {
	FluxRenderCtx rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = fly->brush;
	rc.text    = fly->text;
	rc.theme   = theme;
	rc.dpi     = flux_window_dpi(fly->owner);
	rc.is_dark = is_dark;
	return rc;
}

static FluxRect flyout_bounds(FluxFlyout const *fly) {
	float outer_w = fly->content_w + FLUX_FLYOUT_PAD_LEFT + FLUX_FLYOUT_PAD_RIGHT + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	float outer_h = fly->content_h + FLUX_FLYOUT_PAD_TOP + FLUX_FLYOUT_PAD_BOTTOM + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	return (FluxRect) {0.0f, 0.0f, outer_w, outer_h};
}

static XentDl *flyout_record_chrome(FluxRect const *bounds, FluxColor bg, FluxColor border) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	bool     ok    = flux_rec_fill_rounded(b, bounds, FLUX_FLYOUT_CORNER_RADIUS, bg);
	float    half  = FLUX_FLYOUT_BORDER_WIDTH * 0.5f;
	FluxRect inset = {bounds->x + half, bounds->y + half, bounds->w - half * 2.0f, bounds->h - half * 2.0f};
	if (ok) ok = flux_rec_stroke_rounded(b, &inset, FLUX_FLYOUT_CORNER_RADIUS, border, FLUX_FLYOUT_BORDER_WIDTH);
	return flux_rec_finish(b, ok);
}

static void flyout_paint_content(FluxFlyout *fly) {
	if (!fly->paint_cb) return;

	FluxRect content_rect = {
	  FLUX_FLYOUT_BORDER_WIDTH + FLUX_FLYOUT_PAD_LEFT, FLUX_FLYOUT_BORDER_WIDTH + FLUX_FLYOUT_PAD_TOP, fly->content_w,
	  fly->content_h};
	fly->paint_cb(fly->paint_ctx, fly, &content_rect);
}

static void flyout_paint_thunk(void *ctx, FluxPopup *popup) {
	FluxFlyout *fly = ( FluxFlyout * ) ctx;
	if (!fly) return;

	FluxGfx *gfx = flux_popup_gfx(popup);
	if (!gfx) return;

	ID2D1DeviceContext *d2d = flux_gfx_d2d(gfx);
	if (!d2d) return;

	ensure_brush(fly, d2d);
	if (!fly->brush) return;

	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	flyout_resolve_theme(fly, &theme, &is_dark);

	FluxRect  bounds = flyout_bounds(fly);
	FluxColor bg, border;
	resolve_card_colors(fly, is_dark, &bg, &border);
	bg                 = flux_popup_acrylictint(popup, bg);
	FluxRenderCtx rc   = flyout_render_context(fly, d2d, theme, is_dark);

	XentDl       *list = flyout_record_chrome(&bounds, bg, border);
	if (list) {
		flux_xtk_replay_rc(&rc, list);
		xent_dl_free(list);
	}
	flyout_paint_content(fly);
}

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

	flux_popup_setdismissoutside(fly->popup, true);
	flux_popup_onpaint(fly->popup, flyout_paint_thunk, fly);
	flux_popup_setanim(fly->popup, FLUX_POPUP_ANIM_FLYOUT);

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

void flux_flyout_settext(FluxFlyout *fly, FluxText *tr) {
	if (fly) fly->text = tr;
}

void flux_flyout_settheme(FluxFlyout *fly, FluxThemeColors const *theme, bool is_dark) {
	if (!fly) return;
	fly->theme.colors  = theme;
	fly->theme.is_dark = is_dark;
	fly->theme.manager = NULL;
}

void flux_flyout_setthememanager(FluxFlyout *fly, FluxTheme *tm) {
	if (!fly) return;
	fly->theme.manager = tm;
	if (tm) {
		fly->theme.colors  = NULL;
		fly->theme.is_dark = false;
	}
}

void flux_flyout_setsize(FluxFlyout *fly, float width, float height) {
	if (!fly) return;
	fly->content_w = (width > 1.0f) ? width : 1.0f;
	fly->content_h = (height > 1.0f) ? height : 1.0f;

	float outer_w  = fly->content_w + FLUX_FLYOUT_PAD_LEFT + FLUX_FLYOUT_PAD_RIGHT + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	float outer_h  = fly->content_h + FLUX_FLYOUT_PAD_TOP + FLUX_FLYOUT_PAD_BOTTOM + 2.0f * FLUX_FLYOUT_BORDER_WIDTH;
	flux_popup_setsize(fly->popup, outer_w, outer_h);
}

void flux_flyout_onpaint(FluxFlyout *fly, FluxFlyoutPaintFn cb, void *user_ctx) {
	if (!fly) return;
	fly->paint_cb  = cb;
	fly->paint_ctx = user_ctx;
}

void flux_flyout_ondismiss(FluxFlyout *fly, FluxPopupDismissFn cb, void *user_ctx) {
	if (!fly) return;
	fly->dismiss_cb  = cb;
	fly->dismiss_ctx = user_ctx;
	flux_popup_ondismiss(fly->popup, cb, user_ctx);
}

void flux_flyout_show(FluxFlyout *fly, FluxRect anchor, FluxPlacement placement) {
	if (!fly) return;

	flux_flyout_setsize(fly, fly->content_w, fly->content_h);

	flux_popup_show(fly->popup, anchor, placement);
}

void flux_flyout_dismiss(FluxFlyout *fly) {
	if (!fly) return;
	flux_popup_dismiss(fly->popup);
}

bool        flux_flyout_visible(FluxFlyout const *fly) { return fly ? flux_popup_visible(fly->popup) : false; }

FluxGfx    *flux_flyout_gfx(FluxFlyout *fly) { return fly ? flux_popup_gfx(fly->popup) : NULL; }

FluxWindow *flux_flyout_owner(FluxFlyout *fly) { return fly ? fly->owner : NULL; }

FluxPopup  *flux_flyout_popup(FluxFlyout *fly) { return fly ? fly->popup : NULL; }

FluxThemeColors const *flux_flyout_theme(FluxFlyout const *fly) { return fly ? fly->theme.colors : NULL; }

bool                   flux_flyout_dark(FluxFlyout const *fly) { return fly ? fly->theme.is_dark : false; }

FluxText              *flux_flyout_text(FluxFlyout *fly) { return fly ? fly->text : NULL; }
