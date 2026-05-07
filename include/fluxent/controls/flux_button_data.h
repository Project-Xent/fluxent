/**
 * @file flux_button_data.h
 * @brief Data structures for FluxButton and FluxRepeatButton controls.
 */
#ifndef FLUX_BUTTON_DATA_H
#define FLUX_BUTTON_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Visual style variants for button-like controls. */
typedef enum FluxButtonStyle
{
	FLUX_BUTTON_STANDARD, /**< Default elevated style with background */
	FLUX_BUTTON_SUBTLE,   /**< Transparent until hovered */
	FLUX_BUTTON_TEXT,     /**< Text-only, minimal chrome */
	FLUX_BUTTON_ACCENT,   /**< Accent-colored background */
} FluxButtonStyle;

/**
 * @brief Configuration for a standard push button.
 */
typedef struct FluxButtonData {
	char const     *label;                  /**< Button text (UTF-8) */
	char const     *icon_name;              /**< Icon name (NULL = no icon) */
	FluxColor       label_color;            /**< Text foreground color */
	FluxColor       hover_color;            /**< Background on hover */
	FluxColor       pressed_color;          /**< Background when pressed */
	float           font_size;              /**< Font size in DIPs */
	FluxButtonStyle style;                  /**< Visual style variant */
	bool            enabled;                /**< Is button interactive? */
	bool            is_checked;             /**< For toggle buttons */
	void            (*on_click)(void *ctx); /**< Callback invoked when the button is clicked. */
	void           *on_click_ctx;
} FluxButtonData;

/**
 * @brief Configuration for a repeat button (fires while held down).
 *
 * Like FluxButtonData, but includes repeat timing parameters.
 * on_click fires after repeat_delay_ms, then every repeat_interval_ms
 * while the button remains pressed.
 */
typedef struct FluxRepeatButtonData {
	char const     *label;
	char const     *icon_name;
	FluxColor       label_color;
	float           font_size;
	FluxButtonStyle style;
	bool            enabled;
	uint32_t        repeat_delay_ms;        /**< Initial delay before repeat (default 400) */
	uint32_t        repeat_interval_ms;     /**< Interval between repeats (default 50) */
	void            (*on_click)(void *ctx); /**< Callback invoked for each repeat activation. */
	void           *on_click_ctx;
} FluxRepeatButtonData;

/**
 * @brief Configuration for a hyperlink button.
 */
typedef struct FluxHyperlinkData {
	char const *label;       /**< Link text */
	char const *url;         /**< Target URL (NULL if using on_click) */
	char const *icon_name;   /**< Optional icon */
	FluxColor   label_color; /**< Text color (typically accent) */
	float       font_size;
	bool        enabled;
	bool        visited;                /**< Has the link been clicked? */
	void        (*on_click)(void *ctx); /**< Callback invoked when the link is clicked. */
	void       *on_click_ctx;
} FluxHyperlinkData;

#ifdef __cplusplus
}
#endif

#endif
