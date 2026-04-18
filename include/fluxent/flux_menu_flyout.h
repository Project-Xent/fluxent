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

/* ══════════════════════════════════════════════════════════════════════
 *  WinUI 3 MenuFlyout constants
 *  Source: CommonStyles/MenuFlyout_themeresources.xaml
 * ══════════════════════════════════════════════════════════════════════ */

/* MenuFlyoutPresenter (the outer popup card) */
#define FLUX_MENU_PRESENTER_PAD_TOP     2.0f
#define FLUX_MENU_PRESENTER_PAD_BOTTOM  2.0f
#define FLUX_MENU_PRESENTER_CORNER      8.0f /* OverlayCornerRadius */
#define FLUX_MENU_PRESENTER_BORDER      1.0f

/* MenuFlyoutItem layout — WinUI theme padding.
 *
 * WinUI defines TWO padding variants selected by the PaddingSizeStates
 * VisualStateGroup on every item template:
 *
 *     MenuFlyoutItemThemePadding       = 11,8,11,9   (touch     — DefaultPadding)
 *     MenuFlyoutItemThemePaddingNarrow = 11,4,11,5   (mouse/pen — NarrowPadding)
 *
 * The XAML comment is explicit:
 *   "Narrow padding is only applied when flyout was invoked with pen,
 *    mouse or keyboard. Default padding is applied for all other cases
 *    including touch."
 *
 * MenuFlyoutPresenter picks NarrowPadding in its OnApplyTemplate path when
 * the opening InputDeviceType is not Touch. Fluxent exposes this choice
 * via flux_menu_flyout_show_for_input() / FluxMenuInputKind.             */
#define FLUX_MENU_ITEM_PAD_LEFT         11.0f
#define FLUX_MENU_ITEM_PAD_RIGHT        11.0f

/* NarrowPadding (mouse / pen / keyboard) */
#define FLUX_MENU_ITEM_PAD_TOP_NARROW   4.0f
#define FLUX_MENU_ITEM_PAD_BOT_NARROW   5.0f

/* DefaultPadding (touch) */
#define FLUX_MENU_ITEM_PAD_TOP_TOUCH    8.0f
#define FLUX_MENU_ITEM_PAD_BOT_TOUCH    9.0f

/* Back-compat: the legacy symbols resolve to the NarrowPadding values,
 * matching Fluxent's default desktop (mouse/keyboard) use.              */
#define FLUX_MENU_ITEM_PAD_TOP          FLUX_MENU_ITEM_PAD_TOP_NARROW
#define FLUX_MENU_ITEM_PAD_BOTTOM       FLUX_MENU_ITEM_PAD_BOT_NARROW

/* MenuFlyoutItem margin within the presenter */
#define FLUX_MENU_ITEM_MARGIN_H         4.0f /* left & right */
#define FLUX_MENU_ITEM_MARGIN_V         2.0f /* top & bottom */

/* Item corner radius (ControlCornerRadius) */
#define FLUX_MENU_ITEM_CORNER           4.0f

/* Icon area */
#define FLUX_MENU_ITEM_ICON_SIZE        16.0f
#define FLUX_MENU_ITEM_ICON_GAP         12.0f /* gap icon→text */

/* Check / toggle glyph placeholder */
#define FLUX_MENU_ITEM_CHECK_WIDTH      28.0f /* PlaceholderThemeThickness left */
#define FLUX_MENU_ITEM_CHECK_GLYPH_SIZE 12.0f

/* Keyboard-accelerator text */
#define FLUX_MENU_ITEM_ACCEL_GAP        24.0f /* gap text→accel */
#define FLUX_MENU_ITEM_ACCEL_FONT_SIZE  12.0f /* CaptionTextBlockStyle */

/* Sub-item chevron */
#define FLUX_MENU_ITEM_CHEVRON_GAP      24.0f /* ChevronMargin left */
#define FLUX_MENU_ITEM_CHEVRON_SIZE     12.0f

