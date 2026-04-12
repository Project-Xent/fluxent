#include "../flux_fluent.h"
#include "../flux_icon.h"
#include <string.h>

/* Shared text-content renderer (clip + text + selection + caret) */
extern void flux_draw_textbox_content(const FluxRenderContext *,
                                      const FluxRenderSnapshot *,
                                      const FluxRect *,
                                      const FluxControlState *);

/* Shared chrome helpers */
extern void textbox_draw_elevation_border(const FluxRenderContext *rc,
                                          const FluxRect *bounds,
                                          float radius, bool focused);
extern void textbox_draw_focus_accent(const FluxRenderContext *rc,
                                      const FluxRect *bounds, float radius);

/* ── Geometry constants ── */

/* WinUI NumberBoxSpinButtonStyle: MinWidth=32 */
#define NB_SPIN_BTN_MIN_W  32.0f

/* Spin button margins (WinUI) */
#define NB_UP_MARGIN       4.0f   /* Margin="4" (all sides) */
#define NB_DN_MARGIN_L     0.0f   /* Margin="0,4,4,4" */
#define NB_DN_MARGIN_T     4.0f
#define NB_DN_MARGIN_R     4.0f
#define NB_DN_MARGIN_B     4.0f

#define NB_COL_UP_W   (NB_SPIN_BTN_MIN_W + NB_UP_MARGIN + NB_UP_MARGIN)     /* 40 */
#define NB_COL_DN_W   (NB_SPIN_BTN_MIN_W + NB_DN_MARGIN_L + NB_DN_MARGIN_R) /* 36 */
#define NB_SPIN_TOTAL (NB_COL_UP_W + NB_COL_DN_W)                            /* 76 */

/* Delete (X) button: WinUI NumberBoxTextBoxStyle DeleteButton MinWidth=40 */
#define NB_DEL_BTN_W  40.0f

/* TextBoxInnerButtonMargin = 0,4,4,4 (applies to both X button and spin buttons' inner grid) */
#define NB_INNER_MARGIN_L  0.0f
#define NB_INNER_MARGIN_T  4.0f
#define NB_INNER_MARGIN_R  4.0f
#define NB_INNER_MARGIN_B  4.0f

/* ── Hover zone IDs ── */
#define ZONE_NONE     (-1)
#define ZONE_TEXT       0
#define ZONE_SPIN_UP    1
#define ZONE_SPIN_DN    2
#define ZONE_SPIN_GAP   3
#define ZONE_DELETE     4

/* Compute which zone hover_local_x falls into.
   right_reserved = total width occupied by X button + spin buttons on the right.
   Layout from left to right:  [text] [X btn?] [Up?] [Down?]  */
static int nb_hover_zone(float lx, float bounds_w,
                         bool show_delete, bool spin_visible) {
    if (lx < 0.0f) return ZONE_NONE;

    /* Compute region boundaries from the right edge */
    float right_edge = bounds_w;

    /* Spin buttons occupy the rightmost NB_SPIN_TOTAL if visible */
    float spin_start = right_edge;
    if (spin_visible) {
        spin_start = right_edge - NB_SPIN_TOTAL;
    }

    /* Delete button sits to the left of spin (or at right edge if no spin) */
    float del_start = spin_start;
    if (show_delete) {
        del_start = spin_start - NB_DEL_BTN_W;
    }

    /* Text area is everything to the left of the buttons */
    if (lx < del_start) return ZONE_TEXT;

    /* Delete button zone */
    if (show_delete && lx < spin_start) {
        /* Check if cursor is inside the inner margin area (the actual button bg) */
        float inner_x0 = del_start + NB_INNER_MARGIN_L;
        float inner_x1 = spin_start - NB_INNER_MARGIN_R;
        if (lx >= inner_x0 && lx < inner_x1)
            return ZONE_DELETE;
        return ZONE_DELETE; /* margin area still counts as delete zone for cursor/click */
    }

    /* Spin button zones */
    if (spin_visible && lx >= spin_start) {
        float col_up_start = spin_start;
        float up_x0 = col_up_start + NB_UP_MARGIN;
        float up_x1 = up_x0 + NB_SPIN_BTN_MIN_W;

        float col_dn_start = spin_start + NB_COL_UP_W;
        float dn_x0 = col_dn_start + NB_DN_MARGIN_L;
        float dn_x1 = dn_x0 + NB_SPIN_BTN_MIN_W;

        if (lx >= up_x0 && lx < up_x1) return ZONE_SPIN_UP;
        if (lx >= dn_x0 && lx < dn_x1) return ZONE_SPIN_DN;
        return ZONE_SPIN_GAP;
    }

    return ZONE_TEXT;
}

