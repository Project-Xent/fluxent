#include "../flux_fluent.h"
#include <string.h>

void flux_draw_textbox(const FluxRenderContext *rc,
                       const FluxRenderSnapshot *snap,
                       const FluxRect *bounds,
                       const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;
    float radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;

    /* ---- 1. Chrome: fill ---- */
    FluxColor fill;
    if (!state->enabled) {
        fill = t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4D);
    } else if (state->focused) {
        fill = t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xFF);
    } else if (state->hovered) {
        fill = t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
    } else {
        fill = t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xB3);
    }
    flux_fill_rounded_rect(rc, bounds, radius, fill);

    /* ---- 2. Chrome: elevation border ---- */
    if (!state->enabled) {
        FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
        flux_draw_rounded_rect(rc, bounds, radius, dis_stroke, 1.0f);
    } else {
        /* Top part: subtle stroke */
        FluxColor top_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
        flux_draw_rounded_rect(rc, bounds, radius, top_stroke, 1.0f);

        if (!state->focused) {
            /* Bottom strong stroke when not focused */
            FluxColor bottom_stroke = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9C);
            float bottom_y = bounds->y + bounds->h - 0.5f;
            flux_draw_line(rc, bounds->x + 2.0f, bottom_y,
                           bounds->x + bounds->w - 2.0f, bottom_y, bottom_stroke, 1.0f);
        }
    }

    /* ---- 3. Text bounds (padded) ---- */
    FluxRect tb = {
        bounds->x + FLUX_TEXTBOX_PAD_L,
        bounds->y + FLUX_TEXTBOX_PAD_T,
        flux_maxf(0.0f, bounds->w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
        flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)
    };

    /* Push clip */
    D2D1_RECT_F clip = flux_d2d_rect(&tb);
    ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (!rc->text) {
        ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
        goto draw_accent;
    }

    float font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
    FluxColor text_color = (flux_color_af(snap->text_color) > 0.0f)
        ? snap->text_color
        : (state->enabled
            ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4))
            : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C)));

    FluxTextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.font_family = snap->font_family;
    ts.font_size = font_size;
    ts.font_weight = FLUX_FONT_REGULAR;
    ts.text_align = FLUX_TEXT_LEFT;
    ts.vert_align = FLUX_TEXT_VCENTER;
    ts.color = text_color;
    ts.word_wrap = false;

    const char *content = snap->text_content;
    bool has_text = (content && content[0]);

    /* ---- 4. Placeholder ---- */
    if (!has_text && snap->placeholder && snap->placeholder[0]) {
        FluxTextStyle ph = ts;
        ph.color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9E);
        flux_text_draw(rc->text, FLUX_RT(rc), snap->placeholder, &tb, &ph);
    }

    /* ---- 5. Selection highlight ---- */
    if (has_text && state->focused &&
        snap->selection_start != snap->selection_end) {
        uint32_t sel_s = snap->selection_start < snap->selection_end
                         ? snap->selection_start : snap->selection_end;
        uint32_t sel_e = snap->selection_start > snap->selection_end
                         ? snap->selection_start : snap->selection_end;

        FluxRect sel_rects[16];
        uint32_t n = flux_text_selection_rects(rc->text, content, &ts,
                                               tb.w, (int)sel_s, (int)sel_e,
                                               sel_rects, 16);
        FluxColor sel_color = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        /* Make selection translucent */
        sel_color.rgba = (sel_color.rgba & 0xFFFFFF00) | 0x66;

        for (uint32_t i = 0; i < n; i++) {
            FluxRect sr = {
                tb.x + sel_rects[i].x - snap->scroll_offset_x,
                tb.y + sel_rects[i].y,
                sel_rects[i].w,
                sel_rects[i].h
            };
            flux_fill_rect(rc, &sr, sel_color);
        }
    }

    /* ---- 6. Text content ---- */
    if (has_text) {
        FluxRect draw_bounds = {
            tb.x - snap->scroll_offset_x,
            tb.y,
            tb.w + snap->scroll_offset_x, /* wider so clipped text is still laid out */
            tb.h
        };
        flux_text_draw(rc->text, FLUX_RT(rc), content, &draw_bounds, &ts);
    }

    /* ---- 7. Caret ---- */
    if (state->focused && has_text &&
        snap->selection_start == snap->selection_end) {
        /* Use cache entry for blink state (reserved for future timer-based blinking) */
        FluxCacheEntry *ce = rc->cache
            ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;

        /* Always show the caret for now; true blinking needs timer infrastructure */
        bool show_caret = true;

        if (show_caret) {
            FluxRect caret = flux_text_caret_rect(rc->text, content, &ts,
                                                   tb.w, (int)snap->cursor_position);
            float caret_h = caret.h > 0.0f ? caret.h : font_size * 1.2f;
            FluxRect caret_abs = {
                tb.x + caret.x - snap->scroll_offset_x,
                tb.y + caret.y,
                1.0f,
                caret_h
            };
            FluxColor caret_color = t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4);
            flux_fill_rect(rc, &caret_abs, caret_color);
        }

        (void)ce; /* will be used for blink timing later */
    }

    /* Also show caret when text is empty and focused */
    if (state->focused && !has_text) {
        float caret_h = font_size * 1.2f;
        FluxRect caret_abs = { tb.x, tb.y + (tb.h - caret_h) * 0.5f, 1.0f, caret_h };
        FluxColor caret_color = t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4);
        flux_fill_rect(rc, &caret_abs, caret_color);
    }

    /* Pop clip */
    ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));

    /* ---- 8. Bottom accent line when focused ---- */
draw_accent:
    if (state->focused && state->enabled) {
        FluxColor accent = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        float bottom_y = bounds->y + bounds->h;
        flux_draw_line(rc, bounds->x + 2.0f, bottom_y,
                       bounds->x + bounds->w - 2.0f, bottom_y, accent, 2.0f);
    }
}