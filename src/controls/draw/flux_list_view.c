/**
 * @file flux_list_view.c
 * @brief ListView-family cell renderers, 1:1 with the WinUI native chrome
 * (ListViewBaseItemChrome.cpp + *_themeresources.xaml):
 *
 * - ListViewItem: 4,2,4,2-inset rounded-4 backplate, 3×16 accent pill
 *   (margin 4/20/0/20, pressed −6, radius 1.5), inline 20×20 rounded-3
 *   multi-select checkbox with the  glyph.
 * - ListBoxItem: full-bounds fill; selected = accent at 0.6/0.8/0.9.
 * - GridViewItem: rounded-4 fill, selected = 2px accent ring + 1px solid
 *   inner ring; overlay 20×20 checkbox in the top-right corner drawn with
 *   the ControlOnImage fills.
 * - ListBox root: ChromeMediumLow surface.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

#include <math.h>

/* Accent with an absolute alpha override (SystemControlHighlightListAccent*). */
static FluxColor list_accent_alpha(FluxThemeColors const *t, uint8_t alpha) {
	FluxColor c = t->accent_default;
	c.rgba      = (c.rgba & 0xffffff00u) | alpha;
	return c;
}

static void list_draw_check_glyph(FluxRenderContext const *rc, FluxRect const *box, FluxColor color) {
	if (!rc->text) return;
	flux_button_draw_chevron(rc, box, L"\xE73E", 16.0f, color);
}

/* Pick from the (disabled, pressed, hovered, rest) state ladder. */
static FluxColor list_state_pick(
  FluxControlState const *state, FluxColor disabled, FluxColor pressed, FluxColor hovered, FluxColor rest
) {
	if (!state->enabled) return disabled;
	if (state->pressed) return pressed;
	if (state->hovered) return hovered;
	return rest;
}

static FluxColor list_checkbox_fill(FluxThemeColors const *t, FluxControlState const *state, bool checked, bool on_image) {
	if (checked)
		return list_state_pick(state, t->accent_disabled, t->accent_tertiary, t->accent_secondary, t->accent_default);
	if (on_image)
		return list_state_pick(
		  state, t->ctrl_on_image_disabled, t->ctrl_on_image_tertiary, t->ctrl_on_image_secondary,
		  t->ctrl_on_image_default
		);
	return t->ctrl_alt_fill_secondary;
}

/* Inline (ListView) or overlay (GridView) multi-select checkbox. */
static void list_draw_checkbox(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state,
  bool checked, bool on_image
) {
	flux_fill_rounded_rect(rc, box, 3.0f, list_checkbox_fill(t, state, checked, on_image));
	if (!checked) {
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		flux_draw_rounded_rect(rc, box, 3.0f, stroke, 1.0f);
		return;
	}
	FluxColor glyph = !state->enabled ? t->text_on_accent_disabled
	    : state->pressed              ? t->text_on_accent_secondary
	                                  : t->text_on_accent_primary;
	list_draw_check_glyph(rc, box, glyph);
}

/* ListViewItem / shared subtle-fill ladder (ListViewItem_themeresources). */
static FluxColor list_item_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (state->enabled && state->pressed) return selected ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	if (selected && state->enabled && state->hovered) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static void list_draw_list_cell(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRenderSnapshot const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool     selected = snap->u.list_item.is_selected;

	/* Backplate: s_backplateMargin 4,2,4,2, radius 4. */
	FluxRect ib = {bounds->x + 4.0f, bounds->y + 2.0f, bounds->w - 8.0f, bounds->h - 4.0f};
	if (ib.w < 1.0f || ib.h < 1.0f) return;

	FluxColor fill = list_item_fill(t, state, selected);
	if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, fill);

	if (selected && !snap->u.list_item.multi) {
		/* Pill: 3 wide, margin 4/20/0/20; height max(16, h−40); pressed −6. */
		float pill_h = bounds->h <= 16.0f ? bounds->h : fmaxf(16.0f, bounds->h - 40.0f);
		if (state->pressed) pill_h -= 6.0f;
		if (pill_h >= 3.0f) {
			FluxRect pill = {bounds->x + 4.0f, bounds->y + (bounds->h - pill_h) * 0.5f, 3.0f, pill_h};
			flux_fill_rounded_rect(rc, &pill, 1.5f, state->enabled ? t->accent_default : t->accent_disabled);
		}
	}

	if (snap->u.list_item.multi) {
		/* Inline rounded checkbox: 20×20, left margin 14, vertically centered. */
		FluxRect box = {bounds->x + 14.0f, bounds->y + (bounds->h - 20.0f) * 0.5f, 20.0f, 20.0f};
		list_draw_checkbox(rc, t, &box, state, selected, false);
	}
}

