#ifndef FLUX_CONTROL_UIA_H
#define FLUX_CONTROL_UIA_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct FluxUiaTextView;

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
	FLUX_UIA_SEL_EACH     = 1u << 2,
};

int         flux_expander_uia_expand(FluxStore *store, XentNodeId node);
void        flux_expander_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_combo_uia_expand(FluxStore *store, XentNodeId node);
void        flux_combo_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_split_uia_expand(FluxStore *store, XentNodeId node);
void        flux_split_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_dropdown_uia_expand(FluxStore *store, XentNodeId node);
void        flux_dropdown_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_menubar_uia_expand(FluxStore *store, XentNodeId node);
void        flux_menubar_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_tree_uia_expand(FluxStore *store, XentNodeId node);
void        flux_tree_uia_toggle(FluxStore *store, XentNodeId node);
int         flux_nav_uia_expand(FluxStore *store, XentNodeId node);
void        flux_nav_uia_toggle(FluxStore *store, XentNodeId node);
unsigned    flux_nav_uia_live(FluxStore *store, XentNodeId node);
int         flux_cmdbar_uia_expand(FluxStore *store, XentNodeId node);
void        flux_cmdbar_uia_toggle(FluxStore *store, XentNodeId node);

bool        flux_list_uia_selected(FluxStore *store, XentNodeId node);
bool        flux_list_uia_select(FluxStore *store, XentNodeId node, int op);
unsigned    flux_list_uia_mode(FluxStore *store, XentNodeId node);
int         flux_list_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_list_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
bool        flux_list_uia_subselected(FluxStore *store, XentNodeId node, int sub);
bool        flux_list_uia_subselect(FluxStore *store, XentNodeId node, int sub, int op);
void        flux_list_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
void        flux_listitem_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
int         flux_list_uia_count(FluxStore *store, XentNodeId node);
bool        flux_slider_uia_range(FluxStore *store, XentNodeId id, double *value, double *min, double *max);
void        flux_slider_uia_set(FluxStore *store, XentNodeId id, double value);
bool        flux_text_uia_view(FluxStore *store, XentNodeId node, struct FluxUiaTextView *out);
void        flux_text_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
void        flux_text_uia_select(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end);
bool        flux_textbox_uia_view(FluxStore *store, XentNodeId node, struct FluxUiaTextView *out);
void        flux_textbox_uia_select(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end);
void        flux_textbox_uia_reveal(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end);
void        flux_list_uia_flatten(FluxStore *store, XentNodeId list);
XentNodeId  flux_list_uia_scroller(FluxStore *store, XentNodeId node);
void        flux_uia_name_copy(char const *src, char *out, size_t size);
int         flux_looping_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
void        flux_looping_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
void        flux_loopitem_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_looping_uia_subselected(FluxStore *store, XentNodeId node, int sub);
bool        flux_looping_uia_subselect(FluxStore *store, XentNodeId node, int sub, int op);
void        flux_looping_uia_scrollto(FluxStore *store, XentNodeId node, int sub);
int         flux_looping_uia_count(FluxStore *store, XentNodeId node);
int         flux_looping_uia_index(FluxStore *store, XentNodeId node, XentNodeId child);
XentNodeId  flux_looping_uia_realized(FluxStore *store, XentNodeId node, int sub);
bool        flux_looping_uia_scrollinfo(FluxStore *store, XentNodeId node, double *percent, double *view);
void        flux_looping_uia_scrollby(FluxStore *store, XentNodeId node, int items);
bool        flux_looping_uia_scrollset(FluxStore *store, XentNodeId node, double percent);
int         flux_list_uia_index(FluxStore *store, XentNodeId node, XentNodeId child);
XentNodeId  flux_list_uia_realized(FluxStore *store, XentNodeId node, int sub);
bool        flux_tree_uia_selected(FluxStore *store, XentNodeId node);
bool        flux_tree_uia_select(FluxStore *store, XentNodeId node, int op);
unsigned    flux_tree_uia_mode(FluxStore *store, XentNodeId node);
int         flux_tree_uia_count(FluxStore *store, XentNodeId node);
XentNodeId  flux_tree_uia_realized(FluxStore *store, XentNodeId node, int sub);
int         flux_tree_uia_index(FluxStore *store, XentNodeId node, XentNodeId child);
int         flux_tree_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
void        flux_tree_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
void        flux_tree_uia_realize(FluxStore *store, XentNodeId node, int sub);
XentNodeId  flux_tree_uia_scroller(FluxStore *store, XentNodeId node);
bool        flux_selector_uia_selected(FluxStore *store, XentNodeId node);
void        flux_selector_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_looping_uia_selected(FluxStore *store, XentNodeId node);
unsigned    flux_splitpane_uia_live(FluxStore *store, XentNodeId node);
void        flux_avatar_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_loopitem_uia_concealed(FluxStore *store, XentNodeId node);
bool        flux_splitview_uia_admits(FluxStore *store, XentNodeId node, XentNodeId child);
int         flux_titlebar_uia_heads(FluxStore *store, XentNodeId node);
int         flux_titlebar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_titlebar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void        flux_titlebar_uia_subname(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool        flux_titlebar_uia_enabled(FluxStore *store, XentNodeId node, int sub);
bool        flux_titlebar_uia_focusable(FluxStore *store, XentNodeId node, int sub);
bool        flux_titlebar_uia_focused(FluxStore *store, XentNodeId node, int sub);
void        flux_titlebar_uia_focus(FluxStore *store, XentNodeId node, int sub);
bool        flux_titlebar_uia_invoke(FluxStore *store, XentNodeId node, int sub);
void        flux_titlebar_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);

