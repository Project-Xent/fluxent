#include "controls/behavior/flux_picker_flyout.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_items.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"
#include "render/flux_picker_geom.h"
#include "store/flux_node_data.h"

#include <stdlib.h>
#include <math.h>
#include <fluxent/flux_popup.h>
#include <windows.h>

#define PFLY_BUTTON_FONT 16.0f

typedef struct PflyColumn {
	FluxPickerFlyout *fly;
	int               index;
	bool              (*wheel_key)(void *ctx, FluxKey key, bool down);
	void             *wheel_key_ctx;
} PflyColumn;

struct FluxPickerFlyout {
	FluxPickerFlyoutDesc desc;
	XentNodeId           scrim;
	FluxPopup           *popup;
	XentNodeId           card;
	XentNodeId           accept, dismiss;
	XentNodeId           wheel [FLUX_PICKER_COLUMNS];
	PflyColumn           column [FLUX_PICKER_COLUMNS];
	FluxPickerCardData   geom;
	XentNodeId           return_focus;
	bool                 shown;
};

static void pfly_repaint(FluxPickerFlyout const *fly) {
	HWND hwnd = fly->desc.window ? flux_window_hwnd(fly->desc.window) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void pfly_close(FluxPickerFlyout *fly, bool accepted) {
	if (!fly->shown) return;
	if (accepted && fly->desc.on_accept) fly->desc.on_accept(fly->desc.userdata);
	flux_pflyout_hide(fly);
	if (!accepted && fly->desc.on_dismiss) fly->desc.on_dismiss(fly->desc.userdata);
}

static void pfly_accept_click(void *ctx) { pfly_close(( FluxPickerFlyout * ) ctx, true); }

static void pfly_dismiss_click(void *ctx) { pfly_close(( FluxPickerFlyout * ) ctx, false); }

static void pfly_press_noop(void *ctx, float x, float y, int clicks) {
	( void ) ctx;
	( void ) x;
	( void ) y;
	( void ) clicks;
}

static void pfly_column_select(void *ctx, int index) {
	PflyColumn *col = ( PflyColumn * ) ctx;
	if (col->fly->desc.on_select) col->fly->desc.on_select(col->fly->desc.userdata, col->index, index);
}

static bool pfly_wheel_key(void *ctx, FluxKey key, bool down) {
	PflyColumn       *col = ( PflyColumn * ) ctx;
	FluxPickerFlyout *fly = col->fly;
	if (!down) return false;
	bool alt_accept = (key == FLUX_KEY_UP || key == FLUX_KEY_DOWN) && flux_key_held(FLUX_KEY_ALT);
	if (key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE || alt_accept) {
		pfly_close(fly, true);
		return true;
	}
	if (key != FLUX_KEY_LEFT && key != FLUX_KEY_RIGHT)
		return col->wheel_key && col->wheel_key(col->wheel_key_ctx, key, down);
	int to = col->index + (key == FLUX_KEY_LEFT ? -1 : 1);
	if (to >= 0 && to < fly->geom.columns) flux_input_setfocus(fly->desc.input, fly->wheel [to], false);
	return true;
}

static void       pfly_escape(void *ctx) { pfly_close(( FluxPickerFlyout * ) ctx, false); }

static XentNodeId pfly_make_button(FluxPickerFlyout *fly, char const *icon, void (*click)(void *)) {
	XentNodeId node = flux_button_init(&(FluxButtonDesc) {
	  .ctx = fly->desc.ctx, .store = fly->desc.store, .parent = fly->card, .on_click = click, .userdata = fly});
	if (node == XENT_NODE_INVALID) return node;
	flux_prop_button_style(fly->desc.store, node, FLUX_BUTTON_SUBTLE);
	flux_prop_button_icon(fly->desc.store, node, icon);
	FluxNodeData *nd = flux_store_find(fly->desc.store, node);
	if (nd && nd->component_data) {
		FluxButtonData *bd  = ( FluxButtonData * ) nd->component_data;
		bd->font_size       = PFLY_BUTTON_FONT / 1.14f;
		bd->pressed_primary = true;
	}
	return node;
}

static bool pfly_build(FluxPickerFlyout *fly) {
	XentCtx   *ctx   = fly->desc.ctx;
	FluxStore *store = fly->desc.store;

	fly->scrim       = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_CONTAINER);
	if (fly->scrim == XENT_NODE_INVALID) return false;
	fly->card = flux_factory_create_node(ctx, store, fly->scrim, FLUX_CONTROL_PICKER_FLYOUT);
	if (fly->card == XENT_NODE_INVALID) return false;

	xent_setproto(ctx, fly->scrim, XENT_PROTOCOL_ABSOLUTE);
	xent_setproto(ctx, fly->card, XENT_PROTOCOL_ABSOLUTE);

	FluxNodeData *scrim = flux_store_find(store, fly->scrim);
	FluxNodeData *card  = flux_store_find(store, fly->card);
	if (!scrim || !card) return false;
	card->behavior.on_pointer_down = pfly_press_noop;
	card->component_data           = &fly->geom;

	fly->accept                    = pfly_make_button(fly, "Accept", pfly_accept_click);
	fly->dismiss                   = pfly_make_button(fly, "Cancel", pfly_dismiss_click);
	return fly->accept != XENT_NODE_INVALID && fly->dismiss != XENT_NODE_INVALID;
}

