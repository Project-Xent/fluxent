#ifndef FLUX_PICKER_GEOM_H
#define FLUX_PICKER_GEOM_H

#include "controls/data/flux_picker_data.h"

#define FLUX_PFLY_BORDER      1.0f
#define FLUX_PFLY_SPACER      1.0f
#define FLUX_PFLY_ROW_H       41.0f
#define FLUX_PFLY_BAND_H      40.0f
#define FLUX_PFLY_BAND_PAD    4.0f
#define FLUX_PFLY_OVERLAY_PAD 3.0f
#define FLUX_PFLY_MAX_H       398.0f
#define FLUX_PFLY_RADIUS      4.0f

typedef struct FluxPickerColumn {
	float x, w;
} FluxPickerColumn;

static void inline flux_pgeom_split(FluxPickerCardData const *card, float inner_w, FluxPickerColumn *out) {
	float total = 0.0f;
	for (int i = 0; i < card->columns; i++) total += card->weight [i];
	float room = inner_w - ( float ) (card->columns - 1) * FLUX_PFLY_SPACER;
	float x    = FLUX_PFLY_BORDER;
	for (int i = 0; i < card->columns; i++) {
		out [i]  = ( FluxPickerColumn ) {x, total > 0.0f ? room * card->weight [i] / total : 0.0f};
		x       += out [i].w + FLUX_PFLY_SPACER;
	}
}

static FluxRect inline flux_pgeom_band(FluxPickerCardData const *card, float card_w) {
	float pad = card->time ? FLUX_PFLY_OVERLAY_PAD : FLUX_PFLY_BAND_PAD;
	return ( FluxRect ) {
	  FLUX_PFLY_BORDER + pad, FLUX_PFLY_BORDER + (card->body_h - FLUX_PFLY_BAND_H) * 0.5f,
	  card_w - 2.0f * (FLUX_PFLY_BORDER + pad), FLUX_PFLY_BAND_H};
}

#endif
