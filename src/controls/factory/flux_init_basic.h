#ifndef FLUX_INIT_BASIC_H
#define FLUX_INIT_BASIC_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxButtonDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	void        (*on_click)(void *);
	void       *userdata;
} FluxButtonDesc;

typedef struct FluxToggleDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	bool        checked;
	void        (*on_change)(void *, FluxCheckState);
	void       *userdata;
} FluxToggleDesc;

typedef struct FluxTextDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *content;
	float       font_size;
	FluxText   *text;
} FluxTextDesc;

typedef struct FluxHyperlinkDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	char const *url;
	void        (*on_click)(void *);
	void       *userdata;
} FluxHyperlinkDesc;

typedef struct FluxDropDownButtonDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	char const *icon_name;
} FluxDropDownButtonDesc;

typedef struct FluxSplitButtonDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	char const *icon_name;
	void        (*on_click)(void *);
	void       *userdata;
} FluxSplitButtonDesc;

typedef struct FluxToggleSplitButtonDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	char const *icon_name;
	bool        checked;
	void        (*on_toggle)(void *ctx, bool checked);
	void       *on_toggle_ctx;
	void        (*on_click)(void *ctx);
	void       *on_click_ctx;
} FluxToggleSplitButtonDesc;

XentNodeId flux_button_init(FluxButtonDesc const *info);

XentNodeId flux_text_init(FluxTextDesc const *info);

XentNodeId flux_checkbox_init(FluxToggleDesc const *info);

XentNodeId flux_radio_init(FluxToggleDesc const *info);

XentNodeId flux_switch_init(FluxToggleDesc const *info);

XentNodeId flux_link_init(FluxHyperlinkDesc const *info);

XentNodeId flux_repeat_init(FluxButtonDesc const *info);
void       flux_repeat_retarget(FluxStore *store, XentNodeId node);

XentNodeId flux_dropdown_init(FluxDropDownButtonDesc const *info);

XentNodeId flux_split_init(FluxSplitButtonDesc const *info);

XentNodeId flux_togglesplit_init(FluxToggleSplitButtonDesc const *info);

void       flux_togglesplit_set_checked(FluxStore *store, XentNodeId id, bool checked);

typedef struct FluxRadioItem {
	char const *label;
	bool        disabled;
} FluxRadioItem;

typedef struct FluxRadioButtonsDesc {
	XentCtx             *ctx;
	FluxStore           *store;
	XentNodeId           parent;
	FluxInput           *input;
	char const          *header;
	FluxRadioItem const *items;
	int                  item_count;
	int                  selected;
	int                  max_columns;
	bool                 disabled;
	void                 (*on_select)(void *ctx, int index);
	void                *userdata;
} FluxRadioButtonsDesc;

XentNodeId flux_radiogroup_init(FluxRadioButtonsDesc const *info);

void       flux_radiogroup_set_selected(FluxStore *store, XentNodeId id, int index);

void       flux_radiogroup_set_enabled(FluxStore *store, XentNodeId id, bool enabled);

void       flux_radiogroup_sync_children(FluxStore *store, XentNodeId id);

#endif
