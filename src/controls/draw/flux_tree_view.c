/**
 * @file flux_tree_view.c
 * @brief TreeViewItem row chrome, 1:1 with WinUI (TreeViewItem.xaml +
 * TreeView_themeresources.xaml):
 *
 * - 4,2,4,2-inset rounded-4 backplate on the SubtleFill ladder (Normal
 *   transparent / PointerOver secondary / Pressed tertiary; Selected
 *   secondary, +PointerOver tertiary, +Pressed secondary).
 * - Indentation 16 × depth; chevron zone 14+12+14 with an instant
 *   E76C (collapsed) ↔ E70D (expanded) glyph swap at FontSize 8 — no
 *   rotation; the zone stays reserved on leaves (glyph opacity 0).
 * - Single mode: 3×16 accent selection pill at the backplate's left edge.
 * - Multiple mode: no pill; a 20×20 rounded-3 checkbox at indent+10 shows
 *   the tri-state cascade (check E73E / indeterminate square E73C), and the
 *   chevron padding collapses to 0,0,14,0.
 * - Content: Body 14 text (optional 16px leading icon glyph), foreground
 *   primary / pressed secondary / disabled disabled; chevron follows.
 * - Focus: double ring around the row (margin 0,−1,0,−1 over the 2px row
 *   margin, i.e. the backplate expanded 1px vertically).
 */
#include "controls/draw/flux_button_internal.h"
#include "render/flux_fluent.h"
#include "render/flux_render_internal.h"
#include "fluxent/controls/flux_tree_view_data.h"

#include <string.h>

