#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/behavior/flux_nav_internal.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/behavior/flux_breadcrumb_internal.h"

#include <xtk/xtk.h>

#include <math.h>
#include <string.h>

#define FLUX_NAV_PANE_SHADOW_W  9.0f
#define FLUX_NAV_CONTENT_RADIUS 8.0f
#define FLUX_NAV_ICON_COLLAPSED 8.0f
#define NAV_LEFT_TEXT_LEAD      4.0f
#define NAV_LEFT_TEXT_TRAIL     22.0f
#define NAV_LEFT_CHEVRON_COL    26.0f
#define NAV_TOP_ROOT_MARGIN_X   4.0f
#define NAV_TOP_ROOT_MARGIN_Y   2.0f
#define NAV_TOP_ICON_LEAD       12.0f
#define NAV_TOP_TEXT_LEAD       8.0f
#define NAV_TOP_ICON_ONLY       36.0f
#define NAV_TOP_ICON_ONLY_SLOT  40.0f
#define NAV_CHEVRON_BOX         40.0f
#define NAV_OVF_ICON_LEAD       16.0f
#define NAV_OVF_TEXT_LEAD       12.0f
#define NAV_OVF_TEXT_TRAIL      20.0f
#define NAV_OVF_CHEVRON_END     6.0f

static FluxRect rec_inset(FluxRect const *b, float hx, float hy) {
	return (FluxRect) {b->x + hx, b->y + hy, b->w - 2.0f * hx, b->h - 2.0f * hy};
}

static FluxColor nav_item_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (selected && state->enabled && state->hovered) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static FluxColor list_item_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) {
		bool highlighted = state->enabled && (selected || state->hovered || state->pressed);
		return highlighted ? t->contrast.highlight : t->contrast.window;
	}
	if (state->enabled && state->pressed) return selected ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	if (selected && state->enabled && state->hovered) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static FluxColor list_accent_alpha(FluxThemeColors const *t, uint8_t alpha) {
	FluxColor c = t->accent_default;
	c.rgba      = (c.rgba & 0xffffff00u) | alpha;
	return c;
}

static FluxColor list_state_pick(
  FluxControlState const *state, FluxColor disabled, FluxColor pressed, FluxColor hovered, FluxColor rest
) {
	if (!state->enabled) return disabled;
	if (state->pressed) return pressed;
	if (state->hovered) return hovered;
	return rest;
}

static FluxColor
list_checkbox_fill(FluxThemeColors const *t, FluxControlState const *state, bool checked, bool on_image) {
	if (checked)
		return list_state_pick(state, t->accent_disabled, t->accent_tertiary, t->accent_secondary, t->accent_default);
	if (on_image)
		return list_state_pick(
		  state, t->ctrl_on_image_disabled, t->ctrl_on_image_tertiary, t->ctrl_on_image_secondary,
		  t->ctrl_on_image_default
		);
	return t->ctrl_alt_fill_secondary;
}

static bool list_rec_contrast_checkbox(
  XentDlBuilder *b, FluxContrastColors const *p, FluxRect const *box, FluxControlState const *state, bool checked,
  bool on_image
) {
	FluxColor fill = checked ? p->window : p->button_face;
	if (!flux_rec_fill_rounded(b, box, 3.0f, fill)) return false;
	if (!checked) return flux_rec_stroke_rounded(b, box, 3.0f, p->button_text, 1.0f);
	FluxColor pressed = on_image ? p->window : p->window_text;
	FluxColor glyph   = list_state_pick(state, p->gray_text, pressed, p->window_text, p->window_text);
	return flux_rec_glyph_wc(b, box, L"\xE73E", 16.0f, glyph);
}

static bool list_rec_checkbox(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state, bool checked,
  bool on_image
) {
	if (t->contrast.active) return list_rec_contrast_checkbox(b, &t->contrast, box, state, checked, on_image);
	bool ok = flux_rec_fill_rounded(b, box, 3.0f, list_checkbox_fill(t, state, checked, on_image));
	if (!ok) return false;
	if (!checked) {
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		return flux_rec_stroke_rounded(b, box, 3.0f, stroke, 1.0f);
	}
	FluxColor glyph = !state->enabled ? t->text_on_accent_disabled
	                : state->pressed  ? t->text_on_accent_secondary
	                                  : t->text_on_accent_primary;
	return flux_rec_glyph_wc(b, box, L"\xE73E", 16.0f, glyph);
}

static FluxColor listbox_item_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) {
		bool highlighted = state->enabled && (selected || state->hovered || state->pressed);
		return highlighted ? t->contrast.highlight : (FluxColor) {0};
	}
	if (selected && state->enabled && state->pressed) return list_accent_alpha(t, 0xe6);
	if (selected && state->enabled && state->hovered) return list_accent_alpha(t, 0xcc);
	if (selected) return list_accent_alpha(t, 0x99);
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static bool
list_rec_pill(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, FluxControlState const *state) {
	float height = bounds->h <= 16.0f ? bounds->h : fmaxf(16.0f, bounds->h - 40.0f);
	if (state->pressed) height -= 6.0f;
	if (height < 3.0f) return true;
	FluxThemeColors const *t        = flux_rec_theme(rc);
	FluxColor              fill     = t->contrast.active ? t->contrast.highlight_text : t->accent_default;
	FluxColor              disabled = t->contrast.active ? t->contrast.gray_text : t->accent_disabled;
	float                  x        = flux_rec_snapx(rc, bounds->x + 4.0f);
	FluxRect               pill     = {x, bounds->y + (bounds->h - height) * 0.5f, 3.0f, height};
	return flux_rec_fill_rounded(b, &pill, 1.5f, state->enabled ? fill : disabled);
}

