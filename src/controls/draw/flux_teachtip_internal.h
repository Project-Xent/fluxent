#ifndef FLUX_TEACHTIP_INTERNAL_H
#define FLUX_TEACHTIP_INTERNAL_H

#include "controls/data/flux_teaching_tip_data.h"

#include <stdbool.h>
#include <stdint.h>

#define FLUX_TIP_ZONE_NONE        0
#define FLUX_TIP_ZONE_ACTION      1
#define FLUX_TIP_ZONE_CLOSE       2
#define FLUX_TIP_ZONE_X           3

#define FLUX_TIP_TAIL_EDGE_NONE   0
#define FLUX_TIP_TAIL_EDGE_TOP    1
#define FLUX_TIP_TAIL_EDGE_BOTTOM 2
#define FLUX_TIP_TAIL_EDGE_LEFT   3
#define FLUX_TIP_TAIL_EDGE_RIGHT  4

typedef struct FluxRenderCtx FluxRenderCtx;
typedef struct FluxText      FluxText;

typedef struct FluxTeachingTipPaintInfo {
	FluxTeachingTipData const *model;
	float                      tip_w, tip_h;
	uint8_t                    tail_edge;
	float                      tail_center;
	FluxColor                  background;
	float                      scale_x, scale_y;
	float                      scale_cx, scale_cy;
	int                        hot_zone;
	int                        pressed_zone;
	FluxRect                   action_rect;
	FluxRect                   close_rect;
	FluxRect                   x_rect;
} FluxTeachingTipPaintInfo;

XentDl *flux_teachtip_record_surface(FluxRenderCtx const *rc, FluxTeachingTipPaintInfo const *info);

float   flux_teachtip_text_height(FluxText *text, char const *s, bool title, float max_w);

#endif
