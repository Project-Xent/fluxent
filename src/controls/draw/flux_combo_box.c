#include "render/flux_fluent.h"
#include "controls/draw/flux_button_internal.h"

#include <string.h>

/* WinUI ComboBox closed box: ComboBoxBackground == ControlFillColorDefault with the
 * ControlElevationBorder, i.e. identical chrome to a standard button — reused via
 * flux_button_paint_chrome. ComboBoxPadding left 12; chevron E70D @12 in the right
 * column. The open state reads as pressed. */
#define CB_PAD_LEFT      12.0f
#define CB_CHEVRON_W     32.0f
#define CB_CHEVRON_GLYPH L"\xE70D"
#define CB_CHEVRON_FONT  12.0f

void flux_draw_combo_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxControlState st = *state;
	if (snap->is_checked) st.pressed = true;

	FluxButtonChrome       chrome  = flux_button_paint_chrome(rc, snap, bounds, &st, false);
	FluxThemeColors const *t       = rc->theme ? rc->theme : flux_theme_default_colors();

	bool                   has_sel = snap->text_content && snap->text_content [0];
	char const            *text    = has_sel ? snap->text_content : snap->placeholder;
	FluxColor              color   = !state->enabled ? t->text_disabled : has_sel ? t->text_primary : t->text_secondary;

	if (text && text [0] && rc->text) {
		FluxRect tr = {chrome.sb.x + CB_PAD_LEFT, chrome.sb.y, chrome.sb.w - CB_PAD_LEFT - CB_CHEVRON_W, chrome.sb.h};
		if (tr.w > 0.0f) {
			D2D1_RECT_F clip = {tr.x, tr.y, tr.x + tr.w, tr.y + tr.h};
			ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
			FluxTextStyle ts;
			memset(&ts, 0, sizeof(ts));
			ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
			ts.font_weight = FLUX_FONT_REGULAR;
			ts.text_align  = FLUX_TEXT_LEFT;
			ts.vert_align  = FLUX_TEXT_VCENTER;
			ts.color       = color;
			flux_text_draw(rc->text, FLUX_RT(rc), text, &tr, &ts);
			ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
		}
	}

	FluxRect  chev  = {chrome.sb.x + chrome.sb.w - CB_CHEVRON_W, chrome.sb.y, CB_CHEVRON_W, chrome.sb.h};
	FluxColor chcol = state->enabled ? t->text_secondary : t->text_disabled;
	flux_button_draw_chevron(rc, &chev, CB_CHEVRON_GLYPH, CB_CHEVRON_FONT, chcol);
}
