#ifndef FLUX_REFRESH_INTERNAL_H
#define FLUX_REFRESH_INTERNAL_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>

typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

typedef enum FluxPullDirection
{
	FLUX_PULL_TOP_TO_BOTTOM = 0,
	FLUX_PULL_LEFT_TO_RIGHT,
	FLUX_PULL_RIGHT_TO_LEFT,
	FLUX_PULL_BOTTOM_TO_TOP,
} FluxPullDirection;

typedef enum FluxRefreshState
{
	FLUX_REFRESH_IDLE = 0,
	FLUX_REFRESH_PEEKING,
	FLUX_REFRESH_INTERACTING,
	FLUX_REFRESH_PENDING,
	FLUX_REFRESH_REFRESHING,
} FluxRefreshState;

#define FLUX_REFRESH_INDICATOR_SIZE  30.0f
#define FLUX_REFRESH_PULL_DIMENSION  100.0f
#define FLUX_REFRESH_EXECUTION_RATIO 0.8f
#define FLUX_REFRESH_MIN_OPACITY     0.4f
#define FLUX_REFRESH_PARALLAX_RATIO  0.5f
#define FLUX_REFRESH_OVERPAN_RATIO   0.4f
#define FLUX_REFRESH_OFFSET_THRESH   1.0f
#define FLUX_REFRESH_SPIN_MS         500.0f
#define FLUX_REFRESH_POP_MS          300.0f
#define FLUX_REFRESH_SETTLE_MS       100.0f

typedef struct FluxRefreshData {
	XentNodeId        root;
	XentNodeId        scroll_child;
	XentCtx          *ctx;
	FluxStore        *store;
	FluxWindow       *window;

	FluxPullDirection direction;
	float             visualizer_size;
	float             threshold_ratio;
	float             min_opacity;
	float             overpan_ratio;
	float             start_angle;

	int               state;
	float             interaction_ratio;
	bool              interacting_for_refresh;
	bool              was_at_zero;
	bool              pull_tracking;
	float             content_offset;

	DWORD             spin_start_tick;
	DWORD             pop_start_tick;
	DWORD             offset_start_tick;
	float             offset_from;
	float             offset_to;
	bool              offset_animating;
	bool              anim_registered;

	void              (*on_refresh)(void *ud, int direction);
	void             *userdata;
} FluxRefreshData;

#endif