static bool list_rec_list_cell(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxThemeColors const *t, FluxRenderSnap const *snap,
  FluxRect const *bounds, FluxControlState const *state
) {
	bool     selected = snap->u.list_item.is_selected;
	FluxRect ib       = {bounds->x + 4.0f, bounds->y + 2.0f, bounds->w - 8.0f, bounds->h - 4.0f};
	if (ib.w < 1.0f || ib.h < 1.0f) return true;

	bool      ok   = true;
	FluxColor fill = list_item_fill(t, state, selected);
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, fill);

	if (ok && selected && !snap->u.list_item.multi) ok = list_rec_pill(b, rc, bounds, state);
	if (ok && snap->u.list_item.multi) {
		FluxRect box = {bounds->x + 14.0f, bounds->y + (bounds->h - 20.0f) * 0.5f, 20.0f, 20.0f};
		ok           = list_rec_checkbox(b, t, &box, state, selected, false);
	}
	return ok;
}

static bool list_rec_grid_contrast_outline(
  XentDlBuilder *b, FluxContrastColors const *p, FluxRect const *r, FluxControlState const *state, bool selected
) {
	if (!selected) {
		if (!state->enabled || !state->hovered) return true;
		return flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS, p->highlight, 1.0f);
	}
	FluxColor border = state->enabled ? p->highlight : p->gray_text;
	FluxRect  inner  = rec_inset(r, 2.0f, 2.0f);
	if (!flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS, border, 2.0f)) return false;
	if (inner.w < 1.0f || inner.h < 1.0f) return true;
	return flux_rec_stroke_rounded(b, &inner, 3.0f, p->window_text, 1.0f);
}

static bool list_rec_grid_outline(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *r, FluxControlState const *state, bool selected
) {
	if (t->contrast.active) return list_rec_grid_contrast_outline(b, &t->contrast, r, state, selected);
	if (!selected) {
		return !state->enabled
		    || !state->hovered
		    || flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS, flux_color_rgba(0, 0, 0, 0x37), 1.0f);
	}
	FluxColor ring  = !state->enabled ? t->accent_disabled
	                : state->pressed  ? t->accent_tertiary
	                : state->hovered  ? t->accent_secondary
	                                  : t->accent_default;
	FluxRect  inner = rec_inset(r, 2.0f, 2.0f);
	return flux_rec_stroke_rounded(b, r, FLUX_CORNER_RADIUS, ring, 2.0f)
	    && flux_rec_stroke_rounded(b, &inner, 3.0f, t->ctrl_solid_fill_default, 1.0f);
}

static FluxColor list_grid_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) return t->contrast.window;
	if (selected) return state->pressed ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	return list_item_fill(t, state, false);
}

static bool list_rec_grid_cell(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool     selected = snap->u.list_item.is_selected;
	FluxRect ib       = {bounds->x, bounds->y, bounds->w - 4.0f, bounds->h - 4.0f};
	if (ib.w < 2.0f || ib.h < 2.0f) return true;

	FluxColor fill = list_grid_fill(t, state, selected);
	if (!flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, fill)) return false;
	if (!list_rec_grid_outline(b, t, &ib, state, selected)) return false;
	if (!snap->u.list_item.multi) return true;
	FluxRect box = {ib.x + ib.w - 22.0f, ib.y + 2.0f, 20.0f, 20.0f};
	return list_rec_checkbox(b, t, &box, state, selected, true);
}

static FluxColor rec_scale_alpha(FluxColor c, float scale) {
	uint32_t a = ( uint32_t ) (( float ) (c.rgba & 0xffu) * scale + 0.5f);
	if (a > 0xffu) a = 0xffu;
	FluxColor out = c;
	out.rgba      = (c.rgba & 0xffffff00u) | a;
	return out;
}

static bool items_rec_checkbox(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state, bool checked,
  float alpha
) {
	FluxColor fill = checked ? (state->enabled ? t->accent_default : t->accent_disabled)
	                         : (state->enabled ? t->ctrl_on_image_default : t->ctrl_on_image_disabled);
	bool      ok   = flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, rec_scale_alpha(fill, alpha));
	if (!ok) return false;
	if (!checked) {
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		return flux_rec_stroke_rounded(b, box, FLUX_CORNER_RADIUS, rec_scale_alpha(stroke, alpha), 1.0f);
	}
	FluxColor glyph = state->enabled ? t->text_on_accent_primary : t->text_on_accent_disabled;
	return flux_rec_glyph_wc(b, box, L"\xE73E", 12.0f, rec_scale_alpha(glyph, alpha));
}

static bool items_rec_selection(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, FluxControlState const *state, float alpha
) {
	FluxColor accent = state->enabled ? t->accent_default : t->accent_disabled;
	if (!flux_rec_stroke_rounded(b, bounds, FLUX_CORNER_RADIUS, rec_scale_alpha(accent, alpha), 3.0f)) return false;
	FluxRect inner = {bounds->x + 2.0f, bounds->y + 2.0f, bounds->w - 4.0f, bounds->h - 4.0f};
	if (inner.w <= 1.0f || inner.h <= 1.0f) return true;
	return flux_rec_stroke_rounded(
	  b, &inner, FLUX_CORNER_RADIUS - 2.0f, rec_scale_alpha(t->ctrl_solid_fill_default, alpha), 1.0f
	);
}

static bool items_rec_multi(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, FluxControlState const *state, bool checked,
  float alpha
) {
	FluxRect box = {bounds->x + bounds->w - 24.0f, bounds->y + 2.0f, 20.0f, 20.0f};
	return box.x <= bounds->x || items_rec_checkbox(b, t, &box, state, checked, alpha);
}

static FluxColor items_rec_fill(FluxThemeColors const *t, FluxControlState const *state) {
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static bool list_rec_items_container(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool     selected = snap->u.list_item.is_selected;
	bool     multi    = snap->u.list_item.multi;
	float    alpha    = state->enabled ? 1.0f : 0.3f;
	FluxRect ib       = {bounds->x + 2.0f, bounds->y + 2.0f, bounds->w - 4.0f, bounds->h - 4.0f};
	if (ib.w < 2.0f || ib.h < 2.0f) return true;

	FluxColor fill = items_rec_fill(t, state);
	bool      ok   = true;
	if (flux_color_af(fill) > 0.0f)
		ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, rec_scale_alpha(fill, alpha));

	if (ok && selected) ok = items_rec_selection(b, t, &ib, state, alpha);
	if (ok && multi) ok = items_rec_multi(b, t, &ib, state, selected, alpha);
	return ok;
}

