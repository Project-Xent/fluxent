#ifndef FLUX_TEACHING_TIP_DATA_H
#define FLUX_TEACHING_TIP_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>

typedef XtkTeachingTipPlacement   FluxTeachingTipPlacement;
typedef XtkTeachingTipCloseReason FluxTeachingTipCloseReason;

#define FLUX_TIP_PLACE_AUTO          XTK_TIP_PLACE_AUTO
#define FLUX_TIP_PLACE_TOP           XTK_TIP_PLACE_TOP
#define FLUX_TIP_PLACE_BOTTOM        XTK_TIP_PLACE_BOTTOM
#define FLUX_TIP_PLACE_LEFT          XTK_TIP_PLACE_LEFT
#define FLUX_TIP_PLACE_RIGHT         XTK_TIP_PLACE_RIGHT
#define FLUX_TIP_PLACE_TOP_RIGHT     XTK_TIP_PLACE_TOP_RIGHT
#define FLUX_TIP_PLACE_TOP_LEFT      XTK_TIP_PLACE_TOP_LEFT
#define FLUX_TIP_PLACE_BOTTOM_RIGHT  XTK_TIP_PLACE_BOTTOM_RIGHT
#define FLUX_TIP_PLACE_BOTTOM_LEFT   XTK_TIP_PLACE_BOTTOM_LEFT
#define FLUX_TIP_PLACE_LEFT_TOP      XTK_TIP_PLACE_LEFT_TOP
#define FLUX_TIP_PLACE_LEFT_BOTTOM   XTK_TIP_PLACE_LEFT_BOTTOM
#define FLUX_TIP_PLACE_RIGHT_TOP     XTK_TIP_PLACE_RIGHT_TOP
#define FLUX_TIP_PLACE_RIGHT_BOTTOM  XTK_TIP_PLACE_RIGHT_BOTTOM
#define FLUX_TIP_PLACE_CENTER        XTK_TIP_PLACE_CENTER

#define FLUX_TIP_CLOSE_BUTTON        XTK_TIP_CLOSE_BUTTON
#define FLUX_TIP_CLOSE_LIGHT_DISMISS XTK_TIP_CLOSE_LIGHT_DISMISS
#define FLUX_TIP_CLOSE_PROGRAMMATIC  XTK_TIP_CLOSE_PROGRAMMATIC

#define FLUX_TIP_TAIL_AUTO           0
#define FLUX_TIP_TAIL_VISIBLE        1
#define FLUX_TIP_TAIL_COLLAPSED      2

typedef struct FluxTeachingTipData {
	char const              *title;
	char const              *subtitle;
	uint32_t                 icon_glyph;
	char const              *action_text;
	bool                     action_accent;
	char const              *close_text;
	FluxTeachingTipPlacement preferred;
	FluxTeachingTipPlacement effective;
	int8_t                   tail_visibility;
	bool                     light_dismiss;
	bool                     untargeted;
	float                    placement_margin;
	bool                     open;
	void                     (*on_action)(void *ctx);
	void                    *action_ctx;
	void                     (*on_close)(void *ctx, FluxTeachingTipCloseReason reason);
	void                    *close_ctx;
} FluxTeachingTipData;

#endif
