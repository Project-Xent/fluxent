#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_button_internal.h"

#include "fluxent/controls/flux_tab_view_data.h"

#include <string.h>

/* WinUI TabView (TabView.xaml / _themeresources.xaml). The root paints the
 * content fill + the 1px seam line under the strip; the selected tab fills the
 * exact TabViewItem::UpdateTabGeometry shape — top corners 8 (OverlayCornerRadius)
 * with 4px curving-out bottom flares — in the content fill so it overdraws its
 * seam segment and reads as connected. Unselected tabs draw a 1px right-edge
 * divider (hidden around the active tab), hover/pressed chrome, icon, label
 * (collapsed in Compact), and a 32x24 close button shown when selected or
 * hovered. The add and overflow-scroll buttons are the same node type. */

#define TAB_CLOSE_GLYPH      L"\xE711" /* ChromeClose */
#define TAB_ADD_GLYPH        L"\xE710" /* Add */
#define TAB_SCROLL_DEC_GLYPH L"\xEDD9" /* CaretLeftSolid8 */
#define TAB_SCROLL_INC_GLYPH L"\xEDDA" /* CaretRightSolid8 */

static FluxColor tab_content_fill(FluxThemeColors const *t) { return t->solid_background_tertiary; }

/* Fill @p r with the top corners rounded and the bottom edge square. */
static void      tab_fill_top_rounded(FluxRenderContext const *rc, FluxRect const *r, float radius, FluxColor fill) {
	D2D1_RECT_F clip = flux_d2d_rect(r);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	FluxRect ext = {r->x, r->y, r->w, r->h + radius};
	flux_fill_rounded_rect(rc, &ext, radius, fill);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

/* The selected-tab shape from TabViewItem::UpdateTabGeometry: top corners @c
 * FLUX_TAB_CORNER, sides dropping to 4px curving-out flares that extend
 * FLUX_TAB_FLARE beyond each edge at the seam. */
static ID2D1PathGeometry *tab_selected_geometry(FluxRenderContext const *rc, FluxRect const *b) {
	ID2D1Factory *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
	if (!factory) return NULL;
	ID2D1PathGeometry *geo = NULL;
	ID2D1Factory_CreatePathGeometry(factory, &geo);
	ID2D1Factory_Release(factory);
	if (!geo) return NULL;

	ID2D1GeometrySink *sink = NULL;
	ID2D1PathGeometry_Open(geo, &sink);
	if (!sink) {
		ID2D1PathGeometry_Release(geo);
		return NULL;
	}

	float            f = FLUX_TAB_FLARE;
	float            c = FLUX_TAB_CORNER;
	float            x0 = b->x, x1 = b->x + b->w, y1 = b->y + b->h;
	D2D1_ARC_SEGMENT arc = {
	  {x0, y1 - f},
	  {f, f},
	  0.0f,
	  D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
	  D2D1_ARC_SIZE_SMALL
	};
	ID2D1GeometrySink_BeginFigure(sink, flux_point(x0 - f, y1), D2D1_FIGURE_BEGIN_FILLED);
	ID2D1GeometrySink_AddArc(sink, &arc); /* left flare curls up onto the side */
	ID2D1GeometrySink_AddLine(sink, flux_point(x0, b->y + c));
	arc = (D2D1_ARC_SEGMENT) {
	  {x0 + c, b->y},
	  {c, c},
	  0.0f,
	  D2D1_SWEEP_DIRECTION_CLOCKWISE,
	  D2D1_ARC_SIZE_SMALL
	};
	ID2D1GeometrySink_AddArc(sink, &arc); /* top-left corner */
	ID2D1GeometrySink_AddLine(sink, flux_point(x1 - c, b->y));
	arc = (D2D1_ARC_SEGMENT) {
	  {x1, b->y + c},
	  {c, c},
	  0.0f,
	  D2D1_SWEEP_DIRECTION_CLOCKWISE,
	  D2D1_ARC_SIZE_SMALL
	};
	ID2D1GeometrySink_AddArc(sink, &arc); /* top-right corner */
	ID2D1GeometrySink_AddLine(sink, flux_point(x1, y1 - f));
	arc = (D2D1_ARC_SEGMENT) {
	  {x1 + f, y1},
	  {f, f},
	  0.0f,
	  D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
	  D2D1_ARC_SIZE_SMALL
	};
	ID2D1GeometrySink_AddArc(sink, &arc); /* right flare curls down onto the seam */
	ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
	ID2D1GeometrySink_Close(sink);
	ID2D1GeometrySink_Release(sink);
	return geo;
}

static void tab_fill_selected(FluxRenderContext const *rc, FluxRect const *b, FluxColor fill) {
	ID2D1PathGeometry *geo = tab_selected_geometry(rc, b);
	if (!geo) {
		tab_fill_top_rounded(rc, b, FLUX_TAB_CORNER, fill);
		return;
	}
	flux_set_brush(rc, fill);
	ID2D1RenderTarget_FillGeometry(FLUX_RT(rc), ( ID2D1Geometry * ) geo, ( ID2D1Brush * ) rc->brush, NULL);
	ID2D1PathGeometry_Release(geo);
}

void flux_draw_tab_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FluxThemeColors const *t       = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               b       = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float                  seam    = b.y + FLUX_TAB_STRIP_H;

	FluxRect               content = {b.x, seam, b.w, flux_maxf(0.0f, b.h - FLUX_TAB_STRIP_H)};
	flux_fill_rect(rc, &content, tab_content_fill(t));
	float ly = seam - FLUX_STROKE_WIDTH * 0.5f;
	flux_draw_line(rc, &(FluxLineSpec) {b.x, ly, b.x + b.w, ly}, t->card_stroke_default, FLUX_STROKE_WIDTH);
}

