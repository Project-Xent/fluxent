#include "render/flux_fluent.h"
#include "controls/draw/flux_control_draw.h"

#include <string.h>

/* WinUI MenuBarItem (MenuBarItem.xaml CommonStates). The bar itself is
 * transparent; each item shows a subtle fill that reacts to pointer state and
 * to whether its flyout is open (the Selected state, carried as snap->is_checked).
 * BorderThickness is 0 outside high contrast, so no border is drawn. */
void flux_draw_menu_bar_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t    = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               ib   = flux_snap_bounds(bounds, 1.0f, 1.0f);

	bool                   open = snap->is_checked;
	if (state->enabled && (open || state->pressed))
		flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, t->subtle_fill_tertiary);
	else if (state->enabled && state->hovered)
		flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, t->subtle_fill_secondary);

	if (!snap->label || !rc->text) return;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_MENU_BAR_ITEM_FONT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = state->enabled ? t->text_primary : t->text_disabled;
	flux_text_draw(rc->text, FLUX_RT(rc), snap->label, &ib, &ts);
}
