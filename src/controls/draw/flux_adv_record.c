#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_control_draw.h"

#include "controls/data/flux_nav_view_data.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/behavior/flux_breadcrumb_internal.h"

#include <xtk/xtk.h>

#include <math.h>
#include <string.h>

#define FLUX_NAV_PANE_SHADOW_W  9.0f
#define FLUX_NAV_CONTENT_RADIUS 8.0f
#define FLUX_NAV_ICON_COLLAPSED 8.0f

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

static bool list_rec_checkbox(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state, bool checked,
  bool on_image
) {
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
	if (selected && state->enabled && state->pressed) return list_accent_alpha(t, 0xe6);
	if (selected && state->enabled && state->hovered) return list_accent_alpha(t, 0xcc);
	if (selected) return list_accent_alpha(t, 0x99);
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
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

	if (ok && selected && !snap->u.list_item.multi) {
		float pill_h = bounds->h <= 16.0f ? bounds->h : fmaxf(16.0f, bounds->h - 40.0f);
		if (state->pressed) pill_h -= 6.0f;
		if (pill_h >= 3.0f) {
			float    px   = flux_rec_snapx(rc, bounds->x + 4.0f);
			FluxRect pill = {px, bounds->y + (bounds->h - pill_h) * 0.5f, 3.0f, pill_h};
			ok = flux_rec_fill_rounded(b, &pill, 1.5f, state->enabled ? t->accent_default : t->accent_disabled);
		}
	}
	if (ok && snap->u.list_item.multi) {
		FluxRect box = {bounds->x + 14.0f, bounds->y + (bounds->h - 20.0f) * 0.5f, 20.0f, 20.0f};
		ok           = list_rec_checkbox(b, t, &box, state, selected, false);
	}
	return ok;
}

static bool list_rec_grid_cell(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool     selected = snap->u.list_item.is_selected;
	FluxRect ib       = {bounds->x, bounds->y, bounds->w - 4.0f, bounds->h - 4.0f};
	if (ib.w < 2.0f || ib.h < 2.0f) return true;

	FluxColor fill = list_item_fill(t, state, selected);
	if (selected) fill = state->pressed ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	bool ok = true;
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, fill);

	if (ok && selected) {
		FluxColor ring = !state->enabled ? t->accent_disabled
		               : state->pressed  ? t->accent_tertiary
		               : state->hovered  ? t->accent_secondary
		                                 : t->accent_default;
		ok             = flux_rec_stroke_rounded(b, &ib, FLUX_CORNER_RADIUS, ring, 2.0f);
		if (ok) {
			FluxRect inner = {ib.x + 2.0f, ib.y + 2.0f, ib.w - 4.0f, ib.h - 4.0f};
			ok             = flux_rec_stroke_rounded(b, &inner, 3.0f, t->ctrl_solid_fill_default, 1.0f);
		}
	}
	else if (ok && state->enabled && state->hovered) {
		ok = flux_rec_stroke_rounded(b, &ib, FLUX_CORNER_RADIUS, flux_color_rgba(0, 0, 0, 0x37), 1.0f);
	}
	if (ok && snap->u.list_item.multi) {
		FluxRect box = {ib.x + ib.w - 22.0f, ib.y + 2.0f, 20.0f, 20.0f};
		ok           = list_rec_checkbox(b, t, &box, state, selected, true);
	}
	return ok;
}

static FluxColor items_scale_alpha(FluxColor c, float scale) {
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
	bool      ok   = flux_rec_fill_rounded(b, box, FLUX_CORNER_RADIUS, items_scale_alpha(fill, alpha));
	if (!ok) return false;
	if (!checked) {
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		return flux_rec_stroke_rounded(b, box, FLUX_CORNER_RADIUS, items_scale_alpha(stroke, alpha), 1.0f);
	}
	FluxColor glyph = state->enabled ? t->text_on_accent_primary : t->text_on_accent_disabled;
	return flux_rec_glyph_wc(b, box, L"\xE73E", 12.0f, items_scale_alpha(glyph, alpha));
}