static void pfly_popup_dismiss(void *ctx) { pfly_close(( FluxPickerFlyout * ) ctx, false); }

static void pfly_mouse_button(FluxPopup *popup, FluxPointerEvent *ev, bool down) {
	HWND hwnd          = flux_popup_hwnd(popup);
	ev->kind           = down ? FLUX_POINTER_DOWN : FLUX_POINTER_UP;
	ev->changed_button = FLUX_POINTER_BUTTON_LEFT;
	ev->buttons        = down ? FLUX_POINTER_BUTTON_LEFT : 0;
	if (down) SetCapture(hwnd);
	else if (GetCapture() == hwnd) ReleaseCapture();
}

static void pfly_mouse_wheel(FluxPickerFlyout *fly, FluxPopup *popup, FluxPointerEvent *ev, float delta) {
	POINT pt;
	GetCursorPos(&pt);
	FluxPoint origin = flux_popup_contentorigin(popup);
	float     scale  = flux_window_dpi(fly->desc.window).dpi_y / FLUX_DPI_BASE;
	ev->kind         = FLUX_POINTER_WHEEL;
	ev->wheel_dy     = delta / 48.0f;
	ev->y            = (( float ) pt.y - origin.y) / scale;
}

static void pfly_popup_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	FluxPickerFlyout *fly = ctx;
	if (!fly->shown) return;
	FluxPointerEvent ev = {
	  .x      = x,
	  .y      = y,
	  .kind   = FLUX_POINTER_MOVE,
	  .device = flux_popup_lasttouch(popup) ? FLUX_POINTER_TOUCH : FLUX_POINTER_MOUSE};
	switch (event) {
	case FLUX_POPUP_MOUSE_DOWN  : pfly_mouse_button(popup, &ev, true); break;
	case FLUX_POPUP_MOUSE_UP    : pfly_mouse_button(popup, &ev, false); break;
	case FLUX_POPUP_MOUSE_WHEEL : pfly_mouse_wheel(fly, popup, &ev, y); break;
	case FLUX_POPUP_MOUSE_LEAVE : ev.x = ev.y = -1.0f; break;
	default :
		ev.buttons = GetKeyState(VK_LBUTTON) < 0 || flux_popup_lasttouch(popup) ? FLUX_POINTER_BUTTON_LEFT : 0;
		break;
	}
	flux_input_dispatch(fly->desc.input, fly->card, &ev);
	InvalidateRect(flux_popup_hwnd(popup), NULL, FALSE);
}

