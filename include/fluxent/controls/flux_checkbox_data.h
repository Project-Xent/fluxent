/**
 * @file flux_checkbox_data.h
 * @brief Data structure for FluxCheckbox control.
 */
#ifndef FLUX_CHECKBOX_DATA_H
#define FLUX_CHECKBOX_DATA_H

#include "../flux_types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   Checkbox state enum
   ═══════════════════════════════════════════════════════════════════════ */

typedef enum FluxCheckState
{
	FLUX_CHECK_UNCHECKED,
	FLUX_CHECK_CHECKED,
	FLUX_CHECK_INDETERMINATE, /**< Mixed state (e.g., partial selection) */
} FluxCheckState;

/* ═══════════════════════════════════════════════════════════════════════
   FluxCheckboxData — Control-specific data for FluxCheckbox
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a checkbox control.
 *
 * Supports three states: unchecked, checked, and indeterminate.
 * on_change is called when the user toggles the checkbox.
 */
typedef struct FluxCheckboxData {
	char const    *label;       /**< Checkbox label text (UTF-8) */
	FluxColor      check_color; /**< Checkmark / fill color */
	FluxColor      box_color;   /**< Border / box color */
	FluxCheckState state;       /**< Current check state */
	bool           enabled;     /**< Is checkbox interactive? */
	void           (*on_change)(void *ctx, FluxCheckState new_state);
	void          *on_change_ctx;
} FluxCheckboxData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_CHECKBOX_DATA_H */