static bool items_rec_selection(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, FluxControlState const *state, float alpha
) {
	FluxColor accent = state->enabled ? t->accent_default : t->accent_disabled;
	if (!flux_rec_stroke_rounded(b, bounds, FLUX_CORNER_RADIUS, items_scale_alpha(accent, alpha), 3.0f)) return false;
	FluxRect inner = {bounds->x + 2.0f, bounds->y + 2.0f, bounds->w - 4.0f, bounds->h - 4.0f};
	if (inner.w <= 1.0f || inner.h <= 1.0f) return true;
	return flux_rec_stroke_rounded(
	  b, &inner, FLUX_CORNER_RADIUS - 2.0f, items_scale_alpha(t->ctrl_solid_fill_default, alpha), 1.0f
	);
}

static bool items_rec_multi(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, FluxControlState const *state, bool checked,
  float alpha
) {
	FluxRect box = {bounds->x + bounds->w - 24.0f, bounds->y + 2.0f, 20.0f, 20.0f};
	return box.x <= bounds->x || items_rec_checkbox(b, t, &box, state, checked, alpha);
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

	FluxColor fill = flux_color_rgba(0, 0, 0, 0);
	if (state->enabled && state->pressed) fill = t->subtle_fill_tertiary;
	else if (state->enabled && state->hovered) fill = t->subtle_fill_secondary;
	bool ok = true;
	if (flux_color_af(fill) > 0.0f)
		ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, items_scale_alpha(fill, alpha));

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
	case XTK_LIST_KIND_LIST_BOX : {
		FluxColor fill = listbox_item_fill(t, state, snap->u.list_item.is_selected);
		if (flux_color_af(fill) <= 0.0f) return true;
		return flux_rec_fill_rect(b, bounds, fill);
	}
	case XTK_LIST_KIND_GRID     : return list_rec_grid_cell(b, t, snap, bounds, state);
	case XTK_LIST_KIND_ITEMS    : return list_rec_items_container(b, t, snap, bounds, state);
	case XTK_LIST_KIND_REPEATER : return true;
	default                     : return list_rec_list_cell(b, rc, t, snap, bounds, state);
	}
}

static FluxColor tree_row_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	return list_item_fill(t, state, selected);
}