static bool pfly_build_popup(FluxPickerFlyout *fly) {
	fly->popup = flux_popup_create(fly->desc.window);
	if (!fly->popup) return false;
	flux_popup_setshadow(fly->popup, true, FLUX_PFLY_RADIUS);
	flux_popup_setanim(fly->popup, FLUX_POPUP_ANIM_PICKER);
	flux_popup_ondismiss(fly->popup, pfly_popup_dismiss, fly);
	flux_popup_onmouse(fly->popup, pfly_popup_mouse, fly);
	return flux_popup_setcontent(fly->popup, fly->desc.ctx, fly->desc.store, fly->card);
}

FluxPickerFlyout *flux_pflyout_create(FluxPickerFlyoutDesc const *desc) {
	if (!desc || !desc->ctx || !desc->store) return NULL;
	FluxPickerFlyout *fly = ( FluxPickerFlyout * ) calloc(1, sizeof(*fly));
	if (!fly) return NULL;
	fly->desc         = *desc;
	fly->scrim        = XENT_NODE_INVALID;
	fly->return_focus = XENT_NODE_INVALID;
	if (pfly_build(fly) && pfly_build_popup(fly)) return fly;
	flux_pflyout_destroy(fly);
	return NULL;
}

void flux_pflyout_destroy(FluxPickerFlyout *fly) {
	if (!fly) return;
	flux_pflyout_hide(fly);
	flux_popup_destroy(fly->popup);
	if (fly->scrim != XENT_NODE_INVALID) xent_node_destroy(fly->desc.ctx, fly->scrim);
	free(fly);
}

int flux_pflyout_addcolumn(FluxPickerFlyout *fly, FluxPickerColumnSpec const *spec) {
	int index = fly->geom.columns;
	if (index >= FLUX_PICKER_COLUMNS) return -1;

	fly->column [index] = (PflyColumn) {.fly = fly, .index = index};
	XentNodeId wheel    = flux_looping_init(&(FluxLoopingDesc) {
	  .ctx         = fly->desc.ctx,
	  .store       = fly->desc.store,
	  .parent      = fly->card,
	  .input       = fly->desc.input,
	  .items       = spec->items,
	  .item_count  = spec->count,
	  .selected    = spec->selected,
	  .should_loop = spec->loop,
	  .accent      = true,
	  .left        = spec->left,
	  .on_select   = pfly_column_select,
	  .userdata    = &fly->column [index]});
	if (wheel == XENT_NODE_INVALID) return -1;

	FluxNodeData *nd = flux_store_find(fly->desc.store, wheel);
	if (nd) {
		fly->column [index].wheel_key     = nd->behavior.on_key;
		fly->column [index].wheel_key_ctx = nd->behavior.on_key_ctx;
		nd->behavior.on_key               = pfly_wheel_key;
		nd->behavior.on_key_ctx           = &fly->column [index];
	}
	fly->wheel [index]       = wheel;
	fly->geom.weight [index] = spec->weight;
	fly->geom.columns        = index + 1;
	return index;
}

void flux_pflyout_setcolumn(FluxPickerFlyout *fly, int column, char const *const *items, int count, int selected) {
	if (column < 0 || column >= fly->geom.columns) return;
	flux_looping_setitems(fly->desc.store, fly->wheel [column], items, count);
	flux_looping_setselected(fly->desc.store, fly->wheel [column], selected);
}

static void pfly_layout(FluxPickerFlyout *fly, float card_h) {
	XentCtx         *ctx   = fly->desc.ctx;
	float            w     = fly->desc.width;
	float            inner = w - 2.0f * FLUX_PFLY_BORDER;
	FluxPickerColumn cols [FLUX_PICKER_COLUMNS];

	fly->geom.body_h = card_h - FLUX_PFLY_ROW_H - 2.0f * FLUX_PFLY_BORDER;
	fly->geom.time   = fly->desc.time;
	flux_pgeom_split(&fly->geom, inner, cols);
	xent_setsize(ctx, fly->card, (XentSize) {w, card_h});
	for (int i = 0; i < fly->geom.columns; i++) {
		xent_setpos(ctx, fly->wheel [i], (XentPoint) {cols [i].x, FLUX_PFLY_BORDER});
		xent_setsize(ctx, fly->wheel [i], (XentSize) {cols [i].w, fly->geom.body_h});
	}

	float row_y  = FLUX_PFLY_BORDER + fly->geom.body_h;
	float half   = inner * 0.5f;
	float btn_h  = FLUX_PFLY_ROW_H - 8.0f;
	float accept = fly->desc.time ? 2.0f : 4.0f;
	xent_setpos(ctx, fly->accept, (XentPoint) {FLUX_PFLY_BORDER + accept, row_y + 4.0f});
	xent_setsize(ctx, fly->accept, (XentSize) {half - 6.0f, btn_h});
	xent_setpos(ctx, fly->dismiss, (XentPoint) {FLUX_PFLY_BORDER + half + 2.0f, row_y + 4.0f});
	xent_setsize(ctx, fly->dismiss, (XentSize) {half - 6.0f, btn_h});
}

