#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_control_record_internal.h"
#include "controls/data/flux_colheader_data.h"
#include "render/flux_render_cache.h"

#include <math.h>

#define FLUX_REC_BEGIN()                \
	XentDlBuilder *b = xent_dl_begin(); \
	if (!b) return NULL

#define FLUX_REC_END(ok) return flux_rec_finish(b, ok)

#define COL_SORT_GLYPH   L"\xEDDB"

typedef struct ColDraw {
	FluxThemeColors const     *t;
	FluxColItemSnapshot const *col;
	FluxRect                   cell;
	bool                       hover_button;
	bool                       pressed;
	float                      angle;
} ColDraw;

static FluxColor col_alpha(FluxColor c, float k) {
	uint32_t a = ( uint32_t ) (( float ) (c.rgba & 0xffu) * k);
	c.rgba     = (c.rgba & 0xffffff00u) | (a > 255u ? 255u : a);
	return c;
}

static FluxRect col_button(FluxRect const *cell) {
	return (FluxRect) {
	  cell->x - FLUX_COL_CELL_OVERLAP, cell->y, cell->w - FLUX_COL_SPLITTER_W + 2.0f * FLUX_COL_CELL_OVERLAP, cell->h};
}

static FluxRect col_splitter(FluxRect const *cell) {
	return (FluxRect) {cell->x + cell->w - FLUX_COL_SPLITTER_W, cell->y, FLUX_COL_SPLITTER_W, cell->h};
}

static FluxColor col_fill(ColDraw const *c) {
	if (!c->col->sortable) return c->t->ctrl_fill_disabled;
	if (c->pressed) return c->t->ctrl_fill_tertiary;
	if (c->hover_button) return c->t->ctrl_alt_fill_tertiary;
	return (FluxColor) {0};
}

static bool col_label(XentDlBuilder *b, ColDraw const *c, FluxRect const *button) {
	float    icon = c->col->sort ? FLUX_COL_SORT_SIZE : 0.0f;
	FluxRect area = {
	  button->x + FLUX_COL_PAD_LEFT, button->y + FLUX_COL_PAD_TOP,
	  fmaxf(0.0f, button->w - 2.0f * FLUX_COL_PAD_LEFT - icon), button->h - FLUX_COL_PAD_TOP};
	XentTextStyle style = {
	  .font_size   = FLUX_COL_TEXT_SIZE,
	  .font_weight = 600u,
	  .h_align     = c->col->align,
	  .v_align     = 1u,
	  .max_lines   = 1u,
	  .trim        = XENT_TEXT_TRIM_CHAR_ELLIPSIS,
	};
	return !c->col->label || flux_rec_text_ex(b, &area, c->col->label, &style, c->t->text_tertiary);
}

static bool col_sort_icon(XentDlBuilder *b, ColDraw const *c, FluxRect const *button) {
	if (!c->col->sort) return true;
	float    right = button->x + button->w - FLUX_COL_PAD_LEFT;
	float    cy    = button->y + FLUX_COL_PAD_TOP + (button->h - FLUX_COL_PAD_TOP) * 0.5f;
	FluxRect box = {right - FLUX_COL_SORT_SIZE, cy - FLUX_COL_SORT_SIZE * 0.5f, FLUX_COL_SORT_SIZE, FLUX_COL_SORT_SIZE};
	float    cx  = box.x + box.w * 0.5f;
	float    rad = c->angle * 3.14159265f / 180.0f;
	float    cs  = cosf(rad);
	float    sn  = sinf(rad);
	XentAffine2 m = {cs, -sn, cx - cs * cx + sn * cy, sn, cs, cy - sn * cx - cs * cy};
	return xent_dl_pushtransform(b, m)
	    && flux_rec_glyph_wc(b, &box, COL_SORT_GLYPH, FLUX_COL_SORT_SIZE, c->t->text_tertiary)
	    && xent_dl_poptransform(b);
}