bool flux_rec_listitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t    = flux_rec_theme(rc);
	XtkListKind            kind = ( XtkListKind ) snap->u.list_item.kind;
	switch (kind) {
	case XTK_LIST_KIND_LIST_BOX :
		return flux_rec_fill_rect(b, bounds, listbox_item_fill(t, state, snap->u.list_item.is_selected));
	case XTK_LIST_KIND_GRID     : return list_rec_grid_cell(b, t, snap, bounds, state);
	case XTK_LIST_KIND_ITEMS    : return list_rec_items_container(b, t, snap, bounds, state);
	case XTK_LIST_KIND_REPEATER : return true;
	default                     : return list_rec_list_cell(b, rc, t, snap, bounds, state);
	}
}

static bool collection_focus(XentDlBuilder *b, FluxRect const *bounds, float radius, FluxColor outer, FluxColor inner) {
	if (bounds->w <= 4.0f || bounds->h <= 4.0f) return true;
	FluxRect inset = rec_inset(bounds, 2.0f, 2.0f);
	return flux_rec_stroke_rounded(b, bounds, radius, outer, 2.0f)
	    && flux_rec_stroke_rounded(b, &inset, fmaxf(0.0f, radius - 2.0f), inner, 1.0f);
}

static bool grid_focus(XentDlBuilder *b, FluxContrastColors const *p, FluxRect const *bounds) {
	FluxRect cell    = {bounds->x, bounds->y, bounds->w - 4.0f, bounds->h - 4.0f};
	FluxRect outline = rec_inset(&cell, -3.0f, -3.0f);
	return collection_focus(b, &outline, FLUX_CORNER_RADIUS, p->window_text, p->window);
}

bool flux_rec_listitem_focus(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	FluxContrastColors const *p = &flux_rec_theme(rc)->contrast;
	switch (snap->u.list_item.kind) {
	case XTK_LIST_KIND_GRID     : return grid_focus(b, p, bounds);
	case XTK_LIST_KIND_LIST_BOX : return collection_focus(b, bounds, 0.0f, p->button_text, p->button_face);
	case XTK_LIST_KIND_LIST     : {
		FluxRect outline = rec_inset(bounds, 1.0f, 1.0f);
		return collection_focus(b, &outline, FLUX_CORNER_RADIUS, p->window_text, p->window);
	}
	default : return true;
	}
}

static FluxColor tree_row_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	return list_item_fill(t, state, selected);
}

static FluxColor tree_contrast_ink(FluxContrastColors const *p, FluxControlState const *state, bool selected) {
	if (!state->enabled) return p->gray_text;
	bool interacting = state->hovered || state->pressed;
	return selected != interacting ? p->highlight : p->button_text;
}

static FluxColor tree_row_fg(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) return tree_contrast_ink(&t->contrast, state, selected);
	if (!state->enabled) return t->text_disabled;
	if (state->pressed) return t->text_secondary;
	return t->text_primary;
}

static FluxColor tree_contrast_border(FluxContrastColors const *p, FluxControlState const *state, bool selected) {
	if (!selected) return state->enabled && state->hovered && !state->pressed ? p->highlight : p->window;
	return list_state_pick(state, p->gray_text, p->button_face, p->button_text, p->highlight);
}

static bool tree_rec_multi_border(XentDlBuilder *b, FluxRect const *row, FluxContrastColors const *p) {
	FluxRect inner = rec_inset(row, 1.0f, 1.0f);
	if (inner.w <= 1.0f || inner.h <= 1.0f) return true;
	return flux_rec_fill_rounded(b, &inner, FLUX_CORNER_RADIUS, p->window)
	    && flux_rec_stroke_rounded(b, &inner, FLUX_CORNER_RADIUS, p->button_text, 1.0f);
}

static bool tree_rec_contrast_row(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *row, FluxControlState const *state, bool multi,
  bool selected
) {
	FluxContrastColors const *p    = &flux_rec_theme(rc)->contrast;
	FluxColor                 fill = p->window;
	if (selected && !multi && state->enabled && (state->hovered || state->pressed)) fill = p->button_face;
	FluxColor border = tree_contrast_border(p, state, selected && !multi);
	if (!flux_rec_fill_rounded(b, row, FLUX_CORNER_RADIUS, fill)) return false;
	if (!flux_rec_stroke_rounded(b, row, FLUX_CORNER_RADIUS, border, 1.0f)) return false;
	if (!selected) return true;
	if (multi) return tree_rec_multi_border(b, row, p);
	FluxRect pill
	  = {flux_rec_snapx(rc, row->x), row->y + (row->h - FLUX_TREE_PILL_H) * 0.5f, FLUX_TREE_PILL_W, FLUX_TREE_PILL_H};
	return flux_rec_fill_rounded(b, &pill, 2.0f, tree_contrast_ink(p, state, true));
}

static FluxCheckState tree_check_state(uint8_t sel_state) {
	return sel_state == FLUX_TREE_SEL_SELECTED ? FLUX_CHECK_CHECKED
	     : sel_state == FLUX_TREE_SEL_PARTIAL  ? FLUX_CHECK_INDETERMINATE
	                                           : FLUX_CHECK_UNCHECKED;
}

static bool tree_rec_checkbox(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, float ix,
  FluxControlState const *st, uint8_t sel_state
) {
	float            col_x = ix + FLUX_TREE_CHECK_MARGIN_L;
	float            hx    = snap->hover_local_x + bounds->x;
	bool             over  = snap->hover_local_x >= 0.0f && hx >= col_x && hx < col_x + FLUX_TREE_CHECK_W;
	FluxControlState cs    = *st;
	cs.hovered             = st->hovered && over;
	cs.pressed             = st->pressed && over;
	FluxRect box = {col_x, bounds->y + (bounds->h - FLUX_CHECKBOX_SIZE) * 0.5f, FLUX_CHECKBOX_SIZE, FLUX_CHECKBOX_SIZE};
	return flux_rec_checkbox_box(b, rc, snap, &box, &cs, tree_check_state(sel_state));
}

