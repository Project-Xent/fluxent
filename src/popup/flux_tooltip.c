#include "fluxent/flux_tooltip.h"
#include "fluxent/flux_graphics.h"
#include "flux_render_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
/* ── Debug logging (disabled — change 0 to 1 to re-enable) ───────── */

#define FLUX_TOOLTIP_DEBUG 0

#if FLUX_TOOLTIP_DEBUG
  #include <stdio.h>
static FILE *g_dbg = NULL;

static void  dbg_open(void) {
	if (!g_dbg) g_dbg = fopen("C:\\Users\\Wynn\\Xent\\tooltip_debug.log", "w");
}

  #define DBG(...)                                 \
		  do {                                     \
			  dbg_open();                          \
				  if (g_dbg) {                     \
					  fprintf(g_dbg, __VA_ARGS__); \
					  fflush(g_dbg);               \
				  }                                \
		  }                                        \
	  while (0)
#else
  #define DBG(...) (( void ) 0)
#endif

/* ── Win32 timer plumbing ─────────────────────────────────────────── */

/*
 * We use SetTimer(NULL, ...) with a TIMERPROC callback.  Since there is
 * only one FluxTooltip per application we stash the pointer in a static
 * so the timer callback can reach it.
 */
static FluxTooltip *g_tooltip_instance = NULL;
#define TOOLTIP_RESHOW_WINDOW_MS              \
	500 /* ms after dismiss in which a new    \
	       hover triggers instant re-show  */

/* ── Internal state ───────────────────────────────────────────────── */

struct FluxTooltip {
	/* Popup used to display the tooltip card */
	FluxPopup             *popup;
	FluxWindow            *owner;

	/* Shared rendering resources (not owned) */
	FluxTextRenderer      *text;
	FluxThemeColors const *theme;
	bool                   is_dark;

	/* D2D brush created on the popup's device context (owned) */
	ID2D1SolidColorBrush  *brush;

	/* Hover tracking */
	XentNodeId             hovered_node;
	char const            *tooltip_text;  /* points into FluxNodeData – not owned */
	FluxRect               anchor_screen; /* screen-space bounds of hovered node  */

	/* Timer / visibility */
	UINT_PTR               timer_id;
	bool                   is_visible;

	/* For "quick re-show": if we dismissed within TOOLTIP_RESHOW_WINDOW_MS,
	   the next tooltip appears immediately (0 ms delay). */
	DWORD                  last_dismiss_tick;
};

/* ── Forward declarations ─────────────────────────────────────────── */

static void          tooltip_show(FluxTooltip *tt);
static void          tooltip_hide(FluxTooltip *tt);
static void          tooltip_start_timer(FluxTooltip *tt, UINT delay_ms);
static void          tooltip_kill_timer(FluxTooltip *tt);
static void CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed);
static void          tooltip_paint(void *ctx, FluxPopup *popup);

/* ── Lifecycle ────────────────────────────────────────────────────── */

FluxTooltip         *flux_tooltip_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxTooltip *tt = ( FluxTooltip * ) calloc(1, sizeof(*tt));
	if (!tt) return NULL;

	tt->owner        = owner;
	tt->hovered_node = XENT_NODE_INVALID;

	tt->popup        = flux_popup_create(owner);
		if (!tt->popup) {
			DBG("[tooltip] ERROR: flux_popup_create failed\n");
			free(tt);
			return NULL;
		}

	/* Tooltip does not dismiss on outside click — it hides on hover-out */
	flux_popup_set_dismiss_on_outside(tt->popup, false);

	/* Register our paint callback */
	flux_popup_set_paint_callback(tt->popup, tooltip_paint, tt);

	/* WinUI FadeInThemeAnimation: opacity-only, 83 ms linear. */
	flux_popup_set_anim_style(tt->popup, FLUX_POPUP_ANIM_TOOLTIP);

	g_tooltip_instance = tt;

	DBG("[tooltip] Created tooltip system, popup=%p\n", ( void * ) tt->popup);
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

	if (g_tooltip_instance == tt) g_tooltip_instance = NULL;

	DBG("[tooltip] Destroyed\n");
#if FLUX_TOOLTIP_DEBUG
		if (g_dbg) {
			fclose(g_dbg);
			g_dbg = NULL;
		}
#endif

	free(tt);
}

/* ── Configuration ────────────────────────────────────────────────── */