/* Separator */
#define FLUX_MENU_SEP_HEIGHT            1.0f
#define FLUX_MENU_SEP_PAD_TOP           1.0f
#define FLUX_MENU_SEP_PAD_BOTTOM        1.0f
/* Separator extends 4px past item margins on each side (negative pad) */

/* Font */
#define FLUX_MENU_ITEM_FONT_SIZE        14.0f /* ControlContentThemeFontSize */

/* Presenter minimum height */
#define FLUX_MENU_THEME_MIN_HEIGHT      32.0f

/* Limits */
#define FLUX_MENU_MAX_ITEMS             64

/* ══════════════════════════════════════════════════════════════════════
 *  Item type / definition
 * ══════════════════════════════════════════════════════════════════════ */

typedef enum FluxMenuItemType
{
	FLUX_MENU_ITEM_NORMAL    = 0,
	FLUX_MENU_ITEM_SEPARATOR = 1,
	FLUX_MENU_ITEM_TOGGLE    = 2,
	FLUX_MENU_ITEM_SUBMENU   = 3,
	FLUX_MENU_ITEM_RADIO     = 4,
} FluxMenuItemType;

/* Opening-input hint — selects between DefaultPadding and NarrowPadding
 * visual states exactly like WinUI's MenuFlyoutPresenter does.          */
typedef enum FluxMenuInputKind
{
	FLUX_MENU_INPUT_MOUSE    = 0, /* NarrowPadding */
	FLUX_MENU_INPUT_KEYBOARD = 1, /* NarrowPadding */
	FLUX_MENU_INPUT_PEN      = 2, /* NarrowPadding */
	FLUX_MENU_INPUT_TOUCH    = 3, /* DefaultPadding */
} FluxMenuInputKind;

/**
 * Describes a single menu item to be added via flux_menu_flyout_add_item().
 * The strings are copied internally — callers need not keep them alive.
 */
typedef struct FluxMenuItemDef {
	FluxMenuItemType       type;

	char const            *label;            /* display text (UTF-8)                */
	char const            *icon_glyph;       /* single Segoe Fluent Icons char, UTF-8
	                                            e.g. "\xEE\x9C\xBE" for U+E73E.
	                                            NULL = no icon.                     */
	char const            *accelerator_text; /* e.g. "Ctrl+C".  NULL = none.        */

	bool                   enabled;          /* greyed-out if false                  */
	bool                   checked;          /* meaningful for TOGGLE / RADIO items  */

	void                   (*on_click)(void *ctx);
	void                  *on_click_ctx;

	/* For FLUX_MENU_ITEM_SUBMENU — the nested menu to open on hover/arrow.
	   Ownership stays with the caller (not freed by the parent menu).       */
	struct FluxMenuFlyout *submenu;

	/* For FLUX_MENU_ITEM_RADIO — items with the same non-NULL group name
	   are mutually exclusive within the same menu. The string is copied.    */
	char const            *radio_group;
} FluxMenuItemDef;

/* ══════════════════════════════════════════════════════════════════════
 *  Opaque handle
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct FluxMenuFlyout FluxMenuFlyout;

/* ══════════════════════════════════════════════════════════════════════
 *  Lifecycle
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Create a menu flyout.  One FluxMenuFlyout instance can be shown, dismissed,
 * and re-shown any number of times.  Items survive across show/dismiss cycles
 * until explicitly cleared.
 */
FluxMenuFlyout               *flux_menu_flyout_create(FluxWindow *owner);

/**
 * Destroy the menu flyout and all owned resources.
 */
void                          flux_menu_flyout_destroy(FluxMenuFlyout *menu);

/* ══════════════════════════════════════════════════════════════════════
 *  Configuration (call before show, or after clear + re-add)
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Set the theme colours used when painting.
 * Must be called at least once before the first show.
 */
void flux_menu_flyout_set_theme(FluxMenuFlyout *menu, FluxThemeColors const *theme, bool is_dark);

/**
 * Set the shared text renderer for measuring / drawing item text.
 */
void flux_menu_flyout_set_text_renderer(FluxMenuFlyout *menu, FluxTextRenderer *tr);

