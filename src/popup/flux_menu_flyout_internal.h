#ifndef FLUX_MENU_FLYOUT_INTERNAL_H
#define FLUX_MENU_FLYOUT_INTERNAL_H

#include <cd2d.h>

#include "fluxent/flux_graphics.h"
#include "fluxent/flux_menu_flyout.h"

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
	FluxMenuFlyout  *submenu;
	float            y;
	float            h;
} StoredItem;

typedef struct ThemeRef {
	FluxThemeColors const *colors;
	FluxThemeManager      *manager;
	bool                   is_dark;
} ThemeRef;

struct FluxMenuFlyout {
	FluxPopup               *popup;
	FluxWindow              *owner;
	FluxTextRenderer        *text;
	ThemeRef                 theme;
	ID2D1SolidColorBrush    *brush;

	StoredItem               items [FLUX_MENU_MAX_ITEMS];
	int                      item_count;

	bool                     has_icons;
	bool                     has_toggles;
	bool                     has_accels;
	bool                     has_submenus;

	float                    col_placeholder;
	float                    col_label_w;
	float                    col_accel_w;

	float                    total_w;
	float                    total_h;
	float                    pad_top;
	float                    pad_bot;

	float                    scroll_y;
	float                    scroll_max;

	int                      hovered_index;
	int                      pressed_index;
	int                      keyboard_index;
	int                      open_submenu_index;

	struct FluxMenuFlyout   *parent;
	struct FluxMenuFlyout   *open_submenu;
	int                      parent_item_index;

	FluxPopupDismissCallback dismiss_cb;
	void                    *dismiss_ctx;
};

/** @brief Resolve menu theme from explicit colors or the theme manager. */
void            menu_resolve_theme(FluxMenuFlyout const *m, FluxThemeColors const **out_theme, bool *out_is_dark);

/** @brief Recompute menu columns, item slots, size, and scroll range. */
void            menu_measure(FluxMenuFlyout *m);

/** @brief Return the enabled item under a menu-local point, or -1. */
int             menu_hit_test(FluxMenuFlyout const *m, float px, float py);

/** @brief Invalidate the popup window backing this menu. */
void            invalidate(FluxMenuFlyout *m);

/** @brief Return the root menu for a submenu chain. */
FluxMenuFlyout *root_of(FluxMenuFlyout *m);

/** @brief Set the active leaf to the deepest visible submenu. */
void            refresh_active_leaf(FluxMenuFlyout *root);

/** @brief Return the first enabled non-separator item, or -1. */
int             first_enabled(FluxMenuFlyout const *m);

/** @brief Return the last enabled non-separator item, or -1. */
int             last_enabled(FluxMenuFlyout const *m);

/** @brief Find the next enabled item from an index. */
int             find_next(FluxMenuFlyout const *m, int from, int dir, bool wrap);

/** @brief Dismiss a menu, optionally firing its dismiss callback. */
void            _dismiss_internal(FluxMenuFlyout *m, bool fire_cb);

/** @brief Close the visible direct child submenu, if any. */
void            close_submenu(FluxMenuFlyout *m);

/** @brief Open the submenu owned by an item index. */
void            open_submenu(FluxMenuFlyout *m, int idx, bool focus_first);

/** @brief Invoke or toggle the item at an index. */
void            activate_item(FluxMenuFlyout *m, int idx);

/** @brief Paint callback registered with FluxPopup. */
void            menu_paint(void *ctx, FluxPopup *popup);

#endif
