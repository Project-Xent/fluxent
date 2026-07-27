/**
 * @file flux_expander_internal.h
 * @brief Private shared runtime for the FluxExpander control (WinUI 3 Expander).
 */
#ifndef FLUX_EXPANDER_INTERNAL_H
#define FLUX_EXPANDER_INTERNAL_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>
#include "store/flux_node_data.h"


typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

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
	XentNodeId  root;    /**< Layout-only 2-row grid. */
	XentNodeId  header;  /**< Toggle-button header node (arbitrary content host). */
	XentNodeId  header_text;
	XentNodeId  content; /**< Sliding content panel the caller fills. */
	XentCtx    *ctx;
	FluxStore  *store;
	FluxWindow *window;                                 /**< For repaint requests while animating. */

	float       width;                                  /**< Explicit width, or NAN for parent-constrained width. */
	float       content_height;                         /**< Explicit body height, or NAN for intrinsic height. */

	bool        expanded;                               /**< Logical expanded state (chevron/corner). */
	bool        anim_active;                            /**< A slide animation is in progress. */
	bool        anim_expanding;                         /**< Direction of the active animation. */
	DWORD       anim_start;                             /**< GetTickCount at animation start. */

	void        (*on_toggle)(void *ctx, bool expanded); /**< Invoked on expand/collapse. */
	void       *on_toggle_ctx;
} FluxExpanderData;


#endif
