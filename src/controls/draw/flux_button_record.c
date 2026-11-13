#include "controls/draw/flux_control_record_internal.h"
#include "controls/draw/flux_button_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "fluxent/flux_text.h"

#include <string.h>

typedef struct {
	FluxColor fill;
	FluxColor stroke;
	FluxColor text;
	bool      use_elevation;
	bool      is_accent;
} ButtonColors;

typedef struct {
	char        icon_utf8 [8];
	char const *label;
	bool        has_icon;
	bool        has_text;
} ButtonContent;

static ButtonColors button_colors_disabled(bool is_accent, bool is_subtle, FluxThemeColors const *t) {
	FluxColor    transparent = flux_color_rgba(0, 0, 0, 0);
	ButtonColors c           = {0};
	c.is_accent              = is_accent;
	if (is_accent) {
		c.fill = t->accent_disabled;
		c.text = t->text_on_accent_disabled;
		return c;
	}
	if (is_subtle) {
		c.text = t->text_disabled;
		return c;
	}
	c.fill   = t->ctrl_fill_disabled;
	c.stroke = t->ctrl_stroke_default;
	c.text   = t->text_disabled;
	return c;
}

static ButtonColors button_colors_accent(FluxControlState const *state, FluxThemeColors const *t) {
	ButtonColors c  = {0};
	c.is_accent     = true;
	c.use_elevation = !state->pressed;
	if (state->pressed) {
		c.fill = t->accent_tertiary;
		c.text = t->text_on_accent_secondary;
		return c;
	}
	if (state->hovered) {
		c.fill = t->accent_secondary;
		c.text = t->text_on_accent_primary;
		return c;
	}
	c.fill = t->accent_default;
	c.text = t->text_on_accent_primary;
	return c;
}

static ButtonColors
button_colors_subtle(FluxControlState const *state, FluxThemeColors const *t, bool pressed_primary) {
	ButtonColors c = {0};
	if (state->pressed) {
		c.fill = t->subtle_fill_tertiary;
		c.text = pressed_primary ? t->text_primary : t->text_secondary;
		return c;
	}
	if (state->hovered) {
		c.fill = t->subtle_fill_secondary;
		c.text = t->text_primary;
		return c;
	}
	c.text = t->text_primary;
	return c;
}

static ButtonColors button_colors_default(FluxControlState const *state, FluxThemeColors const *t) {
	ButtonColors c  = {0};
	c.use_elevation = !state->pressed;
	c.text          = t->text_primary;
	c.fill          = t->ctrl_fill_default;
	if (state->hovered) c.fill = t->ctrl_fill_secondary;
	if (state->pressed) {
		c.fill   = t->ctrl_fill_tertiary;
		c.stroke = t->ctrl_stroke_default;
		c.text   = t->text_secondary;
	}
	return c;
}

static ButtonColors resolve_button_colors(
  FluxRenderSnap const *snap, FluxControlState const *state, bool force_accent, FluxThemeColors const *t
) {
	bool is_accent = force_accent || snap->u.button.button_style == FLUX_BUTTON_ACCENT;
	bool is_subtle
	  = snap->u.button.button_style == FLUX_BUTTON_SUBTLE || snap->u.button.button_style == FLUX_BUTTON_TEXT;
	if (!state->enabled) return button_colors_disabled(is_accent, is_subtle, t);
	if (is_accent) return button_colors_accent(state, t);
	if (is_subtle) return button_colors_subtle(state, t, snap->u.button.pressed_primary);
	return button_colors_default(state, t);
}

static FluxColor
apply_user_background(FluxColor base, FluxRenderSnap const *snap, FluxControlState const *state, bool is_accent) {
	FluxColor user_bg = snap->background;
	if (is_accent || !state->enabled || flux_color_af(user_bg) <= 0.0f) return base;
	FluxColor result = user_bg;
	if (state->pressed) {
		uint8_t a   = ( uint8_t ) (flux_color_af(result) * FLUX_INTERACTION_PRESSED_ALPHA * 255.0f);
		result.rgba = (result.rgba & 0xffffff00u) | a;
	}
	else if (state->hovered) {
		uint8_t a   = ( uint8_t ) (flux_color_af(result) * FLUX_INTERACTION_HOVER_ALPHA * 255.0f);
		result.rgba = (result.rgba & 0xffffff00u) | a;
	}
	return result;
}

