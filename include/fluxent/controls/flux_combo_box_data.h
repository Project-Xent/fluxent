/**
 * @file flux_combo_box_data.h
 * @brief Data structure for the FluxComboBox control (WinUI 3 ComboBox).
 */
#ifndef FLUX_COMBO_BOX_DATA_H
#define FLUX_COMBO_BOX_DATA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Model for a combo box: a closed box showing the selected item plus a
 * drop-down list of selectable string items.
 *
 * The @ref items array is borrowed (not copied) and must outlive the control.
 */
typedef struct FluxComboBoxData {
	char const *const *items;                              /**< Borrowed array of UTF-8 item strings. */
	int                item_count;                         /**< Number of items. */
	int                selected_index;                     /**< Selected item, or -1 for none. */
	char const        *placeholder;                        /**< Shown when nothing is selected. */
	bool               open;                               /**< Whether the drop-down is showing. */
	void               (*on_select)(void *ctx, int index); /**< Selection-changed callback. */
	void              *on_select_ctx;
} FluxComboBoxData;

#ifdef __cplusplus
}
#endif

#endif
