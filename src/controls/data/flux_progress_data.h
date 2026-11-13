#ifndef FLUX_PROGRESS_DATA_H
#define FLUX_PROGRESS_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef enum FluxProgressState
{
	FLUX_PROGRESS_NORMAL,
	FLUX_PROGRESS_PAUSED,
	FLUX_PROGRESS_ERROR,
} FluxProgressState;

typedef struct FluxProgressData {
	float      value;
	float      min_value;
	float      max_value;
	FluxColor  fill_color;
	FluxColor  track_color;
	bool       indeterminate;
	uint8_t    state;
	XentInsets padding;
} FluxProgressData;

typedef struct FluxProgressRingData {
	float     value;
	float     min_value;
	float     max_value;
	FluxColor fill_color;
	FluxColor track_color;
	bool      indeterminate;
	bool      inactive;
} FluxProgressRingData;

#endif