FluxRecButtonColors flux_rec_button_colors(
  FluxRenderSnap const *snap, FluxControlState const *state, bool force_accent, FluxThemeColors const *t
) {
	ButtonColors c = resolve_button_colors(snap, state, force_accent, t);
	return (FluxRecButtonColors) {c.fill, c.stroke, c.text, c.use_elevation, c.is_accent};
}

static FluxRect button_fill_rect(FluxRect const *sb, bool is_accent) {
	float    thickness  = FLUX_STROKE_WIDTH;
	float    fill_inset = is_accent ? 0.0f : thickness;
	FluxRect fill_rect = {sb->x + fill_inset, sb->y + fill_inset, sb->w - fill_inset * 2.0f, sb->h - fill_inset * 2.0f};
	if (fill_rect.w < 0.0f) fill_rect.w = 0.0f;
	if (fill_rect.h < 0.0f) fill_rect.h = 0.0f;
	return fill_rect;
}

static float button_fill_radius(float radius, bool is_accent) {
	float fill_inset  = is_accent ? 0.0f : FLUX_STROKE_WIDTH;
	float fill_radius = radius - fill_inset;
	return fill_radius < 0.0f ? 0.0f : fill_radius;
}

static bool button_draw_border(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *sb, float radius, ButtonColors const *colors
) {
	if (colors->use_elevation) {
		if (rc) return flux_rec_elevation_border(b, rc, sb, radius, colors->is_accent);
		FluxColor stroke = colors->is_accent ? flux_color_rgba(0, 0, 0, 0) : colors->stroke;
		if (flux_color_af(stroke) <= 0.0f) {
			FluxThemeColors const *t = flux_theme_defaults();
			stroke                   = colors->is_accent ? t->ctrl_stroke_on_accent_default : t->ctrl_stroke_default;
		}
		return flux_rec_stroke_rounded(b, sb, radius, stroke, FLUX_STROKE_WIDTH);
	}
	if (flux_color_af(colors->stroke) <= 0.0f) return true;
	return flux_rec_stroke_rounded(b, sb, radius, colors->stroke, FLUX_STROKE_WIDTH);
}

bool flux_rec_button_chrome(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRect const *bounds, float radius, FluxRecButtonColors *colors
) {
	if (snap && state) {
		( void ) flux_rec_animstate(rc, snap, state, false);
		colors->fill = flux_rec_tweenfill(rc, snap, colors->fill);
	}
	ButtonColors c           = {colors->fill, colors->stroke, colors->text, colors->use_elevation, colors->is_accent};
	FluxRect     fill_rect   = button_fill_rect(bounds, c.is_accent);
	float        fill_radius = button_fill_radius(radius, c.is_accent);
	bool         ok          = flux_rec_fillsink(b, rc, &fill_rect, fill_radius, c.fill);
	if (ok) ok = button_draw_border(b, rc, bounds, radius, &c);
	return ok;
}

static bool button_paint_chrome(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state, bool force_accent, ButtonColors *out
) {
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxThemeColors const *theme  = flux_rec_theme(rc);
	ButtonColors           colors = resolve_button_colors(snap, state, force_accent, theme);
	colors.fill                   = apply_user_background(colors.fill, snap, state, colors.is_accent);
	( void ) flux_rec_animstate(rc, snap, state, false);
	colors.fill          = flux_rec_tweenfill(rc, snap, colors.fill);

	FluxRect fill_rect   = button_fill_rect(bounds, colors.is_accent);
	float    fill_radius = button_fill_radius(radius, colors.is_accent);
	bool     ok          = flux_rec_fillsink(b, rc, &fill_rect, fill_radius, colors.fill);
	if (ok) ok = button_draw_border(b, rc, bounds, radius, &colors);
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, flux_rec_theme(rc), bounds, radius);
	*out = colors;
	return ok;
}

