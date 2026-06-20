/**
 * @file flux_text_data.h
 * @brief Data structures for FluxText (static text label) control.
 */
#ifndef FLUX_TEXT_DATA_H
#define FLUX_TEXT_DATA_H

#include "../flux_types.h"
#include <xtk/xtk_types.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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
 * @brief Configuration for a static text label.
 *
 * Used by controls that display read-only, non-editable text.
 * For editable text input, see FluxTextBoxData.
 */
typedef struct FluxTextData {
	char const    *content;            /**< UTF-8 text to display */
	char const    *font_family;        /**< Font family name (NULL = system default) */
	FluxColor      text_color;         /**< Text foreground color */
	float          font_size;          /**< Font size in DIPs */
	FluxFontWeight font_weight;        /**< Font weight (FLUX_FONT_*) */
	FluxTextAlign  alignment;          /**< Horizontal text alignment */
	FluxTextVAlign vertical_alignment; /**< Vertical text alignment */
	uint8_t        max_lines;          /**< Max visible lines (0 = unlimited) */
	bool           wrap;               /**< Enable word wrapping */
	bool           selectable;         /**< Allow text selection */
} FluxTextData;

#ifdef __cplusplus
}
#endif

#endif