static FluxColor tree_row_fg(FluxThemeColors const *t, FluxControlState const *state) {
	if (!state->enabled) return t->text_disabled;
	if (state->pressed) return t->text_secondary;
	return t->text_primary;
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
	FluxThemeColors const *t    = flux_rec_theme(rc);
	FluxColor              fill = tree_row_fill(t, st, selected && !multi);
	bool                   ok   = true;
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

	float    ix        = bounds->x + FLUX_TREE_ROW_MARGIN_H + FLUX_TREE_INDENT * ( float ) ti->depth;
	float    pt        = multi ? 0.0f : FLUX_TREE_CONTENT_PAD_T;
	float    pb        = multi ? 0.0f : FLUX_TREE_CONTENT_PAD_B;
	FluxRect cr        = {ib.x, ib.y + pt, ib.w, ib.h - pt - pb};
	float    chevron_x = ix + (multi ? FLUX_TREE_MULTI_CHEVRON_X : FLUX_TREE_CHEVRON_PAD);
	float    text_x    = multi ? chevron_x + FLUX_TREE_CHEVRON_ZONE_MULTI : ix + FLUX_TREE_CHEVRON_ZONE;
	if (multi && !tree_rec_checkbox(b, rc, snap, &ib, ix, &st, ti->sel_state)) return false;
	if (!tree_rec_content(b, ti, &cr, chevron_x, text_x, tree_row_fg(flux_rec_theme(rc), &st))) return false;
	if (!st.focused || !st.enabled) return true;
	FluxRect fr = {ib.x, ib.y - 1.0f, ib.w, ib.h + 2.0f};
	return flux_rec_focus_rect(b, flux_rec_theme(rc), &fr, FLUX_CORNER_RADIUS);
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

static bool
nav_rec_content_layer(XentDlBuilder *b, FluxRect const *root, FluxRenderSnap const *snap, FluxThemeColors const *t) {
	float    cx    = snap->u.nav.nav_content_x;
	float    cy    = snap->u.nav.nav_content_y;
	FluxRect layer = {root->x + cx, root->y + cy, root->w - cx, root->h - cy};
	if (layer.w < 1.0f || layer.h < 1.0f) return true;
	if (!snap->u.nav.nav_top && cx < 0.5f) return flux_rec_fill_rect(b, &layer, t->layer_default);

	float    r   = FLUX_NAV_CONTENT_RADIUS;
	FluxRect ext = {layer.x, layer.y, layer.w + (snap->u.nav.nav_top ? 0.0f : r), layer.h + r};
	bool     ok  = xent_dl_pushcliprect(b, flux_rec_xrect(&layer));
	if (ok) ok = flux_rec_fill_rounded(b, &ext, r, t->layer_default);
	if (ok) {
		FluxRect stroke = {ext.x + 0.5f, ext.y + 0.5f, ext.w - 1.0f, ext.h - 1.0f};
		ok              = flux_rec_stroke_rounded(b, &stroke, r, t->card_stroke_default, FLUX_STROKE_WIDTH);
	}
	return xent_dl_popclip(b) && ok;
}

bool flux_rec_nav(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds) {
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxRect               pb = rec_inset(bounds, 1.0f, 1.0f);
	if (!snap->u.nav.nav_is_pane) return nav_rec_content_layer(b, &pb, snap, t);

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

static FluxColor nav_rec_with_opacity(FluxColor c, float op) {
	float a = flux_color_af(c) * flux_clamp01(op);
	return flux_color_rgba(
	  ( int ) (flux_color_rf(c) * 255.0f + 0.5f), ( int ) (flux_color_gf(c) * 255.0f + 0.5f),
	  ( int ) (flux_color_bf(c) * 255.0f + 0.5f), ( int ) (a * 255.0f + 0.5f)
	);
}

bool flux_rec_nav_overlay(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
) {
	if (snap->u.nav.nav_ind_opacity <= 0.01f) return true;
	float lead   = snap->u.nav.nav_ind_top;
	float extent = snap->u.nav.nav_ind_bottom - snap->u.nav.nav_ind_top;
	if (extent <= 0.0f) return true;

	FluxThemeColors const *t    = flux_rec_theme(rc);
	FluxColor              fill = nav_rec_with_opacity(t->accent_default, snap->u.nav.nav_ind_opacity);
	if (snap->u.nav.nav_top) {
		FluxRect pill
		  = {bounds->x + lead, bounds->y + bounds->h - 4.0f - FLUX_NAV_TOP_IND_H, extent, FLUX_NAV_TOP_IND_H};
		return flux_rec_fill_rounded(b, &pill, FLUX_NAV_INDICATOR_R, fill);
	}

	float    px   = bounds->x + FLUX_NAV_ITEM_MARGIN_H - 1.0f + snap->u.nav.nav_ind_indent;
	FluxRect pill = {px, bounds->y + lead, FLUX_NAV_INDICATOR_W, extent};
	float    ct   = bounds->y + snap->u.nav.nav_ind_clip_top;
	float    cb   = bounds->y + snap->u.nav.nav_ind_clip_bottom;
	FluxRect clip = {bounds->x, ct, bounds->w, cb - ct};
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(&clip))) return false;
	bool ok = flux_rec_fill_rounded(b, &pill, FLUX_NAV_INDICATOR_R, fill);
	return xent_dl_popclip(b) && ok;
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
	float    pad = snap->u.nav.nav_top ? 12.0f : FLUX_NAV_HEADER_PAD;
	FluxRect tr  = {ib->x + pad, ib->y, ib->w - pad * 2.0f, ib->h};
	if (tr.w <= 0.0f) return true;
	return flux_rec_text(b, &tr, snap->u.nav.label, FLUX_NAV_HEADER_FONT, 400u, t->text_secondary);
}

