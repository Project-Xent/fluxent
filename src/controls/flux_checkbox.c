#include "../flux_fluent.h"
#include "../flux_icon.h"

/* U+E73E (CheckMark) encoded as UTF-8 */
static const char kCheckGlyphUtf8[] = "\xEE\x9C\xBE";

void flux_draw_checkbox(const FluxRenderContext *rc,
                        const FluxRenderSnapshot *snap,
                        const FluxRect *bounds,
                        const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;

    FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;

    float box_size = FLUX_CHECKBOX_SIZE;
    float box_radius = FLUX_CHECKBOX_CORNER;
    float cy = bounds->y + bounds->h * 0.5f;
    FluxRect box = { bounds->x, cy - box_size * 0.5f, box_size, box_size };

    bool checked = (snap->check_state == FLUX_CHECK_CHECKED);
    if (ce) {
        bool animating = flux_tween_update_states(
            &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim,
            state->hovered, state->pressed, state->focused, checked, rc->now);
        if (animating && rc->animations_active)
            *rc->animations_active = true;
    }
    float check_progress = ce ? ce->check_anim.current : (checked ? 1.0f : 0.0f);

    FluxColor unchecked_fill, checked_fill;
    FluxColor unchecked_stroke;
    FluxColor glyph_color = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);

    if (!state->enabled) {
        unchecked_fill = t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4D);
        checked_fill = t ? t->accent_disabled : flux_color_rgba(0, 0, 0, 0x37);
        unchecked_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
        glyph_color = t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255);
    } else if (state->pressed) {
        unchecked_fill = t ? t->ctrl_fill_tertiary : flux_color_rgba(249, 249, 249, 0x4D);
        checked_fill = t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xCC);
        unchecked_stroke = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 140);
    } else if (state->hovered) {
        unchecked_fill = t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
        checked_fill = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xE6);
        unchecked_stroke = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 140);
    } else {
        unchecked_fill = t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xB3);
        checked_fill = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        unchecked_stroke = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 140);
    }

    FluxColor fill = flux_anim_lerp_color(unchecked_fill, checked_fill, check_progress);

    float stroke_alpha = flux_color_af(unchecked_stroke) * (1.0f - check_progress);
    FluxColor stroke = flux_color_rgba(
        (unchecked_stroke.rgba >> 24) & 0xFF,
        (unchecked_stroke.rgba >> 16) & 0xFF,
        (unchecked_stroke.rgba >> 8) & 0xFF,
        (uint8_t)(stroke_alpha * 255.0f + 0.5f));

    flux_fill_rounded_rect(rc, &box, box_radius, fill);
    if (flux_color_af(stroke) > 0.0f)
        flux_draw_rounded_rect(rc, &box, box_radius, stroke, 1.0f);

    if (check_progress > 0.01f && rc->text) {
        FluxTextStyle icon_style;
        memset(&icon_style, 0, sizeof(icon_style));
        icon_style.font_family = "Segoe Fluent Icons";
        icon_style.font_size = 12.0f * flux_ease_out_quad(check_progress);
        icon_style.font_weight = FLUX_FONT_REGULAR;
        icon_style.text_align = FLUX_TEXT_CENTER;
        icon_style.vert_align = FLUX_TEXT_VCENTER;
        icon_style.color = glyph_color;
        icon_style.word_wrap = false;

        flux_text_draw(rc->text, (ID2D1RenderTarget *)rc->d2d,
                       kCheckGlyphUtf8, &box, &icon_style);
    }

    /* Label text */
    if (snap->label && snap->label[0] && rc->text) {
        float gap = FLUX_CHECKBOX_GAP;
        FluxRect text_rect = {
            bounds->x + box_size + gap,
            bounds->y,
            flux_maxf(0.0f, bounds->w - box_size - gap),
            bounds->h
        };
        FluxColor label_color = state->enabled
            ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4))
            : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C));

        FluxTextStyle ts;
        memset(&ts, 0, sizeof(ts));
        ts.font_size = 14.0f;
        ts.font_weight = FLUX_FONT_REGULAR;
        ts.text_align = FLUX_TEXT_LEFT;
        ts.vert_align = FLUX_TEXT_VCENTER;
        ts.color = label_color;
        ts.word_wrap = false;

        flux_text_draw(rc->text, (ID2D1RenderTarget *)rc->d2d,
                       snap->label, &text_rect, &ts);
    }

    if (state->focused && state->enabled)
        flux_draw_focus_rect(rc, &box, box_radius);
}