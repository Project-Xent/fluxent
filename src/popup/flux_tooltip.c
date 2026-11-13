#include "fluxent/flux_tooltip.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_gfx.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_internal.h"
#include "runtime/flux_str.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOOLTIP_RESHOW_WINDOW_MS 500

struct FluxTooltip {
	FluxPopup             *popup;
	FluxWindow            *owner;
	FluxText              *text;
	FluxThemeColors const *theme;
	bool                   is_dark;
	ID2D1SolidColorBrush  *brush;
	XentNodeId             hovered_node;
	char const            *tooltip_text;
	FluxRect               anchor_screen;
	UINT_PTR               timer_id;
	bool                   is_visible;
	ULONGLONG              last_dismiss_tick;
};

static void          tooltip_show(FluxTooltip *tt);
static void          tooltip_hide(FluxTooltip *tt);
static void          tooltip_start_timer(FluxTooltip *tt, UINT delay_ms);
static void          tooltip_kill_timer(FluxTooltip *tt);
static void CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed);
static void          tooltip_paint(void *ctx, FluxPopup *popup);

FluxTooltip         *flux_tooltip_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxTooltip *tt = ( FluxTooltip * ) calloc(1, sizeof(*tt));
	if (!tt) return NULL;

	tt->owner        = owner;
	tt->hovered_node = XENT_NODE_INVALID;

	tt->popup        = flux_popup_create(owner);
	if (!tt->popup) {
		free(tt);
		return NULL;
	}

	flux_popup_setdismissoutside(tt->popup, false);
	flux_popup_onpaint(tt->popup, tooltip_paint, tt);
	flux_popup_setanim(tt->popup, FLUX_POPUP_ANIM_TOOLTIP);

	return tt;
}

void flux_tooltip_destroy(FluxTooltip *tt) {
	if (!tt) return;

	tooltip_kill_timer(tt);

	if (tt->brush) {
		ID2D1SolidColorBrush_Release(tt->brush);
		tt->brush = NULL;
	}
	if (tt->popup) flux_popup_destroy(tt->popup);

	flux_str_free(tt->tooltip_text);
	free(tt);
}

void flux_tooltip_settext(FluxTooltip *tt, FluxText *tr) {
	if (tt) tt->text = tr;
}

void flux_tooltip_settheme(FluxTooltip *tt, FluxThemeColors const *theme, bool is_dark) {
	if (!tt) return;
	tt->theme   = theme;
	tt->is_dark = is_dark;
}

static void tooltip_clear_hover(FluxTooltip *tt) {
	tooltip_kill_timer(tt);
	if (tt->is_visible) tooltip_hide(tt);
	tt->hovered_node = XENT_NODE_INVALID;
	flux_str_replace(&tt->tooltip_text, NULL);
}

static bool tooltip_should_reshow(FluxTooltip const *tt, ULONGLONG now_ms) {
	return tt->is_visible
	    || (tt->last_dismiss_tick != 0 && (now_ms - tt->last_dismiss_tick) < TOOLTIP_RESHOW_WINDOW_MS);
}

static void tooltip_schedule_show(FluxTooltip *tt, bool quick) {
	if (tt->is_visible) {
		tooltip_hide(tt);
		tooltip_show(tt);
		return;
	}

	tooltip_kill_timer(tt);
	if (quick) tooltip_show(tt);
	else tooltip_start_timer(tt, FLUX_TOOLTIP_DELAY_MS);
}

static bool tooltip_str_eq(char const *a, char const *b) {
	if (a == b) return true;
	if (!a || !b) return false;
	return strcmp(a, b) == 0;
}

static char const *tooltip_text_at(FluxNodeData const *nd) {
	if (!nd) return NULL;
	if (nd->behavior.tooltip_at)
		return nd->behavior.tooltip_at(nd->behavior.tooltip_at_ctx, nd->hover_local_x, nd->hover_local_y);
	return nd->tooltip_text && nd->tooltip_text [0] ? nd->tooltip_text : NULL;
}

