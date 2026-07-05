#include "controls/draw/flux_button_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"

#include "fluxent/controls/flux_breadcrumb_data.h"

#include <string.h>

/* WinUI BreadcrumbBarItem (BreadcrumbBar_themeresources.xaml). Background and
 * border are Transparent in every state — only the crumb foreground reacts
 * (TextFillColorSecondary on hover, Tertiary on press, discrete swaps, no
 * hover pill). The last (current) item is plain text with no hover/pressed
 * feedback; the separator chevron (E974 @12, E973 in RTL — fluxent is
 * LTR-only for now) stays TextFillColorPrimary in all states. Hover/press
 * recolor only inside the content zone: the chevron cell is a static
 * separator outside the inner button. */
/* Foreground per state; hover/press react only for clickable crumbs with the
 * pointer inside the content zone. */
static FluxColor bc_item_fg(
  FluxThemeColors const *t, FluxControlState const *state, FluxBreadcrumbSnapshot const *bc, bool in_zone
) {
	if (!state->enabled) return t->text_disabled;
	if (bc->is_last || !in_zone) return t->text_primary;
	if (state->pressed) return t->text_tertiary;
	if (state->hovered) return t->text_secondary;
	return t->text_primary;
}

static void bc_item_draw_label(FluxRenderContext const *rc, FluxRect const *content, char const *label, FluxColor fg) {
	if (!label || !rc->text) return;
	FluxRect tr = {
	  content->x + FLUX_BREADCRUMB_PAD_H, content->y, content->w - 2.0f * FLUX_BREADCRUMB_PAD_H, content->h};
	if (tr.w <= 0.0f) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_BREADCRUMB_FONT;
	ts.font_weight = FLUX_FONT_REGULAR; /* NOT SemiBold, even for the current item */
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = fg;
	flux_text_draw(rc->text, FLUX_RT(rc), label, &tr, &ts);
}

void flux_draw_breadcrumb_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	if (bounds->w < 1.0f || bounds->h < 1.0f) return; /* hidden by the collapse (0x0) */
	FluxThemeColors const        *t  = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxBreadcrumbSnapshot const *bc = &snap->u.breadcrumb;

	float     content_w = flux_minf(bc->content_w, bounds->w);
	FluxRect  content   = {bounds->x, bounds->y, content_w, bounds->h};
	bool      in_zone   = snap->hover_local_x >= 0.0f && snap->hover_local_x <= content_w;
	FluxColor fg        = bc_item_fg(t, state, bc, in_zone);

	if (bc->is_ellipsis)
		flux_button_draw_chevron(rc, &content, FLUX_BREADCRUMB_ELLIPSIS, FLUX_BREADCRUMB_ELLIPSIS_FONT, fg);
	else bc_item_draw_label(rc, &content, bc->label, fg);

	if (bc->show_chevron && bounds->w > content_w) {
		FluxRect cell = {bounds->x + content_w, bounds->y, bounds->w - content_w, bounds->h};
		flux_button_draw_chevron(rc, &cell, FLUX_BREADCRUMB_CHEVRON_LTR, FLUX_BREADCRUMB_CHEVRON_FONT, t->text_primary);
	}

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &content, FLUX_CORNER_RADIUS);
}
