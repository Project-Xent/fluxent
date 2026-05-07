#include "render/flux_fluent.h"
#include "render/flux_icon.h"

typedef struct {
	FluxColor fill;
	FluxColor stroke;
	FluxColor text;
	bool      use_elevation;
	bool      is_accent;
} ButtonColors;

typedef struct {
	wchar_t const *icon_wc;
	char           icon_utf8 [8];
	char const    *label;
	bool           has_icon;
	bool           has_text;
} ButtonContent;

typedef struct {
	FluxTextStyle text;
	FluxTextStyle icon;
} ButtonContentStyles;

typedef struct {
	float    icon_w;
	float    text_w;
	float    gap;
	FluxRect area;
} ButtonContentMetrics;

static ButtonColors button_colors_disabled(bool is_accent, bool is_subtle, FluxThemeColors const *t) {
	FluxColor    transparent = flux_color_rgba(0, 0, 0, 0);
	ButtonColors c           = {0};
	c.is_accent              = is_accent;
	c.use_elevation          = false;

	if (is_accent) {
		c.fill   = t ? t->accent_disabled : ft_accent_disabled();
		c.stroke = transparent;
		c.text   = t ? t->text_on_accent_disabled : ft_text_on_accent_disabled();
		return c;
	}

	if (is_subtle) {
		c.fill   = transparent;
		c.stroke = transparent;
		c.text   = t ? t->text_disabled : ft_text_disabled();
		return c;
	}

	c.fill   = t ? t->ctrl_fill_disabled : ft_ctrl_fill_disabled();
	c.stroke = t ? t->ctrl_stroke_default : ft_ctrl_stroke_default();
	c.text   = t ? t->text_disabled : ft_text_disabled();
	return c;
}

static ButtonColors button_colors_accent(FluxControlState const *state, FluxThemeColors const *t) {
	ButtonColors c  = {0};
	c.is_accent     = true;
	c.use_elevation = !state->pressed;
	c.stroke        = flux_color_rgba(0, 0, 0, 0);

	if (state->pressed) {
		c.fill = t ? t->accent_tertiary : ft_accent_tertiary();
		c.text = t ? t->text_on_accent_secondary : ft_text_on_accent_secondary();
		return c;
	}
	if (state->hovered) {
		c.fill = t ? t->accent_secondary : ft_accent_secondary();
		c.text = t ? t->text_on_accent_primary : ft_text_on_accent_primary();
		return c;
	}

	c.fill = t ? t->accent_default : ft_accent_default();
	c.text = t ? t->text_on_accent_primary : ft_text_on_accent_primary();
	return c;
}

static ButtonColors button_colors_subtle(FluxControlState const *state, FluxThemeColors const *t) {
	ButtonColors c  = {0};
	c.stroke        = flux_color_rgba(0, 0, 0, 0);
	c.use_elevation = false;

	if (state->pressed) {
		c.fill = t ? t->subtle_fill_tertiary : ft_subtle_fill_tertiary();
		c.text = t ? t->text_secondary : ft_text_secondary();
		return c;
	}
	if (state->hovered) {
		c.fill = t ? t->subtle_fill_secondary : ft_subtle_fill_secondary();
		c.text = t ? t->text_primary : ft_text_primary();
		return c;
	}

	c.fill = flux_color_rgba(0, 0, 0, 0);
	c.text = t ? t->text_primary : ft_text_primary();
	return c;
}

static ButtonColors button_colors_default(FluxControlState const *state, FluxThemeColors const *t) {
	ButtonColors c  = {0};
	c.use_elevation = !state->pressed;

	if (state->pressed) {
		c.fill   = t ? t->ctrl_fill_tertiary : ft_ctrl_fill_tertiary();
		c.stroke = t ? t->ctrl_stroke_default : ft_ctrl_stroke_default();
		c.text   = t ? t->text_secondary : ft_text_secondary();
		return c;
	}
	if (state->hovered) {
		c.fill   = t ? t->ctrl_fill_secondary : ft_ctrl_fill_secondary();
		c.stroke = flux_color_rgba(0, 0, 0, 0);
		c.text   = t ? t->text_primary : ft_text_primary();
		return c;
	}

	c.fill   = t ? t->ctrl_fill_default : ft_ctrl_fill_default();
	c.stroke = flux_color_rgba(0, 0, 0, 0);
	c.text   = t ? t->text_primary : ft_text_primary();
	return c;
}