static bool tree_rec_background(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *ib, FluxControlState const *st, bool multi, bool selected
) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (t->contrast.active) return tree_rec_contrast_row(b, rc, ib, st, multi, selected);
	FluxColor fill = tree_row_fill(t, st, selected && !multi);
	bool      ok   = true;
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, ib, FLUX_CORNER_RADIUS, fill);
	if (!ok || !selected) return ok;
	if (multi) return flux_rec_fill_rounded(b, ib, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	FluxRect pill = {ib->x, ib->y + (ib->h - FLUX_TREE_PILL_H) * 0.5f, FLUX_TREE_PILL_W, FLUX_TREE_PILL_H};
	pill.x        = flux_rec_snapx(rc, pill.x);
	return flux_rec_fill_rounded(b, &pill, FLUX_TREE_PILL_R, st->enabled ? t->accent_default : t->text_disabled);
}

static bool tree_rec_content(
  XentDlBuilder *b, FluxTreeItemSnapshot const *ti, FluxRect const *cr, float chevron_x, float text_x, FluxColor fg
) {
	bool ok = true;
	if (ti->flags & FLUX_TREE_ROW_HAS_CHILDREN) {
		FluxRect       cb   = {chevron_x, cr->y, FLUX_TREE_CHEVRON_BOX, cr->h};
		wchar_t const *chev = (ti->flags & FLUX_TREE_ROW_EXPANDED) ? L"\xE70D" : L"\xE76C";
		ok                  = flux_rec_glyph_wc(b, &cb, chev, FLUX_TREE_CHEVRON_FONT, fg);
	}
	if (ok && ti->glyph) {
		wchar_t  glyph [2]  = {( wchar_t ) ti->glyph, 0};
		FluxRect gb         = {text_x, cr->y, FLUX_TREE_ICON_GLYPH, cr->h};
		ok                  = flux_rec_glyph_wc(b, &gb, glyph, FLUX_TREE_ICON_GLYPH, fg);
		text_x             += FLUX_TREE_ICON_ADVANCE;
	}
	if (!ok || !ti->label || !ti->label [0]) return ok;
	FluxRect tr = {text_x, cr->y, cr->x + cr->w - text_x - 4.0f, cr->h};
	return tr.w <= 0.0f || flux_rec_text(b, &tr, ti->label, FLUX_TREE_FONT, 400u, fg);
}

static bool tree_rec_row_content(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *row,
  FluxControlState const *state, bool multi
) {
	FluxTreeItemSnapshot const *ti        = &snap->u.tree_item;
	float                       ix        = row->x + FLUX_TREE_INDENT * ( float ) ti->depth;
	float                       pt        = multi ? 0.0f : FLUX_TREE_CONTENT_PAD_T;
	float                       pb        = multi ? 0.0f : FLUX_TREE_CONTENT_PAD_B;
	FluxRect                    cr        = {row->x, row->y + pt, row->w, row->h - pt - pb};
	float                       chevron_x = ix + (multi ? FLUX_TREE_MULTI_CHEVRON_X : FLUX_TREE_CHEVRON_PAD);
	float                       text_x = multi ? chevron_x + FLUX_TREE_CHEVRON_ZONE_MULTI : ix + FLUX_TREE_CHEVRON_ZONE;
	if (multi && !tree_rec_checkbox(b, rc, snap, row, ix, state, ti->sel_state)) return false;
	bool selected = !multi && ti->sel_state == FLUX_TREE_SEL_SELECTED;
	return tree_rec_content(b, ti, &cr, chevron_x, text_x, tree_row_fg(flux_rec_theme(rc), state, selected));
}

bool flux_rec_treeitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxTreeItemSnapshot const *ti = &snap->u.tree_item;
	FluxControlState            st = *state;
	if (ti->flags & FLUX_TREE_ROW_DISABLED) st.enabled = 0;

	bool     multi = (ti->flags & FLUX_TREE_ROW_MULTI) != 0;
	FluxRect ib    = {
	  bounds->x + FLUX_TREE_ROW_MARGIN_H, bounds->y + FLUX_TREE_ROW_MARGIN_V, bounds->w - 2.0f * FLUX_TREE_ROW_MARGIN_H,
	  bounds->h - 2.0f * FLUX_TREE_ROW_MARGIN_V};
	if (ib.w < 1.0f || ib.h < 1.0f) return true;
	if (!tree_rec_background(b, rc, &ib, &st, multi, ti->sel_state == FLUX_TREE_SEL_SELECTED)) return false;

	if (!tree_rec_row_content(b, rc, snap, &ib, &st, multi)) return false;
	if (!st.focused || !st.enabled) return true;
	FluxRect               fr = {ib.x, ib.y - 1.0f, ib.w, ib.h + 2.0f};
	FluxThemeColors const *t  = flux_rec_theme(rc);
	if (t->contrast.active)
		return collection_focus(b, &fr, FLUX_CORNER_RADIUS, t->focus_stroke_outer, t->focus_stroke_inner);
	return flux_rec_focus_rect(b, t, &fr, FLUX_CORNER_RADIUS);
}

static bool nav_rec_shadow(XentDlBuilder *b, FluxRect const *pb, float opacity) {
	if (opacity <= 0.01f) return true;
	float    a  = 0.22f * opacity;
	FluxRect sh = {pb->x + pb->w, pb->y, FLUX_NAV_PANE_SHADOW_W, pb->h};
	return flux_rec_fillgrad(
	  b, &sh, sh.x, sh.y, sh.x + sh.w, sh.y, 0.0f, 1.0f, flux_color_rgba(0, 0, 0, ( int ) (a * 255.0f + 0.5f)),
	  flux_color_rgba(0, 0, 0, 0)
	);
}