/* Chrome shared by the (+) and overflow-scroll buttons (TabViewButton* /
 * TabViewScrollButton* states: transparent fill, subtle hover/pressed). */
static void tab_draw_button_box(
  FluxRenderContext const *rc, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t
) {
	if (state->enabled && state->pressed) flux_fill_rounded_rect(rc, box, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	else if (state->enabled && state->hovered)
		flux_fill_rounded_rect(rc, box, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
}

static void
tab_draw_add(FluxRenderContext const *rc, FluxRect const *b, FluxControlState const *state, FluxThemeColors const *t) {
	tab_draw_button_box(rc, b, state, t);
	FluxColor fg = !state->enabled ? t->text_disabled : (state->pressed ? t->text_secondary : t->text_primary);
	flux_button_draw_chevron(rc, b, TAB_ADD_GLYPH, FLUX_TAB_CLOSE_FONT, fg);
}

static void tab_draw_scroll(
  FluxRenderContext const *rc, FluxRect const *b, FluxControlState const *state, FluxThemeColors const *t, bool dec
) {
	tab_draw_button_box(rc, b, state, t);
	FluxColor fg = state->enabled ? t->text_secondary : t->text_disabled;
	flux_button_draw_chevron(rc, b, dec ? TAB_SCROLL_DEC_GLYPH : TAB_SCROLL_INC_GLYPH, FLUX_TAB_SCROLL_FONT, fg);
}

/* The CloseButton child node (32x24, glyph E711 @12, TabViewCloseButtonStyle):
 * its own hover/press state drives the subtle fill — no hit math needed. */
static void tab_draw_close(
  FluxRenderContext const *rc, FluxRect const *b, FluxControlState const *state, FluxThemeColors const *t
) {
	if (state->pressed) flux_fill_rounded_rect(rc, b, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	else if (state->hovered) flux_fill_rounded_rect(rc, b, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	FluxColor fg = state->pressed ? t->text_secondary : t->text_primary;
	flux_button_draw_chevron(rc, b, TAB_CLOSE_GLYPH, FLUX_TAB_CLOSE_FONT, fg);
}

/* TabViewItemHeaderBackground states: transparent rest, layer-on-mica hover and
 * pressed, content fill when selected. */
static void tab_draw_item_fill(
  FluxRenderContext const *rc, FluxRect const *b, FluxControlState const *state, bool selected, FluxThemeColors const *t
) {
	if (selected) {
		tab_fill_selected(rc, b, tab_content_fill(t));
		return;
	}
	if (state->enabled && state->pressed) tab_fill_top_rounded(rc, b, FLUX_TAB_CORNER, t->layer_on_mica_alt_default);
	else if (state->enabled && state->hovered)
		tab_fill_top_rounded(rc, b, FLUX_TAB_CORNER, t->layer_on_mica_alt_secondary);
}

/* 1px divider at the right edge, inset 8px top/bottom (TabViewItemSeparator). */
static void tab_draw_separator(FluxRenderContext const *rc, FluxRect const *b, FluxThemeColors const *t) {
	float x = b->x + b->w - FLUX_STROKE_WIDTH * 0.5f;
	flux_draw_line(
	  rc, &(FluxLineSpec) {x, b->y + FLUX_TAB_SEP_MARGIN_V, x, b->y + b->h - FLUX_TAB_SEP_MARGIN_V},
	  t->divider_stroke_default, FLUX_STROKE_WIDTH
	);
}

static void tab_draw_icon_label(
  FluxRenderContext const *rc, FluxRect const *b, FluxRenderSnapshot const *snap, FluxColor fg, bool selected
) {
	float          x     = b->x + FLUX_TAB_PAD_L;
	wchar_t const *glyph = flux_icon_lookup(snap->u.tab.icon_name);
	if (glyph) {
		FluxRect ib = {x, b->y, FLUX_TAB_ICON, b->h};
		flux_button_draw_chevron(rc, &ib, glyph, FLUX_TAB_ICON, fg);
		x += FLUX_TAB_ICON + FLUX_TAB_ICON_GAP;
	}

	float close_w    = snap->u.tab.tab_closable ? FLUX_TAB_CLOSE_MARGIN_L + FLUX_TAB_CLOSE_W + FLUX_TAB_PAD_R
	                                      : FLUX_TAB_PAD_R_NOCLOSE;
	float text_right = b->x + b->w - close_w;
	if (!snap->u.tab.label || !rc->text || text_right <= x) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_TAB_FONT;
	ts.font_weight = selected ? FLUX_FONT_SEMI_BOLD : FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = fg;
	FluxRect    tr = {x, b->y, text_right - x, b->h};
	/* Text drawing does not clip itself; pin the label to its column so narrow
	 * (MinWidth) tabs never run under the close button. */
	D2D1_RECT_F cl = flux_d2d_rect(&tr);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &cl, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	flux_text_draw(rc->text, FLUX_RT(rc), snap->u.tab.label, &tr, &ts);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

/* The strip's auxiliary buttons (add / close / overflow scroll) share the
 * item node type. */
static void tab_draw_aux(
  FluxRenderContext const *rc, FluxRect const *b, FluxControlState const *state, FluxThemeColors const *t,
  uint8_t kind
) {
	if (kind == FLUX_TAB_KIND_ADD) tab_draw_add(rc, b, state, t);
	else if (kind == FLUX_TAB_KIND_CLOSE) tab_draw_close(rc, b, state, t);
	else tab_draw_scroll(rc, b, state, t, kind == FLUX_TAB_KIND_SCROLL_DEC);
}

void flux_draw_tab_view_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               b = flux_snap_bounds(bounds, 1.0f, 1.0f);
	/* Hidden strip items (collapsed close/scroll buttons) are 0-sized, but glyph
	 * drawing centers into the rect regardless — skip them entirely. */
	if (b.w < 1.0f || b.h < 1.0f) return;

	if (snap->u.tab.tab_kind != FLUX_TAB_KIND_TAB) {
		tab_draw_aux(rc, &b, state, t, snap->u.tab.tab_kind);
		return;
	}

	bool selected = snap->u.tab.is_checked;
	/* The selected flares extend past the rect; clip everything else to the tab. */
	if (!selected) {
		D2D1_RECT_F cl = flux_d2d_rect(&b);
		ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &cl, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	}
	tab_draw_item_fill(rc, &b, state, selected, t);

	FluxColor fg = !state->enabled ? t->text_disabled : (state->pressed && !selected ? t->text_tertiary : t->text_primary);
	if (!selected && state->enabled && !state->hovered && !state->pressed) fg = t->text_secondary;
	tab_draw_icon_label(rc, &b, snap, fg, selected);

	if (snap->u.tab.tab_separator) tab_draw_separator(rc, &b, t);
	if (!selected) ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}