int         flux_slider_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_slider_uia_span(FluxStore *store, XentNodeId node, int *first, int *last);
bool        flux_slider_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void        flux_slider_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool        flux_slider_uia_focused(FluxStore *store, XentNodeId node, int sub);
void        flux_slider_uia_focus(FluxStore *store, XentNodeId node, int sub);
int         flux_slider_uia_orient(FluxStore *store, XentNodeId node);
void        flux_slider_uia_steps(FluxStore *store, XentNodeId node, double *small_step, double *large_step);
void        flux_scroll_bar_uia_steps(FluxStore *store, XentNodeId id, double *small_step, double *large_step);
void        flux_progress_uia_steps(FluxStore *store, XentNodeId node, double *small_step, double *large_step);
void        flux_ring_uia_steps(FluxStore *store, XentNodeId node, double *small_step, double *large_step);
int         flux_scroll_bar_uia_orient(FluxStore *store, XentNodeId id);
bool        flux_listitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
unsigned    flux_list_uia_live(FluxStore *store, XentNodeId node);
unsigned    flux_listitem_uia_live(FluxStore *store, XentNodeId node);
bool        flux_listitem_uia_invoke(FluxStore *store, XentNodeId node);
unsigned    flux_tree_uia_live(FluxStore *store, XentNodeId node);
unsigned    flux_treeitem_uia_live(FluxStore *store, XentNodeId node);
bool        flux_treeitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_comboitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_suggestitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_navitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_tabitem_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_appbar_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
bool        flux_appbar_uia_concealed(FluxStore *store, XentNodeId node);
bool        flux_cmdmore_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level);
unsigned    flux_cmdbar_uia_live(FluxStore *store, XentNodeId node);
void        flux_cmdmore_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_infobar_uia_concealed(FluxStore *store, XentNodeId node);
XentNodeId  flux_picker_uia_delegate(FluxStore *store, XentNodeId node);
void        flux_picker_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_colitem_uia_invoke(FluxStore *store, XentNodeId node);
unsigned    flux_colitem_uia_live(FluxStore *store, XentNodeId node);
int         flux_infobar_uia_heads(FluxStore *store, XentNodeId node);
int         flux_infobar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_infobar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void        flux_infobar_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool        flux_infobar_uia_focused(FluxStore *store, XentNodeId node, int sub);
bool        flux_infobar_uia_invoke(FluxStore *store, XentNodeId node, int sub);
bool        flux_calendar_uia_text(FluxStore *store, XentNodeId node, int sub, struct FluxUiaTextView *out);
bool        flux_infobar_uia_text(FluxStore *store, XentNodeId node, int sub, struct FluxUiaTextView *out);
int         flux_number_uia_heads(FluxStore *store, XentNodeId node);
int         flux_number_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_number_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void        flux_number_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool        flux_number_uia_focused(FluxStore *store, XentNodeId node, int sub);
bool        flux_number_uia_enabled(FluxStore *store, XentNodeId node, int sub);
bool        flux_number_uia_invoke(FluxStore *store, XentNodeId node, int sub);
void        flux_number_uia_settext(FluxStore *store, XentNodeId node, char const *text);
void        flux_number_uia_steps(FluxStore *store, XentNodeId node, double *small_step, double *large_step);
bool        flux_suggest_uia_invoke(FluxStore *store, XentNodeId node);
XentNodeId  flux_suggest_uia_controls(FluxStore *store, XentNodeId box);
void        flux_suggest_uia_boxname(FluxStore *store, XentNodeId box, char *out, size_t size);
void        flux_textbox_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
void        flux_combo_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
bool        flux_suggestitem_uia_invoke(FluxStore *store, XentNodeId node);
void        flux_pips_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size);
char const *flux_rating_uia_value(FluxStore *store, XentNodeId id);
void        flux_rating_uia_settext(FluxStore *store, XentNodeId id, char const *text);
void        flux_rating_uia_setrange(FluxStore *store, XentNodeId id, double value);
bool        flux_rating_uia_range(FluxStore *store, XentNodeId id, double *value, double *lo, double *hi);
bool        flux_rating_uia_readonly(FluxStore *store, XentNodeId id);
void        flux_rating_uia_steps(FluxStore *store, XentNodeId id, double *small_step, double *large_step);
XentNodeId  flux_colorpicker_uia_delegate(FluxStore *store, XentNodeId node);