/* ══════════════════════════════════════════════════════════════════════
 *  Item management
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Append a menu item.  The definition is deep-copied.
 * Returns the 0-based index of the new item, or -1 on failure.
 */
int  flux_menu_flyout_add_item(FluxMenuFlyout *menu, FluxMenuItemDef const *item);

/**
 * Convenience: append a separator line.
 * Returns the index or -1.
 */
int  flux_menu_flyout_add_separator(FluxMenuFlyout *menu);

/**
 * Remove all items (but keep the menu object alive for reuse).
 */
void flux_menu_flyout_clear(FluxMenuFlyout *menu);

/**
 * Return the number of items (including separators).
 */
int  flux_menu_flyout_item_count(FluxMenuFlyout const *menu);

/* ══════════════════════════════════════════════════════════════════════
 *  Show / dismiss
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Show the menu anchored to `anchor` (screen coordinates).
 * `placement` is the preferred direction; the popup will flip if there
 * is not enough room.
 */
void flux_menu_flyout_show(FluxMenuFlyout *menu, FluxRect anchor, FluxPlacement placement);

/**
 * Show the menu, specifying which input modality invoked it.
 *
 * Touch input uses the WinUI DefaultPadding (11,8,11,9) to give each
 * item a larger hit target. Mouse / pen / keyboard use NarrowPadding
 * (11,4,11,5). This is exactly the behaviour of WinUI's MenuFlyout
 * PaddingSizeStates visual-state group.
 *
 * flux_menu_flyout_show() defers to this with FLUX_MENU_INPUT_MOUSE.
 */
void flux_menu_flyout_show_for_input(
  FluxMenuFlyout *menu, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind
);

/**
 * Programmatically dismiss the menu (and any open sub-menus).
 */
void            flux_menu_flyout_dismiss(FluxMenuFlyout *menu);

/**
 * Dismiss the entire visible menu chain starting from this menu's root.
 * This is the WinUI-style behavior used when a leaf item is invoked.
 */
void            flux_menu_flyout_dismiss_all(FluxMenuFlyout *menu);

/**
 * Returns true while the menu popup is on screen.
 */
bool            flux_menu_flyout_is_visible(FluxMenuFlyout const *menu);

/* ══════════════════════════════════════════════════════════════════════
 *  Keyboard forwarding
 * ══════════════════════════════════════════════════════════════════════
 *
 * Because the popup is WS_EX_NOACTIVATE it does not receive keyboard
 * focus.  The owner window's message loop should call this when a menu
 * flyout is visible so that Up / Down / Enter / Escape / Left / Right
 * are handled correctly.
 *
 * Returns true if the key was consumed.
 */
bool            flux_menu_flyout_on_key(FluxMenuFlyout *menu, unsigned int vk);

/* ══════════════════════════════════════════════════════════════════════
 *  Dismiss callback
 * ══════════════════════════════════════════════════════════════════════ */

void            flux_menu_flyout_set_dismiss_callback(FluxMenuFlyout *menu, FluxPopupDismissCallback cb, void *ctx);

/* ══════════════════════════════════════════════════════════════════════
 *  Hierarchical menu helpers
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Returns the currently open child submenu of this menu, or NULL.
 */
FluxMenuFlyout *flux_menu_flyout_get_open_submenu(FluxMenuFlyout *menu);

/**
 * Returns the parent menu of this submenu, or NULL if this is a root menu.
 */
FluxMenuFlyout *flux_menu_flyout_get_parent(FluxMenuFlyout *menu);

/**
 * Returns the item index in the parent menu that owns this submenu,
 * or -1 if this menu is not currently attached to a parent item.
 */
int             flux_menu_flyout_get_parent_item_index(FluxMenuFlyout *menu);

/* ══════════════════════════════════════════════════════════════════════
 *  Global active-menu query (for keyboard routing from the app loop)
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Returns the deepest, most-recently-shown visible FluxMenuFlyout, or NULL.
 * The app's key handler should forward keys to this active leaf menu.
 */
FluxMenuFlyout *flux_menu_flyout_get_active(void);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_MENU_FLYOUT_H */
