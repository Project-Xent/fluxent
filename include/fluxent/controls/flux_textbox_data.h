/**
 * @file flux_textbox_data.h
 * @brief Data structures for FluxTextBox, FluxPasswordBox, and FluxNumberBox.
 */
#ifndef FLUX_TEXTBOX_DATA_H
#define FLUX_TEXTBOX_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   FluxTextBoxData — Control-specific data for FluxTextBox
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a text input box.
 *
 * Supports single-line and multiline editing with selection, IME
 * composition, and callbacks for text changes and submission.
 */
typedef struct FluxTextBoxData {
	char const    *content;           /**< Current text content (UTF-8) */
	char const    *placeholder;       /**< Placeholder text when empty */
	char const    *font_family;       /**< Font family (NULL = default) */
	float          font_size;         /**< Font size in DIPs */
	FluxColor      text_color;        /**< Text foreground color */
	FluxColor      placeholder_color; /**< Placeholder text color */
	FluxColor      selection_color;   /**< Selection highlight color */
	uint32_t       cursor_position;   /**< Caret position in characters */
	uint32_t       selection_start;   /**< Selection anchor position */
	uint32_t       selection_end;     /**< Selection active end */
	float          scroll_offset_x;   /**< Horizontal scroll offset */
	uint32_t       max_length;        /**< Max characters (0 = unlimited) */
	bool           enabled;           /**< Is text box interactive? */
	bool           readonly;          /**< Read-only mode (selectable but not editable) */
	bool           multiline;         /**< Allow newlines */

	/* -------- IME composition state -------- */
	wchar_t const *composition_text;   /**< Active IME composition string */
	uint32_t       composition_length; /**< Length of composition (chars) */
	uint32_t       composition_cursor; /**< Cursor position in composition */

	/* -------- Callbacks -------- */
	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_submit)(void *ctx); /**< Called on Enter key */
	void          *on_submit_ctx;
} FluxTextBoxData;

/* ═══════════════════════════════════════════════════════════════════════
   FluxPasswordBoxData — Control-specific data for FluxPasswordBox
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a password input box.
 *
 * Like FluxTextBoxData but with masking support.
 */
typedef struct FluxPasswordBoxData {
	char const    *content;
	char const    *placeholder;
	char const    *font_family;
	float          font_size;
	FluxColor      text_color;
	FluxColor      placeholder_color;
	FluxColor      selection_color;
	uint32_t       cursor_position;
	uint32_t       selection_start;
	uint32_t       selection_end;
	float          scroll_offset_x;
	uint32_t       max_length;
	bool           enabled;
	bool           readonly;
	wchar_t        mask_char;  /**< Mask character (default '●' U+25CF) */
	bool           show_plain; /**< true = reveal password text */

	/* -------- IME composition state -------- */
	wchar_t const *composition_text;
	uint32_t       composition_length;
	uint32_t       composition_cursor;

	/* -------- Callbacks -------- */
	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_submit)(void *ctx);
	void          *on_submit_ctx;
} FluxPasswordBoxData;

/* ═══════════════════════════════════════════════════════════════════════
   NumberBox enums
   ═══════════════════════════════════════════════════════════════════════ */

typedef enum FluxNBSpinPlacement
{
	FLUX_NB_SPIN_HIDDEN  = 0, /**< No spin buttons */
	FLUX_NB_SPIN_COMPACT = 1, /**< Small buttons (stacked) */
	FLUX_NB_SPIN_INLINE  = 2, /**< Full-height buttons (side-by-side) */
} FluxNBSpinPlacement;

typedef enum FluxNBValidation
{
	FLUX_NB_VALIDATE_OVERWRITE = 0, /**< Reset to last valid on blur */
	FLUX_NB_VALIDATE_DISABLED  = 1, /**< Allow any text */
} FluxNBValidation;

/* ═══════════════════════════════════════════════════════════════════════
   FluxNumberBoxData — Control-specific data for FluxNumberBox
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a numeric input box with optional spin buttons.
 *
 * Combines text editing with numeric constraints (min, max, step) and
 * spin buttons for increment/decrement.
 */
typedef struct FluxNumberBoxData {
	char const    *content; /**< Text representation of value */
	char const    *placeholder;
	char const    *font_family;
	float          font_size;
	FluxColor      text_color;
	FluxColor      placeholder_color;
	FluxColor      selection_color;
	uint32_t       cursor_position;
	uint32_t       selection_start;
	uint32_t       selection_end;
	float          scroll_offset_x;
	bool           enabled;
	bool           readonly;

	/* -------- Numeric constraints -------- */
	double         min_value;     /**< Minimum allowed value */
	double         max_value;     /**< Maximum allowed value */
	double         step;          /**< Increment/decrement step */
	double         current_value; /**< Parsed numeric value */
	bool           spin_enabled;  /**< Show spin buttons */

	/* -------- IME composition state -------- */
	wchar_t const *composition_text;
	uint32_t       composition_length;
	uint32_t       composition_cursor;

	/* -------- Callbacks -------- */
	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_value_change)(void *ctx, double value);
	void          *on_value_change_ctx;
	void           (*on_submit)(void *ctx);
	void          *on_submit_ctx;
} FluxNumberBoxData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_TEXTBOX_DATA_H */