static ButtonColors resolve_button_colors(
  FluxRenderSnapshot const *snap, FluxControlState const *state, bool force_accent, FluxThemeColors const *t
) {
	bool is_accent = force_accent || (snap->button_style == FLUX_BUTTON_ACCENT);
	bool is_subtle = (snap->button_style == FLUX_BUTTON_SUBTLE || snap->button_style == FLUX_BUTTON_TEXT);

	if (!state->enabled) return button_colors_disabled(is_accent, is_subtle, t);
	if (is_accent) return button_colors_accent(state, t);
	if (is_subtle) return button_colors_subtle(state, t);
	return button_colors_default(state, t);
}

static FluxColor
apply_user_background(FluxColor base, FluxRenderSnapshot const *snap, FluxControlState const *state, bool is_accent) {
	FluxColor user_bg = snap->background;
	if (is_accent || !state->enabled || flux_color_af(user_bg) <= 0.0f) return base;

	FluxColor result = user_bg;
	if (state->pressed) {
		uint8_t a   = ( uint8_t ) (flux_color_af(result) * FLUX_INTERACTION_PRESSED_ALPHA * 255.0f);
		result.rgba = (result.rgba & 0xffffff00) | a;
	}
	else if (state->hovered) {
		uint8_t a   = ( uint8_t ) (flux_color_af(result) * FLUX_INTERACTION_HOVER_ALPHA * 255.0f);
		result.rgba = (result.rgba & 0xffffff00) | a;
	}
	return result;
}

static ButtonContent button_content_from_snapshot(FluxRenderSnapshot const *snap) {
	ButtonContent content = {0};
	content.label         = snap->label ? snap->label : snap->text_content;
	content.has_text      = content.label && content.label [0];

	if (snap->icon_name && snap->icon_name [0]) {
		content.icon_wc = flux_icon_lookup(snap->icon_name);
		if (content.icon_wc) flux_icon_to_utf8(content.icon_wc, content.icon_utf8, sizeof(content.icon_utf8));
	}

	content.has_icon = content.icon_utf8 [0] != '\0';
	return content;
}

static FluxColor button_content_text_color(FluxRenderSnapshot const *snap, FluxColor fallback) {
	if (flux_color_af(snap->text_color) > 0.0f) return snap->text_color;
	return fallback;
}

static FluxTextStyle button_text_style(float font_size, FluxColor color) {
	FluxTextStyle style;
	memset(&style, 0, sizeof(style));
	style.font_size   = font_size;
	style.font_weight = FLUX_FONT_REGULAR;
	style.text_align  = FLUX_TEXT_CENTER;
	style.vert_align  = FLUX_TEXT_VCENTER;
	style.color       = color;
	style.word_wrap   = false;
	return style;
}

static FluxTextStyle button_icon_style(float font_size, FluxColor color) {
	FluxTextStyle style = button_text_style(font_size * FLUX_BTN_ICON_SIZE_MUL, color);
	style.font_family   = "Segoe Fluent Icons";
	return style;
}

static ButtonContentStyles button_content_styles(FluxRenderSnapshot const *snap, FluxColor fallback) {
	float               font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	FluxColor           color     = button_content_text_color(snap, fallback);
	ButtonContentStyles styles    = {0};
	styles.text                   = button_text_style(font_size, color);
	styles.icon                   = button_icon_style(font_size, color);
	return styles;
}

static FluxRect button_content_area(FluxRect const *sb) {
	float pad_l = sb->w > 0 ? FLUX_BTN_PAD_LEFT : 0;
	float pad_r = sb->w > 0 ? FLUX_BTN_PAD_RIGHT : 0;
	float pad_t = sb->h > 0 ? FLUX_BTN_PAD_TOP : 0;
	float pad_b = sb->h > 0 ? FLUX_BTN_PAD_BOTTOM : 0;
	return (FluxRect) {
	  sb->x + pad_l, sb->y + pad_t, flux_maxf(0.0f, sb->w - pad_l - pad_r), flux_maxf(0.0f, sb->h - pad_t - pad_b)};
}