static FluxRect button_content_area(FluxRect const *sb) {
	float pad_l = sb->w > 0.0f ? FLUX_BTN_PAD_LEFT : 0.0f;
	float pad_r = sb->w > 0.0f ? FLUX_BTN_PAD_RIGHT : 0.0f;
	float pad_t = sb->h > 0.0f ? FLUX_BTN_PAD_TOP : 0.0f;
	float pad_b = sb->h > 0.0f ? FLUX_BTN_PAD_BOTTOM : 0.0f;
	return (FluxRect) {
	  sb->x + pad_l, sb->y + pad_t, flux_maxf(0.0f, sb->w - pad_l - pad_r), flux_maxf(0.0f, sb->h - pad_t - pad_b)};
}

static float measure_w(FluxText *tr, char const *utf8, float size, char const *family) {
	if (!tr || !utf8 || !utf8 [0]) return 0.0f;
	FluxTextStyle style;
	memset(&style, 0, sizeof(style));
	style.font_family = family;
	style.font_size   = size;
	style.font_weight = FLUX_FONT_REGULAR;
	return flux_text_measure(tr, utf8, &style, 0.0f).w;
}

static ButtonContent button_content_from_snapshot(FluxRenderSnap const *snap) {
	ButtonContent content = {0};
	content.label         = snap->u.button.label ? snap->u.button.label : snap->u.button.text_content;
	content.has_text      = content.label && content.label [0];
	if (snap->u.button.icon_name && snap->u.button.icon_name [0]) {
		wchar_t const *wc = flux_icon_lookup(snap->u.button.icon_name);
		if (wc) flux_icon_to_utf8(wc, content.icon_utf8, sizeof(content.icon_utf8));
	}
	content.has_icon = content.icon_utf8 [0] != '\0';
	return content;
}