static bool col_body(XentDlBuilder *b, ColDraw const *c) {
	FluxRect  button = col_button(&c->cell);
	FluxColor fill   = col_fill(c);
	if (fill.rgba & 0xffu && !flux_rec_fill_rect(b, &button, fill)) return false;
	return col_label(b, c, &button) && col_sort_icon(b, c, &button);
}

static bool col_divider(XentDlBuilder *b, FluxRenderCtx const *rc, ColDraw const *c) {
	if (!c->col->resizable) return true;
	FluxRect  s     = col_splitter(&c->cell);
	FluxColor color = col_alpha(c->t->ctrl_stroke_default, FLUX_COL_SPLIT_OPACITY);
	return flux_rec_strokevs(b, rc, s.x + s.w * 0.5f, s.y, s.y + s.h, 1.0f, color);
}

static bool col_focus(XentDlBuilder *b, ColDraw const *c, FluxControlState const *state) {
	if (!state->focused || !c->col->focus_part) return true;
	FluxRect r = c->col->focus_part == FLUX_COL_PART_SPLITTER ? col_splitter(&c->cell) : col_button(&c->cell);
	return flux_rec_focus_rect(b, c->t, &r, 0.0f);
}

static float col_sort_angle(FluxRenderCtx const *rc, FluxRenderSnap const *snap, uint8_t sort) {
	float            target = sort == XTK_SORT_DESCENDING ? -180.0f : 0.0f;
	FluxVisualState *vs     = flux_rec_vstate(rc, snap, sort);
	if (!vs) return target;
	if (vs->since == rc->now) vs->aux [1] = vs->aux [0];
	float p = ( float ) ((rc->now - vs->since) / FLUX_COL_SORT_SECONDS);
	if (p >= 1.0f || !sort) {
		vs->aux [0] = sort ? target : vs->aux [0];
		return target;
	}
	vs->aux [0] = vs->aux [1] + (target - vs->aux [1]) * p;
	flux_rec_note_animating(rc, true);
	return vs->aux [0];
}

static ColDraw col_draw(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxColItemSnapshot const *col, FluxRect const *cell,
  FluxControlState const *state
) {
	float split = col->resizable ? cell->w - FLUX_COL_SPLITTER_W : cell->w;
	return (ColDraw) {
	  .t            = flux_rec_theme(rc),
	  .col          = col,
	  .cell         = *cell,
	  .hover_button = state->hovered && snap->hover_local_x >= 0.0f && snap->hover_local_x < split,
	  .pressed      = state->pressed && col->press_part == FLUX_COL_PART_BUTTON,
	  .angle        = col_sort_angle(rc, snap, col->sort),
	};
}

XentDl *flux_colitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FLUX_REC_BEGIN();
	ColDraw c = col_draw(rc, snap, &snap->u.col_item, bounds, state);
	FLUX_REC_END(col_body(b, &c) && col_divider(b, rc, &c) && col_focus(b, &c, state));
}

XentDl *flux_colheader_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) snap;
	( void ) state;
	FLUX_REC_BEGIN();
	FluxThemeColors const *t = flux_rec_theme(rc);
	FLUX_REC_END(flux_rec_linehs(
	  b, rc, bounds->x, bounds->x + bounds->w, bounds->y + bounds->h - 1.0f, 1.0f, t->divider_stroke_default
	));
}

XentDl *flux_colheader_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxColHeaderSnapshot const *h = &snap->u.col_header;
	if (!h->ghost) return NULL;
	FLUX_REC_BEGIN();
	FluxRect         cell  = {bounds->x + h->ghost_x, bounds->y, h->ghost_w, bounds->h};
	FluxControlState press = {.pressed = 1, .enabled = 1};
	ColDraw          c     = col_draw(rc, snap, &h->item, &cell, &press);
	c.angle                = h->item.sort == XTK_SORT_DESCENDING ? -180.0f : 0.0f;
	FLUX_REC_END(
	  xent_dl_pushopacity(b, FLUX_COL_GHOST_OPACITY)
	  && flux_rec_fill_rect(b, &cell, c.t->solid_background_secondary)
	  && col_body(b, &c)
	  && col_divider(b, rc, &c)
	  && xent_dl_popopacity(b)
	);
}