static bool nav_rec_content_layer(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *root, FluxRenderSnap const *snap,
  FluxThemeColors const *t
) {
	float    cx    = snap->u.nav.nav_content_x;
	float    cy    = snap->u.nav.nav_content_y;
	FluxRect raw   = {root->x + cx, root->y + cy, root->w - cx, root->h - cy};
	FluxRect layer = flux_rec_snap_bounds(rc, &raw);
	if (layer.w < 1.0f || layer.h < 1.0f) return true;
	if (t->contrast.active) return flux_rec_fill_rect(b, &layer, t->contrast.window);
	if (snap->u.nav.nav_top || cx < 0.5f)
		return flux_rec_fill_rect(b, &layer, t->layer_default)
		    && flux_rec_line_h(
			  b, layer.x, layer.x + layer.w, layer.y + FLUX_STROKE_WIDTH * 0.5f, FLUX_STROKE_WIDTH,
			  t->card_stroke_default
			);

	/* Same as cards: the fill stops inside the 1px border so the translucent stroke shows against the backdrop. */
	float    r     = FLUX_NAV_CONTENT_RADIUS;
	float    w     = flux_rec_hairline(rc);
	FluxRect ext   = {layer.x, layer.y, layer.w + r, layer.h + r};
	FluxRect inner = {ext.x + w, ext.y + w, ext.w - w * 2.0f, ext.h - w * 2.0f};
	bool     ok    = xent_dl_pushcliprect(b, flux_rec_xrect(&layer));
	if (ok) ok = flux_rec_fill_rounded(b, &inner, r > w ? r - w : 0.0f, t->layer_default);
	if (ok) ok = flux_rec_stroke_rounded(b, &ext, r - w * 0.5f, t->card_stroke_default, w);
	return xent_dl_popclip(b) && ok;
}

bool flux_rec_nav(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxRect               pb = rec_inset(bounds, 1.0f, 1.0f);
	if (!snap->u.nav.nav_is_pane) return nav_rec_content_layer(b, rc, &pb, snap, t);
	if (t->contrast.active) {
		FluxColor fill = snap->u.nav.nav_pane_solid ? t->contrast.window : t->contrast.button_face;
		return flux_rec_fill_rect(b, &pb, fill);
	}

	bool ok = nav_rec_shadow(b, &pb, snap->u.nav.nav_shadow_opacity);
	if (!ok || !snap->u.nav.nav_pane_solid) return ok;
	ok = flux_rec_fill_rect(b, &pb, t->solid_background);
	if (!ok) return false;
	if (snap->u.nav.nav_top)
		return flux_rec_line_h(
		  b, pb.x, pb.x + pb.w, pb.y + pb.h - FLUX_STROKE_WIDTH * 0.5f, FLUX_STROKE_WIDTH, t->divider_stroke_default
		);
	return flux_rec_strokev(
	  b, pb.x + pb.w - FLUX_STROKE_WIDTH * 0.5f, pb.y, pb.y + pb.h, FLUX_STROKE_WIDTH, t->divider_stroke_default
	);
}

static FluxColor nav_indicator_color(FluxThemeColors const *t) {
	return t->contrast.active ? t->contrast.highlight_text : t->accent_default;
}

static bool
nav_rec_separator(XentDlBuilder *b, FluxRect const *ib, FluxRenderSnap const *snap, FluxThemeColors const *t) {
	if (snap->u.nav.nav_top) {
		float x  = ib->x + 3.0f + 0.5f;
		float y0 = ib->y + (ib->h - FLUX_NAV_SEP_TOP_LINE_H) * 0.5f;
		return flux_rec_strokev(b, x, y0, y0 + FLUX_NAV_SEP_TOP_LINE_H, 1.0f, t->divider_stroke_default);
	}
	float y = ib->y + 3.0f + 0.5f;
	return flux_rec_line_h(b, ib->x, ib->x + ib->w, y, 1.0f, t->divider_stroke_default);
}

static bool nav_rec_header(XentDlBuilder *b, FluxRect const *ib, FluxRenderSnap const *snap, FluxThemeColors const *t) {
	if (!snap->u.nav.label) return true;
	float    pad = snap->u.nav.nav_top ? 12.0f : FLUX_NAV_HEADER_PAD - FLUX_NAV_ITEM_MARGIN_H;
	FluxRect tr  = {ib->x + pad, ib->y, ib->w - pad * 2.0f, ib->h};
	if (tr.w <= 0.0f) return true;
	return flux_rec_text(b, &tr, snap->u.nav.label, FLUX_NAV_HEADER_FONT, FLUX_FONT_SEMI_BOLD, t->text_secondary);
}

static FluxColor nav_rec_fg(FluxThemeColors const *t, FluxControlState const *state) {
	if (!state->enabled) return t->text_disabled;
	return state->pressed ? t->text_secondary : t->text_primary;
}

static FluxColor nav_rec_top_fill(FluxThemeColors const *t, FluxControlState const *state) {
	if (!state->enabled) return flux_color_rgba(0, 0, 0, 0);
	if (state->pressed) return t->subtle_fill_tertiary;
	return state->hovered ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0);
}

typedef struct NavChrome {
	FluxColor fill;
	FluxColor text;
} NavChrome;

static NavChrome
nav_contrast_chrome(FluxContrastColors const *p, FluxControlState const *state, bool selected, bool top) {
	FluxColor clear = flux_color_rgba(0, 0, 0, 0);
	if (!state->enabled) return (NavChrome) {selected ? clear : p->button_face, p->gray_text};
	bool active = state->pressed || state->hovered;
	if (top && selected && active) return (NavChrome) {clear, p->button_text};
	if (selected || active) return (NavChrome) {p->highlight, p->highlight_text};
	return (NavChrome) {p->button_face, p->button_text};
}

static NavChrome nav_chrome(FluxThemeColors const *t, FluxControlState const *state, bool selected, bool top) {
	if (t->contrast.active) return nav_contrast_chrome(&t->contrast, state, selected, top);
	FluxColor fill = top ? nav_rec_top_fill(t, state) : nav_item_fill(t, state, selected);
	return (NavChrome) {fill, nav_rec_fg(t, state)};
}

