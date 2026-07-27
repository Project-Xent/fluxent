/**
 * @file flux_infobar_record.c
 * @brief 1:1 port of 58331ad flux_info_bar.c into display-list.
 */
#include "controls/draw/flux_control_record_internal.h"
#include "fluxent/flux_text.h"

#include <string.h>

#define IB_PAD_LEFT      16.0f
#define IB_ICON_TOP      16.0f
#define IB_ICON_SIZE     16.0f
#define IB_ICON_GAP      14.0f
#define IB_TEXT_TOP      14.0f
#define IB_MSG_GAP       12.0f
#define IB_TEXT_FONT     14.0f
#define IB_CLOSE_SIZE    38.0f
#define IB_CLOSE_MARGIN  5.0f
#define IB_CLOSE_FONT    16.0f
#define IB_ICON_BG_GLYPH L"\xF136"
#define IB_CLOSE_GLYPH   L"\xE711"

typedef struct {
	FluxColor      bg;
	FluxColor      icon_bg;
	FluxColor      icon_fg;
	wchar_t const *glyph;
} IbSeverity;

static IbSeverity ib_severity(int sev, bool dark, FluxThemeColors const *t) {
	FluxColor bg [2][4] = {
	  {flux_color_rgba(0xf6, 0xf6, 0xf6, 0x80), flux_color_rgba(0xdf, 0xf6, 0xdd, 0xff),
       flux_color_rgba(0xff, 0xf4, 0xce, 0xff), flux_color_rgba(0xfd, 0xe7, 0xe9, 0xff)},
	  {flux_color_rgba(255,  255,  255,  0x08), flux_color_rgba(0x39, 0x3d, 0x1b, 0xff),
       flux_color_rgba(0x43, 0x35, 0x19, 0xff), flux_color_rgba(0x44, 0x27, 0x26, 0xff)},
	};
	FluxColor icon [2][4] = {
	  {{0}, flux_color_rgba(0x0f, 0x7b, 0x0f, 0xff), flux_color_rgba(0x9d, 0x5d, 0x00, 0xff),
       flux_color_rgba(0xc4, 0x2b, 0x1c, 0xff)},
	  {{0}, flux_color_rgba(0x6c, 0xcb, 0x5f, 0xff), flux_color_rgba(0xfc, 0xe1, 0x00, 0xff),
       flux_color_rgba(0xff, 0x99, 0xa4, 0xff)},
	};
	wchar_t const *glyph [4] = {L"\xF13F", L"\xF13E", L"\xF13C", L"\xF13D"};
	int            d         = dark ? 1 : 0;
	int       s   = (sev >= FLUX_INFOBAR_INFORMATIONAL && sev <= FLUX_INFOBAR_ERROR) ? sev : FLUX_INFOBAR_INFORMATIONAL;
	FluxColor inv = dark ? flux_color_rgba(0, 0, 0, 0xe4) : flux_color_rgba(255, 255, 255, 0xff);
	FluxColor icon_bg = (s == FLUX_INFOBAR_INFORMATIONAL) ? t->accent_default : icon [d][s];
	return (IbSeverity) {bg [d][s], icon_bg, inv, glyph [s]};
}

static float ib_draw_run(
  XentDlBuilder *b, FluxText *tr, char const *text, float x, float top, float right, uint16_t weight, FluxColor color
) {
	if (!text || !text [0] || x >= right) return x;
	float         avail = right - x;
	FluxTextStyle ms;
	memset(&ms, 0, sizeof(ms));
	ms.font_size    = IB_TEXT_FONT;
	ms.font_weight  = ( FluxFontWeight ) weight;
	ms.word_wrap    = true;
	float         w = tr ? flux_minf(flux_text_measure(tr, text, &ms, avail).w, avail) : 0.0f;
	FluxRect      r = {x, top, avail, flux_maxf(20.0f, tr ? flux_text_measure(tr, text, &ms, avail).h : 20.0f)};
	XentTextStyle style
	  = {.font_size = IB_TEXT_FONT, .font_weight = weight, .h_align = 0u, .v_align = 0u, .flags = XENT_DL_TEXT_WRAP};
	if (!flux_rec_text_ex(b, &r, text, &style, color)) return x;
	return x + w;
}

static bool ib_draw_close(
  XentDlBuilder *b, FluxRect const *sb, FluxControlState const *state, float hover_local_x, FluxThemeColors const *t
) {
	FluxRect box
	  = {sb->x + sb->w - IB_CLOSE_MARGIN - IB_CLOSE_SIZE, sb->y + IB_CLOSE_MARGIN, IB_CLOSE_SIZE, IB_CLOSE_SIZE};
	bool over = state->enabled && hover_local_x >= (box.x - sb->x);
	bool ok   = true;
	if (over && state->pressed) ok = flux_rec_fill_rounded(b, &box, FLUX_CORNER_RADIUS, t->ctrl_alt_fill_tertiary);
	else if (over) ok = flux_rec_fill_rounded(b, &box, FLUX_CORNER_RADIUS, t->ctrl_alt_fill_secondary);
	if (ok) ok = flux_rec_glyph_wc(b, &box, IB_CLOSE_GLYPH, IB_CLOSE_FONT, t->text_primary);
	if (ok && state->focused && state->enabled) ok = flux_rec_focus_rect(b, &box, FLUX_CORNER_RADIUS);
	return ok;
}

bool flux_rec_infobar(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
) {
	FluxThemeColors const *t      = flux_rec_theme(rc);
	bool                   dark   = rc && rc->is_dark;
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	IbSeverity             s      = ib_severity(( int ) snap->u.info_bar.severity, dark, t);

	bool                   ok     = flux_rec_fill_rounded(b, bounds, radius, s.bg);
	if (ok) ok = flux_rec_stroke_rounded(b, bounds, radius, t->card_stroke_default, FLUX_STROKE_WIDTH);

	FluxRect icon_box = {bounds->x + IB_PAD_LEFT, bounds->y + IB_ICON_TOP, IB_ICON_SIZE, IB_ICON_SIZE};
	if (ok) ok = flux_rec_glyph_wc(b, &icon_box, IB_ICON_BG_GLYPH, IB_ICON_SIZE, s.icon_bg);
	if (ok) ok = flux_rec_glyph_wc(b, &icon_box, s.glyph, IB_ICON_SIZE, s.icon_fg);
	if (!ok) return false;

	bool      closable   = snap->u.info_bar.is_closable;
	float     close_left = closable ? bounds->x + bounds->w - IB_CLOSE_MARGIN - IB_CLOSE_SIZE : bounds->x + bounds->w;
	float     text_x     = bounds->x + IB_PAD_LEFT + IB_ICON_SIZE + IB_ICON_GAP;
	float     text_top   = bounds->y + IB_TEXT_TOP;
	float     text_right = flux_maxf(text_x, close_left - IB_MSG_GAP);
	FluxText *tr         = rc ? rc->text : NULL;

	float     after = ib_draw_run(b, tr, snap->u.info_bar.label, text_x, text_top, text_right, 600u, t->text_primary);
	float     msg_x = after + (after > text_x ? IB_MSG_GAP : 0.0f);
	( void ) ib_draw_run(b, tr, snap->u.info_bar.text_content, msg_x, text_top, text_right, 400u, t->text_primary);

	if (closable && state) ok = ib_draw_close(b, bounds, state, snap->hover_local_x, t);
	return ok;
}
