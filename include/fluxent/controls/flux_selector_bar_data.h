/**
 * @file flux_selector_bar_data.h
 * @brief Data for FluxSelectorBar + items — a horizontal single-select pill
 * strip. Each item is a self-drawing FLUX_CONTROL_SELECTOR_BAR_ITEM node
 * (icon + text + an accent underline pill that grows 4→16 DIP on selection);
 * the bar is a flex row that owns selection, keyboard, and the pill tween.
 */
#ifndef FLUX_SELECTOR_BAR_DATA_H
#define FLUX_SELECTOR_BAR_DATA_H

#include "../flux_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxInput  FluxInput;
typedef struct FluxWindow FluxWindow;
typedef struct FluxSelectorBarData FluxSelectorBarData;

/** @brief Retained state for one FLUX_CONTROL_SELECTOR_BAR_ITEM. */
typedef struct FluxSelectorBarItemData {
	FluxSelectorBarData *bar;
	int                  index;
	char                *text;       /**< Owned label copy. */
	uint32_t             icon_glyph; /**< 0 = no icon. */
	bool                 disabled;
	bool                 selected;
	float                pill_t;     /**< Pill animation progress 0..1 (grow + fade in). */
} FluxSelectorBarItemData;

/** @brief Retained state for a FLUX_CONTROL_SELECTOR_BAR root. */
typedef struct FluxSelectorBarData {
	XentContext   *ctx;
	FluxNodeStore *store;
	FluxInput     *input;
	FluxWindow    *window;
	XentNodeId     root;

	int            count;
	int            selected; /**< Selected index, or -1. */

	XentNodeId               *items;     /**< count item nodes (owned array). */
	FluxSelectorBarItemData **item_data; /**< count borrowed item-data pointers. */

	unsigned long  anim_start; /**< Pill tween start tick (0 = idle). */
	int            anim_item;  /**< Item whose pill is animating, or -1. */

	void (*on_select)(void *ctx, int index);
	void  *on_select_ctx;
} FluxSelectorBarData;

#ifdef __cplusplus
}
#endif

#endif
