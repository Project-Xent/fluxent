/**
 * @file flux_menu_flyout.h
 * @brief Menu flyout control (WinUI 3 MenuFlyout equivalent).
 *
 * Displays a context menu with items, separators, and nested submenus.
 * Supports keyboard navigation and light-dismiss.
 */

#ifndef FLUX_MENU_FLYOUT_H
#define FLUX_MENU_FLYOUT_H

#include "flux_types.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_window.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FLUX_MENU_PRESENTER_PAD_TOP     2.0f
#define FLUX_MENU_PRESENTER_PAD_BOTTOM  2.0f
#define FLUX_MENU_PRESENTER_CORNER      8.0f
#define FLUX_MENU_PRESENTER_BORDER      1.0f

#define FLUX_MENU_ITEM_PAD_LEFT         11.0f
#define FLUX_MENU_ITEM_PAD_RIGHT        11.0f
#define FLUX_MENU_ITEM_PAD_TOP_NARROW   4.0f
#define FLUX_MENU_ITEM_PAD_BOT_NARROW   5.0f
#define FLUX_MENU_ITEM_PAD_TOP_TOUCH    8.0f
#define FLUX_MENU_ITEM_PAD_BOT_TOUCH    9.0f
#define FLUX_MENU_ITEM_PAD_TOP          FLUX_MENU_ITEM_PAD_TOP_NARROW
#define FLUX_MENU_ITEM_PAD_BOTTOM       FLUX_MENU_ITEM_PAD_BOT_NARROW

#define FLUX_MENU_ITEM_MARGIN_H         4.0f
#define FLUX_MENU_ITEM_MARGIN_V         2.0f
#define FLUX_MENU_ITEM_CORNER           4.0f
#define FLUX_MENU_ITEM_ICON_SIZE        16.0f
#define FLUX_MENU_ITEM_ICON_GAP         12.0f
#define FLUX_MENU_ITEM_CHECK_WIDTH      28.0f
#define FLUX_MENU_ITEM_CHECK_GLYPH_SIZE 12.0f
#define FLUX_MENU_ITEM_ACCEL_GAP        24.0f
#define FLUX_MENU_ITEM_ACCEL_FONT_SIZE  12.0f
#define FLUX_MENU_ITEM_CHEVRON_GAP      24.0f
#define FLUX_MENU_ITEM_CHEVRON_SIZE     12.0f
#define FLUX_MENU_SEP_HEIGHT            1.0f
#define FLUX_MENU_SEP_PAD_TOP           1.0f
#define FLUX_MENU_SEP_PAD_BOTTOM        1.0f
#define FLUX_MENU_ITEM_FONT_SIZE        14.0f
#define FLUX_MENU_THEME_MIN_HEIGHT      32.0f
#define FLUX_MENU_MAX_ITEMS             64

/** @brief Menu item kind. */
typedef enum FluxMenuItemType
{
	FLUX_MENU_ITEM_NORMAL    = 0,
	FLUX_MENU_ITEM_SEPARATOR = 1,
	FLUX_MENU_ITEM_TOGGLE    = 2,
	FLUX_MENU_ITEM_SUBMENU   = 3,
	FLUX_MENU_ITEM_RADIO     = 4,
} FluxMenuItemType;

/** @brief Input modality used to choose menu item padding. */
typedef enum FluxMenuInputKind
{
	FLUX_MENU_INPUT_MOUSE    = 0, /**< Mouse-invoked menu. */
	FLUX_MENU_INPUT_KEYBOARD = 1, /**< Keyboard-invoked menu. */
	FLUX_MENU_INPUT_PEN      = 2, /**< Pen-invoked menu. */
	FLUX_MENU_INPUT_TOUCH    = 3, /**< Touch-invoked menu. */
} FluxMenuInputKind;

/**
 * Describes a single menu item to be added via flux_menu_flyout_add_item().
 * The strings are copied internally — callers need not keep them alive.
 */
typedef struct FluxMenuItemDef {
	FluxMenuItemType       type;

	char const            *label;                  /**< Display text (UTF-8). */
	char const            *icon_glyph;             /**< Single Segoe Fluent Icons glyph, or NULL. */
	char const            *accelerator_text;       /**< Accelerator text, or NULL. */

	bool                   enabled;                /**< Whether the item can be invoked. */
	bool                   checked;                /**< State for toggle and radio items. */

	void                   (*on_click)(void *ctx); /**< Callback invoked when the item is selected. */
	void                  *on_click_ctx;

	struct FluxMenuFlyout *submenu;     /**< Nested submenu for submenu items; not owned. */

	char const            *radio_group; /**< Radio group name copied by the menu, or NULL. */
} FluxMenuItemDef;

