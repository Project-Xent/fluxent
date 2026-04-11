#include "../flux_fluent.h"
#include <string.h>
#include <malloc.h>


/* ---- UTF-8 / UTF-16 conversion helpers for IME composition ---- */
static int __attribute__((unused)) utf8_to_wide(const char *src, wchar_t *dst, int dst_cap) {
    if (!src) { if (dst && dst_cap > 0) dst[0] = 0; return 0; }
    int len = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_cap);
    return len > 0 ? len - 1 : 0; /* return char count without null */
}

static int __attribute__((unused)) wide_to_utf8(const wchar_t *src, int src_len, char *dst, int dst_cap) {
    if (!src || src_len == 0) { if (dst && dst_cap > 0) dst[0] = 0; return 0; }
    int len = WideCharToMultiByte(CP_UTF8, 0, src, src_len, dst, dst_cap, NULL, NULL);
    if (dst && len < dst_cap) dst[len] = 0;
    return len;
}

/* Count UTF-16 code units for a given number of UTF-8 bytes */
static uint32_t utf8_bytes_to_utf16_count(const char *s, uint32_t byte_count) {
    if (!s || byte_count == 0) return 0;
    return (uint32_t)MultiByteToWideChar(CP_UTF8, 0, s, (int)byte_count, NULL, 0);
}

/* ---- TextBox-specific elevation border (gradient brush, bottom→top) ---- */
static void textbox_draw_elevation_border(const FluxRenderContext *rc,
                                          const FluxRect *bounds,
                                          float radius, bool is_focused) {
    const FluxThemeColors *t = rc->theme;

    FluxColor bottom_color, top_color;
    if (is_focused) {
        bottom_color = t ? t->accent_default : flux_color_rgb(0, 120, 212);
        top_color    = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
    } else {
        bottom_color = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9C);
        top_color    = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
    }

    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.0f;
    stops[0].color    = flux_d2d_color(bottom_color);
    stops[1].position = 1.0f;
    stops[1].color    = flux_d2d_color(top_color);

    ID2D1GradientStopCollection *collection = NULL;
    FLUX_RT(rc)->lpVtbl->CreateGradientStopCollection(
        FLUX_RT(rc), stops, 2,
        D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &collection);
    if (!collection) return;

    /* Gradient runs from bottom edge upward by 2 DIPs */
    D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gp;
    gp.startPoint = flux_point(0.0f, bounds->y + bounds->h);
    gp.endPoint   = flux_point(0.0f, bounds->y + bounds->h - 2.0f);

    D2D1_BRUSH_PROPERTIES bp;
    bp.opacity = 1.0f;
    bp.transform._11 = 1; bp.transform._12 = 0;
    bp.transform._21 = 0; bp.transform._22 = 1;
    bp.transform._31 = 0; bp.transform._32 = 0;

    ID2D1LinearGradientBrush *grad = NULL;
    FLUX_RT(rc)->lpVtbl->CreateLinearGradientBrush(
        FLUX_RT(rc), &gp, &bp, collection, &grad);
    ID2D1GradientStopCollection_Release(collection);
    if (!grad) return;

    D2D1_ROUNDED_RECT rr = flux_rounded_rect(bounds, radius);
    ID2D1RenderTarget_DrawRoundedRectangle(FLUX_RT(rc), &rr, (ID2D1Brush *)grad, 1.0f, NULL);

    ID2D1LinearGradientBrush_Release(grad);
}

/* ---- Focused accent line clipped to the rounded-rect border ---- */
static void textbox_draw_focus_accent(const FluxRenderContext *rc,
                                      const FluxRect *bounds,
                                      float radius) {
    const FluxThemeColors *t = rc->theme;

    /* Create rounded-rect geometry for clipping */
    ID2D1Factory *factory = NULL;
    ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
    if (!factory) return;

    D2D1_ROUNDED_RECT rr = flux_rounded_rect(bounds, radius);
    ID2D1RoundedRectangleGeometry *clip_geom = NULL;
    ID2D1Factory_CreateRoundedRectangleGeometry(factory, &rr, &clip_geom);
    ID2D1Factory_Release(factory);
    if (!clip_geom) return;

    /* Create accent brush */
    FluxColor accent = t ? t->accent_default : flux_color_rgb(0, 120, 212);
    flux_set_brush(rc, accent);

    /* Push layer with geometry clip */
    D2D1_LAYER_PARAMETERS lp;
    lp.contentBounds.left   = -1e6f;
    lp.contentBounds.top    = -1e6f;
    lp.contentBounds.right  =  1e6f;
    lp.contentBounds.bottom =  1e6f;
    lp.geometricMask = (ID2D1Geometry *)clip_geom;
    lp.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
    lp.maskTransform._11 = 1; lp.maskTransform._12 = 0;
    lp.maskTransform._21 = 0; lp.maskTransform._22 = 1;
    lp.maskTransform._31 = 0; lp.maskTransform._32 = 0;
    lp.opacity = 1.0f;
    lp.opacityBrush = NULL;
    lp.layerOptions = D2D1_LAYER_OPTIONS_NONE;

    ID2D1RenderTarget_PushLayer(FLUX_RT(rc), &lp, NULL);

    /* Draw 2px accent line at the bottom inside the clip */
    float line_y = bounds->y + bounds->h - 1.0f;
    ID2D1RenderTarget_DrawLine(FLUX_RT(rc),
                               flux_point(bounds->x, line_y),
                               flux_point(bounds->x + bounds->w, line_y),
                               (ID2D1Brush *)rc->brush, 2.0f, NULL);

    ID2D1RenderTarget_PopLayer(FLUX_RT(rc));
    ID2D1RoundedRectangleGeometry_Release(clip_geom);
}