static FluxColor list_box_cell_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (selected && state->enabled && state->pressed) return list_accent_alpha(t, 0xe6); /* AccentHigh 0.9 */
	if (selected && state->enabled && state->hovered) return list_accent_alpha(t, 0xcc); /* AccentMedium 0.8 */
	if (selected) return list_accent_alpha(t, 0x99);                                     /* AccentLow 0.6 */
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

static void list_draw_list_box_cell(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRenderSnapshot const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxColor fill = list_box_cell_fill(t, state, snap->u.list_item.is_selected);
	if (flux_color_af(fill) > 0.0f) flux_fill_rect(rc, bounds, fill);
}

static void list_draw_grid_cell(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRenderSnapshot const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	bool selected = snap->u.list_item.is_selected;

	/* The 0,0,4,4 item margin is layout padding; the chrome fills the cell
	 * content box (bounds minus that spacing). */
	FluxRect ib = {bounds->x, bounds->y, bounds->w - 4.0f, bounds->h - 4.0f};
	if (ib.w < 2.0f || ib.h < 2.0f) return;

	FluxColor fill = list_item_fill(t, state, selected);
	if (selected) fill = state->pressed ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, fill);

	if (selected) {
		/* 2px accent ring + 1px inner solid ring (SelectedInnerBorder). */
		FluxColor ring = !state->enabled ? t->accent_disabled
		    : state->pressed             ? t->accent_tertiary
		    : state->hovered             ? t->accent_secondary
		                                 : t->accent_default;
		flux_draw_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, ring, 2.0f);
		FluxRect inner = {ib.x + 2.0f, ib.y + 2.0f, ib.w - 4.0f, ib.h - 4.0f};
		flux_draw_rounded_rect(rc, &inner, 3.0f, t->ctrl_solid_fill_default, 1.0f);
	}
	else if (state->enabled && state->hovered) {
		/* PointerOverBorder → ControlStrokeColorOnAccentTertiary #37000000. */
		flux_draw_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, flux_color_rgba(0, 0, 0, 0x37), 1.0f);
	}

	if (snap->u.list_item.multi) {
		/* Overlay checkbox: top-right, margin 0,2,2,0, ControlOnImage fills. */
		FluxRect box = {ib.x + ib.w - 20.0f - 2.0f, ib.y + 2.0f, 20.0f, 20.0f};
		list_draw_checkbox(rc, t, &box, state, selected, true);
	}
}

void flux_draw_list_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t = rc->theme ? rc->theme : flux_theme_default_colors();
	switch (( XtkListKind ) snap->u.list_item.kind) {
	case XTK_LIST_KIND_LIST_BOX : list_draw_list_box_cell(rc, t, snap, bounds, state); break;
	case XTK_LIST_KIND_GRID     : list_draw_grid_cell(rc, t, snap, bounds, state); break;
	case XTK_LIST_KIND_REPEATER : break;
	default                     : list_draw_list_cell(rc, t, snap, bounds, state); break;
	}
}

/** @brief ListBox surface: ChromeMediumLow fill (border 0 in Default theme). */
void flux_draw_list_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t  = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxColor              bg = snap->background.rgba ? snap->background : t->list_box_bg;
	flux_fill_rounded_rect(rc, bounds, snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS, bg);
}
