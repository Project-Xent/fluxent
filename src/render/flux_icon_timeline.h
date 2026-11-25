#ifndef FLUX_ICON_TIMELINE_H
#define FLUX_ICON_TIMELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct FluxIconSegment {
	uint8_t from, to;
	float   start, end;
} FluxIconSegment;

typedef struct FluxIconTimeline {
	FluxIconSegment const *segments;
	size_t                 count;
	float                  frames_per_second;
} FluxIconTimeline;

typedef struct FluxIconPlayback {
	double  started_at;
	float   start_frame, end_frame;
	uint8_t current, queued [4], queue_count;
	bool    initialized;
} FluxIconPlayback;

float flux_icon_timeline_frame(
  FluxIconTimeline const *timeline, FluxIconPlayback *p, uint8_t state, double now, bool animate, bool *playing
);

#endif
