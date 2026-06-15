#include "render/flux_fluent.h"
#include "controls/draw/flux_button_internal.h"

/* WinUI DropDownButton: standard Button chrome, then a trailing chevron (column 1,
 * Margin 8,0,0,0) drawn in DropDownButtonForegroundSecondary — one tier weaker
 * than the label foreground. */
#define FLUX_DDB_CHEVRON_GAP 8.0f

static FluxColor dropdown_chevron_color(FluxControlState const *state, bool is_accent, FluxThemeColors const *t) {
	if (!state->enabled) return t->text_disabled;
	if (is_accent) return state->pressed ? t->text_on_accent_secondary : t->text_on_accent_primary;
	if (state->hovered || state->pressed) return t->text_tertiary;
	return t->text_secondary;
}

void flux_draw_dropdown_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxButtonChrome       chrome  = flux_button_paint_chrome(rc, snap, bounds, state, snap->is_checked);
	FluxThemeColors const *theme   = rc->theme ? rc->theme : flux_theme_default_colors();

	/* flux_button_draw_content insets the box by the standard symmetric button
	 * padding itself, so only the chevron box + gap are trimmed here; including
	 * FLUX_BTN_PAD_RIGHT again would shift the centered label off to the left. */
	float                  reserve = FLUX_CHEVRON_BOX + FLUX_DDB_CHEVRON_GAP;
	FluxRect content_box           = {chrome.sb.x, chrome.sb.y, flux_maxf(0.0f, chrome.sb.w - reserve), chrome.sb.h};
	flux_button_draw_content(rc, snap, &content_box, chrome.text_color);

	FluxRect chevron_box
	  = {chrome.sb.x + chrome.sb.w - FLUX_BTN_PAD_RIGHT - FLUX_CHEVRON_BOX, chrome.sb.y, FLUX_CHEVRON_BOX, chrome.sb.h};
	flux_button_draw_chevron(
	  rc, &chevron_box, FLUX_CHEVRON_GLYPH, FLUX_CHEVRON_FONT, dropdown_chevron_color(state, chrome.is_accent, theme)
	);
}
