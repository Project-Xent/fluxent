#include "../flux_fluent.h"
#include "../flux_icon.h"
#include <string.h>

/* Shared text-content renderer (clip + text + selection + caret) */
extern void flux_draw_textbox_content(
  FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *
);

/* Shared chrome helpers */
extern void
textbox_draw_elevation_border(FluxRenderContext const *rc, FluxRect const *bounds, float radius, bool focused);
extern void textbox_draw_focus_accent(FluxRenderContext const *rc, FluxRect const *bounds, float radius);

/* WinUI: RevealButton Width="30" */
#define PB_REVEAL_BTN_W 30.0f

/* Count UTF-8 codepoints in [src, src+byte_count) */
static uint32_t count_codepoints(char const *src, uint32_t byte_count) {
	uint32_t             cp  = 0;
	unsigned char const *p   = ( unsigned char const * ) src;
	unsigned char const *end = p + byte_count;
		while (p < end && *p) {
			if (*p < 0x80) p += 1;
			else if (*p < 0xe0) p += 2;
			else if (*p < 0xf0) p += 3;
			else p += 4;
			cp++;
		}
	return cp;
}

/* Build mask: each codepoint → ● (U+25CF, 3 UTF-8 bytes) */
static int build_mask(char const *src, char *dst, int dst_cap) {
		if (!src || !src [0]) {
			if (dst && dst_cap > 0) dst [0] = '\0';
			return 0;
		}
	int                  out = 0;
	unsigned char const *p   = ( unsigned char const * ) src;
		while (*p) {
			int skip;
			if (*p < 0x80) skip = 1;
			else if (*p < 0xe0) skip = 2;
			else if (*p < 0xf0) skip = 3;
			else skip = 4;
				if (out + 3 < dst_cap) {
					dst [out++] = ( char ) 0xe2;
					dst [out++] = ( char ) 0x97;
					dst [out++] = ( char ) 0x8f;
				}
			for (int i = 0; i < skip && *p; i++) p++;
		}
	if (out < dst_cap) dst [out] = '\0';
	else if (dst_cap > 0) dst [dst_cap - 1] = '\0';
	return out;
}

/* Remap byte offset from original text to mask text space */
static uint32_t remap_offset(char const *original, uint32_t byte_offset) {
	if (!original) return 0;
	uint32_t cp = count_codepoints(original, byte_offset);
	return cp * 3;
}

