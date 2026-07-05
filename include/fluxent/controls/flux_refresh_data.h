/**
 * @file flux_refresh_data.h
 * @brief Data structures for the FluxRefresh control (WinUI 3 RefreshContainer
 * + RefreshVisualizer / PullToRefresh).
 *
 * WinUI composes a RefreshContainer (a ContentControl wrapping a scroller) with
 * a RefreshVisualizer (the spinner surface). Over-pulling past the scroll edge
 * drives the visualizer through Idle/Peeking/Interacting/Pending/Refreshing; the
 * single circular-arrow Refresh glyph rotates with the pull distance and then
 * spins continuously while refreshing. Crossing the execution threshold and
 * releasing fires the refresh request; the app ends it by clearing the
 * refreshing flag (the fluxent analogue of completing the WinRT Deferral).
 */
#ifndef FLUX_REFRESH_DATA_H
#define FLUX_REFRESH_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore FluxNodeStore;
typedef struct FluxWindow    FluxWindow;

/**
 * @brief Pull edge / gesture direction (WinUI RefreshPullDirection).
 *
 * Values match XtkPullDirection so the declarative desc's int passes straight
 * through. TopToBottom (pull down from the top edge) is the default.
 */
typedef enum FluxPullDirection
{
	FLUX_PULL_TOP_TO_BOTTOM = 0, /**< Default: pull down from the top edge. */
	FLUX_PULL_LEFT_TO_RIGHT,     /**< Pull right from the left edge. */
	FLUX_PULL_RIGHT_TO_LEFT,     /**< Pull left from the right edge. */
	FLUX_PULL_BOTTOM_TO_TOP,     /**< Pull up from the bottom edge. */
} FluxPullDirection;

/**
 * @brief RefreshVisualizer state machine (RefreshVisualizerState).
 */
typedef enum FluxRefreshState
{
	FLUX_REFRESH_IDLE = 0,   /**< At rest; glyph at min opacity. */
	FLUX_REFRESH_PEEKING,    /**< Scroller not at its pull edge; pulls only peek. */
	FLUX_REFRESH_INTERACTING,/**< Armed pull below the execution threshold. */
	FLUX_REFRESH_PENDING,    /**< Pull crossed the threshold; release will refresh. */
	FLUX_REFRESH_REFRESHING, /**< Refresh in progress; glyph spins continuously. */
} FluxRefreshState;

/** @brief WinUI RefreshVisualizer metric defaults. */
#define FLUX_REFRESH_INDICATOR_SIZE  30.0f  /**< SymbolIcon(Refresh) box, 30x30. */
#define FLUX_REFRESH_PULL_DIMENSION  100.0f /**< Default visualizer size (DEFAULT_PULL_DIMENSION_SIZE). */
#define FLUX_REFRESH_EXECUTION_RATIO 0.8f   /**< DEFAULT_EXECUTION_RATIO. */
#define FLUX_REFRESH_MIN_OPACITY     0.4f   /**< MINIMUM_INDICATOR_OPACITY. */
#define FLUX_REFRESH_PARALLAX_RATIO  0.5f   /**< PARALLAX_POSITION_RATIO. */
#define FLUX_REFRESH_OVERPAN_RATIO   0.4f   /**< REFRESH_VISUALIZER_OVERPAN_RATIO. */
#define FLUX_REFRESH_OFFSET_THRESH   1.0f   /**< INITIAL_OFFSET_THRESHOLD (DIP). */
#define FLUX_REFRESH_SPIN_MS         500.0f /**< Continuous spin period (loop Forever, Linear). */
#define FLUX_REFRESH_POP_MS          300.0f /**< Pending scale pop (1.0->1.5->1.0). */

/**
 * @brief Shared runtime for a RefreshContainer.
 *
 * The root (FLUX_CONTROL_REFRESH) owns and frees this struct; it wraps a single
 * FLUX_CONTROL_SCROLL child that hosts the scrollable content. The state machine
 * is driven either by a best-effort over-pull gesture (pointer deltas taken at
 * the scroll edge) or programmatically via flux_refresh_request_refresh /
 * flux_refresh_set_refreshing.
 */
typedef struct FluxRefreshData {
	XentNodeId        root;            /**< FLUX_CONTROL_REFRESH root (owner). */
	XentNodeId        scroll_child;    /**< FLUX_CONTROL_SCROLL that hosts the content. */
	XentContext      *ctx;
	FluxNodeStore    *store;
	FluxWindow       *window;          /**< For repaint requests while animating; may be NULL. */

	FluxPullDirection direction;       /**< Pull edge / rotation origin. */
	float             visualizer_size; /**< Pull dimension in DIPs (100 default). */
	float             threshold_ratio; /**< Execution ratio (0.8 default). */
	float             min_opacity;     /**< Idle/Interacting glyph opacity (0.4). */
	float             overpan_ratio;   /**< Content follow clamp (0.4). */
	float             start_angle;     /**< Base glyph rotation (rad) from direction. */

	/* Live gesture. */
	int               state;                 /**< FluxRefreshState. */
	float             interaction_ratio;     /**< Pull distance / visualizer size, clamped [0,1]. */
	bool              interacting_for_refresh;/**< Armed touch interaction in progress. */
	bool              was_at_zero;            /**< Previous ratio was 0 (frame-jump guard). */
	float             pull_origin;            /**< Pointer coordinate at press (pull axis). */
	bool              pull_tracking;          /**< A press is being tracked for over-pull. */

	/* Animation clocks. */
	DWORD             spin_start_tick;        /**< GetTickCount when Refreshing began. */
	DWORD             pop_start_tick;         /**< GetTickCount when Pending pop began. */
	bool              anim_registered;        /**< Registered with the shared anim driver. */

	void            (*on_refresh)(void *ud, int direction); /**< Fired when a pull refresh starts. */
	void             *userdata;
} FluxRefreshData;

#ifdef __cplusplus
}
#endif

#endif