void flux_tooltip_hover(FluxTooltip *tt, XentNodeId hovered, FluxNodeData const *nd, FluxRect const *screen_bounds) {
	if (!tt) return;

	char const *new_text = tooltip_text_at(nd);

	if (hovered == tt->hovered_node && hovered != XENT_NODE_INVALID && tooltip_str_eq(new_text, tt->tooltip_text)) {
		if (screen_bounds && !tt->is_visible) tt->anchor_screen = *screen_bounds;
		return;
	}

	if (hovered == XENT_NODE_INVALID || !new_text) {
		tooltip_clear_hover(tt);
		return;
	}

	tt->hovered_node = hovered;
	flux_str_replace(&tt->tooltip_text, new_text);
	if (screen_bounds) tt->anchor_screen = *screen_bounds;

	ULONGLONG now_ms = GetTickCount64();
	tooltip_schedule_show(tt, tooltip_should_reshow(tt, now_ms));
}

void flux_tooltip_dismiss(FluxTooltip *tt) {
	if (!tt) return;
	tooltip_kill_timer(tt);
	if (tt->is_visible) tooltip_hide(tt);
}

bool            flux_tooltip_visible(FluxTooltip const *tt) { return tt ? tt->is_visible : false; }

static FluxSize tooltip_measure_content(FluxTooltip *tt) {
	FluxSize size = {0.0f, 0.0f};
	if (!tt->text || !tt->tooltip_text) return size;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family    = NULL;
	ts.font_size      = FLUX_TOOLTIP_FONT_SIZE;
	ts.font_weight    = FLUX_FONT_REGULAR;
	ts.text_align     = FLUX_TEXT_LEFT;
	ts.vert_align     = FLUX_TEXT_TOP;
	ts.color          = flux_color_rgba(0, 0, 0, 0xe4);
	ts.word_wrap      = true;

	float max_text_w  = FLUX_TOOLTIP_MAX_WIDTH - FLUX_TOOLTIP_PAD_LEFT - FLUX_TOOLTIP_PAD_RIGHT;
	size              = flux_text_measure(tt->text, tt->tooltip_text, &ts, max_text_w);

	size.w           += FLUX_TOOLTIP_PAD_LEFT + FLUX_TOOLTIP_PAD_RIGHT;
	size.h           += FLUX_TOOLTIP_PAD_TOP + FLUX_TOOLTIP_PAD_BOTTOM;
	if (size.w > FLUX_TOOLTIP_MAX_WIDTH) size.w = FLUX_TOOLTIP_MAX_WIDTH;

	return size;
}

static void tooltip_show(FluxTooltip *tt) {
	if (!tt || !tt->tooltip_text || !tt->popup) return;

	FluxSize content_size = tooltip_measure_content(tt);
	if (content_size.w < 1.0f || content_size.h < 1.0f) return;

	flux_popup_setsize(tt->popup, content_size.w, content_size.h);
	flux_popup_show(tt->popup, tt->anchor_screen, FLUX_PLACEMENT_BOTTOM);
	tt->is_visible = true;
}

static void tooltip_hide(FluxTooltip *tt) {
	if (!tt) return;
	flux_popup_dismiss(tt->popup);
	tt->is_visible        = false;
	tt->last_dismiss_tick = GetTickCount64();
}

static void tooltip_start_timer(FluxTooltip *tt, UINT delay_ms) {
	tooltip_kill_timer(tt);
	HWND hwnd = flux_window_hwnd(tt->owner);
	if (!hwnd) return;
	tt->timer_id = SetTimer(hwnd, ( UINT_PTR ) tt, delay_ms, tooltip_timer_proc);
}

static void tooltip_kill_timer(FluxTooltip *tt) {
	if (!tt->timer_id) return;
	HWND hwnd = flux_window_hwnd(tt->owner);
	if (hwnd) KillTimer(hwnd, tt->timer_id);
	tt->timer_id = 0;
}

static void CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
	( void ) hwnd;
	( void ) msg;
	( void ) elapsed;

	FluxTooltip *tt = ( FluxTooltip * ) id;
	if (!tt || tt->timer_id != id) return;
	tooltip_kill_timer(tt);
	if (tt->tooltip_text && !tt->is_visible) tooltip_show(tt);
}

static void tooltip_ensure_brush(FluxTooltip *tt, ID2D1DeviceContext *d2d) {
	if (tt->brush || !d2d) return;
	D2D1_COLOR_F          black = {0.0f, 0.0f, 0.0f, 1.0f};
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	HRESULT hr       = ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &tt->brush);
	( void ) hr;
}

