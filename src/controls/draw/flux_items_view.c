/**
 * @file flux_items_view.c
 * @brief ItemContainer chrome (WinUI 1.5 ItemContainer template), the one
 * genuinely-new renderer ItemsView needs on top of the shared items-host.
 *
 * Unlike the classic ListViewItem (left accent pill + subtle backplate),
 * ItemContainer conveys selection with a full accent *border*:
 *
 *   z-order, bottom -> top:
 *     1. PART_CommonVisual fill  : translucent hover/press overlay (subtle
 *        secondary/tertiary), the SAME whether selected or not — selection is
 *        never a fill.
 *     2. PART_SelectionVisual    : 3px AccentFillColorDefault border at the
 *        outer edge (opacity 0 unselected -> 1 selected).
 *     3. PART_CommonVisual stroke: 1px ControlSolidFillColorDefault inner ring
 *        inset by ItemContainerSelectedInnerMargin (2), when selected.
 *     4. PART_SelectionCheckbox  : 20x20 rounded-4 top-right checkbox in
 *        Multiple mode (accent + U+E73E when checked, ControlOnImage fill +
 *        ControlStrongStroke otherwise).
 *
 * Disabled multiplies the whole container by ItemContainerDisabledOpacity
 * (0.3). All overlays are hit-test transparent — the LIST_ITEM wrapper owns
 * the click. Selection/hover/press come from FluxControlState; is_selected +
 * multi come from the (unchanged) LIST_ITEM snapshot. Reached from
 * flux_draw_list_item's kind switch (case XTK_LIST_KIND_ITEMS).
 *
 * Note: fluxent's items-host chrome is static (no per-cell storyboards), so
 * the template's 167ms accent/checkbox fades are drawn as instant swaps,
 * consistent with the existing ListView/GridView cell renderers.
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"

/* Scale a color's alpha (low byte, 0xRRGGBBAA) — used for the 0.3 disabled
 * pass so every chrome layer dims together. */
static FluxColor items_scale_alpha(FluxColor c, float scale) {
	uint32_t a = ( uint32_t ) (( float ) (c.rgba & 0xffu) * scale + 0.5f);
	if (a > 0xffu) a = 0xffu;
	FluxColor out = c;
	out.rgba      = (c.rgba & 0xffffff00u) | a;
	return out;
}

/* PART_SelectionCheckbox: 20x20 rounded-4, CheckBoxGlyphSize 12 accept glyph. */
static void items_draw_checkbox(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state,
  bool checked, float alpha
) {
	FluxColor fill = checked ? (state->enabled ? t->accent_default : t->accent_disabled)
	                         : (state->enabled ? t->ctrl_on_image_default : t->ctrl_on_image_disabled);
	flux_fill_rounded_rect(rc, box, FLUX_CORNER_RADIUS, items_scale_alpha(fill, alpha));

	if (!checked) {
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		flux_draw_rounded_rect(rc, box, FLUX_CORNER_RADIUS, items_scale_alpha(stroke, alpha), 1.0f);
	}
	else if (rc->text) {
		FluxColor glyph = state->enabled ? t->text_on_accent_primary : t->text_on_accent_disabled;
		flux_button_draw_chevron(rc, box, L"\xE73E", 12.0f, items_scale_alpha(glyph, alpha));
	}
}

/** @brief Draw the ItemContainer chrome for one ItemsView cell. */
void flux_draw_items_container(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t        = rc->theme ? rc->theme : flux_theme_default_colors();
	bool                   selected = snap->u.list_item.is_selected;
	bool                   multi    = snap->u.list_item.multi;
	float                  alpha    = state->enabled ? 1.0f : 0.3f; /* ItemContainerDisabledOpacity */

	/* Container box: a 2px inset (ItemContainerSelectedInnerMargin) leaves a
	 * ~4px gap between neighbours for the contiguous cell pitch; extra
	 * item_spacing widens the cell and shows as transparent padding. Radius 4. */
	FluxRect ib = {bounds->x + 2.0f, bounds->y + 2.0f, bounds->w - 4.0f, bounds->h - 4.0f};
	if (ib.w < 2.0f || ib.h < 2.0f) return;

	/* (1) hover / press overlay — identical for selected and unselected. */
	FluxColor fill = flux_color_rgba(0, 0, 0, 0);
	if (state->enabled && state->pressed) fill = t->subtle_fill_tertiary;   /* SubtleFillColorTertiary */
	else if (state->enabled && state->hovered) fill = t->subtle_fill_secondary; /* SubtleFillColorSecondary */
	if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, items_scale_alpha(fill, alpha));

	/* (2) selection visual: 3px accent border at the outer edge, then a 1px
	 * ControlSolidFillColorDefault ring inset by 2 (ItemContainerSelectedInnerMargin). */
	if (selected) {
		FluxColor accent = state->enabled ? t->accent_default : t->accent_disabled;
		flux_draw_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, items_scale_alpha(accent, alpha), 3.0f);

		FluxRect inner = {ib.x + 2.0f, ib.y + 2.0f, ib.w - 4.0f, ib.h - 4.0f};
		if (inner.w > 1.0f && inner.h > 1.0f)
			flux_draw_rounded_rect(
			  rc, &inner, FLUX_CORNER_RADIUS - 2.0f, items_scale_alpha(t->ctrl_solid_fill_default, alpha), 1.0f
			);
	}

	/* (3) Multiple-mode checkbox: top-right, margin R=4 / T=2. */
	if (multi) {
		FluxRect box = {ib.x + ib.w - 20.0f - 4.0f, ib.y + 2.0f, 20.0f, 20.0f};
		if (box.x > ib.x) items_draw_checkbox(rc, t, &box, state, selected, alpha);
	}
}
