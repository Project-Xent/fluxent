/**
 * @file flux_text.h
 * @brief Text rendering, measurement, and hit-testing for Fluxent.
 *
 * FluxText wraps DirectWrite to provide high-level text operations.
 * It handles text layout creation, caching, and rendering with consistent
 * styling across all Fluxent controls.
 *
 * ## Usage Pattern
 *
 * 1. Create renderer with `flux_text_create()`
 * 2. Register with Xent context: `flux_text_register(tr, ctx)`
 *    (This hooks the text measure function into the layout engine)
 * 3. Use rendering functions:
 *    - `flux_text_draw()` for rendering
 *    - `flux_text_measure()` for layout measurement
 *    - `flux_text_hittest()` for click-to-position mapping
 *    - `flux_text_caret()` for cursor positioning
 *    - `flux_text_selection()` for selection highlighting
 * 4. Destroy with `flux_text_destroy()`
 *
 * ## Text Styling
 *
 * Use FluxTextStyle to specify font, size, weight, alignment, and color.
 * The renderer maintains a layout cache to avoid recreating DirectWrite
 * layouts for unchanged text/style combinations.
 *
 * ## Thread Safety
 *
 * All functions must be called from the render thread.
 */
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

/** FluxFontWeight values are DWrite-style numerics; 0 falls back to regular. */
static uint16_t inline flux_font_weight_numeric(FluxFontWeight weight) {
	return ( uint16_t ) (weight > 0 ? weight : FLUX_FONT_REGULAR);
}

/**
 * @brief Text styling parameters.
 *
 * Specifies how text should be rendered: font, size, weight, alignment,
 * color, and wrapping behavior.
 */
typedef struct FluxTextStyle {
	char const    *font_family; /**< Font family name (NULL = system default) */
	float          font_size;   /**< Font size in DIPs */
	FluxFontWeight font_weight; /**< Font weight (FLUX_FONT_*) */
	FluxTextAlign  text_align;  /**< Horizontal alignment */
	FluxTextVAlign vert_align;  /**< Vertical alignment */
	FluxColor      color;       /**< Text foreground color */
	bool           word_wrap;   /**< Enable word wrapping */
} FluxTextStyle;

/** @brief Text layout input shared by hit-testing and range queries. */
typedef struct FluxTextLayoutQuery {
	FluxText            *renderer;
	char const          *text;
	FluxTextStyle const *style;
	float                max_width;
} FluxTextLayoutQuery;

/** @brief Text-local hit-test input. */
typedef struct FluxTextHitTestQuery {
	FluxTextLayoutQuery layout;
	float               x;
	float               y;
} FluxTextHitTestQuery;

/** @brief Caret rectangle input. */
typedef struct FluxTextCaretQuery {
	FluxTextLayoutQuery layout;
	int                 index;
} FluxTextCaretQuery;

/** @brief Selection rectangle input and output buffers. */
typedef struct FluxTextSelectionQuery {
	FluxTextLayoutQuery layout;
	int                 start;
	int                 end;
	FluxRect           *out_rects;
	uint32_t            max_rects;
} FluxTextSelectionQuery;

/** @brief Grapheme-cluster navigation input. @ref index is a UTF-16 code-unit offset. */
typedef struct FluxTextClusterQuery {
	FluxTextLayoutQuery layout;
	uint32_t            index;
} FluxTextClusterQuery;

/**
 * @brief Create a text renderer.
 *
 * Initializes DirectWrite resources and layout cache.
 *
 * @return New text renderer, or NULL on failure.
 */
XENT_NODISCARD FluxText *flux_text_create(void);

/**
 * @brief Destroy a text renderer.
 * @param tr Text renderer to destroy (NULL is safe).
 */
void                     flux_text_destroy(FluxText *tr);

/**
 * @brief Register the text renderer with a Xent layout context.
 *
 * Hooks the text measurement function into Xent so that text nodes
 * can be measured during layout computation.
 *
 * @param tr Text renderer.
 * @param ctx Xent context to register with.
 * @return true on success.
 */
bool                     flux_text_register(FluxText *tr, XentCtx *ctx);

/**
 * @brief Draw text to a D2D render target.
 *
 * @param tr Text renderer.
 * @param rt D2D render target (typically from flux_gfx_d2d).
 * @param text UTF-8 text to render.
 * @param bounds Bounding rectangle for text layout.
 * @param style Text styling parameters.
 */
void                     flux_text_draw(
  FluxText *tr, ID2D1RenderTarget *rt, char const *text, FluxRect const *bounds, FluxTextStyle const *style
);

/**
 * @brief Measure text size.
 *
 * Returns the size needed to render the text with the given style.
 * Uses max_width to determine line wrapping (if word_wrap is enabled).
 *
 * @param tr Text renderer.
 * @param text UTF-8 text to measure.
 * @param style Text styling parameters.
 * @param max_width Maximum width for wrapping (FLT_MAX for no limit).
 * @return Measured text size.
 */
FluxSize flux_text_measure(FluxText *tr, char const *text, FluxTextStyle const *style, float max_width);

/**
 * @brief Hit-test a point to find the character index.
 *
 * Given a point in text-local coordinates, returns the character index
 * at that position. Useful for click-to-caret positioning.
 *
 * @param tr Text renderer.
 * @param text UTF-8 text.
 * @param style Text styling parameters.
 * @param max_width Maximum width for layout.
 * @param x X coordinate (relative to text bounds origin).
 * @param y Y coordinate (relative to text bounds origin).
 * @return Character index at the point, or 0 when input is invalid.
 */
int      flux_text_hittest(FluxTextHitTestQuery const *query);

/**
 * @brief Get the caret rectangle for a character index.
 *
 * Returns a thin rectangle representing the caret (text cursor) position
 * at the specified character index.
 *
 * @param tr Text renderer.
 * @param text UTF-8 text.
 * @param style Text styling parameters.
 * @param max_width Maximum width for layout.
 * @param index Character index.
 * @return Caret rectangle (relative to text bounds origin).
 */
FluxRect flux_text_caret(FluxTextCaretQuery const *query);

/**
 * @brief Get rectangles covering a text selection.
 *
 * Returns one or more rectangles that cover the selected text range.
 * Multiple rectangles are returned for multi-line selections.
 *
 * @param tr Text renderer.
 * @param text UTF-8 text.
 * @param style Text styling parameters.
 * @param max_width Maximum width for layout.
 * @param start Selection start index.
 * @param end Selection end index.
 * @param out_rects Output array for rectangles.
 * @param max_rects Maximum number of rectangles to output.
 * @return Number of rectangles written to out_rects.
 */
uint32_t flux_text_selection(FluxTextSelectionQuery const *query);

/**
 * @brief UTF-16 offset of the next grapheme-cluster boundary at or after the
 * query index, using DirectWrite cluster analysis. Returns the text's UTF-16
 * length when already at the end.
 */
uint32_t flux_text_next(FluxTextClusterQuery const *query);

/**
 * @brief UTF-16 offset of the previous grapheme-cluster boundary before the
 * query index, using DirectWrite cluster analysis. Returns 0 at the start.
 */
uint32_t flux_text_prev(FluxTextClusterQuery const *query);


#endif
