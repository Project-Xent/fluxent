#include "render/flux_fluent.h"
#include "controls/draw/flux_button_internal.h"
#include "controls/draw/flux_control_draw.h"

/* WinUI SplitButton: 3-column grid — primary action (*), 1px divider, 35px
 * secondary dropdown zone. Outer corners follow ControlCornerRadius; the inner
 * (shared) edge is square. SplitButtonPadding 11,6,11,7. */
#define FLUX_SB_SECONDARY_W FLUX_SPLIT_SECONDARY_W
#define FLUX_SB_DIVIDER_W   FLUX_SPLIT_DIVIDER_W
#define FLUX_SB_CHEVRON_PAD 12.0f /* SecondaryButton Padding 0,0,12,0 */

typedef enum
{
	SB_ZONE_NONE,
	SB_ZONE_PRIMARY,
	SB_ZONE_SECONDARY,
} SbZone;

typedef struct {
	FluxColor fill;
	FluxColor text;
} SbZoneColors;

static SbZone sb_zone_at(float local_x, float primary_w) {
	if (local_x < 0.0f) return SB_ZONE_NONE;
	return local_x >= primary_w + FLUX_SB_DIVIDER_W ? SB_ZONE_SECONDARY : SB_ZONE_PRIMARY;
}

static SbZoneColors sb_zone_colors(bool hover, bool press, bool checked, bool enabled, FluxThemeColors const *t) {
	if (!enabled)
		return (SbZoneColors) {flux_color_rgba(0, 0, 0, 0), checked ? t->text_on_accent_disabled : t->text_disabled};
	if (checked)
		return (SbZoneColors) {
		  press   ? t->accent_tertiary
		  : hover ? t->accent_secondary
				  : t->accent_default,
		  press ? t->text_on_accent_secondary : t->text_on_accent_primary};
	return (SbZoneColors) {
	  press   ? t->ctrl_fill_tertiary
	  : hover ? t->ctrl_fill_secondary
			  : t->ctrl_fill_default,
	  press ? t->text_secondary : t->text_primary};
}

/* Secondary foreground (the chevron): default SplitButtonForegroundSecondary
 * (text_secondary), brightening to SplitButtonForegroundPointerOver (text_primary)
 * on hover and dimming to SplitButtonForegroundSecondaryPressed (text_tertiary)
 * on press — matching the SecondaryPointerOver / SecondaryPressed states. */
static FluxColor sb_chevron_color(bool hover, bool press, bool checked, bool enabled, FluxThemeColors const *t) {
	if (!enabled) return t->text_disabled;
	if (checked) return press ? t->text_on_accent_secondary : t->text_on_accent_primary;
	if (press) return t->text_tertiary;
	if (hover) return t->text_primary;
	return t->text_secondary;
}

/* Fill one zone with exactly one (possibly translucent) layer — no whole-button
 * base fill underneath, so a hovered zone's tint never bleeds into the other. The
 * rounded rect is extended past the inner edge and clipped to the zone, so only the
 * outer corners round and the shared edge stays square. */
static void sb_fill_zone(
  FluxRenderContext const *rc, FluxRect const *zone, FluxRect const *sb, float radius, FluxColor fill, bool round_left
) {
	D2D1_RECT_F clip = flux_d2d_rect(zone);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	FluxRect ext = round_left ? (FluxRect) {zone->x, sb->y, zone->w + radius, sb->h}
	                          : (FluxRect) {zone->x - radius, sb->y, zone->w + radius, sb->h};
	flux_fill_rounded_rect(rc, &ext, radius, fill);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void sb_draw_border(
  FluxRenderContext const *rc, FluxRect const *sb, float radius, bool checked, bool enabled, bool pressed
) {
	if (enabled && !pressed) {
		flux_draw_elevation_border(rc, sb, radius, checked);
		return;
	}
	FluxThemeColors const *t      = rc->theme ? rc->theme : flux_theme_default_colors();
	float                  half   = FLUX_STROKE_WIDTH * 0.5f;
	FluxRect               stroke = {sb->x + half, sb->y + half, sb->w - half * 2.0f, sb->h - half * 2.0f};
	flux_draw_rounded_rect(rc, &stroke, radius, t->ctrl_stroke_default, FLUX_STROKE_WIDTH);
}

void flux_draw_split_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxRect               sb         = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float                  radius     = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxThemeColors const *t          = rc->theme ? rc->theme : flux_theme_default_colors();
	bool                   checked    = snap->is_checked;
	bool                   enabled    = state->enabled;

	float                  primary_w  = flux_maxf(0.0f, sb.w - FLUX_SB_SECONDARY_W - FLUX_SB_DIVIDER_W);
	SbZone                 hov        = enabled ? sb_zone_at(snap->hover_local_x, primary_w) : SB_ZONE_NONE;
	bool                   prim_press = hov == SB_ZONE_PRIMARY && state->pressed;
	bool                   sec_press  = hov == SB_ZONE_SECONDARY && state->pressed;

	SbZoneColors           prim       = sb_zone_colors(hov == SB_ZONE_PRIMARY, prim_press, checked, enabled, t);
	SbZoneColors           sec        = sb_zone_colors(hov == SB_ZONE_SECONDARY, sec_press, checked, enabled, t);

	FluxRect               prim_zone  = {sb.x, sb.y, primary_w, sb.h};
	FluxRect               sec_rect   = {sb.x + primary_w + FLUX_SB_DIVIDER_W, sb.y, FLUX_SB_SECONDARY_W, sb.h};
	sb_fill_zone(rc, &prim_zone, &sb, radius, prim.fill, true);
	sb_fill_zone(rc, &sec_rect, &sb, radius, sec.fill, false);

	if (enabled) {
		FluxColor div = checked ? t->ctrl_stroke_on_accent_secondary : t->ctrl_stroke_default;
		float     dx  = sb.x + primary_w + FLUX_SB_DIVIDER_W * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {dx, sb.y, dx, sb.y + sb.h}, div, FLUX_SB_DIVIDER_W);
	}

	sb_draw_border(rc, &sb, radius, checked, enabled, prim_press || sec_press);
	if (state->focused && enabled) flux_draw_focus_rect(rc, &sb, radius);

	FluxRect prim_rect = {sb.x, sb.y, primary_w, sb.h};
	flux_button_draw_content(rc, snap, &prim_rect, prim.text);

	FluxRect chev = {sec_rect.x + sec_rect.w - FLUX_SB_CHEVRON_PAD - FLUX_CHEVRON_BOX, sb.y, FLUX_CHEVRON_BOX, sb.h};
	flux_button_draw_chevron(
	  rc, &chev, FLUX_CHEVRON_GLYPH, FLUX_CHEVRON_FONT,
	  sb_chevron_color(hov == SB_ZONE_SECONDARY, sec_press, checked, enabled, t)
	);
}
