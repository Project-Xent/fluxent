#include "../flux_fluent.h"
#include <string.h>

void flux_draw_radio(const FluxRenderContext *rc,
                     const FluxRenderSnapshot *snap,
                     const FluxRect *bounds,
                     const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;

    FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;

    float outer_r = 9.0f;
    float cy = bounds->y + bounds->h * 0.5f;
    float cx = bounds->x + outer_r;

    bool checked = (snap->check_state == FLUX_CHECK_CHECKED);

    if (ce) {
        bool animating = flux_tween_update_states(
            &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim,
            state->hovered, state->pressed, state->focused, checked, rc->now);
        if (animating && rc->animations_active)
            *rc->animations_active = true;
    }
    float check_progress = ce ? ce->check_anim.current : (checked ? 1.0f : 0.0f);

    /* --- State-based fill colors (matches C++ reference) --- */
    FluxColor outer_fill, outer_stroke, glyph_fill;

    if (!state->enabled) {
        outer_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
        outer_fill = t ? t->ctrl_alt_fill_disabled : flux_color_rgba(0, 0, 0, 0);
        glyph_fill = checked ? (t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255))
                             : flux_color_rgba(0, 0, 0, 0);
    } else if (checked) {
        outer_stroke = state->pressed ? (t ? t->accent_tertiary : flux_color_rgba(0, 120, 212, 0xCC))
                       : state->hovered ? (t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xE6))
                       : (t ? t->accent_default : flux_color_rgb(0, 120, 212));
        outer_fill = outer_stroke;
        glyph_fill = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
    } else {
        outer_stroke = state->pressed ? (t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37))
                                      : (t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9C));
        outer_fill = state->pressed ? (t ? t->ctrl_alt_fill_quarternary : flux_color_rgba(0, 0, 0, 0x12))
                     : state->hovered ? (t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0F))
                     : (t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06));
        glyph_fill = flux_color_rgba(0, 0, 0, 0);
    }

    /* Draw outer circle */
    flux_fill_ellipse(rc, cx, cy, outer_r, outer_r, outer_fill);
    flux_stroke_ellipse(rc, cx, cy, outer_r, outer_r, outer_stroke, 1.0f);

    /* Checked glyph (inner dot) — use check_progress for animated radius */
    if (check_progress > 0.01f) {
        float inner_r = 4.0f * flux_ease_out_quad(check_progress);
        flux_fill_ellipse(rc, cx, cy, inner_r, inner_r, glyph_fill);
    }

    /* Pressed + unchecked filled dot */
    if (state->pressed && !checked && state->enabled) {
        float pressed_r = 5.0f; /* RadioButton.PressedGlyphSize / 2 */
        FluxColor pressed_glyph = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
        flux_fill_ellipse(rc, cx, cy, pressed_r, pressed_r, pressed_glyph);
    }

    /* Label text */
    if (snap->label && snap->label[0] && rc->text) {
        float gap = 8.0f;
        float radio_size = outer_r * 2.0f;
        FluxRect text_rect = {
            bounds->x + radio_size + gap,
            bounds->y,
            flux_maxf(0.0f, bounds->w - radio_size - gap),
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

    /* Focus rect */
    if (state->focused && state->enabled) {
        FluxRect circle_rect = { bounds->x, cy - outer_r, outer_r * 2.0f, outer_r * 2.0f };
        flux_draw_focus_rect(rc, &circle_rect, outer_r);
    }
}