/* ---- Main render entry ---- */
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
        textbox_draw_elevation_border(rc, bounds, radius, state->focused);
        if (state->focused)
            textbox_draw_focus_accent(rc, bounds, radius);
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
        return;
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

    /* Hit-test style: same as ts but with TOP alignment.
       flux_text_selection_rects / flux_text_caret_rect use UNBOUNDED height,
       so VCENTER would offset y by ~50000px.  Use TOP for correct coords. */
    FluxTextStyle ts_hit = ts;
    ts_hit.vert_align = FLUX_TEXT_TOP;

    /* ---- Build display text (with IME composition inserted) ---- */
    const char *content = snap->text_content;
    bool has_composition = (snap->composition_text && snap->composition_length > 0);

    /* Stack buffer for composed text; heap-allocate if needed */
    char  composed_stack[512];
    char *composed_buf = NULL;
    int   composition_start_u16 = 0; /* UTF-16 offset where composition begins */

    if (has_composition && content && content[0]) {
        /* Convert original text to wide */
        int wlen = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
        int total_w = wlen + (int)snap->composition_length + 1;
        wchar_t *wtmp = (wchar_t *)_alloca(total_w * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, content, -1, wtmp, wlen);
        int orig_wlen = wlen - 1; /* exclude null */

        /* Cursor position is in UTF-8 bytes; convert to UTF-16 index */
        composition_start_u16 = (int)utf8_bytes_to_utf16_count(content, snap->cursor_position);
        if (composition_start_u16 > orig_wlen) composition_start_u16 = orig_wlen;

        /* Build wide string: [0..cursor] + composition + [cursor..end] */
        int result_wlen = orig_wlen + (int)snap->composition_length;
        wchar_t *wresult = (wchar_t *)_alloca((result_wlen + 1) * sizeof(wchar_t));
        memcpy(wresult, wtmp, composition_start_u16 * sizeof(wchar_t));
        memcpy(wresult + composition_start_u16, snap->composition_text, snap->composition_length * sizeof(wchar_t));
        memcpy(wresult + composition_start_u16 + snap->composition_length,
               wtmp + composition_start_u16, (orig_wlen - composition_start_u16) * sizeof(wchar_t));
        wresult[result_wlen] = 0;

        /* Convert back to UTF-8 */
        int u8len = WideCharToMultiByte(CP_UTF8, 0, wresult, result_wlen, NULL, 0, NULL, NULL);
        if (u8len + 1 <= (int)sizeof(composed_stack)) {
            composed_buf = composed_stack;
        } else {
            composed_buf = (char *)_alloca(u8len + 1);
        }
        WideCharToMultiByte(CP_UTF8, 0, wresult, result_wlen, composed_buf, u8len, NULL, NULL);
        composed_buf[u8len] = 0;
        content = composed_buf;
    } else if (has_composition && (!content || !content[0])) {
        /* No existing text, just composition */
        composition_start_u16 = 0;
        int u8len = WideCharToMultiByte(CP_UTF8, 0, snap->composition_text, (int)snap->composition_length, NULL, 0, NULL, NULL);
        if (u8len + 1 <= (int)sizeof(composed_stack)) {
            composed_buf = composed_stack;
        } else {
            composed_buf = (char *)_alloca(u8len + 1);
        }
        WideCharToMultiByte(CP_UTF8, 0, snap->composition_text, (int)snap->composition_length, composed_buf, u8len, NULL, NULL);
        composed_buf[u8len] = 0;
        content = composed_buf;
    }

    bool has_text = (content && content[0]);

    /* ---- 4. Placeholder (always TextSecondary, matching legacy) ---- */
    if (!has_text && snap->placeholder && snap->placeholder[0]) {
        FluxTextStyle ph = ts;
        ph.color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9E);
        flux_text_draw(rc->text, FLUX_RT(rc), snap->placeholder, &tb, &ph);
    }

    /* ---- 5. Selection highlight ---- */
    if (has_text && state->focused &&
        snap->selection_start != snap->selection_end) {
        /* Convert byte offsets → UTF-16 indices for DirectWrite */
        uint32_t sel_s_byte = snap->selection_start < snap->selection_end
                              ? snap->selection_start : snap->selection_end;
        uint32_t sel_e_byte = snap->selection_start > snap->selection_end
                              ? snap->selection_start : snap->selection_end;
        int sel_s = (int)utf8_bytes_to_utf16_count(snap->text_content, sel_s_byte);
        int sel_e = (int)utf8_bytes_to_utf16_count(snap->text_content, sel_e_byte);

        FluxRect sel_rects[16];
        uint32_t n = flux_text_selection_rects(rc->text, content, &ts_hit,
                                               tb.w, sel_s, sel_e,
                                               sel_rects, 16);

        /* Selection brush: use custom selection_color if set, otherwise accent at 40% opacity */
        FluxColor sel_base = (flux_color_af(snap->selection_color) > 0.0f)
            ? snap->selection_color
            : (t ? t->accent_default : flux_color_rgb(0, 120, 212));
        D2D1_COLOR_F sel_d2d = flux_d2d_color(sel_base);
        ID2D1SolidColorBrush *sel_brush = NULL;
        D2D1_BRUSH_PROPERTIES sbp;
        sbp.opacity = 0.4f;
        sbp.transform._11 = 1; sbp.transform._12 = 0;
        sbp.transform._21 = 0; sbp.transform._22 = 1;
        sbp.transform._31 = 0; sbp.transform._32 = 0;
        ID2D1RenderTarget_CreateSolidColorBrush(FLUX_RT(rc), &sel_d2d, &sbp, &sel_brush);

        if (sel_brush) {
            for (uint32_t i = 0; i < n; i++) {
                FluxRect sr = {
                    tb.x + sel_rects[i].x - snap->scroll_offset_x,
                    tb.y + sel_rects[i].y,
                    sel_rects[i].w,
                    sel_rects[i].h
                };
                D2D1_RECT_F srd = flux_d2d_rect(&sr);
                ID2D1RenderTarget_FillRectangle(FLUX_RT(rc), &srd, (ID2D1Brush *)sel_brush);
            }
            ID2D1SolidColorBrush_Release(sel_brush);
        }
    }

    /* ---- 5.5 IME composition underline ---- */
    if (has_composition && has_text) {
        int comp_end_u16 = composition_start_u16 + (int)snap->composition_length;
        FluxRect comp_rects[16];
        uint32_t cn = flux_text_selection_rects(rc->text, content, &ts_hit,
                                                tb.w, composition_start_u16, comp_end_u16,
                                                comp_rects, 16);
        FluxColor ul_color = state->enabled
            ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4))
            : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C));
        for (uint32_t i = 0; i < cn; i++) {
            float lx0 = tb.x + comp_rects[i].x - snap->scroll_offset_x;
            float ly  = tb.y + comp_rects[i].y + comp_rects[i].h;
            float lx1 = lx0 + comp_rects[i].w;
            flux_draw_line(rc, lx0, ly, lx1, ly, ul_color, 1.0f);
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

    /* ---- 7. Caret (with blinking, matching legacy 500ms toggle) ---- */
    /* Also drive animation when selection is active so re-render keeps happening */
    if (state->focused) {
        if (rc->animations_active)
            *rc->animations_active = true;
    }
    if (state->focused && (has_composition ? true : (snap->selection_start == snap->selection_end))) {
        FluxCacheEntry *ce = rc->cache
            ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;

        /* Convert byte offset to UTF-16 index for DirectWrite */
        int caret_u16 = has_composition
            ? (composition_start_u16 + (int)snap->composition_cursor)
            : (int)utf8_bytes_to_utf16_count(snap->text_content, snap->cursor_position);

        /* Blink: 500ms on, 500ms off.  Reset phase on cursor move. */
        bool show_caret = true;
        if (ce) {
            float cur_pos_f = (float)caret_u16;
            if (!ce->focus_anim.initialized || ce->focus_anim.start_val != cur_pos_f) {
                ce->focus_anim.start_time = rc->now;
                ce->focus_anim.start_val  = cur_pos_f;
                ce->focus_anim.initialized = true;
            }
            double elapsed = rc->now - ce->focus_anim.start_time;
            int phase = (int)(elapsed / 0.5);
            show_caret = (phase % 2) == 0;
            if (rc->animations_active)
                *rc->animations_active = true;
        }

        if (show_caret) {
            FluxRect caret;
            float caret_h;

            if (has_text) {
                caret = flux_text_caret_rect(rc->text, content, &ts_hit,
                                             tb.w, caret_u16);
                caret_h = caret.h > 0.0f ? caret.h : font_size * 1.2f;
            } else {
                caret.x = 0.0f;
                caret.y = 0.0f;
                caret_h = font_size * 1.2f;
            }

            FluxRect caret_abs = {
                tb.x + caret.x - snap->scroll_offset_x,
                has_text ? (tb.y + caret.y) : (tb.y + (tb.h - caret_h) * 0.5f),
                1.0f,
                caret_h
            };

            FluxColor caret_color = t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xE4);
            flux_fill_rect(rc, &caret_abs, caret_color);
        }
    } else if (!state->focused) {
        /* Clear blink state when not focused (matching legacy caret_states_.erase) */
        if (rc->cache) {
            FluxCacheEntry *ce = flux_render_cache_get(rc->cache, snap->id);
            if (ce)
                ce->focus_anim.initialized = false;
        }
    }

    /* Pop clip */
    ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}