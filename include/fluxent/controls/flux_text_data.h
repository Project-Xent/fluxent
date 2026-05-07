/**
 * @file flux_text_data.h
 * @brief Data structures for FluxText (static text label) control.
 */
#ifndef FLUX_TEXT_DATA_H
#define FLUX_TEXT_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum FluxTextAlign
{
	FLUX_TEXT_LEFT,
	FLUX_TEXT_CENTER,
	FLUX_TEXT_RIGHT,
	FLUX_TEXT_JUSTIFY,
} FluxTextAlign;

typedef enum FluxTextVAlign
{
	FLUX_TEXT_TOP,
	FLUX_TEXT_VCENTER,
	FLUX_TEXT_BOTTOM,
} FluxTextVAlign;

typedef enum FluxFontWeight
{
	FLUX_FONT_THIN        = 100,
	FLUX_FONT_EXTRA_LIGHT = 200,
	FLUX_FONT_LIGHT       = 300,
	FLUX_FONT_REGULAR     = 400,
	FLUX_FONT_MEDIUM      = 500,
	FLUX_FONT_SEMI_BOLD   = 600,
	FLUX_FONT_BOLD        = 700,
	FLUX_FONT_EXTRA_BOLD  = 800,
	FLUX_FONT_BLACK       = 900,
} FluxFontWeight;

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
