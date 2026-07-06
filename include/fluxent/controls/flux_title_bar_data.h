/**
 * @file flux_title_bar_data.h
 * @brief Data for FluxTitleBar — a WinUI 3 TitleBar band.
 *
 * A single leaf laying out (left to right) an optional back button, an optional
 * pane-toggle button, an optional icon, then the title and subtitle. The
 * remaining width is a drag region. Layout is shared between the renderer and
 * hit-testing via flux_titlebar_layout. When window integration is enabled the
 * control reports its drag rect and the interactive (passthrough) button rects
 * to the window so the band can move the window while the buttons stay clickable.
 */
#ifndef FLUX_TITLE_BAR_DATA_H
#define FLUX_TITLE_BAR_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore FluxNodeStore;
typedef struct FluxWindow    FluxWindow;

#define FLUX_TB_HEIGHT 48.0f /**< TitleBarCompactHeight. */
#define FLUX_TB_BTN    40.0f /**< Back / pane-toggle hit box. */
#define FLUX_TB_ICON   16.0f /**< App icon box. */
#define FLUX_TB_GAP    8.0f
#define FLUX_TB_PAD    12.0f /**< Leading/trailing content pad. */
#define FLUX_TB_CAPTION_W   46.0f /**< Caption-button width (WinUI caption buttons). */
#define FLUX_TB_GLYPH_BACK  0xE72Bu
#define FLUX_TB_GLYPH_PANE  0xE700u
#define FLUX_TB_GLYPH_MIN   0xE921u
#define FLUX_TB_GLYPH_MAX   0xE922u
#define FLUX_TB_GLYPH_REST  0xE923u
#define FLUX_TB_GLYPH_CLOSE 0xE8BBu

/** @brief Interactive region under the pointer. */
enum
{
	FLUX_TB_REGION_NONE = 0,
	FLUX_TB_REGION_BACK,
	FLUX_TB_REGION_PANE,
	FLUX_TB_REGION_MIN,
	FLUX_TB_REGION_MAX,
	FLUX_TB_REGION_CLOSE,
};

/** @brief Resolved element rects for one layout pass. */
typedef struct FluxTitleBarLayout {
	bool     has_back, has_toggle, has_icon, has_caption;
	FluxRect back, toggle, icon;
	FluxRect min, max, close; /**< Caption buttons (right cluster). */
	float    title_x;         /**< Left edge of the title text. */
	float    content_right;   /**< Right edge available to the title (before the caption cluster). */
} FluxTitleBarLayout;

/** @brief Retained state for a FLUX_CONTROL_TITLE_BAR node. */
typedef struct FluxTitleBarData {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     node;
	FluxWindow    *window;

	char          *title;    /**< Owned. */
	char          *subtitle; /**< Owned, or NULL. */
	char           icon_glyph [8]; /**< Resolved UTF-8 icon glyph, or "". */
	bool           show_back;
	bool           back_disabled;
	bool           show_pane_toggle;
	bool           integrate_window;
	bool           maximized; /**< Window is maximized (restore vs maximize glyph). */

	float          title_w;  /**< Measured title width (for subtitle placement). */
	int            pressed;  /**< FLUX_TB_REGION_* under an active press. */

	void         (*on_back)(void *ctx);
	void          *on_back_ctx;
	void         (*on_pane_toggle)(void *ctx);
	void          *on_pane_toggle_ctx;
} FluxTitleBarData;

/** @brief Compute the back/pane-toggle/icon/caption rects and the title bounds. */
static FluxTitleBarLayout inline flux_titlebar_layout(
  FluxRect const *b, bool back, bool toggle, bool icon, bool caption
) {
	FluxTitleBarLayout l = {0};
	l.has_back    = back;
	l.has_toggle  = toggle;
	l.has_icon    = icon;
	l.has_caption = caption;

	float cy = b->y + b->h * 0.5f;
	float x  = b->x + 4.0f;
	if (back) {
		l.back = (FluxRect) {x, cy - FLUX_TB_BTN * 0.5f, FLUX_TB_BTN, FLUX_TB_BTN};
		x += FLUX_TB_BTN;
	}
	if (toggle) {
		l.toggle = (FluxRect) {x, cy - FLUX_TB_BTN * 0.5f, FLUX_TB_BTN, FLUX_TB_BTN};
		x += FLUX_TB_BTN;
	}
	x += (back || toggle) ? FLUX_TB_GAP : (FLUX_TB_PAD - 4.0f);
	if (icon) {
		l.icon = (FluxRect) {x, cy - FLUX_TB_ICON * 0.5f, FLUX_TB_ICON, FLUX_TB_ICON};
		x += FLUX_TB_ICON + FLUX_TB_GAP;
	}
	l.title_x = x;

	/* Caption cluster (close | max | min), right-aligned, full height. */
	float right = b->x + b->w;
	if (caption) {
		l.close = (FluxRect) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right -= FLUX_TB_CAPTION_W;
		l.max = (FluxRect) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right -= FLUX_TB_CAPTION_W;
		l.min = (FluxRect) {right - FLUX_TB_CAPTION_W, b->y, FLUX_TB_CAPTION_W, b->h};
		right -= FLUX_TB_CAPTION_W;
	}
	l.content_right = right;
	return l;
}

#ifdef __cplusplus
}
#endif

#endif
