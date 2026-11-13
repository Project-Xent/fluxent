#ifndef FLUX_TITLE_BAR_DATA_H
#define FLUX_TITLE_BAR_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

#define FLUX_TB_HEIGHT      48.0f
#define FLUX_TB_BTN         40.0f
#define FLUX_TB_ICON        16.0f
#define FLUX_TB_GAP         8.0f
#define FLUX_TB_PAD         12.0f
#define FLUX_TB_CAPTION_W   46.0f
#define FLUX_TB_GLYPH_BACK  0xe72bu
#define FLUX_TB_GLYPH_PANE  0xe700u
#define FLUX_TB_GLYPH_MIN   0xe921u
#define FLUX_TB_GLYPH_MAX   0xe922u
#define FLUX_TB_GLYPH_REST  0xe923u
#define FLUX_TB_GLYPH_CLOSE 0xe8bbu

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
} FluxTitleBarLayout;

typedef struct FluxTitleBarData {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  node;
	FluxWindow *window;

	char       *title;
	char       *subtitle;
	char        icon_glyph [8];
	bool        show_back;
	bool        back_disabled;
	bool        show_pane_toggle;
	bool        integrate_window;
	bool        maximized;

	float       title_w;
	int         pressed;

	void        (*on_back)(void *ctx);
	void       *on_back_ctx;
	void        (*on_pane_toggle)(void *ctx);
	void       *on_pane_toggle_ctx;
} FluxTitleBarData;

static FluxTitleBarLayout inline flux_titlebar_layout(
  FluxRect const *b, bool back, bool toggle, bool icon, bool caption
) {
	FluxTitleBarLayout l = {0};
	l.has_back           = back;
	l.has_toggle         = toggle;
	l.has_icon           = icon;
	l.has_caption        = caption;

	float cy             = b->y + b->h * 0.5f;
	float x              = b->x + 4.0f;
	if (back) {
		l.back  = ( FluxRect ) {x, cy - FLUX_TB_BTN * 0.5f, FLUX_TB_BTN, FLUX_TB_BTN};
		x      += FLUX_TB_BTN;
	}
	if (toggle) {
		l.toggle  = ( FluxRect ) {x, cy - FLUX_TB_BTN * 0.5f, FLUX_TB_BTN, FLUX_TB_BTN};
		x        += FLUX_TB_BTN;
	}
	x += (back || toggle) ? FLUX_TB_GAP : (FLUX_TB_PAD - 4.0f);
	if (icon) {
		l.icon  = ( FluxRect ) {x, cy - FLUX_TB_ICON * 0.5f, FLUX_TB_ICON, FLUX_TB_ICON};
		x      += FLUX_TB_ICON + FLUX_TB_GAP;
	}
	l.title_x   = x;

	float right = b->x + b->w;
	if (caption) {
		l.close  = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
		l.max    = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
		l.min    = ( FluxRect ) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right   -= FLUX_TB_CAPTION_W;
	}
	l.content_right = right;
	return l;
}

#endif
