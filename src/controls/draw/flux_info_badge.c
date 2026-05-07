#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include <stdio.h>
#include <string.h>

static FluxTextStyle
info_badge_text_style(FluxColor color, float font_size, FluxFontWeight weight, char const *family) {
	FluxTextStyle style;
	memset(&style, 0, sizeof(style));
	style.font_family = family;
	style.font_size   = font_size;
	style.font_weight = weight;
	style.text_align  = FLUX_TEXT_CENTER;
	style.vert_align  = FLUX_TEXT_VCENTER;
	style.color       = color;
	style.word_wrap   = false;
	return style;
}

static void draw_info_badge_dot(FluxRenderContext const *rc, FluxRect const *bounds, FluxColor accent) {
	float dot_size = 8.0f;
	float dot_r    = dot_size * 0.5f;
	float cx       = bounds->x + bounds->w * 0.5f;
	float cy       = bounds->y + bounds->h * 0.5f;
	flux_fill_ellipse(rc, &(FluxEllipseSpec) {cx, cy, dot_r, dot_r}, accent);
}

static void draw_info_badge_icon(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxColor accent,
  FluxColor text_color
) {
	float badge_size = 16.0f;
	float badge_r    = badge_size * 0.5f;
	float cx         = bounds->x + bounds->w * 0.5f;
	float cy         = bounds->y + bounds->h * 0.5f;
	flux_fill_ellipse(rc, &(FluxEllipseSpec) {cx, cy, badge_r, badge_r}, accent);

	if (!snap->icon_name || !snap->icon_name [0] || !rc->text) return;

	wchar_t const *icon_wc       = flux_icon_lookup(snap->icon_name);
	char           icon_utf8 [8] = {0};
	if (icon_wc) flux_icon_to_utf8(icon_wc, icon_utf8, sizeof(icon_utf8));
	if (!icon_utf8 [0]) return;

	FluxTextStyle is        = info_badge_text_style(text_color, 10.0f, FLUX_FONT_REGULAR, "Segoe Fluent Icons");
	FluxRect      icon_rect = {cx - badge_r, cy - badge_r, badge_size, badge_size};
	flux_text_draw(rc->text, FLUX_RT(rc), icon_utf8, &icon_rect, &is);
}

static float info_badge_number_width(FluxRenderContext const *rc, char const *num_buf, FluxTextStyle const *ts) {
	float badge_w = 16.0f;
	if (!rc->text) return badge_w;

	FluxSize measured = flux_text_measure(rc->text, num_buf, ts, 0);
	float    needed_w = measured.w + 12.0f;
	if (needed_w > badge_w) badge_w = needed_w;
	return badge_w;
}

static void draw_info_badge_number(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxColor accent,
  FluxColor text_color
) {
	int  value = ( int ) (snap->current_value + 0.5f);
	char num_buf [16];
	snprintf(num_buf, sizeof(num_buf), "%d", value);

	float         badge_h    = 16.0f;
	FluxTextStyle ts         = info_badge_text_style(text_color, 10.0f, FLUX_FONT_SEMI_BOLD, NULL);
	float         badge_w    = info_badge_number_width(rc, num_buf, &ts);
	float         badge_r    = badge_h * 0.5f;
	float         bx         = bounds->x + (bounds->w - badge_w) * 0.5f;
	float         by         = bounds->y + (bounds->h - badge_h) * 0.5f;
	FluxRect      badge_rect = {bx, by, badge_w, badge_h};
	flux_fill_rounded_rect(rc, &badge_rect, badge_r, accent);
	if (rc->text) flux_text_draw(rc->text, FLUX_RT(rc), num_buf, &badge_rect, &ts);
}

void flux_draw_info_badge(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t          = rc->theme;

	FluxColor              accent     = t ? t->accent_default : ft_accent_default();
	FluxColor              text_color = t ? t->text_on_accent_primary : ft_text_on_accent_primary();

	if (snap->is_checked) {
		draw_info_badge_dot(rc, bounds, accent);
		return;
	}
	if (snap->indeterminate) {
		draw_info_badge_icon(rc, snap, bounds, accent, text_color);
		return;
	}
	if (snap->current_value >= 1.0f) draw_info_badge_number(rc, snap, bounds, accent, text_color);
}
