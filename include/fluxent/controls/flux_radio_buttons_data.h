/**
 * @file flux_radio_buttons_data.h
 * @brief Data for FluxRadioButtons — the WinUI RadioButtons grouping control.
 *
 * A container that lays out child FLUX_CONTROL_RADIO items in a column-major
 * uniform grid (MaxColumns), enforces single selection across the group, and
 * runs the group keyboard model (arrows move focus AND select unless Ctrl is
 * held; one tab stop). The items are ordinary radios created via
 * flux_create_radio; this owns only the grouping/selection/keyboard layer.
 */
#ifndef FLUX_RADIO_BUTTONS_DATA_H
#define FLUX_RADIO_BUTTONS_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxInput FluxInput;
typedef struct FluxRadioButtonsData FluxRadioButtonsData;

/** @brief Per-child click/key context: which group + index. */
typedef struct FluxRadioItemCtx {
	FluxRadioButtonsData *group;
	int                   index;
} FluxRadioItemCtx;

/** @brief Retained state for a FLUX_CONTROL_RADIO_BUTTONS group. */
typedef struct FluxRadioButtonsData {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxInput     *input;
	XentNodeId     root;

	int            count;
	int            selected; /**< Selected item index, or -1. */
	int            columns;  /**< Requested MaxColumns (>=1). */
	bool           disabled;

	XentNodeId       *items;    /**< count child radio nodes (owned array). */
	FluxRadioItemCtx *item_ctx; /**< count per-child callback contexts (owned array). */

	void (*on_select)(void *ctx, int index);
	void  *on_select_ctx;
} FluxRadioButtonsData;

#ifdef __cplusplus
}
#endif

#endif