/* ── Draw a single inner button (X or spin) with hover background ── */
/* WinUI three-state button fill:
   Normal:      Transparent
   PointerOver: SubtleFillColorSecondary  (ctrl_alt_fill_secondary / 0x06)
   Pressed:     SubtleFillColorTertiary   (ctrl_alt_fill_tertiary  / 0x0F) */
static void nb_draw_inner_button(const FluxRenderContext *rc,
                                 const FluxRect *outer,
                                 float margin_l, float margin_t,
                                 float margin_r, float margin_b,
                                 float radius, bool hovered, bool pressed,
                                 const FluxThemeColors *t) {
    float x = outer->x + margin_l;
    float y = outer->y + margin_t;
    float w = outer->w - margin_l - margin_r;
    float h = outer->h - margin_t - margin_b;
    if (w <= 0.0f || h <= 0.0f) return;

    FluxRect r = { x, y, w, h };
    if (pressed) {
        FluxColor fill = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0F);
        flux_fill_rounded_rect(rc, &r, radius, fill);
    } else if (hovered) {
        FluxColor fill = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
        flux_fill_rounded_rect(rc, &r, radius, fill);
    }
}

/* ── Draw icon centered in the margin-inset area ── */
static void nb_draw_icon(const FluxRenderContext *rc,
                         const FluxRect *outer,
                         float margin_l, float margin_t,
                         float margin_r, float margin_b,
                         const char *icon_utf8,
                         float font_size, FluxColor color) {
    float x = outer->x + margin_l;
    float y = outer->y + margin_t;
    float w = outer->w - margin_l - margin_r;
    float h = outer->h - margin_t - margin_b;
    if (w <= 0.0f || h <= 0.0f) return;

    FluxRect r = { x, y, w, h };
    FluxTextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.font_family = "Segoe Fluent Icons";
    ts.font_size = font_size;
    ts.font_weight = FLUX_FONT_REGULAR;
    ts.text_align = FLUX_TEXT_CENTER;
    ts.vert_align = FLUX_TEXT_VCENTER;
    ts.color = color;
    ts.word_wrap = false;
    flux_text_draw(rc->text, FLUX_RT(rc), icon_utf8, &r, &ts);
}

/* ════════════════════════════════════════════════════════════════════
   Main NumberBox renderer
   ════════════════════════════════════════════════════════════════════ */

