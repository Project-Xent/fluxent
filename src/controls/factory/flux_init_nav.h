#ifndef FLUX_INIT_NAV_H
#define FLUX_INIT_NAV_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxNavViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	FluxText   *text;
	FluxTheme  *theme;
	float       width;
	float       height;
	int         mode;
	bool        adaptive;
	bool        window_title_bar;
	char const *app_title;
	void        (*on_selection_changed)(void *ctx, int index);
	void       *userdata;
	XentNodeId *out_content;
} FluxNavViewDesc;

XentNodeId flux_nav_init(FluxNavViewDesc const *info);

bool       flux_nav_set_chrome(FluxStore *store, XentNodeId nav, bool window_title_bar, char const *app_title);

bool       flux_nav_reset_items(FluxStore *store, XentNodeId nav);

int        flux_nav_add_item(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label);

int        flux_nav_addfooter(FluxStore *store, XentNodeId nav, char const *icon_name, char const *label);

void       flux_nav_select(FluxStore *store, XentNodeId nav, int index);

void       flux_nav_set_selected(FluxStore *store, XentNodeId nav, int index);

XentNodeId flux_nav_content_node(FluxStore *store, XentNodeId nav);

int        flux_nav_add_separator(FluxStore *store, XentNodeId nav);

int        flux_nav_add_header(FluxStore *store, XentNodeId nav, char const *label);

int flux_nav_addchild(FluxStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label);

typedef struct FluxTabViewDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	FluxText   *text;
	FluxInput  *input;
	int         width_mode;
	int         close_overlay;
	bool        disable_reorder;
	void        (*on_selection_changed)(void *ctx, int index);
	void       *selection_ctx;
	bool        (*on_close_requested)(void *ctx, int index);
	void       *close_requested_ctx;
	void        (*on_tab_closed)(void *ctx, int index);
	void       *tab_closed_ctx;
	void        (*on_add_tab)(void *ctx);
	void       *add_tab_ctx;
} FluxTabViewDesc;

XentNodeId flux_tabs_init(FluxTabViewDesc const *info);

int        flux_tabs_add_tab(
  FluxStore *store, XentNodeId tabview, char const *icon_name, char const *label, XentNodeId *out_content
);

bool flux_tabs_set_items(
  FluxStore *store, XentNodeId tabview, char const *const *icons, char const *const *labels, int count
);
bool flux_tabs_mount_items(
  FluxStore *store, XentNodeId tabview, char const *const *icons, char const *const *labels, int count, int selected,
  bool add_visible
);

XentNodeId flux_tabs_content_node(FluxStore *store, XentNodeId tabview);

void       flux_tabs_sync_layout(FluxStore *store, XentNodeId tabview);

void       flux_tabs_select(FluxStore *store, XentNodeId tabview, int index);

void       flux_tabs_setaddvisible(FluxStore *store, XentNodeId tabview, bool visible);

typedef struct FluxTitleBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	char const *title;
	char const *subtitle;
	char const *icon;
	bool        show_back;
	bool        back_disabled;
	bool        show_pane_toggle;
	bool        integrate_window;
	void        (*on_back)(void *ctx);
	void       *back_ctx;
	void        (*on_pane_toggle)(void *ctx);
	void       *pane_toggle_ctx;
} FluxTitleBarDesc;

XentNodeId flux_titlebar_init(FluxTitleBarDesc const *info);

void       flux_titlebar_sync(XentCtx *ctx, XentNodeId node, struct FluxNodeData *nd);

void       flux_titlebar_set_title(FluxStore *store, XentNodeId id, char const *title, char const *subtitle);

void       flux_titlebar_set_back(FluxStore *store, XentNodeId id, bool show, bool disabled);

void       flux_titlebar_settoggle(FluxStore *store, XentNodeId id, bool show);

void       flux_titlebar_set_icon(FluxStore *store, XentNodeId id, char const *icon);
void       flux_titlebar_set_window_integration(FluxStore *store, XentNodeId id, bool integrate);

typedef struct FluxMenuBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxWindow *window;
	FluxText   *text;
	FluxTheme  *theme;
} FluxMenuBarDesc;

XentNodeId flux_menubar_init(FluxMenuBarDesc const *info);
XentNodeId flux_menubar_content_node(FluxStore *store, XentNodeId bar);

FluxMenu  *flux_menubar_add_menu(FluxStore *store, XentNodeId bar, char const *title);
void       flux_menubar_clear_menus(FluxStore *store, XentNodeId bar);
bool       flux_menubar_add_menu_ref(FluxStore *store, XentNodeId bar, char const *title, FluxMenu *flyout);

typedef struct FluxBreadcrumbBarDesc {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         parent;
	FluxWindow        *window;
	FluxText          *text;
	FluxTheme         *theme;
	FluxInput         *input;
	char const *const *items;
	int                item_count;
	bool               disabled;
	void               (*on_item_clicked)(void *ctx, int index);
	void              *userdata;
} FluxBreadcrumbBarDesc;

XentNodeId flux_breadcrumb_init(FluxBreadcrumbBarDesc const *info);

void       flux_breadcrumb_set_items(FluxStore *store, XentNodeId bar, char const *const *items, int count);

void       flux_breadcrumb_set_disabled(FluxStore *store, XentNodeId bar, bool disabled);

void       flux_breadcrumb_sync(XentCtx *ctx, XentNodeId bar, struct FluxNodeData *nd);

typedef struct FluxSelectorBarDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxInput  *input;
	FluxWindow *window;
	int         item_count;
	int         selected;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxSelectorBarDesc;

XentNodeId flux_selector_init(FluxSelectorBarDesc const *info);

void       flux_selector_set_item(
  FluxStore *store, XentNodeId bar, int i, char const *text, uint32_t icon_glyph, bool disabled
);

void flux_selector_set_items(FluxStore *store, XentNodeId bar, XtkSelectorItem const *items, int count, int selected);

void flux_selector_set_selected(FluxStore *store, XentNodeId bar, int index);

typedef struct FluxPagerDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	int         count;
	int         selected;
	int         display_mode;
	int         first_button;
	int         prev_button;
	int         next_button;
	int         last_button;
	char const *prefix;
	char const *suffix;
	void        (*on_select)(void *ctx, int index);
	void       *userdata;
} FluxPagerDesc;

XentNodeId flux_pager_init(FluxPagerDesc const *info);

void       flux_pager_configure(FluxStore *store, XentNodeId id, int count, int selected, int display_mode);

void       flux_pager_set_selected(FluxStore *store, XentNodeId id, int index);

void       flux_pager_setbuttons(FluxStore *store, XentNodeId id, int first, int prev, int next, int last);
void       flux_pager_settext(FluxStore *store, XentNodeId id, char const *prefix, char const *suffix);

typedef struct FluxPipsPagerDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	bool       vertical;
	void       (*on_select)(void *ctx, int index);
	void      *userdata;
} FluxPipsPagerDesc;

XentNodeId flux_pips_init(FluxPipsPagerDesc const *info);

void flux_pips_configure(FluxStore *store, XentNodeId pips, int count, int selected, int max_visible, int nav_vis);
void flux_pips_set_vertical(FluxStore *store, XentNodeId pips, bool vertical);
void flux_pips_set_count(FluxStore *store, XentNodeId pips, int count);
void flux_pips_set_selected(FluxStore *store, XentNodeId pips, int selected);
void flux_pips_set_max_visible(FluxStore *store, XentNodeId pips, int max_visible);
void flux_pips_set_nav_visibility(FluxStore *store, XentNodeId pips, int nav_vis);

#endif
