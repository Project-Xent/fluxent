/**
 * @file flux_init_value.h
 * @brief Private Desc + init prototypes for value/range controls:
 * Slider / ProgressBar / ProgressRing / RatingControl.
 */
#ifndef FLUX_INIT_VALUE_H
#define FLUX_INIT_VALUE_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxSliderDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	float      min;
	float      max;
	float      value;
	void       (*on_change)(void *, float);
	void      *userdata;
} FluxSliderDesc;

typedef struct FluxProgressDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	float      value;
	float      max_value;
	float      min_value;
} FluxProgressDesc;

/** @brief Create a slider node. */
XentNodeId flux_slider_init(FluxSliderDesc const *info);

/** @brief Create a progress bar node. */
XentNodeId flux_progress_init(FluxProgressDesc const *info);

/** @brief Create a progress ring node. */
XentNodeId flux_progressring_init(FluxProgressDesc const *info);

/* -------------------------------------------------------------------------
 * RatingControl
 * ---------------------------------------------------------------------- */

typedef struct FluxRatingDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	double      value;             /**< <= 0 = unset. */
	int         max_rating;        /**< 0 = 5. */
	double      placeholder_value; /**< < 0 = none. */
	int         initial_set_value; /**< 0 = 1. */
	bool        is_clear_enabled;
	bool        is_read_only;
	char const *caption;
	double      item_spacing; /**< 0 = 8. */
	double      star_size;    /**< 0 = 16. */
	uint32_t    set_glyph;    /**< 0 = U+E735. */
	uint32_t    unset_glyph;  /**< 0 = U+E734. */
	void        (*on_change)(void *ctx, double value, int stars);
	void       *userdata;
} FluxRatingDesc;

/** @brief Create a RatingControl (star strip with hover preview + clear). */
XentNodeId flux_rating_init(FluxRatingDesc const *info);
void       flux_rating_retarget(FluxStore *store, XentNodeId id);

/** @brief Set the value (< 0 = unset). */
void       flux_rating_set_value(FluxStore *store, XentNodeId id, double value);

/** @brief Set MaxRating and refresh preferred width. */
void       flux_rating_set_max(FluxStore *store, XentNodeId id, int max_rating);

/** @brief Set trailing caption and refresh preferred width. */
void       flux_rating_set_caption(FluxStore *store, XentNodeId id, char const *caption);

/** @brief Toggle read-only. */
void       flux_rating_setreadonly(FluxStore *store, XentNodeId id, bool read_only);
void       flux_rating_setbehavior(
  FluxStore *store, XentNodeId id, double placeholder, int initial_value, bool clear_enabled
);

#endif
