#include "../flux_fluent.h"
#include "../flux_icon.h"

typedef struct {
    FluxColor fill;
    FluxColor stroke;
    FluxColor text;
    bool      use_elevation;
    bool      is_accent;
} ButtonColors;

static ButtonColors resolve_button_colors(const FluxRenderSnapshot *snap,
                                          const FluxControlState *state,
                                          bool force_accent,
                                          const FluxThemeColors *t) {
    ButtonColors c;
    c.is_accent = force_accent || (snap->button_style == FLUX_BUTTON_ACCENT);
    c.use_elevation = state->enabled && !state->pressed;

    FluxColor transparent = flux_color_rgba(0, 0, 0, 0);

    if (!state->enabled) {
        if (c.is_accent) {
            c.fill   = t ? t->accent_disabled          : ft_accent_disabled();
            c.stroke = transparent;
            c.text   = t ? t->text_on_accent_disabled   : ft_text_on_accent_disabled();
        } else {
            c.fill   = t ? t->ctrl_fill_disabled        : ft_ctrl_fill_disabled();
            c.stroke = t ? t->ctrl_stroke_default       : ft_ctrl_stroke_default();
            c.text   = t ? t->text_disabled             : ft_text_disabled();
        }
        c.use_elevation = false;
    } else if (c.is_accent) {
        if (state->pressed) {
            c.fill = t ? t->accent_tertiary             : ft_accent_tertiary();
            c.text = t ? t->text_on_accent_secondary    : ft_text_on_accent_secondary();
        } else if (state->hovered) {
            c.fill = t ? t->accent_secondary            : ft_accent_secondary();
            c.text = t ? t->text_on_accent_primary      : ft_text_on_accent_primary();
        } else {
            c.fill = t ? t->accent_default              : ft_accent_default();
            c.text = t ? t->text_on_accent_primary      : ft_text_on_accent_primary();
        }
        c.stroke = transparent;
    } else {
        if (state->pressed) {
            c.fill   = t ? t->ctrl_fill_tertiary        : ft_ctrl_fill_tertiary();
            c.stroke = t ? t->ctrl_stroke_default       : ft_ctrl_stroke_default();
            c.text   = t ? t->text_secondary            : ft_text_secondary();
        } else if (state->hovered) {
            c.fill   = t ? t->ctrl_fill_secondary       : ft_ctrl_fill_secondary();
            c.stroke = transparent;
            c.text   = t ? t->text_primary              : ft_text_primary();
        } else {
            c.fill   = t ? t->ctrl_fill_default         : ft_ctrl_fill_default();
            c.stroke = transparent;
            c.text   = t ? t->text_primary              : ft_text_primary();
        }
    }

    return c;
}

static FluxColor apply_user_background(FluxColor base, const FluxRenderSnapshot *snap,
                                       const FluxControlState *state, bool is_accent) {
    FluxColor user_bg = snap->background;
    if (is_accent || !state->enabled || flux_color_af(user_bg) <= 0.0f)
        return base;

    FluxColor result = user_bg;
    if (state->pressed) {
        uint8_t a = (uint8_t)(flux_color_af(result) * FLUX_INTERACTION_PRESSED_ALPHA * 255.0f);
        result.rgba = (result.rgba & 0xFFFFFF00) | a;
    } else if (state->hovered) {
        uint8_t a = (uint8_t)(flux_color_af(result) * FLUX_INTERACTION_HOVER_ALPHA * 255.0f);
        result.rgba = (result.rgba & 0xFFFFFF00) | a;
    }
    return result;
}

