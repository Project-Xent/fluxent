#ifndef FLUX_RATING_DATA_H
#define FLUX_RATING_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#define FLUX_RATING_CAPTION_GAP 12.0f

typedef struct FluxRatingData {
	FluxStore *store;
	XentNodeId node;
	double     value;
	int        max_rating;
	double     placeholder_value;
	int        initial_set_value;
	bool       is_clear_enabled;
	bool       is_read_only;
	double     item_spacing;
	double     star_size;
	uint32_t   set_glyph;
	uint32_t   unset_glyph;
	char      *caption;

	bool       pointer_over;
	bool       pointer_down;
	double     hover_value;

	void       (*on_change)(void *ctx, double value, int stars);
	void      *on_change_ctx;
} FluxRatingData;

#endif
