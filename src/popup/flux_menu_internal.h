#ifndef FLUX_MENU_INTERNAL_H
#define FLUX_MENU_INTERNAL_H

#include <cd2d.h>

#include "fluxent/flux_gfx.h"
#include "fluxent/flux_menu.h"
#include "fluxent/flux_node_store.h"

#define BREADCRUMB_FLYOUT_MARGIN_H 5.0f
#define BREADCRUMB_FLYOUT_MARGIN_V 3.0f
#define BREADCRUMB_FLYOUT_PAD_TOP  7.0f
#define BREADCRUMB_FLYOUT_PAD_BOT  9.0f

typedef struct StoredItem {
	FluxMenuItemType type;
	char            *label;
	char            *icon_glyph;
	char            *accelerator_text;
	char            *radio_group;
	char            *access_key;
	bool             enabled;
	bool             checked;
	void             (*on_click)(void *);
	void            *on_click_ctx;
	FluxMenu        *submenu;
	float            y;
	float            h;
} StoredItem;

typedef struct MenuRow {
	struct FluxMenu *menu;
	XentNodeId       node;
	int              index;
} MenuRow;

typedef struct ThemeRef {
	FluxThemeColors const *colors;
	FluxTheme             *manager;
	bool                   is_dark;
} ThemeRef;

struct FluxMenu {
	struct FluxMenu   *live_next;
	uint64_t           lifetime;
	FluxPopup         *popup;
	FluxWindow        *owner;
	FluxText          *text;
	ThemeRef           theme;
	FluxStore         *store;
	XentCtx           *ctx;
	struct FluxInput  *input;
	XentNodeId         host;
	XentNodeId         return_focus;
	bool               return_focus_suppressed;
	XentNodeId         presenter;
	XentNodeId         shift;
	XentNodeId         viewport;
	XentNodeId         body;
	MenuRow           *rows;
	int                row_count;
	int                row_capacity;

	StoredItem        *items;
	int                item_count;
	int                item_capacity;

	bool               has_icons;
	bool               has_toggles;
	bool               has_accels;
	bool               has_submenus;
	bool               breadcrumb;

	float              col_placeholder;
	float              col_label_w;
	float              col_accel_w;

	float              total_w;
	float              total_h;
	float              pad_top;
	float              pad_bot;
	float              min_item_height;

	float              scroll_y;
	float              scroll_max;

	int                hovered_index;
	int                keyboard_index;
	bool               keyboard_pressed;
	int                open_submenu_index;

	struct FluxMenu   *parent;
	struct FluxMenu   *open_submenu;
	int                parent_item_index;

	FluxPopupDismissFn dismiss_cb;
	void              *dismiss_ctx;
	bool               announced;
};

void        menu_resolve_theme(FluxMenu const *m, FluxThemeColors const **out_theme, bool *out_is_dark);

void        menu_measure(FluxMenu *m);
float       menu_body_inset(FluxMenu const *m);
float       menu_view_height(FluxMenu const *m);

void        invalidate(FluxMenu *m);

FluxMenu   *root_of(FluxMenu *m);

void        refresh_active_leaf(FluxMenu *root);

int         first_enabled(FluxMenu const *m);

int         last_enabled(FluxMenu const *m);

int         find_next(FluxMenu const *m, int from, int dir, bool wrap);

void        menu_focus_row(FluxMenu *m, int index, bool keyboard);

void        menu_cancel_activation(FluxMenu *m);

bool        menu_key_event(FluxMenu *m, FluxKey key, bool down);

void        _dismiss_internal(FluxMenu *m, bool fire_cb);

void        close_submenu(FluxMenu *m);

void        open_submenu(FluxMenu *m, int idx, bool focus_first);

void        activate_item(FluxMenu *m, int idx);

void        menu_rows_layout(FluxMenu *m);
char const *flux_menu_row_access_key(FluxStore *store, XentNodeId node);
bool        flux_menu_row_access_invoke(FluxStore *store, XentNodeId node);

#endif
