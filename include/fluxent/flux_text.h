#ifndef FLUX_TEXT_H
#define FLUX_TEXT_H

#include "flux_types.h"
#include <xent/xent.h>

typedef struct ID2D1RenderTarget ID2D1RenderTarget;

typedef struct FluxText          FluxText;

#include <xtk/xtk_types.h>

typedef XtkTextAlign FluxTextAlign;
#define FLUX_TEXT_LEFT    XTK_TEXT_LEFT
#define FLUX_TEXT_CENTER  XTK_TEXT_CENTER
#define FLUX_TEXT_RIGHT   XTK_TEXT_RIGHT
#define FLUX_TEXT_JUSTIFY XTK_TEXT_JUSTIFY

typedef enum FluxTextVAlign
{
	FLUX_TEXT_TOP,
	FLUX_TEXT_VCENTER,
	FLUX_TEXT_BOTTOM,
} FluxTextVAlign;

typedef XtkFontWeight FluxFontWeight;
#define FLUX_FONT_THIN        XTK_FONT_THIN
#define FLUX_FONT_EXTRA_LIGHT XTK_FONT_EXTRA_LIGHT
#define FLUX_FONT_LIGHT       XTK_FONT_LIGHT
#define FLUX_FONT_REGULAR     XTK_FONT_REGULAR
#define FLUX_FONT_MEDIUM      XTK_FONT_MEDIUM
#define FLUX_FONT_SEMI_BOLD   XTK_FONT_SEMI_BOLD
#define FLUX_FONT_BOLD        XTK_FONT_BOLD
#define FLUX_FONT_EXTRA_BOLD  XTK_FONT_EXTRA_BOLD
#define FLUX_FONT_BLACK       XTK_FONT_BLACK

static uint16_t inline flux_font_weightnum(FluxFontWeight weight) {
	return ( uint16_t ) (weight > 0 ? weight : FLUX_FONT_REGULAR);
}

typedef struct FluxTextStyle {
	char const        *font_family;
	float              font_size;
	FluxFontWeight     font_weight;
	FluxTextAlign      text_align;
	FluxTextVAlign     vert_align;
	FluxColor          color;
	bool               word_wrap;
	XentTextRun const *runs;
	uint32_t           run_count;
	XentTextTrim       trim;
	uint32_t           max_lines;
} FluxTextStyle;

typedef struct FluxTextLayoutQuery {
	FluxText            *renderer;
	char const          *text;
	FluxTextStyle const *style;
	float                max_width;
} FluxTextLayoutQuery;

typedef struct FluxTextHitTestQuery {
	FluxTextLayoutQuery layout;
	float               x;
	float               y;
} FluxTextHitTestQuery;

typedef struct FluxTextCaretQuery {
	FluxTextLayoutQuery layout;
	int                 index;
} FluxTextCaretQuery;

typedef struct FluxTextSelectionQuery {
	FluxTextLayoutQuery layout;
	int                 start;
	int                 end;
	FluxRect           *out_rects;
	uint32_t            max_rects;
} FluxTextSelectionQuery;

typedef struct FluxTextClusterQuery {
	FluxTextLayoutQuery layout;
	uint32_t            index;
} FluxTextClusterQuery;

XENT_NODISCARD FluxText *flux_text_create(void);

void                     flux_text_destroy(FluxText *tr);

bool                     flux_text_register(FluxText *tr, XentCtx *ctx);

void                     flux_text_draw(
  FluxText *tr, ID2D1RenderTarget *rt, char const *text, FluxRect const *bounds, FluxTextStyle const *style
);

FluxSize flux_text_measure(FluxText *tr, char const *text, FluxTextStyle const *style, float max_width);

int      flux_text_hittest(FluxTextHitTestQuery const *query);

FluxRect flux_text_caret(FluxTextCaretQuery const *query);

uint32_t flux_text_selection(FluxTextSelectionQuery const *query);

uint32_t flux_text_next(FluxTextClusterQuery const *query);

uint32_t flux_text_prev(FluxTextClusterQuery const *query);

#endif
