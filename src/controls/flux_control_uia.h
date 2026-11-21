#ifndef FLUX_CONTROL_UIA_H
#define FLUX_CONTROL_UIA_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <stddef.h>

enum
{
	FLUX_UIA_SEL_SELECT = 0,
	FLUX_UIA_SEL_ADD    = 1,
	FLUX_UIA_SEL_REMOVE = 2,
};

enum
{
	FLUX_UIA_SEL_MULTIPLE = 1u << 0,
	FLUX_UIA_SEL_REQUIRED = 1u << 1,
};

int      flux_expander_uia_expand(FluxStore *store, XentNodeId node);
void     flux_expander_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_combo_uia_expand(FluxStore *store, XentNodeId node);
void     flux_combo_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_split_uia_expand(FluxStore *store, XentNodeId node);
void     flux_split_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_dropdown_uia_expand(FluxStore *store, XentNodeId node);
void     flux_dropdown_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_menubar_uia_expand(FluxStore *store, XentNodeId node);
void     flux_menubar_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_tree_uia_expand(FluxStore *store, XentNodeId node);
void     flux_tree_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_nav_uia_expand(FluxStore *store, XentNodeId node);
void     flux_nav_uia_toggle(FluxStore *store, XentNodeId node);
int      flux_cmdbar_uia_expand(FluxStore *store, XentNodeId node);
void     flux_cmdbar_uia_toggle(FluxStore *store, XentNodeId node);

bool     flux_list_uia_selected(FluxStore *store, XentNodeId node);
bool     flux_list_uia_select(FluxStore *store, XentNodeId node, int op);
unsigned flux_list_uia_mode(FluxStore *store, XentNodeId node);
bool     flux_tree_uia_selected(FluxStore *store, XentNodeId node);
bool     flux_tree_uia_select(FluxStore *store, XentNodeId node, int op);
unsigned flux_tree_uia_mode(FluxStore *store, XentNodeId node);
bool     flux_selector_uia_selected(FluxStore *store, XentNodeId node);
bool     flux_looping_uia_selected(FluxStore *store, XentNodeId node);
unsigned flux_splitpane_uia_live(FluxStore *store, XentNodeId node);

int      flux_slider_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool     flux_slider_uia_span(FluxStore *store, XentNodeId node, int *first, int *last);
bool     flux_slider_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void     flux_slider_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool     flux_slider_uia_focused(FluxStore *store, XentNodeId node, int sub);

int      flux_calendar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool     flux_calendar_uia_span(FluxStore *store, XentNodeId node, int *first, int *last);
bool     flux_calendar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void     flux_calendar_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool     flux_calendar_uia_focused(FluxStore *store, XentNodeId node, int sub);
bool     flux_calendar_uia_invoke(FluxStore *store, XentNodeId node, int sub);
bool     flux_calendar_uia_selected(FluxStore *store, XentNodeId node, int sub);
bool     flux_calendar_uia_select(FluxStore *store, XentNodeId node, int sub, int op);
unsigned flux_calendar_uia_mode(FluxStore *store, XentNodeId node);
bool     flux_comboitem_uia_selected(FluxStore *store, XentNodeId node);
bool     flux_comboitem_uia_select(FluxStore *store, XentNodeId node, int op);
bool     flux_suggestitem_uia_selected(FluxStore *store, XentNodeId node);
bool     flux_suggestitem_uia_select(FluxStore *store, XentNodeId node, int op);
bool     flux_menu_wheel(FluxStore *store, XentNodeId node, float delta, float *remaining);

#endif