static float pfly_constrain(float at, float size, LONG lo, LONG hi, bool vertical) {
	bool below = at <= ( float ) lo;
	if (at + size > ( float ) hi)
		at = below && vertical ? ( float ) lo : at - fminf(size, fmaxf(0.0f, at - ( float ) lo));
	if (size <= ( float ) (hi - lo)) at = fmaxf(( float ) lo, fminf(at, ( float ) hi - size));
	return at;
}

static FluxRect pfly_place(FluxPickerFlyout *fly, FluxRect const *target) {
	HWND  owner  = flux_window_hwnd(fly->desc.window);
	float scale  = flux_window_dpi(fly->desc.window).dpi_y / FLUX_DPI_BASE;
	POINT origin = {0};
	ClientToScreen(owner, &origin);
	POINT       point   = {origin.x + ( LONG ) (target->x * scale), origin.y + ( LONG ) (target->y * scale)};
	MONITORINFO monitor = {.cbSize = sizeof(monitor)};
	GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
	RECT const work = monitor.rcWork;
	fly->desc.width = fminf(target->w, (work.right - work.left) / scale);
	float height    = fminf(FLUX_PFLY_MAX_H, (work.bottom - work.top) / scale);
	pfly_layout(fly, height);
	xent_setpos(fly->desc.ctx, fly->card, (XentPoint) {0.0f, 0.0f});
	xent_layout(fly->desc.ctx, fly->card, fly->desc.width, height);

	float center = point.y + target->h * scale * 0.5f;
	float x      = pfly_constrain(( float ) point.x, fly->desc.width * scale, work.left, work.right, false);
	float y      = pfly_constrain(
	  center - (FLUX_PFLY_BORDER + fly->geom.body_h * 0.5f) * scale, height * scale, work.top, work.bottom, true
	);
	flux_popup_setsize(fly->popup, fly->desc.width, height);
	flux_popup_setsplit(fly->popup, height * 0.5f, (center - y) / scale - height * 0.5f);
	return (FluxRect) {x, y, 0.0f, 0.0f};
}

void flux_pflyout_show(FluxPickerFlyout *fly, FluxRect const *target) {
	if (!fly || fly->shown || fly->geom.columns == 0) return;
	FluxRect anchor = pfly_place(fly, target);
	flux_popup_show(fly->popup, anchor, FLUX_PLACEMENT_BOTTOM);

	fly->shown        = true;
	fly->return_focus = flux_input_focus(fly->desc.input);
	flux_input_setmodal(fly->desc.input, fly->card, pfly_escape, fly);
	flux_input_setfocus(fly->desc.input, fly->wheel [0], true);
	pfly_repaint(fly);
}

void flux_pflyout_hide(FluxPickerFlyout *fly) {
	if (!fly || !fly->shown) return;
	fly->shown = false;
	flux_input_setmodal(fly->desc.input, XENT_NODE_INVALID, NULL, NULL);
	flux_popup_dismiss(fly->popup);
	if (fly->return_focus != XENT_NODE_INVALID) flux_input_setfocus(fly->desc.input, fly->return_focus, true);
	pfly_repaint(fly);
}

void flux_pflyout_refresh(FluxPickerFlyout const *fly) {
	if (fly && fly->shown) InvalidateRect(flux_popup_hwnd(fly->popup), NULL, FALSE);
}