static void tooltip_resolve_colors(FluxTooltip const *tt, FluxColor *bg, FluxColor *border, FluxColor *text_color) {
	FluxThemeColors const *theme = tt->theme;

	*bg                          = tt->is_dark ? flux_color_rgb(44, 44, 44) : flux_color_rgb(249, 249, 249);
	if (tt->is_dark) *border = flux_color_rgba(0, 0, 0, 0x33);
	else *border = flux_color_rgba(0, 0, 0, 0x0f);
	if (theme) *text_color = theme->text_primary;
	else if (tt->is_dark) *text_color = flux_color_rgba(255, 255, 255, 0xe4);
	else *text_color = flux_color_rgba(0, 0, 0, 0xe4);
}

static FluxRenderCtx tooltip_render_context(FluxTooltip *tt, ID2D1DeviceContext *d2d) {
	FluxRenderCtx rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = tt->brush;
	rc.text    = tt->text;
	rc.theme   = tt->theme;
	rc.dpi     = flux_window_dpi(tt->owner);
	rc.is_dark = tt->is_dark;
	return rc;
}

static XentDl *
tooltip_record(FluxTooltip *tt, FluxRect const *bounds, FluxColor bg, FluxColor border, FluxColor text_color) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	bool     ok    = flux_rec_fill_rounded(b, bounds, FLUX_TOOLTIP_CORNER_RADIUS, bg);
	float    half  = FLUX_TOOLTIP_BORDER_WIDTH * 0.5f;
	FluxRect inset = {bounds->x + half, bounds->y + half, bounds->w - half * 2.0f, bounds->h - half * 2.0f};
	if (ok) ok = flux_rec_stroke_rounded(b, &inset, FLUX_TOOLTIP_CORNER_RADIUS, border, FLUX_TOOLTIP_BORDER_WIDTH);
	if (ok && tt->tooltip_text) {
		FluxRect text_rect = {
		  FLUX_TOOLTIP_PAD_LEFT, FLUX_TOOLTIP_PAD_TOP, bounds->w - FLUX_TOOLTIP_PAD_LEFT - FLUX_TOOLTIP_PAD_RIGHT,
		  bounds->h - FLUX_TOOLTIP_PAD_TOP - FLUX_TOOLTIP_PAD_BOTTOM};
		XentTextStyle style = {
		  .font_size   = FLUX_TOOLTIP_FONT_SIZE,
		  .font_weight = FLUX_FONT_REGULAR,
		  .flags       = XENT_DL_TEXT_WRAP,
		};
		ok = flux_rec_text_ex(b, &text_rect, tt->tooltip_text, &style, text_color);
	}
	return flux_rec_finish(b, ok);
}

static void tooltip_paint(void *ctx, FluxPopup *popup) {
	FluxTooltip *tt = ( FluxTooltip * ) ctx;
	if (!tt || !tt->tooltip_text) return;

	FluxGfx *gfx = flux_popup_gfx(popup);
	if (!gfx) return;

	ID2D1DeviceContext *d2d = flux_gfx_d2d(gfx);
	if (!d2d) return;

	tooltip_ensure_brush(tt, d2d);
	if (!tt->brush) return;

	FluxColor bg         = {0};
	FluxColor border     = {0};
	FluxColor text_color = {0};
	tooltip_resolve_colors(tt, &bg, &border, &text_color);
	bg                       = flux_popup_acrylictint(popup, bg);

	FluxSize      popup_size = tooltip_measure_content(tt);
	FluxRect      bounds     = {0.0f, 0.0f, popup_size.w, popup_size.h};
	FluxRenderCtx rc         = tooltip_render_context(tt, d2d);
	XentDl       *list       = tooltip_record(tt, &bounds, bg, border, text_color);
	if (list) {
		flux_xtk_replay_rc(&rc, list);
		xent_dl_free(list);
	}
}

void flux_tooltip_showat(FluxTooltip *tt, char const *text, FluxRect anchor, FluxPlacement placement) {
	if (!tt || !text || !tt->popup) return;
	tooltip_kill_timer(tt);
	flux_str_replace(&tt->tooltip_text, text);
	tt->hovered_node  = XENT_NODE_INVALID;
	tt->anchor_screen = anchor;
	FluxSize size     = tooltip_measure_content(tt);
	if (size.w < 1.0f || size.h < 1.0f) return;
	flux_popup_setsize(tt->popup, size.w, size.h);
	flux_popup_show(tt->popup, anchor, placement);
	tt->is_visible = true;
	flux_popup_reposition(tt->popup);
}