/** @brief Opaque menu flyout handle. */
typedef struct FluxMenuFlyout FluxMenuFlyout;

/**
 * @brief Create a menu flyout. One FluxMenuFlyout instance can be shown, dismissed,
 * and re-shown any number of times.  Items survive across show/dismiss cycles
 * until explicitly cleared.
 */
FluxMenuFlyout               *flux_menu_flyout_create(FluxWindow *owner);

/**
 * @brief Destroy the menu flyout and all owned resources.
 */
void                          flux_menu_flyout_destroy(FluxMenuFlyout *menu);

/**
 * @brief Set the theme colours used when painting.
 * @deprecated Use flux_menu_flyout_set_theme_manager() instead for automatic theme updates.
 */
void flux_menu_flyout_set_theme(FluxMenuFlyout *menu, FluxThemeColors const *theme, bool is_dark);

/**
 * @brief Set the theme manager for automatic theme synchronization.
 * The menu will query the manager on each show/paint to stay in sync
 * with theme changes (including system dark/light mode switches).
 * Pass NULL to disable automatic theme management.
 */
void flux_menu_flyout_set_theme_manager(FluxMenuFlyout *menu, FluxThemeManager *tm);

/**
 * @brief Set the shared text renderer for measuring and drawing item text.
 */
void flux_menu_flyout_set_text_renderer(FluxMenuFlyout *menu, FluxTextRenderer *tr);

/**
 * @brief Append a menu item. The definition is deep-copied.
 * Returns the 0-based index of the new item, or -1 on failure.
 */
int  flux_menu_flyout_add_item(FluxMenuFlyout *menu, FluxMenuItemDef const *item);

/**
 * @brief Append a separator line.
 * Returns the index or -1.
 */
int  flux_menu_flyout_add_separator(FluxMenuFlyout *menu);

/**
 * @brief Remove all items while keeping the menu object alive.
 */
void flux_menu_flyout_clear(FluxMenuFlyout *menu);

/**
 * @brief Return the number of items, including separators.
 */
int  flux_menu_flyout_item_count(FluxMenuFlyout const *menu);

/**
 * @brief Show the menu anchored to `anchor` in screen coordinates.
 * `placement` is the preferred direction; the popup will flip if there
 * is not enough room.
 */
void flux_menu_flyout_show(FluxMenuFlyout *menu, FluxRect anchor, FluxPlacement placement);

/**
 * @brief Show the menu with an explicit input modality.
 */
void flux_menu_flyout_show_for_input(
  FluxMenuFlyout *menu, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind
);

/**
 * @brief Programmatically dismiss the menu and any open submenus.
 */
void            flux_menu_flyout_dismiss(FluxMenuFlyout *menu);

/**
 * @brief Dismiss the entire visible menu chain starting from this menu's root.
 */
void            flux_menu_flyout_dismiss_all(FluxMenuFlyout *menu);

/**
 * @brief Returns true while the menu popup is on screen.
 */
bool            flux_menu_flyout_is_visible(FluxMenuFlyout const *menu);

/** @brief Forward keyboard input to a visible menu flyout; returns true when consumed. */
bool            flux_menu_flyout_on_key(FluxMenuFlyout *menu, unsigned int vk);

/** @brief Set the callback fired when the menu is dismissed. */
void            flux_menu_flyout_set_dismiss_callback(FluxMenuFlyout *menu, FluxPopupDismissCallback cb, void *ctx);

/**
 * @brief Returns the currently open child submenu of this menu, or NULL.
 */
FluxMenuFlyout *flux_menu_flyout_get_open_submenu(FluxMenuFlyout *menu);

/**
 * @brief Returns the parent menu of this submenu, or NULL if this is a root menu.
 */
FluxMenuFlyout *flux_menu_flyout_get_parent(FluxMenuFlyout *menu);

/**
 * @brief Returns the item index in the parent menu that owns this submenu,
 * or -1 if this menu is not currently attached to a parent item.
 */
int             flux_menu_flyout_get_parent_item_index(FluxMenuFlyout *menu);

/**
 * @brief Returns the deepest visible FluxMenuFlyout for a window, or NULL.
 * The app's key handler should forward keys to this active leaf menu.
 */
FluxMenuFlyout *flux_menu_flyout_get_active(FluxWindow *owner);

#ifdef __cplusplus
}
#endif

#endif