/* Convert a wide-char icon codepoint to UTF-8 (BMP only, max 3 bytes). */
static int icon_wchar_to_utf8(const wchar_t *wc, char *out, int cap) {
    if (!wc || !wc[0] || cap < 4) { if (out && cap > 0) out[0] = '\0'; return 0; }
    unsigned int cp = (unsigned int)wc[0];
    int n = 0;
    if (cp < 0x80) {
        out[n++] = (char)cp;
    } else if (cp < 0x800) {
        out[n++] = (char)(0xC0 | (cp >> 6));
        out[n++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[n++] = (char)(0xE0 | (cp >> 12));
        out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[n++] = (char)(0x80 | (cp & 0x3F));
    }
    out[n] = '\0';
    return n;
}

static void draw_button_content(const FluxRenderContext *rc,
                                const FluxRenderSnapshot *snap,
                                const FluxRect *sb,
                                FluxColor text_color) {
    /* Resolve icon codepoint */
    const wchar_t *icon_wc = NULL;
    char icon_utf8[8] = {0};
    if (snap->icon_name && snap->icon_name[0]) {
        icon_wc = flux_icon_lookup(snap->icon_name);
        if (icon_wc)
            icon_wchar_to_utf8(icon_wc, icon_utf8, sizeof(icon_utf8));
    }

    bool has_icon = (icon_utf8[0] != '\0');
    const char *label = snap->label ? snap->label : snap->text_content;
    bool has_text = (label && label[0]);

    if (!has_icon && !has_text)
        return;
    if (!rc->text)
        return;

    float font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;

    FluxColor actual_text = text_color;
    if (flux_color_af(snap->text_color) > 0.0f)
        actual_text = snap->text_color;

    /* Text style */
    FluxTextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.font_size = font_size;
    ts.font_weight = FLUX_FONT_REGULAR;
    ts.text_align = FLUX_TEXT_CENTER;
    ts.vert_align = FLUX_TEXT_VCENTER;
    ts.color = actual_text;
    ts.word_wrap = false;

    /* Icon style — Segoe Fluent Icons, slightly larger */
    FluxTextStyle is;
    memset(&is, 0, sizeof(is));
    is.font_family = "Segoe Fluent Icons";
    is.font_size = font_size * FLUX_BTN_ICON_SIZE_MUL;
    is.font_weight = FLUX_FONT_REGULAR;
    is.text_align = FLUX_TEXT_CENTER;
    is.vert_align = FLUX_TEXT_VCENTER;
    is.color = actual_text;
    is.word_wrap = false;

    /* Measure content */
    float icon_w = 0.0f;
    float text_w = 0.0f;

    if (has_icon)
        icon_w = flux_text_measure(rc->text, icon_utf8, &is, 0).w;

    if (has_text)
        text_w = flux_text_measure(rc->text, label, &ts, 0).w;

    float content_gap = (has_icon && has_text) ? FLUX_BTN_CONTENT_GAP : 0.0f;
    float total_w = icon_w + text_w + content_gap;

    /* Layout */
    float pad_l = sb->w > 0 ? FLUX_BTN_PAD_LEFT : 0;
    float pad_r = sb->w > 0 ? FLUX_BTN_PAD_RIGHT : 0;
    float pad_t = sb->h > 0 ? FLUX_BTN_PAD_TOP : 0;
    float pad_b = sb->h > 0 ? FLUX_BTN_PAD_BOTTOM : 0;

    FluxRect area = {
        sb->x + pad_l, sb->y + pad_t,
        flux_maxf(0.0f, sb->w - pad_l - pad_r),
        flux_maxf(0.0f, sb->h - pad_t - pad_b)
    };

    float start_x = area.x + (area.w - total_w) * 0.5f;

    D2D1_RECT_F clip = flux_d2d_rect(sb);
    ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float cur_x = start_x;

    if (has_icon) {
        FluxRect icon_rect = { cur_x, area.y, icon_w, area.h };
        flux_text_draw(rc->text, FLUX_RT(rc), icon_utf8, &icon_rect, &is);
        cur_x += icon_w + content_gap;
    }

    if (has_text) {
        FluxRect text_rect = { cur_x, area.y, text_w, area.h };
        flux_text_draw(rc->text, FLUX_RT(rc), label, &text_rect, &ts);
    }

    ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static void render_button_common(const FluxRenderContext *rc,
                                 const FluxRenderSnapshot *snap,
                                 const FluxRect *bounds,
                                 const FluxControlState *state,
                                 bool force_accent) {
    float sx = 1.0f, sy = 1.0f;
    FluxRect sb = flux_snap_bounds(bounds, sx, sy);

    float radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
    ButtonColors colors = resolve_button_colors(snap, state, force_accent, rc->theme);

    colors.fill = apply_user_background(colors.fill, snap, state, colors.is_accent);

    FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
    if (ce) {
        bool animating = flux_tween_update_states(
            &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim,
            state->hovered, state->pressed, state->focused, false,
            rc->now);
        if (animating && rc->animations_active)
            *rc->animations_active = true;

        /* Animate fill color via tween */
        FluxColor tweened_fill;
        bool color_active = flux_color_tween_update(
            &ce->color_anim, colors.fill,
            FLUX_ANIM_DURATION_FAST, rc->now, &tweened_fill);
        colors.fill = tweened_fill;
        if (color_active && rc->animations_active)
            *rc->animations_active = true;
    }

    float thickness = FLUX_STROKE_WIDTH / sy;
    float fill_inset = colors.is_accent ? 0.0f : thickness;
    float fill_radius = radius - fill_inset;
    if (fill_radius < 0.0f) fill_radius = 0.0f;

    FluxRect fill_rect = {
        sb.x + fill_inset, sb.y + fill_inset,
        sb.w - fill_inset * 2.0f, sb.h - fill_inset * 2.0f
    };
    if (fill_rect.w < 0.0f) fill_rect.w = 0.0f;
    if (fill_rect.h < 0.0f) fill_rect.h = 0.0f;

    flux_fill_rounded_rect(rc, &fill_rect, fill_radius, colors.fill);

    if (colors.use_elevation) {
        flux_draw_elevation_border(rc, &sb, radius, colors.is_accent);
    } else if (flux_color_af(colors.stroke) > 0.0f) {
        float half = thickness * 0.5f;
        FluxRect stroke_rect = {
            sb.x + half, sb.y + half,
            sb.w - half * 2.0f, sb.h - half * 2.0f
        };
        flux_draw_rounded_rect(rc, &stroke_rect, radius, colors.stroke, thickness);
    }

    if (state->focused && state->enabled)
        flux_draw_focus_rect(rc, &sb, radius);

    draw_button_content(rc, snap, &sb, colors.text);
}

void flux_draw_button(const FluxRenderContext *rc,
                      const FluxRenderSnapshot *snap,
                      const FluxRect *bounds,
                      const FluxControlState *state) {
    render_button_common(rc, snap, bounds, state, false);
}

void flux_draw_toggle(const FluxRenderContext *rc,
                      const FluxRenderSnapshot *snap,
                      const FluxRect *bounds,
                      const FluxControlState *state) {
    render_button_common(rc, snap, bounds, state, snap->is_checked);
}