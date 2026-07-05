/**
 * @file flux_rating_data.h
 * @brief Data for FluxRatingControl — a star-rating strip.
 *
 * A row of star glyphs: an outline "unset" background layer, and a "set"
 * foreground layer clipped horizontally to the current (or hovered) value so
 * fractional programmatic values render as partial fills. Hover previews the
 * prospective value in a distinct brush; click commits; re-clicking the
 * current rating clears (when IsClearEnabled).
 */
#ifndef FLUX_RATING_DATA_H
#define FLUX_RATING_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Gap between the star strip and the trailing caption (DIP). */
#define FLUX_RATING_CAPTION_GAP 12.0f

/** @brief Retained state for a FLUX_CONTROL_RATING node. */
typedef struct FluxRatingData {
	double   value;             /**< Current rating; < 0 = unset. */
	int      max_rating;        /**< Star count (>= 1). */
	double   placeholder_value; /**< >= 0 shows placeholder fill while unset. */
	int      initial_set_value; /**< Value applied on first keyboard step from unset. */
	bool     is_clear_enabled;  /**< Re-clicking the current rating clears to unset. */
	bool     is_read_only;      /**< No hover/commit. */
	double   item_spacing;      /**< Gap between star boxes (DIP). */
	double   star_size;         /**< Per-star box (DIP). */
	uint32_t set_glyph;         /**< Filled star codepoint (default U+E735). */
	uint32_t unset_glyph;       /**< Outline star codepoint (default U+E734). */
	char    *caption;           /**< Owned trailing caption, or NULL. */

	bool     pointer_over;      /**< Cursor within the strip. */
	bool     pointer_down;      /**< Pressed (enables drag scrub / drag-off clear). */
	double   hover_value;       /**< Preview value while hovering; -1 = none. */

	void (*on_change)(void *ctx, double value, int stars);
	void  *on_change_ctx;
} FluxRatingData;

#ifdef __cplusplus
}
#endif

#endif