bool flux_rec_button_label(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds, FluxColor text_color) {
	char const *label = snap->u.button.label ? snap->u.button.label : snap->u.button.text_content;
	if (!label || !label [0]) return true;
	if (flux_color_af(snap->u.button.text_color) > 0.0f) text_color = snap->u.button.text_color;
	float         font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	FluxRect      area      = button_content_area(bounds);
	XentTextStyle style     = {.font_size = font_size, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	return flux_rec_text_ex(b, &area, label, &style, text_color);
}

static bool button_draw_icon_label(
  XentDlBuilder *b, FluxText *tr, FluxRenderSnap const *snap, FluxRect const *bounds, FluxColor text_color
) {
	ButtonContent content = button_content_from_snapshot(snap);
	if (!content.has_icon && !content.has_text) return true;
	if (flux_color_af(snap->u.button.text_color) > 0.0f) text_color = snap->u.button.text_color;

	float font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	float icon_size = font_size * FLUX_BTN_ICON_SIZE_MUL;
	float icon_w    = content.has_icon ? measure_w(tr, content.icon_utf8, icon_size, flux_icon_font_family()) : 0.0f;
	float text_w    = content.has_text ? measure_w(tr, content.label, font_size, NULL) : 0.0f;
	if (content.has_icon && icon_w < icon_size) icon_w = icon_size;
	float    gap     = (content.has_icon && content.has_text) ? FLUX_BTN_CONTENT_GAP : 0.0f;
	FluxRect area    = button_content_area(bounds);
	float    total_w = icon_w + text_w + gap;
	float    cur_x   = area.x + (area.w - total_w) * 0.5f;
	bool     ok      = true;

	if (!xent_dl_pushcliprect(b, flux_rec_xrect(bounds))) return false;
	if (content.has_icon) {
		FluxRect      ir = {cur_x, area.y, icon_w, area.h};
		XentTextStyle is = {
		  .font_size   = icon_size,
		  .font_weight = 400u,
		  .h_align     = 1u,
		  .v_align     = 1u,
		  .flags       = XENT_DL_TEXT_ICON,
		  .font_family = flux_icon_font_family(),
		};
		ok     = flux_rec_text_ex(b, &ir, content.icon_utf8, &is, text_color);
		cur_x += icon_w + gap;
	}
	if (ok && content.has_text) {
		FluxRect      trc = {cur_x, area.y, text_w, area.h};
		XentTextStyle ts  = {.font_size = font_size, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
		ok                = flux_rec_text_ex(b, &trc, content.label, &ts, text_color);
	}
	return xent_dl_popclip(b) && ok;
}

bool flux_rec_build_button(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state, bool force_accent
) {
	FluxRect     sb     = flux_rec_snap_bounds(rc, bounds);
	ButtonColors colors = {0};
	bool         ok     = button_paint_chrome(b, rc, snap, &sb, state, force_accent, &colors);
	if (!ok) return false;
	return button_draw_icon_label(b, rc ? rc->text : NULL, snap, &sb, colors.text);
}

#define REC_DDB_CHEVRON_GAP 8.0f
#define REC_CB_PAD_LEFT     12.0f

bool flux_rec_build_dropdown(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxRecButtonColors    colors = flux_rec_button_colors(snap, state, snap->u.button.is_checked, t);
	bool                   ok     = flux_rec_button_chrome(b, rc, snap, state, bounds, radius, &colors);
	if (!ok) return false;

	float    reserve = FLUX_CHEVRON_BOX + REC_DDB_CHEVRON_GAP;
	FluxRect content = {bounds->x, bounds->y, flux_maxf(0.0f, bounds->w - reserve), bounds->h};
	ok               = flux_rec_button_label(b, snap, &content, colors.text);
	if (!ok) return false;

	FluxRect chev
	  = {bounds->x + bounds->w - FLUX_BTN_PAD_RIGHT - FLUX_CHEVRON_BOX, bounds->y, FLUX_CHEVRON_BOX, bounds->h};
	FluxColor chcol = state->enabled ? t->text_secondary : t->text_disabled;
	return flux_rec_glyph_wc(b, &chev, FLUX_CHEVRON_GLYPH, FLUX_CHEVRON_FONT, chcol);
}

static bool combo_record_text(XentDlBuilder *b, FluxRect const *bounds, char const *text, FluxColor color) {
	if (!text || !text [0]) return true;
	FluxRect tr
	  = {bounds->x + REC_CB_PAD_LEFT, bounds->y, bounds->w - REC_CB_PAD_LEFT - FLUX_COMBO_GLYPH_COL, bounds->h};
	if (tr.w <= 0.0f) return true;
	if (!xent_dl_pushcliprect(b, flux_rec_xrect(&tr))) return false;
	bool ok = flux_rec_text(b, &tr, text, FLUX_FONT_SIZE_DEFAULT, 400u, color);
	return xent_dl_popclip(b) && ok;
}

static bool combo_record_highlight(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds) {
	FluxRect hb = {bounds->x - 4.0f, bounds->y - 4.0f, bounds->w + 8.0f, bounds->h + 8.0f};
	bool     ok = flux_rec_fill_rounded(b, &hb, 7.0f, t->ctrl_fill_default);
	return ok && flux_rec_stroke_rounded(b, &hb, 7.0f, t->focus_stroke_outer, 2.0f);
}

static bool combo_record_pill(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds) {
	FluxRect pill = {bounds->x + 1.0f, bounds->y + (bounds->h - 16.0f) * 0.5f, 3.0f, 16.0f};
	return flux_rec_fill_rounded(b, &pill, 1.5f, t->accent_default);
}

static bool combo_record_overlay(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	if (!state->enabled || !state->hovered || snap->hover_local_x < bounds->w - FLUX_COMBO_GLYPH_COL) return true;
	FluxRect ov = {bounds->x + bounds->w - 34.0f, bounds->y + 4.0f, 30.0f, bounds->h - 8.0f};
	return flux_rec_fill_rounded(b, &ov, 4.0f, state->pressed ? t->subtle_fill_tertiary : t->subtle_fill_secondary);
}

static float combo_chevron_release_y(double age) {
	if (age < 0.0833) return 31.5f - 10.5f * flux_cubic_bezier(( float ) (age / 0.0833), 0.55f, 0.0f, 0.75f, 1.0f);
	return 21.0f + 3.0f * flux_cubic_bezier(( float ) fmin((age - 0.0833) / 0.2333, 1.0), 0.35f, 0.0f, 0.0f, 1.0f);
}

static float combo_chevron_dy(FluxRenderCtx const *rc, FluxRenderSnap const *snap, bool pressed) {
	FluxVisualState const *vs = flux_rec_vstate(rc, snap, pressed ? 1u : 0u);
	if (!vs) return pressed ? 1.875f : 0.0f;
	double age     = rc->now - vs->since;
	bool   release = !pressed && vs->prev == 1u && age < 0.3167;
	float  y       = 24.0f;
	if (pressed) y += 7.5f * flux_cubic_bezier(( float ) fmin(age / 0.15, 1.0), 0.167f, 0.167f, 0.65f, 1.0f);
	if (release) y = combo_chevron_release_y(age);
	if (release || (pressed && age < 0.15)) flux_rec_note_animating(rc, true);
	return (y - 24.0f) * 0.25f;
}

static FluxColor combo_text_color(FluxThemeColors const *t, FluxControlState const *st, bool has_sel, bool focused) {
	if (!st->enabled) return t->text_disabled;
	bool pressed = st->pressed && !focused;
	if (has_sel) return pressed ? t->text_secondary : t->text_primary;
	return pressed ? t->text_tertiary : t->text_secondary;
}

bool flux_rec_build_combo(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t       = flux_rec_theme(rc);
	float                  radius  = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   focused = state->focused && state->enabled && !snap->u.combo.is_checked;
	FluxRecButtonColors    colors  = flux_rec_button_colors(snap, state, false, t);
	bool                   ok      = !focused || combo_record_highlight(b, t, bounds);
	if (ok) ok = flux_rec_button_chrome(b, rc, snap, state, bounds, radius, &colors);
	if (!ok || snap->u.combo.edit_active) return ok;
	if (focused && !combo_record_pill(b, t, bounds)) return false;
	if (snap->u.combo.editable && !combo_record_overlay(b, t, snap, bounds, state)) return false;

	bool        has_sel = snap->u.combo.text_content && snap->u.combo.text_content [0];
	char const *text    = has_sel ? snap->u.combo.text_content : snap->u.combo.placeholder;
	if (!combo_record_text(b, bounds, text, combo_text_color(t, state, has_sel, focused))) return false;
	float    dy    = combo_chevron_dy(rc, snap, state->pressed && state->enabled);
	FluxRect glyph = {bounds->x + bounds->w - 26.0f, bounds->y + dy, 12.0f, bounds->h};
	return flux_rec_glyph_wc(b, &glyph, L"\xE70D", 12.0f, state->enabled ? t->text_secondary : t->text_disabled);
}

static bool rec_sb_fill_zone(
  XentDlBuilder *b, FluxRect const *zone, FluxRect const *sb, float radius, FluxColor fill, bool round_left
) {
	bool ok = xent_dl_pushcliprect(b, flux_rec_xrect(zone));
	if (!ok) return false;
	FluxRect ext = round_left ? (FluxRect) {zone->x, sb->y, zone->w + radius, sb->h}
	                          : (FluxRect) {zone->x - radius, sb->y, zone->w + radius, sb->h};
	ok           = flux_rec_fill_rounded(b, &ext, radius, fill);
	return xent_dl_popclip(b) && ok;
}

static FluxColor sb_zone_fill_color(bool hover, bool press, bool checked, bool enabled, FluxThemeColors const *t) {
	if (!enabled) return flux_color_rgba(0, 0, 0, 0);
	if (checked) return press ? t->accent_tertiary : hover ? t->accent_secondary : t->accent_default;
	return press ? t->ctrl_fill_tertiary : hover ? t->ctrl_fill_secondary : t->ctrl_fill_default;
}

static FluxColor sb_zone_text_color(bool press, bool checked, bool enabled, FluxThemeColors const *t) {
	if (!enabled) return checked ? t->text_on_accent_disabled : t->text_disabled;
	if (checked) return press ? t->text_on_accent_secondary : t->text_on_accent_primary;
	return press ? t->text_secondary : t->text_primary;
}

static bool rec_sb_border(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, float radius, bool checked,
  FluxControlState const *state, bool pressed, FluxThemeColors const *t
) {
	if (state->enabled && !pressed) return flux_rec_elevation_border(b, rc, bounds, radius, checked);
	return flux_rec_stroke_rounded(b, bounds, radius, t->ctrl_stroke_default, FLUX_STROKE_WIDTH);
}

static FluxColor sb_chevron_color(bool hover, bool press, bool checked, bool enabled, FluxThemeColors const *t) {
	FluxColor color = sb_zone_text_color(press, checked, enabled, t);
	if (!enabled || checked) return color;
	if (press) return t->text_tertiary;
	return hover ? t->text_primary : t->text_secondary;
}

bool flux_rec_split(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxRect               sb        = flux_rec_snap_bounds(rc, bounds);
	FluxThemeColors const *t         = flux_rec_theme(rc);
	float                  radius    = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   checked   = snap->u.button.is_checked;
	float                  primary_w = flux_maxf(0.0f, sb.w - FLUX_SPLIT_SECONDARY_W - FLUX_SPLIT_DIVIDER_W);
	bool                   prim_hov
	  = state->enabled && snap->hover_local_x >= 0.0f && snap->hover_local_x < primary_w + FLUX_SPLIT_DIVIDER_W;
	bool      sec_hov        = state->enabled && snap->hover_local_x >= primary_w + FLUX_SPLIT_DIVIDER_W;
	bool      prim_press     = prim_hov && state->pressed;
	bool      sec_press      = sec_hov && state->pressed;

	FluxColor primary_fill   = sb_zone_fill_color(prim_hov, prim_press, checked, state->enabled, t);
	FluxColor secondary_fill = sb_zone_fill_color(sec_hov, sec_press, checked, state->enabled, t);
	FluxRect  primary        = {sb.x, sb.y, primary_w, sb.h};
	FluxRect  divider        = {sb.x + primary_w, sb.y, FLUX_SPLIT_DIVIDER_W, sb.h};
	FluxRect  secondary      = {divider.x + divider.w, sb.y, FLUX_SPLIT_SECONDARY_W, sb.h};
	FluxRect  divider_draw   = divider;
	divider_draw.x           = flux_rec_snapx(rc, divider.x);

	bool ok                  = rec_sb_fill_zone(b, &primary, &sb, radius, primary_fill, true);
	if (ok && state->enabled)
		ok
		  = flux_rec_fill_rect(b, &divider_draw, checked ? t->ctrl_stroke_on_accent_secondary : t->ctrl_stroke_default);
	if (ok) ok = rec_sb_fill_zone(b, &secondary, &sb, radius, secondary_fill, false);
	if (ok) ok = rec_sb_border(b, rc, &sb, radius, checked, state, prim_press || sec_press, t);
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, flux_rec_theme(rc), &sb, radius);
	FluxColor text = sb_zone_text_color(prim_press, checked, state->enabled, t);
	if (ok) ok = flux_rec_button_label(b, snap, &primary, text);
	FluxColor chev     = sb_chevron_color(sec_hov, sec_press, checked, state->enabled, t);
	FluxRect  chev_box = {secondary.x + secondary.w - 12.0f - FLUX_CHEVRON_BOX, sb.y, FLUX_CHEVRON_BOX, sb.h};
	if (ok) ok = flux_rec_glyph_wc(b, &chev_box, FLUX_CHEVRON_GLYPH, FLUX_CHEVRON_FONT, chev);
	return ok;
}