void flux_tooltip_set_text_renderer(FluxTooltip *tt, FluxTextRenderer *tr) {
	if (tt) tt->text = tr;
}

void flux_tooltip_set_theme(FluxTooltip *tt, FluxThemeColors const *theme, bool is_dark) {
	if (!tt) return;
	tt->theme   = theme;
	tt->is_dark = is_dark;
}

/* ── Hover notification ───────────────────────────────────────────── */

void flux_tooltip_on_hover(FluxTooltip *tt, XentNodeId hovered, FluxNodeData const *nd, FluxRect const *screen_bounds) {
	if (!tt) return;

	char const *new_text = (nd && nd->tooltip_text && nd->tooltip_text [0]) ? nd->tooltip_text : NULL;

		/* Same node — just update the anchor position (so the tooltip
	       appears at the latest cursor position when the timer fires),
	       but skip the rest of the state-change logic. */
		if (hovered == tt->hovered_node && hovered != XENT_NODE_INVALID) {
			if (screen_bounds && !tt->is_visible) tt->anchor_screen = *screen_bounds;
			return;
		}

	DBG(
	  "[tooltip] on_hover: node=%u nd=%p new_text=%s\n", ( unsigned ) hovered, ( void * ) nd,
	  new_text ? new_text : "(null)"
	);

		/* ---- Hover left all nodes ---- */
		if (hovered == XENT_NODE_INVALID || !new_text) {
			tooltip_kill_timer(tt);
			if (tt->is_visible) tooltip_hide(tt);
			tt->hovered_node = XENT_NODE_INVALID;
			tt->tooltip_text = NULL;
			return;
		}

	/* ---- Hover entered a new node with tooltip text ---- */
	tt->hovered_node = hovered;
	tt->tooltip_text = new_text;
		if (screen_bounds) {
			tt->anchor_screen = *screen_bounds;
			DBG(
			  "[tooltip]   anchor: x=%.0f y=%.0f w=%.0f h=%.0f\n", screen_bounds->x, screen_bounds->y, screen_bounds->w,
			  screen_bounds->h
			);
		}

	/* If a tooltip is currently visible or was recently dismissed,
	   show the new one immediately (0 ms) — matches WinUI 3 behaviour.
	   Otherwise start the full 800 ms delay. */
	bool quick = tt->is_visible
	          || (tt->last_dismiss_tick != 0 && (GetTickCount() - tt->last_dismiss_tick) < TOOLTIP_RESHOW_WINDOW_MS);

		if (tt->is_visible) {
			/* Already showing a different tooltip — switch instantly */
			tooltip_hide(tt);
			tooltip_show(tt);
		}
		else if (quick) {
			tooltip_kill_timer(tt);
			tooltip_show(tt);
		}
		else {
			tooltip_kill_timer(tt);
			tooltip_start_timer(tt, FLUX_TOOLTIP_DELAY_MS);
		}
}

void flux_tooltip_dismiss(FluxTooltip *tt) {
	if (!tt) return;
	tooltip_kill_timer(tt);
	if (tt->is_visible) tooltip_hide(tt);
}

bool            flux_tooltip_is_visible(FluxTooltip const *tt) { return tt ? tt->is_visible : false; }

/* ── Internal: measure + show ─────────────────────────────────────── */

static FluxSize tooltip_measure_content(FluxTooltip *tt) {
	FluxSize size = {0.0f, 0.0f};
		if (!tt->text || !tt->tooltip_text) {
			DBG(
			  "[tooltip] measure: text_renderer=%p tooltip_text=%p -> ZERO\n", ( void * ) tt->text,
			  ( void * ) tt->tooltip_text
			);
			return size;
		}

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family   = NULL; /* default (Segoe UI) */
	ts.font_size     = FLUX_TOOLTIP_FONT_SIZE;
	ts.font_weight   = FLUX_FONT_REGULAR;
	ts.text_align    = FLUX_TEXT_LEFT;
	ts.vert_align    = FLUX_TEXT_TOP;
	ts.color         = flux_color_rgba(0, 0, 0, 0xe4); /* placeholder */
	ts.word_wrap     = true;

	float max_text_w = FLUX_TOOLTIP_MAX_WIDTH - FLUX_TOOLTIP_PAD_LEFT - FLUX_TOOLTIP_PAD_RIGHT;

	size             = flux_text_measure(tt->text, tt->tooltip_text, &ts, max_text_w);

	DBG("[tooltip] measure: text='%s' raw=%.1f x %.1f\n", tt->tooltip_text, size.w, size.h);

	/* Add padding */
	size.w += FLUX_TOOLTIP_PAD_LEFT + FLUX_TOOLTIP_PAD_RIGHT;
	size.h += FLUX_TOOLTIP_PAD_TOP + FLUX_TOOLTIP_PAD_BOTTOM;

	/* Clamp width to max */
	if (size.w > FLUX_TOOLTIP_MAX_WIDTH) size.w = FLUX_TOOLTIP_MAX_WIDTH;

	DBG("[tooltip] measure: final=%.1f x %.1f\n", size.w, size.h);
	return size;
}

