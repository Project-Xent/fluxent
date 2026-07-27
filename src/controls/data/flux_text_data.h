/**
 * @file flux_text_data.h
 * @brief Data structures for FluxText (static text label) control.
 */
#ifndef FLUX_TEXT_DATA_H
#define FLUX_TEXT_DATA_H

#include <fluxent/flux_types.h>
#include <fluxent/flux_text.h>

#include <stdbool.h>
#include <stdint.h>

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


#endif
