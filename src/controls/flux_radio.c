#include "../flux_fluent.h"
#include <string.h>

/* WinUI 3 RadioButton metrics */
#define RADIO_OUTER_R       10.0f
#define RADIO_GLYPH_NORMAL   6.0f   /* inner dot radius at rest (GlyphSize 12 / 2) */
#define RADIO_GLYPH_HOVER    7.0f   /* grows slightly on hover (GlyphSize 14 / 2)  */
#define RADIO_GLYPH_PRESS    5.0f   /* shrinks on press  (GlyphSize 10 / 2)        */
#define RADIO_PRESS_GLYPH_R  5.0f   /* unchecked + pressed indicator radius        */

void flux_draw_radio(const FluxRenderContext *rc,
                     const FluxRenderSnapshot *snap,
                     const FluxRect *bounds,
                     const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;

    FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;

    const float outer_r = RADIO_OUTER_R;
    const float cy = bounds->y + bounds->h * 0.5f;
    const float cx = bounds->x + outer_r;

    bool checked = (snap->check_state == FLUX_CHECK_CHECKED);

    if (ce) {
        bool animating = flux_tween_update_states(
            &ce->hover_anim, &ce->press_anim, &ce->focus_anim, &ce->check_anim,
            state->hovered, state->pressed, state->focused, checked, rc->now);
        if (animating && rc->animations_active)
            *rc->animations_active = true;
    }

    float check_progress = ce ? ce->check_anim.current : (checked ? 1.0f : 0.0f);
    float hover_t        = ce ? ce->hover_anim.current : (state->hovered ? 1.0f : 0.0f);
    float press_t        = ce ? ce->press_anim.current : (state->pressed ? 1.0f : 0.0f);

    /* ── State colours ──────────────────────────────────────────────── */
    FluxColor outer_fill, outer_stroke, glyph_fill;

    if (!state->enabled) {
        outer_stroke = t ? t->ctrl_strong_stroke_disabled : flux_color_rgba(0, 0, 0, 0x37);
        outer_fill   = t ? t->ctrl_alt_fill_disabled      : flux_color_rgba(0, 0, 0, 0);
        glyph_fill   = checked ? (t ? t->text_on_accent_disabled : flux_color_rgb(255, 255, 255))
                                : flux_color_rgba(0, 0, 0, 0);
    } else if (checked) {
        /*
         * Checked: circle fill = stroke = accent color.
         * Lerp:  accent_default → accent_secondary (hover)
         *        → accent_tertiary (press)
         */
        FluxColor acc_normal = t ? t->accent_default   : flux_color_rgb(0, 120, 212);
        FluxColor acc_hover  = t ? t->accent_secondary : flux_color_rgba(0, 120, 212, 0xE6);
        FluxColor acc_press  = t ? t->accent_tertiary  : flux_color_rgba(0, 120, 212, 0xCC);

        outer_stroke = flux_anim_lerp_color(
            flux_anim_lerp_color(acc_normal, acc_hover, hover_t),
            acc_press, press_t);
        outer_fill = outer_stroke;
        glyph_fill = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
    } else {
        /*
         * Unchecked: stroke only darkens on press (no change on hover).
         * Fill lerps:  secondary → tertiary (hover) → quarternary (press).
         */
        FluxColor stroke_normal = t ? t->ctrl_strong_stroke_default   : flux_color_rgba(0, 0, 0, 0x9C);
        FluxColor stroke_press  = t ? t->ctrl_strong_stroke_disabled  : flux_color_rgba(0, 0, 0, 0x37);
        outer_stroke = flux_anim_lerp_color(stroke_normal, stroke_press, press_t);

        FluxColor fill_normal = t ? t->ctrl_alt_fill_secondary   : flux_color_rgba(0, 0, 0, 0x06);
        FluxColor fill_hover  = t ? t->ctrl_alt_fill_tertiary    : flux_color_rgba(0, 0, 0, 0x0F);
        FluxColor fill_press  = t ? t->ctrl_alt_fill_quarternary : flux_color_rgba(0, 0, 0, 0x12);
        outer_fill = flux_anim_lerp_color(
            flux_anim_lerp_color(fill_normal, fill_hover, hover_t),
            fill_press, press_t);

        glyph_fill = flux_color_rgba(0, 0, 0, 0);
    }

    /* ── Outer circle ───────────────────────────────────────────────── */
    flux_fill_ellipse(rc, cx, cy, outer_r, outer_r, outer_fill);
    flux_stroke_ellipse(rc, cx, cy, outer_r, outer_r, outer_stroke, 1.0f);

    /* ── Inner dot (checked) ────────────────────────────────────────── */
    if (check_progress > 0.01f) {
        /*
         * Target glyph radius interpolates between normal/hover/press,
         * then scaled by check_progress so it animates in/out cleanly.
         */
        /* Priority-based: press overrides hover, matching legacy Animator<float> target selection.
         * lerp(lerp(normal, hover, hover_t), press, press_t)
         * → when hover_t=1, press_t=1: lerp(7, 5, 1) = 5  ✓
         * → when hover_t=1, press_t=0: lerp(7, 5, 0) = 7  ✓
         * → when hover_t=0, press_t=1: lerp(6, 5, 1) = 5  ✓
         */
        float target_r = flux_anim_mixf(
            flux_anim_mixf(RADIO_GLYPH_NORMAL, RADIO_GLYPH_HOVER, hover_t),
            RADIO_GLYPH_PRESS, press_t);
        float inner_r = target_r * flux_ease_out_quad(check_progress);
        flux_fill_ellipse(rc, cx, cy, inner_r, inner_r, glyph_fill);
    }

    /* ── Unchecked + pressed indicator dot ─────────────────────────── */
    /*
     * WinUI 3 shows a small white dot when pressing an unchecked radio.
     * Fade it in/out with press_t so it doesn't snap.
     */
    if (press_t > 0.01f && !checked && state->enabled) {
        FluxColor pressed_glyph = t ? t->text_on_accent_primary : flux_color_rgb(255, 255, 255);
        float r = RADIO_PRESS_GLYPH_R * press_t;
        flux_fill_ellipse(rc, cx, cy, r, r, pressed_glyph);
    }

    /* ── Label text ─────────────────────────────────────────────────── */
    if (snap->label && snap->label[0] && rc->text) {
        const float gap        = 8.0f;
        const float radio_size = outer_r * 2.0f;
        FluxRect text_rect = {
            bounds->x + radio_size + gap,
            bounds->y,
            flux_maxf(0.0f, bounds->w - radio_size - gap),
            bounds->h
        };
        FluxColor label_color = state->enabled
            ? (t ? t->text_primary  : flux_color_rgba(0, 0, 0, 0xE4))
            : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C));

        FluxTextStyle ts;
        memset(&ts, 0, sizeof(ts));
        ts.font_size   = 14.0f;
        ts.font_weight = FLUX_FONT_REGULAR;
        ts.text_align  = FLUX_TEXT_LEFT;
        ts.vert_align  = FLUX_TEXT_VCENTER;
        ts.color       = label_color;
        ts.word_wrap   = false;

        flux_text_draw(rc->text, (ID2D1RenderTarget *)rc->d2d,
                       snap->label, &text_rect, &ts);
    }

    /* ── Focus rect ─────────────────────────────────────────────────── */
    if (state->focused && state->enabled) {
        FluxRect circle_rect = { bounds->x, cy - outer_r, outer_r * 2.0f, outer_r * 2.0f };
        flux_draw_focus_rect(rc, &circle_rect, outer_r);
    }
}