void flux_draw_number_box(const FluxRenderContext *rc,
                          const FluxRenderSnapshot *snap,
                          const FluxRect *bounds,
                          const FluxControlState *state) {
    const FluxThemeColors *t = rc->theme;
    float radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
    bool has_text = snap->text_content && snap->text_content[0];
    bool spin_visible = (snap->nb_spin_placement == 2);

    /* Delete (X) button: visible when has_text && focused && !readonly && enabled */
    bool show_delete = has_text && state->focused && !snap->readonly && state->enabled;

    /* ── Compute right-side reserved width ── */
    float spin_w   = spin_visible ? NB_SPIN_TOTAL : 0.0f;
    float delete_w = show_delete  ? NB_DEL_BTN_W  : 0.0f;
    float reserved = delete_w + spin_w;
    float text_col_w = bounds->w - reserved;
    if (text_col_w < 0.0f) text_col_w = 0.0f;

    /* ── Hover zone ── */
    int zone = ZONE_NONE;
    if (state->hovered) {
        zone = nb_hover_zone(snap->hover_local_x, bounds->w,
                             show_delete, spin_visible);
    }

    /* ═══════════════════════════════════════════════════════════
       1. Chrome: background fill (full width)
       Only show hover chrome when cursor is in the text area.
       ═══════════════════════════════════════════════════════════ */
    FluxColor fill;
    bool chrome_hovered = (zone == ZONE_TEXT);
    if (!state->enabled) {
        fill = t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4D);
    } else if (state->focused) {
        fill = t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xFF);
    } else if (chrome_hovered) {
        fill = t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
    } else {
        fill = t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xB3);
    }
    flux_fill_rounded_rect(rc, bounds, radius, fill);

    /* ═══════════════════════════════════════════════════════════
       2. Chrome: elevation border + focus accent (full width)
       ═══════════════════════════════════════════════════════════ */
    if (!state->enabled) {
        FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0F);
        flux_draw_rounded_rect(rc, bounds, radius, dis_stroke, 1.0f);
    } else {
        textbox_draw_elevation_border(rc, bounds, radius, state->focused);
        if (state->focused)
            textbox_draw_focus_accent(rc, bounds, radius);
    }

    /* ═══════════════════════════════════════════════════════════
       3. Text content: clipped to the text column
       ═══════════════════════════════════════════════════════════ */
    {
        FluxRect text_area = {
            bounds->x + FLUX_TEXTBOX_PAD_L,
            bounds->y + FLUX_TEXTBOX_PAD_T,
            flux_maxf(0.0f, text_col_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
            flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)
        };

        /* Build a modified state: only pass focused to text renderer when
           the TextBox is actually in edit mode (not in stepping mode).
           The readonly flag from the snapshot already suppresses caret/selection. */
        FluxControlState text_state = *state;
        if (zone != ZONE_TEXT && !state->focused) {
            text_state.hovered = 0;
        }
        flux_draw_textbox_content(rc, snap, &text_area, &text_state);
    }

    if (!state->enabled || !rc->text) return;

    /* Pressed detection: node is pressed AND cursor is still over a button zone */
    bool node_pressed = state->pressed;

    /* ═══════════════════════════════════════════════════════════
       4. Delete (X) button — left of spin buttons (or at right edge)
       ═══════════════════════════════════════════════════════════ */
    if (show_delete) {
        float del_x = bounds->x + text_col_w;
        FluxRect del_outer = { del_x, bounds->y, NB_DEL_BTN_W, bounds->h };

        bool del_hovered = (zone == ZONE_DELETE);
        bool del_pressed = (del_hovered && node_pressed);
        nb_draw_inner_button(rc, &del_outer,
                             NB_INNER_MARGIN_L, NB_INNER_MARGIN_T,
                             NB_INNER_MARGIN_R, NB_INNER_MARGIN_B,
                             radius, del_hovered, del_pressed, t);

        /* Cancel icon: U+E894, FontSize=12
           WinUI: TextFillColorTertiary on pressed, TextFillColorSecondary otherwise */
        FluxColor del_icon_color;
        if (del_pressed) {
            del_icon_color = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72);
        } else {
            del_icon_color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9E);
        }

        char x_utf8[4] = { (char)0xEE, (char)0xA2, (char)0x94, '\0' };
        nb_draw_icon(rc, &del_outer,
                     NB_INNER_MARGIN_L, NB_INNER_MARGIN_T,
                     NB_INNER_MARGIN_R, NB_INNER_MARGIN_B,
                     x_utf8, 12.0f, del_icon_color);
    }

    /* ═══════════════════════════════════════════════════════════
       5. Spin buttons — rightmost columns
       ═══════════════════════════════════════════════════════════ */
    if (!spin_visible) return;

    float spin_base_x = bounds->x + bounds->w - NB_SPIN_TOTAL;

    /* Up button outer rect (Col 1, 40px) */
    FluxRect up_outer = { spin_base_x, bounds->y, NB_COL_UP_W, bounds->h };
    /* Down button outer rect (Col 2, 36px) */
    FluxRect dn_outer = { spin_base_x + NB_COL_UP_W, bounds->y, NB_COL_DN_W, bounds->h };

    /* Per-button hover + pressed */
    bool up_hovered = (zone == ZONE_SPIN_UP && snap->nb_up_enabled);
    bool dn_hovered = (zone == ZONE_SPIN_DN && snap->nb_down_enabled);
    bool up_pressed = (up_hovered && node_pressed);
    bool dn_pressed = (dn_hovered && node_pressed);

    nb_draw_inner_button(rc, &up_outer,
                         NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN,
                         radius, up_hovered, up_pressed, t);
    nb_draw_inner_button(rc, &dn_outer,
                         NB_DN_MARGIN_L, NB_DN_MARGIN_T, NB_DN_MARGIN_R, NB_DN_MARGIN_B,
                         radius, dn_hovered, dn_pressed, t);

    /* Icons */
    /* WinUI icon colors: Disabled → TextFillColorDisabled,
       Pressed → TextFillColorTertiary, Normal/Hover → TextFillColorSecondary */
    FluxColor up_icon_color;
    if (!snap->nb_up_enabled)
        up_icon_color = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C);
    else if (up_pressed)
        up_icon_color = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72);
    else
        up_icon_color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9E);

    FluxColor dn_icon_color;
    if (!snap->nb_down_enabled)
        dn_icon_color = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5C);
    else if (dn_pressed)
        dn_icon_color = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72);
    else
        dn_icon_color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9E);

    /* FontSize from NumberBox's own FontSize (TemplateBinding overrides style's 12px) */
    float icon_fs = snap->font_size > 0.0f ? snap->font_size : 14.0f;

    /* ChevronUp U+E70E / ChevronDown U+E70D */
    char up_utf8[4] = { (char)0xEE, (char)0x9C, (char)0x8E, '\0' };
    char dn_utf8[4] = { (char)0xEE, (char)0x9C, (char)0x8D, '\0' };

    nb_draw_icon(rc, &up_outer,
                 NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN,
                 up_utf8, icon_fs, up_icon_color);
    nb_draw_icon(rc, &dn_outer,
                 NB_DN_MARGIN_L, NB_DN_MARGIN_T, NB_DN_MARGIN_R, NB_DN_MARGIN_B,
                 dn_utf8, icon_fs, dn_icon_color);
}