static ButtonContentMetrics button_content_metrics(
  FluxRenderContext const *rc, ButtonContent const *content, ButtonContentStyles const *styles, FluxRect const *sb
) {
	ButtonContentMetrics metrics = {0};
	metrics.icon_w = content->has_icon ? flux_text_measure(rc->text, content->icon_utf8, &styles->icon, 0).w : 0.0f;
	metrics.text_w = content->has_text ? flux_text_measure(rc->text, content->label, &styles->text, 0).w : 0.0f;
	metrics.gap    = (content->has_icon && content->has_text) ? FLUX_BTN_CONTENT_GAP : 0.0f;
	metrics.area   = button_content_area(sb);
	return metrics;
}

static void button_draw_content_parts(
  FluxRenderContext const *rc, ButtonContent const *content, ButtonContentStyles const *styles,
  ButtonContentMetrics const *metrics
) {
	float total_w = metrics->icon_w + metrics->text_w + metrics->gap;
	float cur_x   = metrics->area.x + (metrics->area.w - total_w) * 0.5f;

	if (content->has_icon) {
		FluxRect icon_rect = {cur_x, metrics->area.y, metrics->icon_w, metrics->area.h};
		flux_text_draw(rc->text, FLUX_RT(rc), content->icon_utf8, &icon_rect, &styles->icon);
		cur_x += metrics->icon_w + metrics->gap;
	}

	if (content->has_text) {
		FluxRect text_rect = {cur_x, metrics->area.y, metrics->text_w, metrics->area.h};
		flux_text_draw(rc->text, FLUX_RT(rc), content->label, &text_rect, &styles->text);
	}
}

static void draw_button_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *sb, FluxColor text_color
) {
	if (!rc->text) return;

	ButtonContent content = button_content_from_snapshot(snap);
	if (!content.has_icon && !content.has_text) return;

	ButtonContentStyles  styles  = button_content_styles(snap, text_color);
	ButtonContentMetrics metrics = button_content_metrics(rc, &content, &styles, sb);

	D2D1_RECT_F          clip    = flux_d2d_rect(sb);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	button_draw_content_parts(rc, &content, &styles, &metrics);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void button_update_animation(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state, ButtonColors *colors
) {
	FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (!ce) return;

	FluxTweenChannels channels  = {&ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim};
	FluxTweenTargets  targets   = {state->hovered, state->pressed, state->focused, false};
	bool              animating = flux_tween_update_states(&channels, targets, rc->now);
	if (animating && rc->animations_active) *rc->animations_active = true;

	FluxColor tweened_fill;
	bool      color_active
	  = flux_color_tween_update(&ce->color_anim, colors->fill, FLUX_ANIM_DURATION_FAST, rc->now, &tweened_fill);
	colors->fill = tweened_fill;
	if (color_active && rc->animations_active) *rc->animations_active = true;
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

static void
button_draw_border(FluxRenderContext const *rc, FluxRect const *sb, float radius, ButtonColors const *colors) {
	if (colors->use_elevation) {
		flux_draw_elevation_border(rc, sb, radius, colors->is_accent);
		return;
	}

	if (flux_color_af(colors->stroke) <= 0.0f) return;

	float    half        = FLUX_STROKE_WIDTH * 0.5f;
	FluxRect stroke_rect = {sb->x + half, sb->y + half, sb->w - half * 2.0f, sb->h - half * 2.0f};
	flux_draw_rounded_rect(rc, &stroke_rect, radius, colors->stroke, FLUX_STROKE_WIDTH);
}

static void render_button_common(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state,
  bool force_accent
) {
	FluxRect     sb     = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float        radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	ButtonColors colors = resolve_button_colors(snap, state, force_accent, rc->theme);

	colors.fill         = apply_user_background(colors.fill, snap, state, colors.is_accent);
	button_update_animation(rc, snap, state, &colors);

	FluxRect fill_rect   = button_fill_rect(&sb, colors.is_accent);
	float    fill_radius = button_fill_radius(radius, colors.is_accent);
	flux_fill_rounded_rect(rc, &fill_rect, fill_radius, colors.fill);
	button_draw_border(rc, &sb, radius, &colors);

	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &sb, radius);

	draw_button_content(rc, snap, &sb, colors.text);
}

void flux_draw_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	render_button_common(rc, snap, bounds, state, false);
}

void flux_draw_toggle(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	render_button_common(rc, snap, bounds, state, snap->is_checked);
}
