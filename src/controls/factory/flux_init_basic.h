/**
 * @file flux_init_basic.h
 * @brief Private Desc + init prototypes for the basic controls:
 * Button / Text / CheckBox / Radio / Switch / Hyperlink / RepeatButton /
 * DropDownButton / SplitButton / ToggleSplitButton / RadioButtons.
 */
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
	void        (*on_click)(void *); /**< Primary action (left zone). */
	void       *userdata;
} FluxSplitButtonDesc;

typedef struct FluxToggleSplitButtonDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	char const *label;
	char const *icon_name;
	bool        checked;
	void        (*on_toggle)(void *ctx, bool checked); /**< Primary-zone toggle. */
	void       *on_toggle_ctx;
	void        (*on_click)(void *ctx);
	void       *on_click_ctx;
} FluxToggleSplitButtonDesc;

/** @brief Create a button node. */
XentNodeId flux_button_init(FluxButtonDesc const *info);

/** @brief Create a plain text leaf node with the requested font size. */
XentNodeId flux_text_init(FluxTextDesc const *info);

/** @brief Create a checkbox node. */
XentNodeId flux_checkbox_init(FluxToggleDesc const *info);

/** @brief Create a radio button node. */
XentNodeId flux_radio_init(FluxToggleDesc const *info);

/** @brief Create a switch node. */
XentNodeId flux_switch_init(FluxToggleDesc const *info);

/** @brief Create a hyperlink button node. */
XentNodeId flux_link_init(FluxHyperlinkDesc const *info);

/** @brief Create a repeat button node. */
XentNodeId flux_repeat_init(FluxButtonDesc const *info);
void       flux_repeat_retarget(FluxStore *store, XentNodeId node);

/** @brief Create a drop-down button node. Bind a flyout with flux_prop_flyout_ex. */
XentNodeId flux_dropdown_init(FluxDropDownButtonDesc const *info);

/** @brief Create a split button node (primary action + dropdown chevron). */
XentNodeId flux_split_init(FluxSplitButtonDesc const *info);

/** @brief Create a ToggleSplitButton (SplitButton whose primary zone toggles IsChecked). */
XentNodeId flux_togglesplit_init(FluxToggleSplitButtonDesc const *info);

/** @brief Set the toggle state (controlled from the model each frame). */
void       flux_togglesplit_set_checked(FluxStore *store, XentNodeId id, bool checked);

/* -------------------------------------------------------------------------
 * RadioButtons (grouping control)
 * ---------------------------------------------------------------------- */

typedef struct FluxRadioItem {
	char const *label;
	bool        disabled;
} FluxRadioItem;

typedef struct FluxRadioButtonsDesc {
	XentCtx             *ctx;
	FluxStore           *store;
	XentNodeId           parent;
	FluxInput           *input; /**< Keyboard focus moves within the group. */
	char const          *header;
	FluxRadioItem const *items;
	int                  item_count;
	int                  selected;    /**< -1 = none. */
	int                  max_columns; /**< 0/1 = single column. */
	bool                 disabled;
	void                 (*on_select)(void *ctx, int index);
	void                *userdata;
} FluxRadioButtonsDesc;

/** @brief Create a RadioButtons group (single selection + group keyboard nav). */
XentNodeId flux_radiogroup_init(FluxRadioButtonsDesc const *info);

/** @brief Programmatically set the selected item (no callback). */
void       flux_radiogroup_set_selected(FluxStore *store, XentNodeId id, int index);

/** @brief Enable/disable the whole group. */
void       flux_radiogroup_set_enabled(FluxStore *store, XentNodeId id, bool enabled);

/** @brief Rebind group selection/keyboard to public RADIO children under the root. */
void       flux_radiogroup_sync_children(FluxStore *store, XentNodeId id);

#endif
