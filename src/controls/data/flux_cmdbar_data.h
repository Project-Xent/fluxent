#ifndef FLUX_CMDBAR_DATA_H
#define FLUX_CMDBAR_DATA_H

#include "fluxent/flux_node_store.h"

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#define FLUX_APPBAR_MIN_H           64.0f
#define FLUX_APPBAR_COMPACT_H       48.0f
#define FLUX_APPBAR_MINIMAL_H       24.0f
#define FLUX_CMDBAR_PAD_LEFT        4.0f
#define FLUX_CMDBAR_CORNER          4.0f
#define FLUX_CMDBAR_MORE_W          48.0f
#define FLUX_CMDBAR_MORE_MIN_W      6.0f
#define FLUX_CMDBAR_MORE_GLYPH      20.0f
#define FLUX_CMDBAR_OPEN_MS         250u
#define FLUX_CMDBAR_CLOSE_MS        167u

#define FLUX_ABB_W                  68.0f
#define FLUX_ABB_CORNER             4.0f
#define FLUX_ABB_ICON               16.0f
#define FLUX_ABB_ICON_TOP           16.0f
#define FLUX_ABB_ICON_BOTTOM        2.0f
#define FLUX_ABB_ICON_RIGHT_LEFT    12.0f
#define FLUX_ABB_LABEL_SIZE         12.0f
#define FLUX_ABB_LABEL_LINE         16.0f
#define FLUX_ABB_LABEL_PAD_H        2.0f
#define FLUX_ABB_LABEL_PAD_BOTTOM   8.0f
#define FLUX_ABB_RIGHT_LABEL_LEFT   8.0f
#define FLUX_ABB_RIGHT_LABEL_RIGHT  12.0f
#define FLUX_ABB_BORDER_H           2.0f
#define FLUX_ABB_BORDER_V           6.0f
#define FLUX_ABB_BORDER_COMPACT_BOT 22.0f
#define FLUX_ABB_SEP_W              1.0f
#define FLUX_ABB_SEP_PAD_H          2.0f
#define FLUX_ABB_SEP_PAD_V          8.0f

#define FLUX_CMDBAR_OVF_MIN_W       160.0f
#define FLUX_CMDBAR_OVF_TOUCH_MIN_W 240.0f
#define FLUX_CMDBAR_OVF_MAX_W       480.0f
#define FLUX_CMDBAR_OVF_PAD_V       4.0f
#define FLUX_CMDBAR_OVF_CORNER      8.0f
#define FLUX_CMDBAR_OVF_BORDER      1.0f
#define FLUX_CMDBAR_OVF_ITEM_MARGIN 4.0f
#define FLUX_CMDBAR_OVF_LABEL_SIZE  14.0f
#define FLUX_CMDBAR_OVF_LINE        19.0f
#define FLUX_CMDBAR_OVF_PAD_TOP     5.0f
#define FLUX_CMDBAR_OVF_PAD_BOT     8.0f
#define FLUX_CMDBAR_OVF_TOUCH_TOP   9.0f
#define FLUX_CMDBAR_OVF_TOUCH_BOT   12.0f
#define FLUX_CMDBAR_OVF_INDENT      12.0f
#define FLUX_CMDBAR_OVF_INDENT_ONE  38.0f
#define FLUX_CMDBAR_OVF_INDENT_TWO  76.0f
#define FLUX_CMDBAR_OVF_ACCEL_LEFT  24.0f
#define FLUX_CMDBAR_OVF_ACCEL_SIZE  12.0f
#define FLUX_CMDBAR_OVF_CHECK_LEFT  12.0f
#define FLUX_CMDBAR_OVF_CHECK       14.0f
#define FLUX_CMDBAR_OVF_CHECK_GLYPH 16.0f
#define FLUX_CMDBAR_OVF_SEP_PAD     4.0f
#define FLUX_CMDBAR_MAX_ROWS        64

typedef struct FluxWindow FluxWindow;
typedef struct FluxText   FluxText;
typedef struct FluxTheme  FluxTheme;
typedef struct FluxInput  FluxInput;
typedef struct FluxPopup  FluxPopup;

typedef enum FluxAppBarKind
{
	FLUX_APPBAR_BUTTON = 0,
	FLUX_APPBAR_TOGGLE,
	FLUX_APPBAR_SEPARATOR,
} FluxAppBarKind;

typedef enum FluxAppBarView
{
	FLUX_APPBAR_VIEW_FULL = 0,
	FLUX_APPBAR_VIEW_COMPACT,
	FLUX_APPBAR_VIEW_RIGHT,
	FLUX_APPBAR_VIEW_COLLAPSED,
} FluxAppBarView;

typedef struct FluxAppBarItemData {
	XentCtx    *ctx;
	FluxStore  *store;
	FluxText   *text;
	XentNodeId  node;
	uint8_t     kind;
	uint8_t     view;
	char const *label;
	char const *icon;
	char const *accel;
	bool        enabled;
	bool        checked;
	bool        secondary;
	bool        label_collapsed;
	bool        hidden;
	bool        mdirty;
	int32_t     order;
	float       user_w;
	float       width;
	float       height;
	float       label_h;
} FluxAppBarItemData;

typedef struct FluxCmdBarRow {
	XentNodeId item;
	bool       separator;
	float      y;
	float      h;
} FluxCmdBarRow;

typedef struct FluxCmdBarData {
	XentCtx      *ctx;
	FluxStore    *store;
	XentNodeId    node;
	XentNodeId    root;
	XentNodeId    content;
	XentNodeId    primary;
	XentNodeId    more;
	FluxWindow   *window;
	FluxText     *text;
	FluxTheme    *theme;
	FluxInput    *input;
	FluxPopup    *popup;

	bool          open;
	bool          sticky;
	bool          dynamic;
	bool          enabled;
	uint8_t       mode;
	uint8_t       label_pos;
	uint8_t       more_vis;
	bool          open_up;
	bool          with_button;
	bool          touch;
	bool          keyboard;
	bool          more_pointer;
	bool          dirty;

	float         width;
	float         content_h;
	float         compact_h;
	float         closed_h;
	float         visible_h;
	float         shift;
	float         last_avail;
	float         restore_min;
	int           restore_count;
	bool          more_shown;

	XentNodeId    moved [FLUX_CMDBAR_MAX_ROWS];
	int           nmoved;
	FluxCmdBarRow rows [FLUX_CMDBAR_MAX_ROWS];
	int           nrows;
	bool          with_icons;
	bool          with_toggles;
	float         accel_w;
	float         ovf_w;
	float         ovf_h;
	float         ovf_view;
	float         scroll_y;
	int           hover;
	int           pressed;
	int           focus_row;

	unsigned long anim_start;
	bool          anim_open;
	bool          animating;
	float         reveal;
} FluxCmdBarData;

#endif
