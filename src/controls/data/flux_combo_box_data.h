#ifndef FLUX_COMBO_BOX_DATA_H
#define FLUX_COMBO_BOX_DATA_H

#include <stdbool.h>

typedef struct FluxComboBoxData {
	char const *const *items;
	int                item_count;
	int                selected_index;
	char const        *placeholder;
	bool               open;
	bool               editable;
	char const *const *edit_text;
	bool               edit_active;
	void               (*on_select)(void *ctx, int index);
	void               (*on_submit)(void *ctx, char const *text);
	void              *on_select_ctx;
} FluxComboBoxData;

#endif