static bool nav_rec_fill(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor fill) {
	if (flux_color_af(fill) <= 0.0f) return true;
	return radius > 0.0f ? flux_rec_fill_rounded(b, r, radius, fill) : flux_rec_fill_rect(b, r, fill);
}

static bool
nav_rec_top_icon_only(XentDlBuilder *b, FluxRect const *slot, wchar_t const *glyph, FluxColor fill, FluxColor fg) {
	FluxRect box = {
	  slot->x + (NAV_TOP_ICON_ONLY_SLOT - NAV_TOP_ICON_ONLY) * 0.5f, slot->y + (slot->h - NAV_TOP_ICON_ONLY) * 0.5f,
	  NAV_TOP_ICON_ONLY, NAV_TOP_ICON_ONLY};
	return nav_rec_fill(b, &box, FLUX_CORNER_RADIUS, fill)
	    && flux_rec_glyph_wc(b, &box, glyph, FLUX_NAV_ICON_GLYPH, fg);
}

static bool nav_rec_label(XentDlBuilder *b, FluxRect const *tr, char const *label, FluxColor fg) {
	if (!label || tr->w <= 0.0f) return true;
	return flux_rec_text(b, tr, label, FLUX_NAV_LABEL_FONT, 400u, fg);
}

static bool
nav_rec_icon_at(XentDlBuilder *b, float *x, FluxRect const *row, wchar_t const *glyph, float gap, FluxColor fg) {
	if (!glyph) return true;
	FluxRect icon  = {*x, row->y, FLUX_NAV_ICON_GLYPH, row->h};
	*x            += FLUX_NAV_ICON_GLYPH + gap;
	return flux_rec_glyph_wc(b, &icon, glyph, FLUX_NAV_ICON_GLYPH, fg);
}

static bool nav_rec_top_item(
  XentDlBuilder *b, FluxRect const *slot, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRenderCtx const *rc
) {
	NavChrome      style = nav_chrome(flux_rec_theme(rc), state, snap->u.nav.is_checked, true);
	wchar_t const *glyph = flux_icon_lookup(snap->u.nav.icon_name);
	if (glyph && !snap->u.nav.label) return nav_rec_top_icon_only(b, slot, glyph, style.fill, style.text);
	FluxRect lr   = rec_inset(slot, NAV_TOP_ROOT_MARGIN_X, NAV_TOP_ROOT_MARGIN_Y);
	float    x    = lr.x + NAV_TOP_ICON_LEAD;
	float    chev = snap->u.nav.nav_has_children ? NAV_CHEVRON_BOX : 0.0f;
	bool     ok   = nav_rec_fill(b, &lr, FLUX_CORNER_RADIUS, style.fill)
	             && nav_rec_icon_at(b, &x, &lr, glyph, NAV_TOP_TEXT_LEAD, style.text);
	FluxRect tr   = {x, lr.y, lr.x + lr.w - x - chev, lr.h};
	FluxRect cb   = {lr.x + lr.w - NAV_CHEVRON_BOX, lr.y, NAV_CHEVRON_BOX, lr.h};
	ok            = ok && nav_rec_label(b, &tr, snap->u.nav.label, style.text);
	return ok
	    && (chev <= 0.0f || flux_rec_up_down_chevron(b, rc, snap, &cb, state, snap->u.nav.nav_expanded, style.text));
}

static bool nav_rec_overflow_item(
  XentDlBuilder *b, FluxRect const *row, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRenderCtx const *rc
) {
	FluxThemeColors const *t     = flux_rec_theme(rc);
	NavChrome              style = nav_chrome(t, state, snap->u.nav.is_checked, false);
	bool                   kids  = snap->u.nav.nav_has_children;
	float                  x     = row->x + ( float ) snap->u.nav.nav_depth * 31.0f + NAV_OVF_ICON_LEAD;
	wchar_t const         *glyph = flux_icon_lookup(snap->u.nav.icon_name);
	bool                   ok
	  = nav_rec_fill(b, row, 0.0f, style.fill) && nav_rec_icon_at(b, &x, row, glyph, NAV_OVF_TEXT_LEAD, style.text);
	float    end = row->x + row->w - NAV_OVF_CHEVRON_END;
	FluxRect cb  = {end - NAV_CHEVRON_BOX, row->y, NAV_CHEVRON_BOX, row->h};
	FluxRect tr  = {x, row->y, (kids ? cb.x : end) - x - NAV_OVF_TEXT_TRAIL, row->h};
	ok           = ok && nav_rec_label(b, &tr, snap->u.nav.label, style.text);
	return ok && (!kids || flux_rec_up_down_chevron(b, rc, snap, &cb, state, snap->u.nav.nav_expanded, style.text));
}

static bool
nav_rec_more(XentDlBuilder *b, FluxRect const *box, FluxControlState const *state, FluxThemeColors const *t) {
	NavChrome style = nav_chrome(t, state, false, true);
	return nav_rec_fill(b, box, 0.0f, style.fill)
	    && flux_rec_glyph_wc(b, box, L"\xE712", FLUX_NAV_ICON_GLYPH, style.text);
}

static XentAffine2 nav_ind_scale_transform(FluxNavSnapshot const *nav, FluxRect const *pill) {
	float scale = nav->nav_ind_scale;
	if (nav->nav_ind_horizontal)
		return (XentAffine2) {scale, 0, nav->nav_ind_offset + (1.0f - scale) * pill->x, 0, 1, 0};
	return (XentAffine2) {1, 0, 0, 0, scale, nav->nav_ind_offset + (1.0f - scale) * pill->y};
}

