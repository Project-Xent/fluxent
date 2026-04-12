#include "../flux_fluent.h"

void flux_draw_hyperlink(const FluxRenderContext *rc,
                         const FluxRenderSnapshot *snap,
                         const FluxRect *bounds,
                         const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;
    FluxRect sb = flux_snap_bounds(bounds, 1.0f, 1.0f);

    /* ── Resolve text color based on state ──
       Legacy uses AccentTextFillColor* tokens (SystemAccentColorDark2/Dark3/Dark1).
       We approximate with accent_* for now; the real fix comes with system accent
       color integration.  Disabled uses TextFillColorDisabled (not accent). */
    FluxColor text_color;
    if (!state->enabled) {
        /* Legacy: AccentTextFillColorDisabledBrush = #5C000000 (Light)
           which is identical to TextFillColorDisabled. */
        text_color = t ? t->text_disabled : ft_text_disabled();
    } else if (state->pressed) {
        text_color = t ? t->accent_tertiary : ft_accent_tertiary();
    } else if (state->hovered) {
        text_color = t ? t->accent_secondary : ft_accent_secondary();
    } else {
        text_color = t ? t->accent_default : ft_accent_default();
    }

    /* Override with user-specified color if set */
    if (flux_color_af(snap->text_color) > 0.0f && state->enabled)
        text_color = snap->text_color;

    const char *label = snap->label ? snap->label : snap->text_content;
    if (!label || !label[0] || !rc->text)
        return;

    float font_size = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
    float radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;

    /* ── Background ──
       Legacy: SubtleFillColor tokens — far more transparent than ControlFillColor.
       Light:  Secondary=#09000000  Tertiary=#06000000
       Dark:   Secondary=#0FFFFFFF  Tertiary=#0AFFFFFF
       Normal & Disabled = Transparent (SubtleFillColorTransparent). */
    if (state->enabled && (state->hovered || state->pressed)) {
        FluxColor hover_bg;
        if (state->pressed) {
            /* SubtleFillColorTertiary */
            hover_bg = t ? t->subtle_fill_tertiary : flux_color_rgba(0, 0, 0, 0x06);
        } else {
            /* SubtleFillColorSecondary */
            hover_bg = t ? t->subtle_fill_secondary : flux_color_rgba(0, 0, 0, 0x09);
        }
        flux_fill_rounded_rect(rc, &sb, radius, hover_bg);
    }

    /* Focus rect */
    if (state->focused && state->enabled)
        flux_draw_focus_rect(rc, &sb, radius);

    /* Text style */
    FluxTextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.font_size = font_size;
    ts.font_weight = FLUX_FONT_REGULAR;
    ts.text_align = FLUX_TEXT_CENTER;
    ts.vert_align = FLUX_TEXT_VCENTER;
    ts.color = text_color;
    ts.word_wrap = false;

    /* ── Padding ──
       Legacy: Padding="{StaticResource ButtonPadding}" = 11,5,11,6  (L,T,R,B).
       Same as the standard Button padding. */
    FluxRect text_bounds = {
        sb.x + FLUX_BTN_PAD_LEFT,
        sb.y + FLUX_BTN_PAD_TOP,
        sb.w - FLUX_BTN_PAD_LEFT - FLUX_BTN_PAD_RIGHT,
        sb.h - FLUX_BTN_PAD_TOP  - FLUX_BTN_PAD_BOTTOM
    };
    if (text_bounds.w < 0.0f) text_bounds.w = 0.0f;
    if (text_bounds.h < 0.0f) text_bounds.h = 0.0f;

    /* Draw text */
    flux_text_draw(rc->text, FLUX_RT(rc), label, &text_bounds, &ts);

    /* ── Underline ──
       WinUI HyperlinkButton adds TextDecorations.Underline in code-behind
       (OnApplyTemplate), not in the XAML template.  We replicate that here
       for all enabled states. */
    if (state->enabled) {
        FluxSize text_size = flux_text_measure(rc->text, label, &ts, 0);
        float text_start_x = text_bounds.x + (text_bounds.w - text_size.w) * 0.5f;
        float underline_y = sb.y + (sb.h + text_size.h) * 0.5f;
        flux_draw_line(rc, text_start_x, underline_y,
                       text_start_x + text_size.w, underline_y,
                       text_color, 1.0f);
    }
}