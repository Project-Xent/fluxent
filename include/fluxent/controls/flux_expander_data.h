/**
 * @file flux_expander_data.h
 * @brief Data structure for the FluxExpander control (WinUI 3 Expander).
 */
#ifndef FLUX_EXPANDER_DATA_H
#define FLUX_EXPANDER_DATA_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxNodeStore FluxNodeStore;
typedef struct FluxWindow    FluxWindow;

/**
 * @brief Shared runtime for an expander, mirroring the WinUI control tree:
 * a layout-only root grid (FLUX_CONTROL_EXPANDER), a toggle-button header
 * (FLUX_CONTROL_EXPANDER_HEADER) hosting arbitrary content, and a sliding
 * content panel (FLUX_CONTROL_EXPANDER_CONTENT). All three nodes reference
 * this same struct as their component_data; the root owns and frees it.
 *
 * @ref expanded drives the header chevron/corner state and flips immediately
 * on toggle; the content panel slides in/out over the WinUI durations and is
 * only detached once a collapse animation completes.
 */
typedef struct FluxExpanderData {
	XentNodeId     root;    /**< Layout-only 2-row grid. */
	XentNodeId     header;  /**< Toggle-button header node (arbitrary content host). */
	XentNodeId     content; /**< Sliding content panel the caller fills. */
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxWindow    *window;                                 /**< For repaint requests while animating. */

	float          width;                                  /**< Pinned control width in DIPs. */
	float          content_height;                         /**< Reserved content-region height when expanded. */

	bool           expanded;                               /**< Logical expanded state (chevron/corner). */
	bool           anim_active;                            /**< A slide animation is in progress. */
	bool           anim_expanding;                         /**< Direction of the active animation. */
	DWORD          anim_start;                             /**< GetTickCount at animation start. */

	void           (*on_toggle)(void *ctx, bool expanded); /**< Invoked on expand/collapse. */
	void          *on_toggle_ctx;
} FluxExpanderData;

#ifdef __cplusplus
}
#endif

#endif
