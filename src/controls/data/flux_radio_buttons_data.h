/**
 * @file flux_radiogroup_data.h
 * @brief Data for FluxRadioButtons — the WinUI RadioButtons grouping control.
 *
 * A container that lays out child FLUX_CONTROL_RADIO items in a column-major
 * uniform grid (MaxColumns), enforces single selection across the group, and
 * runs the group keyboard model (arrows move focus AND on_select unless Ctrl is
 * held; one tab stop). The items are ordinary radios created via
 * individual radio leaves; this owns only the grouping/selection/keyboard layer.
 */
#ifndef FLUX_RADIO_BUTTONS_DATA_H
#define FLUX_RADIO_BUTTONS_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <xent/xent.h>


typedef struct FluxInput            FluxInput;
typedef struct FluxRadioButtonsData FluxRadioButtonsData;

/** @brief Per-child click/key context: which group + index. */
typedef struct FluxRadioItemCtx {
	FluxRadioButtonsData *group;
	int                   index;
} FluxRadioItemCtx;

/** @brief Retained state for a FLUX_CONTROL_RADIO_BUTTONS group. */
typedef struct FluxRadioButtonsData {
	XentCtx          *ctx;
	FluxStore        *store;
	FluxInput        *input;
	XentNodeId        root;

	int               count;
	int               selected; /**< Selected item index, or -1. */
	int               columns;  /**< Requested MaxColumns (>=1). */
	bool              disabled;

	XentNodeId       *items;    /**< count child radio nodes (owned array). */
	FluxRadioItemCtx *item_ctx; /**< count per-child callback contexts (owned array). */

	void              (*on_select)(void *ctx, int index);
	void             *on_select_ctx;
} FluxRadioButtonsData;


#endif