static bool
nav_rec_item_indicator(XentDlBuilder *b, FluxRect const *row, FluxNavSnapshot const *nav, FluxThemeColors const *t) {
	float scale = nav->nav_ind_scale;
	if (scale <= 0.0f || nav->nav_ind_opacity <= 0.0f) return true;
	FluxRect  pill   = flux_nav_indicator_rect(*row, nav->nav_place, nav->nav_depth, nav->nav_in_flyout);
	float     radius = nav->nav_place == FLUX_NAV_PLACE_OVERFLOW ? 0.0f : FLUX_NAV_INDICATOR_R;
	FluxColor fill   = rec_scale_alpha(nav_indicator_color(t), nav->nav_ind_opacity);
	if (scale == 1.0f && nav->nav_ind_offset == 0.0f) return flux_rec_fill_rounded(b, &pill, radius, fill);
	if (!xent_dl_pushtransform(b, nav_ind_scale_transform(nav, &pill))) return false;
	bool ok = flux_rec_fill_rounded(b, &pill, radius, fill);
	return xent_dl_poptransform(b) && ok;
}

static bool nav_rec_left_item(
  XentDlBuilder *b, FluxRect const *ib, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRenderCtx const *rc
) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	NavChrome              style  = nav_chrome(t, state, snap->u.nav.is_checked, false);
	float                  indent = ( float ) snap->u.nav.nav_depth * 31.0f;
	FluxRect               ci     = {ib->x + indent, ib->y, ib->w - indent, ib->h};
	wchar_t const         *glyph  = flux_icon_lookup(snap->u.nav.icon_name);
	float                  x      = ci.x;
	bool                   ok     = nav_rec_fill(b, ib, FLUX_CORNER_RADIUS, style.fill);
	if (ok && glyph) {
		FluxRect icon = {ci.x, ci.y, FLUX_NAV_ICON_BOX, ci.h};
		ok            = flux_rec_glyph_wc(b, &icon, glyph, FLUX_NAV_ICON_GLYPH, style.text);
	}
	float kids   = snap->u.nav.nav_has_children ? NAV_LEFT_CHEVRON_COL : 0.0f;
	x           += (glyph ? FLUX_NAV_ICON_BOX : FLUX_NAV_ICON_COLLAPSED) + NAV_LEFT_TEXT_LEAD;
	FluxRect tr  = {x, ci.y, ci.x + ci.w - NAV_LEFT_TEXT_TRAIL - kids - x, ci.h};
	FluxRect cb  = {ib->x + ib->w - 28.0f, ib->y, 16.0f, ib->h};
	ok           = ok && nav_rec_label(b, &tr, snap->u.nav.label, style.text);
	if (ok && snap->u.nav.nav_has_children && snap->u.nav.label)
		ok = flux_rec_up_down_chevron(b, rc, snap, &cb, state, snap->u.nav.nav_expanded, style.text);
	return ok;
}

static bool nav_rec_item(
  XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state,
  FluxRenderCtx const *rc
) {
	if (snap->u.nav.nav_item_kind == FLUX_NAV_ITEM_MORE) return nav_rec_more(b, bounds, state, flux_rec_theme(rc));
	if (snap->u.nav.nav_place == FLUX_NAV_PLACE_TOP) return nav_rec_top_item(b, bounds, snap, state, rc);
	if (snap->u.nav.nav_place == FLUX_NAV_PLACE_OVERFLOW) return nav_rec_overflow_item(b, bounds, snap, state, rc);
	return nav_rec_left_item(b, bounds, snap, state, rc);
}

static bool
nav_rec_focus(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds, FluxThemeColors const *t) {
	if (snap->u.nav.nav_item_kind == FLUX_NAV_ITEM_MORE) return flux_rec_focus_rect(b, t, bounds, FLUX_CORNER_RADIUS);
	if (snap->u.nav.nav_place == FLUX_NAV_PLACE_TOP) {
		FluxRect r = rec_inset(bounds, NAV_TOP_ROOT_MARGIN_X, NAV_TOP_ROOT_MARGIN_Y);
		return flux_rec_focus_rect(b, t, &r, FLUX_CORNER_RADIUS);
	}
	FluxRect r      = rec_inset(bounds, FLUX_FOCUS_OUTER_PAD, FLUX_FOCUS_OUTER_PAD);
	float    radius = snap->u.nav.nav_place == FLUX_NAV_PLACE_OVERFLOW ? 0.0f : FLUX_CORNER_RADIUS;
	return flux_rec_focus_rect(b, t, &r, radius);
}

bool flux_rec_navitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (bounds->w < 3.0f || bounds->h < 3.0f) return true;
	FluxThemeColors const *t    = flux_rec_theme(rc);
	uint8_t                kind = snap->u.nav.nav_item_kind;
	if (kind == FLUX_NAV_ITEM_SEPARATOR) return nav_rec_separator(b, bounds, snap, t);
	if (kind == FLUX_NAV_ITEM_HEADER) return nav_rec_header(b, bounds, snap, t);
	return nav_rec_item(b, snap, bounds, state, rc)
	    && nav_rec_item_indicator(b, bounds, &snap->u.nav, t)
	    && (!state->focused || !state->enabled || nav_rec_focus(b, snap, bounds, t));
}

