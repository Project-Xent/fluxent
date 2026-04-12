#include "../flux_fluent.h"
#include "../flux_icon.h"
#include <stdio.h>

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

void flux_draw_info_badge(const FluxRenderContext *rc,
                          const FluxRenderSnapshot *snap,
                          const FluxRect *bounds,
                          const FluxControlState *state) {
    (void)state;
    const FluxThemeColors *t = rc->theme;

    FluxColor accent = t ? t->accent_default : ft_accent_default();
    FluxColor text_color = t ? t->text_on_accent_primary : ft_text_on_accent_primary();

    /* ---- Dot mode ---- */
    if (snap->is_checked) {
        float dot_size = 8.0f;
        float dot_r = dot_size * 0.5f;
        float cx = bounds->x + bounds->w * 0.5f;
        float cy = bounds->y + bounds->h * 0.5f;
        flux_fill_ellipse(rc, cx, cy, dot_r, dot_r, accent);
        return;
    }

    /* ---- Icon mode ---- */
    if (snap->indeterminate) {
        float badge_size = 16.0f;
        float badge_r = badge_size * 0.5f;

        /* Center the badge circle in bounds */
        float cx = bounds->x + bounds->w * 0.5f;
        float cy = bounds->y + bounds->h * 0.5f;
        flux_fill_ellipse(rc, cx, cy, badge_r, badge_r, accent);

        /* Draw icon glyph if available */
        if (snap->icon_name && snap->icon_name[0] && rc->text) {
            const wchar_t *icon_wc = flux_icon_lookup(snap->icon_name);
            char icon_utf8[8] = {0};
            if (icon_wc)
                icon_wchar_to_utf8(icon_wc, icon_utf8, sizeof(icon_utf8));

            if (icon_utf8[0]) {
                FluxTextStyle is;
                memset(&is, 0, sizeof(is));
                is.font_family = "Segoe Fluent Icons";
                is.font_size   = 10.0f;
                is.font_weight = FLUX_FONT_REGULAR;
                is.text_align  = FLUX_TEXT_CENTER;
                is.vert_align  = FLUX_TEXT_VCENTER;
                is.color       = text_color;
                is.word_wrap   = false;

                FluxRect icon_rect = {
                    cx - badge_r, cy - badge_r,
                    badge_size, badge_size
                };
                flux_text_draw(rc->text, FLUX_RT(rc), icon_utf8, &icon_rect, &is);
            }
        }
        return;
    }

    /* ---- Number mode ---- */
    if (snap->current_value >= 1.0f) {
        int value = (int)(snap->current_value + 0.5f);
        char num_buf[16];
        snprintf(num_buf, sizeof(num_buf), "%d", value);

        float badge_h = 16.0f;
        float badge_min_w = 16.0f;
        float font_size = 10.0f;

        FluxTextStyle ts;
        memset(&ts, 0, sizeof(ts));
        ts.font_size   = font_size;
        ts.font_weight = FLUX_FONT_SEMI_BOLD;
        ts.text_align  = FLUX_TEXT_CENTER;
        ts.vert_align  = FLUX_TEXT_VCENTER;
        ts.color       = text_color;
        ts.word_wrap   = false;

        /* Measure text to determine badge width */
        float badge_w = badge_min_w;
        if (rc->text) {
            FluxSize measured = flux_text_measure(rc->text, num_buf, &ts, 0);
            float pad_h = 6.0f; /* horizontal padding on each side */
            float needed_w = measured.w + pad_h * 2.0f;
            if (needed_w > badge_w)
                badge_w = needed_w;
        }

        float badge_r = badge_h * 0.5f;

        /* Center the badge in bounds */
        float bx = bounds->x + (bounds->w - badge_w) * 0.5f;
        float by = bounds->y + (bounds->h - badge_h) * 0.5f;

        FluxRect badge_rect = { bx, by, badge_w, badge_h };
        flux_fill_rounded_rect(rc, &badge_rect, badge_r, accent);

        /* Draw the number text */
        if (rc->text) {
            flux_text_draw(rc->text, FLUX_RT(rc), num_buf, &badge_rect, &ts);
        }
        return;
    }

    /* Value < 1 and not dot/icon mode: draw nothing */
}