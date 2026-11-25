#ifndef FLUX_TITLE_BAR_DATA_H
#define FLUX_TITLE_BAR_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

#define FLUX_TB_HEIGHT          32.0f
#define FLUX_TB_TALL_HEIGHT     48.0f
#define FLUX_TB_BTN             40.0f
#define FLUX_TB_BTN_MARGIN      2.0f
#define FLUX_TB_ICON            16.0f
#define FLUX_TB_TEXT_SIZE       12.0f
#define FLUX_TB_TITLE_MARGIN    8.0f
#define FLUX_TB_SUBTITLE_MARGIN 16.0f
#define FLUX_TB_CAPTION_W       46.0f
#define FLUX_TB_GLYPH_BACK      0xe72bu
#define FLUX_TB_GLYPH_PANE      0xe700u
#define FLUX_TB_GLYPH_MIN       0xe921u
#define FLUX_TB_GLYPH_MAX       0xe922u
#define FLUX_TB_GLYPH_REST      0xe923u
#define FLUX_TB_GLYPH_CLOSE     0xe8bbu

enum
{
	FLUX_TB_REGION_NONE = 0,
	FLUX_TB_REGION_BACK,
	FLUX_TB_REGION_PANE,
	FLUX_TB_REGION_MIN,
	FLUX_TB_REGION_MAX,
	FLUX_TB_REGION_CLOSE,
};

typedef struct FluxTitleBarLayout {
	bool     has_back, has_toggle, has_icon, has_caption;
	FluxRect back, toggle, icon;
	FluxRect min, max, close;
	float    title_x;
	float    content_right;
	float    left_header_x;
	float    right_header_x;
} FluxTitleBarLayout;

typedef struct FluxTitleBarFrame {
	float leading_inset;
	float trailing_inset;
	float left_header_width;
	float right_header_width;
	bool  integrated;
	bool  native_buttons;
	bool  compact_text;
} FluxTitleBarFrame;

typedef struct FluxTitleBarData {
	XentCtx          *ctx;
	FluxStore        *store;
	XentNodeId        node;
	FluxWindow       *window;

	char             *title;
	char             *subtitle;
	char              icon_glyph [8];
	bool              show_back;
	bool              back_disabled;
	bool              show_pane_toggle;
	bool              integrate_window;
	bool              maximized;
	FluxTitleBarFrame frame;
	bool              has_slots;
	XentNodeId        slot_node [3];
	FluxRect          slot_rect [3];

	float             title_w;
	float             subtitle_w;
	int               pressed;
	int               focus_part;
	FluxKey           activate_key;

	void              (*on_back)(void *ctx);
	void             *on_back_ctx;
	void              (*on_pane_toggle)(void *ctx);
	void             *on_pane_toggle_ctx;
} FluxTitleBarData;

static FluxTitleBarLayout inline flux_titlebar_layout(
  FluxRect const *b, bool back, bool toggle, bool icon, FluxTitleBarFrame const *frame
) {
	FluxTitleBarLayout l = {0};
	l.has_back           = back;
	l.has_toggle         = toggle;
	l.has_icon           = icon;
	l.has_caption        = frame->integrated && !frame->native_buttons;

	float cy             = b->y + b->h * 0.5f;
	float x              = b->x + (frame->integrated ? frame->leading_inset : 2.0f);
	float button_y       = b->y + FLUX_TB_BTN_MARGIN;
	float button_h       = b->h > 2.0f * FLUX_TB_BTN_MARGIN ? b->h - 2.0f * FLUX_TB_BTN_MARGIN : 0.0f;
	if (back) {
		l.back  = ( FluxRect ) {x + FLUX_TB_BTN_MARGIN, button_y, FLUX_TB_BTN, button_h};
		x      += FLUX_TB_BTN + 2.0f * FLUX_TB_BTN_MARGIN;
	}
	if (toggle) {
		l.toggle  = ( FluxRect ) {x + FLUX_TB_BTN_MARGIN, button_y, FLUX_TB_BTN, button_h};
		x        += FLUX_TB_BTN + 2.0f * FLUX_TB_BTN_MARGIN;
	}
	l.left_header_x  = x;
	x               += frame->left_header_width + (back == toggle ? 14.0f : 2.0f);
	if (icon) {
		l.icon  = ( FluxRect ) {x, cy - FLUX_TB_ICON * 0.5f, FLUX_TB_ICON, FLUX_TB_ICON};
		x      += FLUX_TB_ICON + 16.0f;
	}
	l.title_x   = x;

	float right = b->x + b->w - frame->trailing_inset;
	if (l.has_caption) {
		l.close  = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
		l.max    = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
		l.min    = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
	}
	l.right_header_x = right - 48.0f - frame->right_header_width;
	l.content_right  = right - frame->right_header_width;
	return l;
}

static FluxRect inline flux_titlebar_text_rect(
  FluxTitleBarLayout const *layout, float h, float title_w, float text_w, bool has_title, bool subtitle
) {
	float x = layout->title_x;
	if (subtitle && has_title) x += title_w + FLUX_TB_TITLE_MARGIN;
	float margin    = subtitle ? FLUX_TB_SUBTITLE_MARGIN : FLUX_TB_TITLE_MARGIN;
	float available = layout->content_right - 48.0f - margin - x;
	float width     = available < text_w ? available : text_w;
	return ( FluxRect ) {x, 0.0f, width > 0.0f ? width : 0.0f, h};
}

#endif
