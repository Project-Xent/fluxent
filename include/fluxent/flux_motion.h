#ifndef FLUX_MOTION_H
#define FLUX_MOTION_H

#include <fluxent/flux_types.h>
#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore FluxStore;

typedef enum FluxMotionPropMask
{
	FLUX_MOTION_PROP_OPACITY       = 1u << 0,
	FLUX_MOTION_PROP_TRANSLATE_X   = 1u << 1,
	FLUX_MOTION_PROP_TRANSLATE_Y   = 1u << 2,
	FLUX_MOTION_PROP_SCALE_X       = 1u << 3,
	FLUX_MOTION_PROP_SCALE_Y       = 1u << 4,
	FLUX_MOTION_PROP_ROTATION      = 1u << 5,
	FLUX_MOTION_PROP_BACKGROUND    = 1u << 6,
	FLUX_MOTION_PROP_BORDER_COLOR  = 1u << 7,
	FLUX_MOTION_PROP_CORNER_RADIUS = 1u << 8,
} FluxMotionPropMask;

typedef enum FluxThemeTransitionKind
{
	FLUX_THEME_TRANSITION_NONE = 0,
	FLUX_THEME_TRANSITION_ENTRANCE,
	FLUX_THEME_TRANSITION_ADD_DELETE,
	FLUX_THEME_TRANSITION_CONTENT,
	FLUX_THEME_TRANSITION_REORDER,
	FLUX_THEME_TRANSITION_POPUP,
} FluxThemeTransitionKind;

typedef enum FluxNavTransitionKind
{
	FLUX_NAV_TRANSITION_ENTRANCE = 0,
	FLUX_NAV_TRANSITION_SLIDE_RIGHT,
	FLUX_NAV_TRANSITION_SLIDE_LEFT,
	FLUX_NAV_TRANSITION_SLIDE_BOTTOM,
	FLUX_NAV_TRANSITION_DRILL_IN,
	FLUX_NAV_TRANSITION_SUPPRESS,
} FluxNavTransitionKind;

typedef enum FluxConnectedKind
{
	FLUX_CONNECTED_GRAVITY = 0,
	FLUX_CONNECTED_DIRECT  = 1,
	FLUX_CONNECTED_BASIC   = 2,
} FluxConnectedKind;

typedef struct FluxImplicitCfg {
	uint32_t enabled_mask;
	double   opacity_duration;
	double   transform_duration;
	double   brush_duration;
} FluxImplicitCfg;

typedef struct FluxConnectedCfg {
	FluxConnectedKind kind;
	bool              is_shadow_enabled;
	float             dip_x;
	float             dip_y;
	double            duration;
} FluxConnectedCfg;

bool flux_motion_tick(FluxStore *store, XentCtx *ctx, double now_sec, bool animations_enabled);

void flux_motion_set_implicit(FluxStore *store, XentNodeId node, FluxImplicitCfg const *cfg);
void flux_motion_set_theme_transition(FluxStore *store, XentNodeId node, FluxThemeTransitionKind kind);

void flux_motion_set_opacity(FluxStore *store, XentNodeId node, float value);
void flux_motion_set_translation(FluxStore *store, XentNodeId node, float x, float y);
void flux_motion_set_scale(FluxStore *store, XentNodeId node, float sx, float sy);
void flux_motion_set_rotation(FluxStore *store, XentNodeId node, float degrees);
void flux_motion_set_pivot(FluxStore *store, XentNodeId node, float px, float py);

bool flux_motion_connected_prepare(
  FluxStore *store, XentCtx *ctx, char const *key, XentNodeId source, FluxConnectedCfg const *cfg
);
bool flux_motion_connected_start(
  FluxStore *store, XentCtx *ctx, char const *key, XentNodeId dest, XentNodeId const *coords, uint32_t coord_count
);
void flux_motion_connected_cancel(FluxStore *store, char const *key);

#endif