int         flux_calendar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns);
bool        flux_calendar_uia_span(FluxStore *store, XentNodeId node, int *first, int *last);
bool        flux_calendar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect);
void        flux_calendar_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size);
bool        flux_calendar_uia_focused(FluxStore *store, XentNodeId node, int sub);
void        flux_calendar_uia_focus(FluxStore *store, XentNodeId node, int sub);
bool        flux_calendar_uia_enabled(FluxStore *store, XentNodeId node, int sub);
bool        flux_calendar_uia_invoke(FluxStore *store, XentNodeId node, int sub);
bool        flux_calendar_uia_selected(FluxStore *store, XentNodeId node, int sub);
bool        flux_calendar_uia_select(FluxStore *store, XentNodeId node, int sub, int op);
unsigned    flux_calendar_uia_mode(FluxStore *store, XentNodeId node);
int         flux_calendar_uia_heads(FluxStore *store, XentNodeId node);
bool        flux_calendar_uia_grid(FluxStore *store, XentNodeId node, int *rows, int *cols);
bool        flux_calendar_uia_cell(FluxStore *store, XentNodeId node, int sub, int *row, int *col);
int         flux_calendar_uia_rowhead(FluxStore *store, XentNodeId node, int sub);
void        flux_calendar_uia_scroll(FluxStore *store, XentNodeId node, int sub);
char const *flux_calendar_uia_value(FluxStore *store, XentNodeId node);
bool        flux_comboitem_uia_selected(FluxStore *store, XentNodeId node);
bool        flux_comboitem_uia_select(FluxStore *store, XentNodeId node, int op);
bool        flux_suggestitem_uia_selected(FluxStore *store, XentNodeId node);
bool        flux_suggestitem_uia_select(FluxStore *store, XentNodeId node, int op);
bool        flux_menu_wheel(FluxStore *store, XentNodeId node, float delta, float *remaining);

#endif