static bool menu_bar_item_background(
  XentDlBuilder *b, FluxRect const *bounds, FluxThemeColors const *t, FluxControlState const *state, bool open
) {
	bool pressed = state->enabled && state->pressed;
	bool hovered = state->enabled && state->hovered && !pressed;
	if (t->contrast.active) {
		FluxContrastColors const *p      = &t->contrast;
		bool                      active = pressed || hovered || (state->enabled && open);
		FluxColor                 fill   = active ? p->button_face : (FluxColor) {0};
		FluxColor                 border = active ? p->highlight : p->button_text;
		return flux_rec_fill_rounded(b, bounds, FLUX_CORNER_RADIUS, fill)
		    && flux_rec_stroke_rounded(b, bounds, FLUX_CORNER_RADIUS, border, 2.0f);
	}
	FluxRect inner = rec_inset(bounds, 1.0f, 1.0f);
	if (pressed || (state->enabled && open))
		return flux_rec_fill_rounded(b, &inner, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	return !hovered || flux_rec_fill_rounded(b, &inner, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
}

bool flux_rec_menuitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t = flux_rec_theme(rc);
	if (!menu_bar_item_background(b, bounds, t, state, snap->u.menu.is_checked)) return false;
	if (state->focused && state->enabled) {
		float    outset = FLUX_FOCUS_OUTER_PAD - 3.0f;
		FluxRect focus  = rec_inset(bounds, outset, outset);
		if (!flux_rec_focus_rect(b, t, &focus, FLUX_CORNER_RADIUS)) return false;
	}
	FluxColor fg = t->contrast.active ? t->contrast.button_text : t->text_primary;
	if (!state->enabled) fg = t->text_disabled;
	XentTextStyle style = {.font_size = FLUX_MENU_BAR_ITEM_FONT, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, bounds, snap->u.menu.label, &style, fg);
}

static FluxColor
breadcrumb_contrast_fg(FluxContrastColors const *p, FluxControlState const *state, bool is_last, bool in_zone) {
	if (is_last) return p->button_text;
	if (!state->enabled) return p->gray_text;
	bool hot = state->focused || (in_zone && (state->hovered || state->pressed));
	return hot ? p->highlight : p->button_text;
}

static FluxColor breadcrumb_fg(FluxThemeColors const *t, FluxControlState const *state, bool is_last, bool in_zone) {
	if (t->contrast.active) return breadcrumb_contrast_fg(&t->contrast, state, is_last, in_zone);
	if (!state->enabled) return t->text_disabled;
	if (!is_last && in_zone && state->pressed) return t->text_tertiary;
	if (!is_last && in_zone && state->hovered) return t->text_secondary;
	return t->text_primary;
}

static bool
breadcrumb_draw_chevron(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, float content_w, bool rtl) {
	FluxRect       cell  = {bounds->x + content_w, bounds->y, bounds->w - content_w, bounds->h};
	wchar_t const *glyph = rtl ? FLUX_BREADCRUMB_CHEVRON_RTL : FLUX_BREADCRUMB_CHEVRON_LTR;
	FluxColor      fg    = t->contrast.active ? t->contrast.button_text : t->text_primary;
	return flux_rec_glyph_wc(b, &cell, glyph, FLUX_BREADCRUMB_CHEVRON_FONT, fg);
}

bool flux_rec_build_breadcrumb(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (bounds->w < 1.0f || bounds->h < 1.0f) return true;
	FluxThemeColors const        *t         = flux_rec_theme(rc);
	FluxBreadcrumbSnapshot const *bc        = &snap->u.breadcrumb;
	float                         content_w = flux_minf(bc->content_w, bounds->w);
	FluxRect                      content   = {bounds->x, bounds->y, content_w, bounds->h};
	bool                          in_zone   = snap->hover_local_x >= 0.0f && snap->hover_local_x <= content_w;
	FluxColor                     fg        = breadcrumb_fg(t, state, bc->is_last, in_zone);

	bool                          ok;
	if (bc->is_ellipsis)
		ok = flux_rec_glyph_wc(b, &content, FLUX_BREADCRUMB_ELLIPSIS, FLUX_BREADCRUMB_ELLIPSIS_FONT, fg);
	else {
		FluxRect tr
		  = {content.x + FLUX_BREADCRUMB_PAD_H, content.y, content.w - 2.0f * FLUX_BREADCRUMB_PAD_H, content.h};
		ok = flux_rec_text(b, &tr, bc->label, FLUX_BREADCRUMB_FONT, 400u, fg);
	}
	if (ok && bc->show_chevron) ok = breadcrumb_draw_chevron(b, t, bounds, content_w, snap->rtl);
	if (ok && state->focused && state->enabled)
		ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &content, FLUX_CORNER_RADIUS);
	return ok;
}

static FluxColor selector_fg(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (t->contrast.active) {
		bool muted = !state->enabled || state->pressed || state->hovered;
		return muted ? t->contrast.gray_text : t->contrast.button_text;
	}
	if (!state->enabled) return t->text_disabled;
	if (state->pressed) return selected ? t->text_secondary : t->text_tertiary;
	if (state->hovered) return t->text_secondary;
	return t->text_primary;
}

static bool
selector_draw_pill(XentDlBuilder *b, FluxRect const *bounds, float pt, FluxThemeColors const *t, bool enabled) {
	if (pt <= 0.001f) return true;
	float     w    = 4.0f + 12.0f * pt;
	FluxColor pill = t->contrast.active ? t->contrast.highlight : t->accent_default;
	if (!enabled) pill = t->accent_disabled;
	pill.rgba   = (pill.rgba & 0xffffff00u) | ( uint32_t ) (( float ) (pill.rgba & 0xffu) * pt);
	FluxRect pr = {bounds->x + (bounds->w - w) * 0.5f, bounds->y + bounds->h - 3.0f, w, 3.0f};
	return flux_rec_fill_rounded(b, &pr, 1.0f, pill);
}

bool flux_rec_selectoritem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t         = flux_rec_theme(rc);
	FluxColor              fg        = selector_fg(t, state, snap->u.selector.selected);
	float                  content_x = bounds->x + 12.0f;

	if (snap->u.selector.icon_glyph) {
		wchar_t  g [2] = {( wchar_t ) snap->u.selector.icon_glyph, 0};
		FluxRect ir    = {content_x, bounds->y, 16.0f, bounds->h - 3.0f};
		if (!flux_rec_glyph_wc(b, &ir, g, 16.0f, fg)) return false;
		content_x += 24.0f;
	}
	if (snap->u.selector.text) {
		FluxRect tr = {content_x, bounds->y, bounds->x + bounds->w - 12.0f - content_x, bounds->h - 3.0f};
		if (!flux_rec_text(b, &tr, snap->u.selector.text, 14.0f, 400u, fg)) return false;
	}
	if (!selector_draw_pill(b, bounds, snap->u.selector.pill_t, t, state->enabled)) return false;
	if (state->focused && state->enabled) {
		FluxRect fr = {bounds->x - 2.0f, bounds->y - 2.0f, bounds->w + 4.0f, bounds->h + 4.0f};
		if (!flux_rec_focus_rect(b, flux_rec_theme(rc), &fr, FLUX_CORNER_RADIUS)) return false;
	}
	return true;
}