static void tooltip_show(FluxTooltip *tt) {
		if (!tt || !tt->tooltip_text || !tt->popup) {
			DBG("[tooltip] show: ABORTED (null check failed)\n");
			return;
		}

	FluxSize content_size = tooltip_measure_content(tt);
		if (content_size.w < 1.0f || content_size.h < 1.0f) {
			DBG("[tooltip] show: ABORTED (size too small: %.1f x %.1f)\n", content_size.w, content_size.h);
			return;
		}

	flux_popup_set_size(tt->popup, content_size.w, content_size.h);

	DBG(
	  "[tooltip] show: anchor=(%.0f,%.0f,%.0f,%.0f) size=(%.1f,%.1f)\n", tt->anchor_screen.x, tt->anchor_screen.y,
	  tt->anchor_screen.w, tt->anchor_screen.h, content_size.w, content_size.h
	);

	/* Show anchored below the hovered node (BOTTOM placement) */
	flux_popup_show(tt->popup, tt->anchor_screen, FLUX_PLACEMENT_BOTTOM);

	tt->is_visible = true;
	DBG("[tooltip] show: VISIBLE!\n");
}

static void tooltip_hide(FluxTooltip *tt) {
	if (!tt) return;

	DBG("[tooltip] hide\n");
	flux_popup_dismiss(tt->popup);
	tt->is_visible        = false;
	tt->last_dismiss_tick = GetTickCount();
}

/* ── Win32 timer ──────────────────────────────────────────────────── */

static void tooltip_start_timer(FluxTooltip *tt, UINT delay_ms) {
	tooltip_kill_timer(tt);
	tt->timer_id = SetTimer(NULL, 0, delay_ms, tooltip_timer_proc);
	DBG("[tooltip] start_timer: delay=%u timer_id=%llu\n", delay_ms, ( unsigned long long ) tt->timer_id);
}

static void tooltip_kill_timer(FluxTooltip *tt) {
		if (tt->timer_id) {
			KillTimer(NULL, tt->timer_id);
			DBG("[tooltip] kill_timer: id=%llu\n", ( unsigned long long ) tt->timer_id);
			tt->timer_id = 0;
		}
}

static void CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
	( void ) hwnd;
	( void ) msg;
	( void ) elapsed;

	DBG("[tooltip] timer_proc: id=%llu g_tooltip=%p\n", ( unsigned long long ) id, ( void * ) g_tooltip_instance);

	FluxTooltip *tt = g_tooltip_instance;
	if (!tt || tt->timer_id != id) return;

	tooltip_kill_timer(tt);

	if (tt->tooltip_text && !tt->is_visible) tooltip_show(tt);
}

/* ── Paint callback (renders inside the popup's WM_PAINT) ─────────── */

static void tooltip_ensure_brush(FluxTooltip *tt, ID2D1DeviceContext *d2d) {
	if (tt->brush) return;
	if (!d2d) return;

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

	DBG("[tooltip] ensure_brush: hr=0x%08lx brush=%p\n", ( unsigned long ) hr, ( void * ) tt->brush);
}

