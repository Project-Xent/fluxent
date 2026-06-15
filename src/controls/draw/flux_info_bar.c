#include "render/flux_fluent.h"
#include "render/flux_icon.h"

#include <string.h>

/* WinUI InfoBar metrics (InfoBar_themeresources.xaml). ContentRoot padding
 * 16,0,0,0; icon margin 0,16,14,16 @16px; title/message top margin 14, message
 * left gap 12; close button 38x38 margin 5, glyph 16. MinHeight 48. */
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

/* Severity icon + symbol + background, from SystemFillColor* (#AARRGGBB). The
 * symbol foreground is TextFillColorInverse; informational uses the theme accent. */
typedef struct {
	FluxColor      bg;
	FluxColor      icon_bg;
	FluxColor      icon_fg;
	wchar_t const *glyph;
} IbSeverity;

/* Per-severity palette indexed [dark][severity]; columns follow FluxInfoBarSeverity
 * (informational, success, warning, error). Informational's icon background is the
 * runtime theme accent, so it is filled in below rather than tabled. */
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

static void
ib_draw_glyph(FluxRenderContext const *rc, FluxRect const *box, wchar_t const *glyph, float font, FluxColor color) {
	if (!rc->text) return;
	char utf8 [8];
	flux_icon_to_utf8(glyph, utf8, sizeof(utf8));

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = "Segoe Fluent Icons";
	ts.font_size   = font;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = color;
	flux_text_draw(rc->text, FLUX_RT(rc), utf8, box, &ts);
}

static float ib_draw_text(
  FluxRenderContext const *rc, char const *text, float x, float top, float right, FluxFontWeight weight, FluxColor color
) {
	if (!text || !text [0] || !rc->text || x >= right) return x;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = IB_TEXT_FONT;
	ts.font_weight = weight;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_TOP;
	ts.color       = color;
	ts.word_wrap   = true;

	float    avail = right - x;
	FluxSize sz    = flux_text_measure(rc->text, text, &ts, avail);
	float    w     = flux_minf(sz.w, avail);
	FluxRect r     = {x, top, avail, sz.h};
	flux_text_draw(rc->text, FLUX_RT(rc), text, &r, &ts);
	return x + w;
}

static void ib_draw_close(
  FluxRenderContext const *rc, FluxRect const *sb, FluxControlState const *state, float hover_local_x,
  FluxThemeColors const *t
) {
	FluxRect box
	  = {sb->x + sb->w - IB_CLOSE_MARGIN - IB_CLOSE_SIZE, sb->y + IB_CLOSE_MARGIN, IB_CLOSE_SIZE, IB_CLOSE_SIZE};
	bool over = state->enabled && hover_local_x >= (box.x - sb->x);

	if (over && state->pressed) flux_fill_rounded_rect(rc, &box, FLUX_CORNER_RADIUS, t->ctrl_alt_fill_tertiary);
	else if (over) flux_fill_rounded_rect(rc, &box, FLUX_CORNER_RADIUS, t->ctrl_alt_fill_secondary);

	ib_draw_glyph(rc, &box, IB_CLOSE_GLYPH, IB_CLOSE_FONT, t->text_primary);

	/* Keyboard focus lands on the close button (the InfoBar's only tab stop). */
	if (state->focused && state->enabled) flux_draw_focus_rect(rc, &box, FLUX_CORNER_RADIUS);
}

void flux_draw_info_bar(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxRect               sb     = flux_snap_bounds(bounds, 1.0f, 1.0f);
	float                  radius = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	FluxThemeColors const *t      = rc->theme ? rc->theme : flux_theme_default_colors();
	IbSeverity             s      = ib_severity(( int ) (snap->current_value + 0.5f), rc->is_dark, t);

	if (rc->fill_sink) {
		rc->fill_sink->color         = s.bg;
		rc->fill_sink->corner_radius = radius;
		rc->fill_sink->written       = true;
	}
	else { flux_fill_rounded_rect(rc, &sb, radius, s.bg); }

	float    half        = FLUX_STROKE_WIDTH * 0.5f;
	FluxRect stroke_rect = {sb.x + half, sb.y + half, sb.w - half * 2.0f, sb.h - half * 2.0f};
	flux_draw_rounded_rect(rc, &stroke_rect, radius, t->card_stroke_default, FLUX_STROKE_WIDTH);

	FluxRect icon_box = {sb.x + IB_PAD_LEFT, sb.y + IB_ICON_TOP, IB_ICON_SIZE, IB_ICON_SIZE};
	ib_draw_glyph(rc, &icon_box, IB_ICON_BG_GLYPH, IB_ICON_SIZE, s.icon_bg);
	ib_draw_glyph(rc, &icon_box, s.glyph, IB_ICON_SIZE, s.icon_fg);

	bool  closable   = snap->is_checked;
	float close_left = closable ? sb.x + sb.w - IB_CLOSE_MARGIN - IB_CLOSE_SIZE : sb.x + sb.w;
	float text_x     = sb.x + IB_PAD_LEFT + IB_ICON_SIZE + IB_ICON_GAP;
	float text_top   = sb.y + IB_TEXT_TOP;
	float text_right = flux_maxf(text_x, close_left - IB_MSG_GAP);

	float after_title
	  = ib_draw_text(rc, snap->label, text_x, text_top, text_right, FLUX_FONT_SEMI_BOLD, t->text_primary);
	float msg_x = after_title + (after_title > text_x ? IB_MSG_GAP : 0.0f);
	ib_draw_text(rc, snap->text_content, msg_x, text_top, text_right, FLUX_FONT_REGULAR, t->text_primary);

	if (closable) ib_draw_close(rc, &sb, state, snap->hover_local_x, t);
}