bool flux_rec_navitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t  = flux_rec_theme(rc);
	FluxRect               ib = rec_inset(bounds, 1.0f, 1.0f);
	if (ib.w < 1.0f || ib.h < 1.0f) return true;

	uint8_t kind = snap->u.nav.nav_item_kind;
	if (kind == FLUX_NAV_ITEM_SEPARATOR) return nav_rec_separator(b, &ib, snap, t);
	if (kind == FLUX_NAV_ITEM_HEADER) return nav_rec_header(b, &ib, snap, t);

	bool      selected = snap->u.nav.is_checked;
	bool      ok       = true;
	FluxColor fill     = nav_item_fill(t, state, selected);
	if (flux_color_af(fill) > 0.0f) ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, fill);

	FluxColor fg          = !state->enabled ? t->text_disabled : (state->pressed ? t->text_secondary : t->text_primary);
	FluxRect  ci          = ib;
	ci.x                 += ( float ) snap->u.nav.nav_depth * 31.0f;
	ci.w                 -= ( float ) snap->u.nav.nav_depth * 31.0f;

	wchar_t const *glyph  = flux_icon_lookup(snap->u.nav.icon_name);
	float          icon_w = glyph ? FLUX_NAV_ICON_BOX : FLUX_NAV_ICON_COLLAPSED;
	if (ok && glyph) {
		FluxRect icon = {ci.x, ci.y, FLUX_NAV_ICON_BOX, ci.h};
		ok            = flux_rec_glyph_wc(b, &icon, glyph, FLUX_NAV_ICON_GLYPH, fg);
	}
	if (ok && snap->u.nav.label) {
		FluxRect tr = {ci.x + icon_w, ci.y, ci.w - icon_w - 8.0f, ci.h};
		if (tr.w > 0.0f) ok = flux_rec_text(b, &tr, snap->u.nav.label, FLUX_NAV_LABEL_FONT, 400u, fg);
	}
	if (ok && snap->u.nav.nav_has_children && (snap->u.nav.label || snap->u.nav.nav_top)) {
		FluxRect cb = {ib.x + ib.w - 28.0f, ib.y, 16.0f, ib.h};
		ok          = flux_rec_glyph_wc(b, &cb, snap->u.nav.nav_expanded ? L"\xE70E" : L"\xE70D", 12.0f, fg);
	}
	return ok;
}

bool flux_rec_menuitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t    = flux_rec_theme(rc);
	FluxRect               ib   = rec_inset(bounds, 1.0f, 1.0f);
	bool                   open = snap->u.menu.is_checked;
	bool                   ok   = true;

	if (state->enabled && (open || state->pressed))
		ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	else if (state->enabled && state->hovered)
		ok = flux_rec_fill_rounded(b, &ib, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);
	if (!ok || !snap->u.menu.label) return ok;

	FluxColor     fg    = state->enabled ? t->text_primary : t->text_disabled;
	XentTextStyle style = {.font_size = FLUX_MENU_BAR_ITEM_FONT, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, &ib, snap->u.menu.label, &style, fg);
}

static FluxColor breadcrumb_fg(FluxThemeColors const *t, FluxControlState const *state, bool is_last, bool in_zone) {
	if (!state->enabled) return t->text_disabled;
	if (!is_last && in_zone && state->pressed) return t->text_tertiary;
	if (!is_last && in_zone && state->hovered) return t->text_secondary;
	return t->text_primary;
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
	if (ok && bc->show_chevron) {
		FluxRect cell = {bounds->x + content_w, bounds->y, bounds->w - content_w, bounds->h};
		ok = flux_rec_glyph_wc(b, &cell, FLUX_BREADCRUMB_CHEVRON_LTR, FLUX_BREADCRUMB_CHEVRON_FONT, t->text_primary);
	}
	if (ok && state->focused && state->enabled)
		ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &content, FLUX_CORNER_RADIUS);
	return ok;
}

static FluxColor selector_fg(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (!state->enabled) return t->text_disabled;
	if (state->pressed && !selected) return t->text_tertiary;
	if (state->hovered) return t->text_secondary;
	return t->text_primary;
}

static bool
selector_draw_pill(XentDlBuilder *b, FluxRect const *bounds, float pt, FluxThemeColors const *t, bool enabled) {
	if (pt <= 0.001f) return true;
	float     w    = 4.0f + 12.0f * pt;
	FluxColor pill = enabled ? t->accent_default : t->accent_disabled;
	pill.rgba      = (pill.rgba & 0xffffff00u) | ( uint32_t ) (( float ) (pill.rgba & 0xffu) * pt);
	FluxRect pr    = {bounds->x + (bounds->w - w) * 0.5f, bounds->y + bounds->h - 3.0f, w, 3.0f};
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