/* Row background — TreeViewItemBackground* (SubtleFill*). */
static FluxColor tree_row_fill(FluxThemeColors const *t, FluxControlState const *state, bool selected) {
	if (state->enabled && state->pressed) return selected ? t->subtle_fill_secondary : t->subtle_fill_tertiary;
	if (selected && state->enabled && state->hovered) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

/* Content foreground and chevron — TreeViewItemForeground* (chevron follows). */
static FluxColor tree_row_fg(FluxThemeColors const *t, FluxControlState const *state) {
	if (!state->enabled) return t->text_disabled;
	if (state->pressed) return t->text_secondary;
	return t->text_primary;
}

/* Multi-select tri-state checkbox (standard CheckBox visuals; the row fill
 * carries the selected state, so the box only mirrors sel_state). */
static void tree_draw_checkbox(
  FluxRenderContext const *rc, FluxThemeColors const *t, FluxRect const *box, FluxControlState const *state,
  uint8_t sel_state
) {
	if (sel_state == FLUX_TREE_SEL_NONE) {
		flux_fill_rounded_rect(rc, box, 3.0f, t->ctrl_alt_fill_secondary);
		FluxColor stroke = state->enabled ? t->ctrl_strong_stroke_default : t->ctrl_strong_stroke_disabled;
		flux_draw_rounded_rect(rc, box, 3.0f, stroke, 1.0f);
		return;
	}
	FluxColor fill = !state->enabled ? t->accent_disabled
	    : state->pressed             ? t->accent_tertiary
	    : state->hovered             ? t->accent_secondary
	                                 : t->accent_default;
	flux_fill_rounded_rect(rc, box, 3.0f, fill);

	FluxColor glyph = !state->enabled ? t->text_on_accent_disabled
	    : state->pressed              ? t->text_on_accent_secondary
	                                  : t->text_on_accent_primary;
	/* Selected: CheckMark E73E; Partial: the E73C indeterminate square. */
	if (sel_state == FLUX_TREE_SEL_SELECTED) flux_button_draw_chevron(rc, box, L"\xE73E", 16.0f, glyph);
	else flux_button_draw_chevron(rc, box, L"\xE73C", 12.0f, glyph);
}

static void tree_draw_label(FluxRenderContext const *rc, FluxRect const *tr, char const *label, FluxColor fg) {
	if (!label || !label [0] || !rc->text || tr->w <= 0.0f) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_TREE_FONT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = fg;
	flux_text_draw(rc->text, FLUX_RT(rc), label, tr, &ts);
}

void flux_draw_tree_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const      *t  = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxTreeItemSnapshot const *ti = &snap->u.tree_item;

	/* Per-node disable folds into the state the brush tables read. */
	FluxControlState st = *state;
	if (ti->flags & FLUX_TREE_ROW_DISABLED) st.enabled = 0;

	bool     multi    = (ti->flags & FLUX_TREE_ROW_MULTI) != 0;
	bool     selected = ti->sel_state == FLUX_TREE_SEL_SELECTED;

	/* Backplate: TreeViewItemPresenterMargin 4,2 inset, radius 4. */
	FluxRect ib = {
	  bounds->x + FLUX_TREE_ROW_MARGIN_H, bounds->y + FLUX_TREE_ROW_MARGIN_V,
	  bounds->w - 2.0f * FLUX_TREE_ROW_MARGIN_H, bounds->h - 2.0f * FLUX_TREE_ROW_MARGIN_V};
	if (ib.w < 1.0f || ib.h < 1.0f) return;

	FluxColor fill = tree_row_fill(t, &st, selected);
	if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, fill);

	/* Selection pill: Single mode only — 3×16 r1.5 at the backplate's left
	 * edge, vertically centered; Multiple shows selection via the checkbox. */
	if (!multi && selected) {
		FluxRect pill = {ib.x, ib.y + (ib.h - FLUX_TREE_PILL_H) * 0.5f, FLUX_TREE_PILL_W, FLUX_TREE_PILL_H};
		flux_fill_rounded_rect(rc, &pill, FLUX_TREE_PILL_R, st.enabled ? t->accent_default : t->text_disabled);
	}

	FluxColor fg = tree_row_fg(t, &st);

	/* [indent 16×depth] [chevron 14+12+14] [content] — Multiple mode:
	 * [indent] [checkbox 10+32] [chevron 12+14] [content]. */
	float ix = bounds->x + FLUX_TREE_ROW_MARGIN_H + FLUX_TREE_INDENT * ( float ) ti->depth;
	float chevron_x, text_x;
	if (multi) {
		FluxRect box = {
		  ix + FLUX_TREE_CHECK_MARGIN_L, ib.y + (ib.h - FLUX_TREE_CHECK_BOX) * 0.5f, FLUX_TREE_CHECK_BOX,
		  FLUX_TREE_CHECK_BOX};
		tree_draw_checkbox(rc, t, &box, &st, ti->sel_state);
		chevron_x = ix + FLUX_TREE_MULTI_CHEVRON_X; /* chevron padding 0,0,14,0 */
		text_x    = chevron_x + FLUX_TREE_CHEVRON_ZONE_MULTI;
	}
	else {
		chevron_x = ix + FLUX_TREE_CHEVRON_PAD;
		text_x    = ix + FLUX_TREE_CHEVRON_ZONE;
	}

	/* Instant E76C ↔ E70D swap at FontSize 8; hidden on leaves (the zone
	 * still occupies layout — it never collapses). */
	if (ti->flags & FLUX_TREE_ROW_HAS_CHILDREN) {
		FluxRect cb = {chevron_x, ib.y, FLUX_TREE_CHEVRON_BOX, ib.h};
		flux_button_draw_chevron(
		  rc, &cb, (ti->flags & FLUX_TREE_ROW_EXPANDED) ? L"\xE70D" : L"\xE76C", FLUX_TREE_CHEVRON_FONT, fg
		);
	}

	/* Optional leading icon glyph (16px, NavigationView-style cell). */
	if (ti->glyph) {
		wchar_t  glyph [2] = {( wchar_t ) ti->glyph, 0};
		FluxRect gb        = {text_x, ib.y, FLUX_TREE_ICON_GLYPH, ib.h};
		flux_button_draw_chevron(rc, &gb, glyph, FLUX_TREE_ICON_GLYPH, fg);
		text_x += FLUX_TREE_ICON_ADVANCE;
	}

	FluxRect tr = {text_x, ib.y, ib.x + ib.w - text_x - 4.0f, ib.h};
	tree_draw_label(rc, &tr, ti->label, fg);

	/* Focus ring around the whole row: margin 0,−1,0,−1 expands the
	 * backplate 1px vertically over the 2px row margin. */
	if (st.focused && st.enabled) {
		FluxRect fr = {ib.x, ib.y - 1.0f, ib.w, ib.h + 2.0f};
		flux_draw_focus_rect(rc, &fr, FLUX_CORNER_RADIUS);
	}
}
