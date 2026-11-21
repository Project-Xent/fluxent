#ifndef FLUX_MENU_INTERNAL_H
#define FLUX_MENU_INTERNAL_H

#include <cd2d.h>

#include "fluxent/flux_gfx.h"
#include "fluxent/flux_menu.h"
#include "fluxent/flux_node_store.h"

typedef struct StoredItem {
	FluxMenuItemType type;
	char            *label;
	char            *icon_glyph;
	char            *accelerator_text;
	char            *radio_group;
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
	XentNodeId         presenter;
	XentNodeId         shift;
	XentNodeId         body;
	XentNodeId         rows [FLUX_MENU_MAX_ITEMS];
	MenuRow            row_data [FLUX_MENU_MAX_ITEMS];
	int                row_count;

	StoredItem         items [FLUX_MENU_MAX_ITEMS];
	int                item_count;

	bool               has_icons;
	bool               has_toggles;
	bool               has_accels;
	bool               has_submenus;

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
	int                open_submenu_index;

	struct FluxMenu   *parent;
	struct FluxMenu   *open_submenu;
	int                parent_item_index;

	FluxPopupDismissFn dismiss_cb;
	void              *dismiss_ctx;
};

void      menu_resolve_theme(FluxMenu const *m, FluxThemeColors const **out_theme, bool *out_is_dark);

void      menu_measure(FluxMenu *m);

void      invalidate(FluxMenu *m);

FluxMenu *root_of(FluxMenu *m);

void      refresh_active_leaf(FluxMenu *root);

int       first_enabled(FluxMenu const *m);

int       last_enabled(FluxMenu const *m);

int       find_next(FluxMenu const *m, int from, int dir, bool wrap);

void      _dismiss_internal(FluxMenu *m, bool fire_cb);

void      close_submenu(FluxMenu *m);

void      open_submenu(FluxMenu *m, int idx, bool focus_first);

void      activate_item(FluxMenu *m, int idx);

void      menu_rows_layout(FluxMenu *m);

#endif