static void tooltip_paint(void *ctx, FluxPopup *popup) {
	FluxTooltip *tt = ( FluxTooltip * ) ctx;
		if (!tt || !tt->tooltip_text) {
			DBG("[tooltip] paint: SKIP (tt=%p text=%p)\n", ( void * ) tt, tt ? ( void * ) tt->tooltip_text : NULL);
			return;
		}

	DBG("[tooltip] paint: ENTER text='%s'\n", tt->tooltip_text);

	FluxGraphics *gfx = flux_popup_get_graphics(popup);
		if (!gfx) {
			DBG("[tooltip] paint: no gfx!\n");
			return;
		}

	ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
		if (!d2d) {
			DBG("[tooltip] paint: no d2d!\n");
			return;
		}

	tooltip_ensure_brush(tt, d2d);
		if (!tt->brush) {
			DBG("[tooltip] paint: no brush!\n");
			return;
		}

	/* ---- Resolve theme colours ---- */
	FluxThemeColors const *t = tt->theme;

	/*
	 * Background: AcrylicInAppFillColorDefaultBrush FallbackColor
	 * (fully opaque — we can't do real acrylic in a standalone popup)
	 * Dark:  #2C2C2C    Light: #F9F9F9
	 */
	FluxColor              bg;
	if (tt->is_dark) bg = flux_color_rgb(44, 44, 44);
	else bg = flux_color_rgb(249, 249, 249);

	/* Border: SurfaceStrokeColorFlyout
	 * Dark:  #33000000    Light: #0F000000 */
	FluxColor border;
	if (tt->is_dark) border = flux_color_rgba(0, 0, 0, 0x33);
	else border = flux_color_rgba(0, 0, 0, 0x0f);

	/* Text: TextFillColorPrimary */
	FluxColor text_color;
	if (t) text_color = t->text_primary;
	else if (tt->is_dark) text_color = flux_color_rgba(255, 255, 255, 0xe4);
	else text_color = flux_color_rgba(0, 0, 0, 0xe4);

	/* ---- Compute content size ---- */
	FluxSize popup_size = tooltip_measure_content(tt);
	FluxRect bounds     = {0.0f, 0.0f, popup_size.w, popup_size.h};

	DBG(
	  "[tooltip] paint: bounds=%.1f x %.1f bg=0x%08x border=0x%08x text=0x%08x\n", bounds.w, bounds.h, bg.rgba,
	  border.rgba, text_color.rgba
	);

	/* ---- Build a temporary FluxRenderContext for the helpers ---- */
	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = d2d;
	rc.brush   = tt->brush;
	rc.text    = tt->text;
	rc.theme   = t;
	rc.dpi     = flux_window_dpi(tt->owner);
	rc.is_dark = tt->is_dark;

	/* ---- Draw card background ---- */
	flux_fill_rounded_rect(&rc, &bounds, FLUX_TOOLTIP_CORNER_RADIUS, bg);

	/* ---- Draw border ---- */
	{
		float    half  = FLUX_TOOLTIP_BORDER_WIDTH * 0.5f;
		FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - half * 2.0f, bounds.h - half * 2.0f};
		flux_draw_rounded_rect(&rc, &inset, FLUX_TOOLTIP_CORNER_RADIUS, border, FLUX_TOOLTIP_BORDER_WIDTH);
	}

		/* ---- Draw text ---- */
		if (tt->text && tt->tooltip_text) {
			FluxRect text_rect = {
			  FLUX_TOOLTIP_PAD_LEFT, FLUX_TOOLTIP_PAD_TOP, bounds.w - FLUX_TOOLTIP_PAD_LEFT - FLUX_TOOLTIP_PAD_RIGHT,
			  bounds.h - FLUX_TOOLTIP_PAD_TOP - FLUX_TOOLTIP_PAD_BOTTOM};

			FluxTextStyle ts;
			memset(&ts, 0, sizeof(ts));
			ts.font_family = NULL;
			ts.font_size   = FLUX_TOOLTIP_FONT_SIZE;
			ts.font_weight = FLUX_FONT_REGULAR;
			ts.text_align  = FLUX_TEXT_LEFT;
			ts.vert_align  = FLUX_TEXT_TOP;
			ts.color       = text_color;
			ts.word_wrap   = true;

			DBG(
			  "[tooltip] paint: drawing text rect=(%.1f,%.1f,%.1f,%.1f)\n", text_rect.x, text_rect.y, text_rect.w,
			  text_rect.h
			);

			flux_text_draw(tt->text, ( ID2D1RenderTarget * ) d2d, tt->tooltip_text, &text_rect, &ts);

			DBG("[tooltip] paint: DONE\n");
		}
		else {
			DBG(
			  "[tooltip] paint: SKIP text draw (renderer=%p text=%p)\n", ( void * ) tt->text,
			  ( void * ) tt->tooltip_text
			);
		}
}