void flux_draw_password_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t           = rc->theme;
	float                  radius      = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool                   has_text    = snap->text_content && snap->text_content [0];
	/* RevealButton visible only when focused AND has text (not on hover alone).
	   Reveal (show plain text) is press-to-reveal: semantic_checked is set to 1
	   on pointer_down in the button area, cleared to 0 on pointer_up / blur. */
	bool                   show_reveal = state->enabled && has_text && state->focused;
	bool                   revealed    = snap->is_checked;

	/* Column widths:
	   Col 0 = * (Star) = bounds->w - col1_w
	   Col 1 = Auto = RevealButton Width=30 when visible, 0 when collapsed */
	float                  col1_w      = show_reveal ? PB_REVEAL_BTN_W : 0.0f;
	float                  col0_w      = bounds->w - col1_w;
	if (col0_w < 0.0f) col0_w = 0.0f;

	/* ═══════════════════════════════════════════════════════
	   BorderElement: Row=1, ColumnSpan=2 — full width chrome
	   ═══════════════════════════════════════════════════════ */

	/* Background fill */
	FluxColor fill;
	if (!state->enabled) fill = t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4d);
	else if (state->focused) fill = t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xff);
	else if (state->hovered) fill = t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
	else fill = t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xb3);
	/* Smooth color cross-fade between chrome states (WinUI ~83 ms). */
	FluxCacheEntry *ce_fill = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
		if (ce_fill) {
			FluxColor tweened;
			bool a = flux_color_tween_update(&ce_fill->color_anim, fill, FLUX_ANIM_DURATION_FAST, rc->now, &tweened);
			fill   = tweened;
			if (a && rc->animations_active) *rc->animations_active = true;
		}
	flux_fill_rounded_rect(rc, bounds, radius, fill);

		/* Elevation border + focus accent */
		if (!state->enabled) {
			FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
			flux_draw_rounded_rect(rc, bounds, radius, dis_stroke, 1.0f);
		}
		else {
			textbox_draw_elevation_border(rc, bounds, radius, state->focused);
			if (state->focused) textbox_draw_focus_accent(rc, bounds, radius);
		}

	/* ═══════════════════════════════════════════════════════
	   ContentElement: Row=1, Col=0 — text clipped to Column 0
	   ═══════════════════════════════════════════════════════ */

	/* Build masked snapshot (unless revealed) */
	char               mask_buf [2048];
	FluxRenderSnapshot masked_snap = *snap;

		if (has_text && !revealed) {
			build_mask(snap->text_content, mask_buf, sizeof(mask_buf));
			masked_snap.text_content    = mask_buf;
			masked_snap.cursor_position = remap_offset(snap->text_content, snap->cursor_position);
			masked_snap.selection_start = remap_offset(snap->text_content, snap->selection_start);
			masked_snap.selection_end   = remap_offset(snap->text_content, snap->selection_end);
			masked_snap.scroll_offset_x = 0.0f;
		}

	/* Padded text area within Column 0 */
	FluxRect text_area = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, col0_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};

	flux_draw_textbox_content(rc, &masked_snap, &text_area, state);

		/* ═══════════════════════════════════════════════════════
	       RevealButton: Row=1, Col=1, Width=30
	       Collapsed by default; visible when has_text && (focused || hovered)
	       ═══════════════════════════════════════════════════════ */
		if (show_reveal && rc->text) {
			/* WinUI: ButtonLayoutGrid has Margin="{ThemeResource TextBoxInnerButtonMargin}"
			   which is 0,4,4,4 (left=0, top=4, right=4, bottom=4). */
			float btn_margin_l = 0.0f;
			float btn_margin_t = 4.0f;
			float btn_margin_r = 4.0f;
			float btn_margin_b = 4.0f;

			float btn_x        = bounds->x + col0_w + btn_margin_l;
			float btn_y        = bounds->y + btn_margin_t;
			float btn_w        = col1_w - btn_margin_l - btn_margin_r;
			float btn_h        = bounds->h - btn_margin_t - btn_margin_b;
			if (btn_w < 0.0f) btn_w = 0.0f;
			if (btn_h < 0.0f) btn_h = 0.0f;
			FluxRect btn_rect    = {btn_x, btn_y, btn_w, btn_h};

			/* WinUI three-state button:
			   Normal:      Transparent background, TextFillColorSecondary icon
			   PointerOver: SubtleFillColorSecondary bg, TextFillColorSecondary icon
			   Pressed:     SubtleFillColorTertiary bg, TextFillColorTertiary icon */
			bool     btn_hovered = (state->hovered && snap->hover_local_x >= col0_w);
			bool     btn_pressed = (btn_hovered && state->pressed);

				if (btn_pressed) {
					FluxColor pressed_fill = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
					flux_fill_rounded_rect(rc, &btn_rect, radius, pressed_fill);
				}
				else if (btn_hovered) {
					FluxColor hover_fill = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
					flux_fill_rounded_rect(rc, &btn_rect, radius, hover_fill);
				}

			/* Icon color: tertiary when pressed, secondary otherwise (matching legacy —
			   no special color for revealed/checked state) */
			FluxColor icon_color;
			if (btn_pressed) icon_color = t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72);
			else icon_color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e);

			char icon_utf8 [4];
			icon_utf8 [0] = ( char ) 0xef;
			icon_utf8 [1] = ( char ) 0x9e;
			icon_utf8 [2] = ( char ) 0x8d;
			icon_utf8 [3] = '\0';

			FluxTextStyle is;
			memset(&is, 0, sizeof(is));
			is.font_family = "Segoe Fluent Icons";
			is.font_size   = 12.0f; /* WinUI: PasswordBoxIconFontSize = 12 */
			is.font_weight = FLUX_FONT_REGULAR;
			is.text_align  = FLUX_TEXT_CENTER;
			is.vert_align  = FLUX_TEXT_VCENTER;
			is.color       = icon_color;
			is.word_wrap   = false;

			flux_text_draw(rc->text, FLUX_RT(rc), icon_utf8, &btn_rect, &is);
		}
}
