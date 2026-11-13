#ifndef FLUX_MENU_FLYOUT_H
#define FLUX_MENU_FLYOUT_H

#include "flux_types.h"
#include "flux_popup.h"
#include "flux_text.h"
#include "flux_theme.h"
#include "flux_window.h"

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

typedef enum FluxMenuItemType
{
	FLUX_MENU_ITEM_NORMAL    = 0,
	FLUX_MENU_ITEM_SEPARATOR = 1,
	FLUX_MENU_ITEM_TOGGLE    = 2,
	FLUX_MENU_ITEM_SUBMENU   = 3,
	FLUX_MENU_ITEM_RADIO     = 4,
} FluxMenuItemType;

typedef enum FluxMenuInputKind
{
	FLUX_MENU_INPUT_MOUSE    = 0,
	FLUX_MENU_INPUT_KEYBOARD = 1,
	FLUX_MENU_INPUT_PEN      = 2,
	FLUX_MENU_INPUT_TOUCH    = 3,
} FluxMenuInputKind;

typedef struct FluxMenuItemDef {
	FluxMenuItemType type;

	char const      *label;
	char const      *icon_glyph;
	char const      *accelerator_text;

	bool             enabled;
	bool             checked;

	void             (*on_click)(void *ctx);
	void            *on_click_ctx;

	struct FluxMenu *submenu;

	char const      *radio_group;
} FluxMenuItemDef;

typedef struct FluxMenu  FluxMenu;

XENT_NODISCARD FluxMenu *flux_menu_create(FluxWindow *owner);

void                     flux_menu_destroy(FluxMenu *menu);

void                     flux_menu_settheme(FluxMenu *menu, FluxThemeColors const *theme, bool is_dark);

void                     flux_menu_setthememanager(FluxMenu *menu, FluxTheme *tm);

void                     flux_menu_settext(FluxMenu *menu, FluxText *tr);

void                     flux_menu_setminh(FluxMenu *menu, float height);

void                     flux_menu_setanim(FluxMenu *menu, FluxPopupAnimStyle style);

int                      flux_menu_additem(FluxMenu *menu, FluxMenuItemDef const *item);

int                      flux_menu_addsep(FluxMenu *menu);

void                     flux_menu_clear(FluxMenu *menu);

int                      flux_menu_nitem(FluxMenu const *menu);

void                     flux_menu_show(FluxMenu *menu, FluxRect anchor, FluxPlacement placement);

void      flux_menu_showinput(FluxMenu *menu, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind);

void      flux_menu_dismiss(FluxMenu *menu);

void      flux_menu_dismissall(FluxMenu *menu);

bool      flux_menu_visible(FluxMenu const *menu);

bool      flux_menu_onkey(FluxMenu *menu, FluxKey key);

void      flux_menu_ondismiss(FluxMenu *menu, FluxPopupDismissFn cb, void *ctx);

FluxMenu *flux_menu_submenu(FluxMenu *menu);

FluxMenu *flux_menu_parent(FluxMenu *menu);

int       flux_menu_parentidx(FluxMenu *menu);

FluxMenu *flux_menu_active(FluxWindow *owner);

#endif
