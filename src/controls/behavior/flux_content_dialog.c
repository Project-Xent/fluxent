#include "controls/flux_module.h"
#include "a11y/flux_uia_ids.h"
#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_anim_driver.h"

#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_popup.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_window.h"
#include "input/flux_input_internal.h"
#include "runtime/flux_str.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define DLG_MIN_W       320.0f
#define DLG_MAX_W       548.0f
#define DLG_MIN_H       184.0f
#define DLG_MAX_H       756.0f
#define DLG_CORNER      8.0f
#define DLG_PADDING     24.0f
#define DLG_BTN_SPACING 8.0f
#define DLG_TITLE_GAP   12.0f
#define DLG_TITLE_SIZE  20.0f
#define DLG_TITLE_H     28.0f
#define DLG_BTN_H       32.0f

typedef struct FluxDialogRuntime FluxDialogRuntime;

typedef struct FluxDialogButton {
	FluxDialogRuntime *runtime;
	FluxDialogResult   result;
	XentNodeId         node;
} FluxDialogButton;

struct FluxDialogRuntime {
	XentNodeId       root;
	XentNodeId       layer;
	XentNodeId       card;
	XentNodeId       content;
	XentNodeId       overlay_parent;
	XentNodeId       default_focus;
	XentNodeId       return_focus;
	XtkDialogButton  default_button;
	XentCtx         *ctx;
	FluxStore       *store;
	FluxInput       *input;
	FluxWindow      *window;
	void             (*on_result)(void *ctx, FluxDialogResult result);
	void            *on_result_ctx;
	FluxDialogButton buttons [3];
	char            *title;
	char            *primary;
	char            *secondary;
	char            *close;
	float            width;
	float            content_height;
	bool             open;
	DWORD            anim_start;
};

#define DLG_ANIM_MS 250.0f
#define DLG_FADE_MS 83.0f

static FluxDialogRuntime *dialog_runtime(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_CONTENT_DIALOG) return NULL;
	return ( FluxDialogRuntime * ) nd->component_data;
}

static void dialog_repaint(FluxDialogRuntime *rt) {
	HWND h = flux_window_hwnd(rt->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float dialog_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static void  dialog_set_card_transform(FluxDialogRuntime *rt, float scale, float opacity) {
	FluxNodeData *card  = flux_store_find(rt->store, rt->card);
	FluxNodeData *layer = flux_store_find(rt->store, rt->layer);
	if (card) card->render_scale = scale;
	if (layer) layer->render_opacity = opacity;
}

static void dialog_anim_remove(FluxDialogRuntime *rt) { flux_anim_unregister(rt); }

static void dialog_park(FluxDialogRuntime *rt) {
	if (rt && !rt->open) xent_setcollapsed(rt->ctx, rt->layer, true);
}

static void dialog_ensure_on_overlay(FluxDialogRuntime *rt) {
	if (!rt || rt->overlay_parent == XENT_NODE_INVALID) return;
	xent_setcollapsed(rt->ctx, rt->layer, false);
	if (xent_node_parent(rt->ctx, rt->layer) != rt->overlay_parent)
		xent_node_append(rt->ctx, rt->overlay_parent, rt->layer);
}

static bool dialog_step(void *ctx, unsigned long now) {
	FluxDialogRuntime *rt      = ( FluxDialogRuntime * ) ctx;
	float              elapsed = ( float ) (( unsigned long ) now - rt->anim_start);
	float              t       = dialog_clamp01(elapsed / DLG_ANIM_MS);
	float              ease    = 1.0f - powf(1.0f - t, 3.0f);
	float              fade    = dialog_clamp01(elapsed / DLG_FADE_MS);
	dialog_set_card_transform(rt, 1.05f - 0.05f * ease, fade);
	dialog_repaint(rt);
	if (elapsed >= DLG_ANIM_MS) {
		dialog_set_card_transform(rt, 1.0f, 1.0f);
		return false;
	}
	return true;
}

static void dialog_anim_start(FluxDialogRuntime *rt) {
	dialog_set_card_transform(rt, 1.05f, 0.0f);
	rt->anim_start = flux_anim_now();
	flux_anim_register(rt, dialog_step);
}

static void dialog_close(FluxDialogRuntime *rt, FluxDialogResult result, XtkEventCause cause) {
	if (!rt->open) return;
	rt->open = false;
	dialog_anim_remove(rt);
	dialog_set_card_transform(rt, 1.0f, 1.0f);
	flux_input_setmodal(rt->input, XENT_NODE_INVALID, NULL, NULL);
	XentNodeId target = input_focus_available(rt->ctx, rt->return_focus) ? rt->return_focus : XENT_NODE_INVALID;
	flux_input_setfocus(rt->input, target, true);
	dialog_park(rt);
	dialog_repaint(rt);
	XtkHost *host = flux_store_host(rt->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_CLOSE,
		  .target         = ( XtkCtrl ) rt->root,
		  .source         = ( XtkCtrl ) rt->root,
		  .current_target = ( XtkCtrl ) rt->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = cause,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = result},
		};
		flux_host_route(host, &event);
	}
	if (rt->on_result) rt->on_result(rt->on_result_ctx, result);
}

static void dialog_escape(void *ctx) {
	dialog_close(( FluxDialogRuntime * ) ctx, FLUX_DIALOG_NONE, XTK_EVENT_CAUSE_USER);
}

static void dialog_button_click(void *ctx) {
	FluxDialogButton *b = ( FluxDialogButton * ) ctx;
	if (b && b->runtime) dialog_close(b->runtime, b->result, XTK_EVENT_CAUSE_USER);
}

static XentNodeId dialog_default_node(FluxDialogRuntime const *rt) {
	if (rt->default_button < XTK_DIALOG_BUTTON_PRIMARY || rt->default_button > XTK_DIALOG_BUTTON_CLOSE)
		return XENT_NODE_INVALID;
	return rt->buttons [rt->default_button - XTK_DIALOG_BUTTON_PRIMARY].node;
}

static bool dialog_enter(void *ctx) {
	FluxDialogRuntime *rt   = ctx;
	XentNodeId         node = dialog_default_node(rt);
	if (node == XENT_NODE_INVALID || !xent_sem_enabled(rt->ctx, node)) return false;
	dialog_button_click(&rt->buttons [rt->default_button - XTK_DIALOG_BUTTON_PRIMARY]);
	return true;
}

static void dialog_set_modal(FluxDialogRuntime *rt) {
	flux_input_setmodal(rt->input, rt->card, dialog_escape, rt);
	flux_input_setmodal_enter(rt->input, dialog_enter);
}

void flux_dialog_setdefaultbutton(FluxStore *store, XentNodeId dialog, XtkDialogButton button) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt || rt->default_button == button) return;
	XentNodeId old     = dialog_default_node(rt);
	rt->default_button = button;
	XentNodeId next    = dialog_default_node(rt);
	if (old != XENT_NODE_INVALID) flux_prop_button_style(store, old, FLUX_BUTTON_STANDARD);
	if (next != XENT_NODE_INVALID) flux_prop_button_style(store, next, FLUX_BUTTON_ACCENT);
	dialog_repaint(rt);
}

static void dialog_destroy(void *component_data) {
	FluxDialogRuntime *rt = ( FluxDialogRuntime * ) component_data;
	dialog_anim_remove(rt);
	flux_str_free(rt->title);
	flux_str_free(rt->primary);
	flux_str_free(rt->secondary);
	flux_str_free(rt->close);
	free(rt);
}

static void dialog_uia_close(FluxStore *store, XentNodeId layer) {
	FluxNodeData *nd = flux_store_find(store, layer);
	if (nd && nd->component_data)
		dialog_close(( FluxDialogRuntime * ) nd->component_data, FLUX_DIALOG_NONE, XTK_EVENT_CAUSE_ACCESSIBILITY);
}

static void dialog_uia_name(FluxStore *store, XentNodeId layer, char *out, size_t size) {
	FluxNodeData            *nd = flux_store_find(store, layer);
	FluxDialogRuntime const *rt = nd ? ( FluxDialogRuntime const * ) nd->component_data : NULL;
	if (rt && rt->title) snprintf(out, size, "%s", rt->title);
}

static FluxUiaHooks const dialog_layer_uia = {
  .role     = FLUX_UIA_WINDOW,
  .patterns = FLUX_UIA_P_WINDOW,
  .close    = dialog_uia_close,
  .name     = dialog_uia_name,
  .modal    = true,
  .dialog   = true};

static XentNodeId dialog_make_layer(FluxContentDialogDesc const *info) {
	XentNodeId layer = flux_factory_create_node(info->ctx, info->store, info->overlay_parent, FLUX_CONTROL_CONTAINER);
	if (layer == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	flux_prop_node_background(info->store, layer, flux_color_rgba(0, 0, 0, 0x4d));
	xent_setpos(info->ctx, layer, (XentPoint) {0.0f, 0.0f});
	xent_setsizepct(info->ctx, layer, (XentSize) {1.0f, 1.0f});
	xent_setproto(info->ctx, layer, XENT_PROTOCOL_GRID);
	XentGridSizeMode tri_mode [] = {XENT_GRID_STAR, XENT_GRID_AUTO, XENT_GRID_STAR};
	float            tri_val []  = {1.0f, 0.0f, 1.0f};
	xent_grid_setrows(info->ctx, layer, tri_mode, tri_val, 3);
	xent_grid_setcols(info->ctx, layer, tri_mode, tri_val, 3);
	return layer;
}

static XentNodeId dialog_make_card(XentCtx *ctx, FluxStore *store, XentNodeId root) {
	XentNodeId    card = flux_factory_create_node(ctx, store, root, FLUX_CONTROL_CARD);
	FluxNodeData *nd   = flux_store_find(store, card);
	if (nd) nd->visuals.dialog_surface = FLUX_DIALOG_SURFACE_CARD;
	flux_prop_radius(store, card, DLG_CORNER);
	xent_grid_setrow(ctx, card, 1);
	xent_grid_setcol(ctx, card, 1);
	xent_setminsize(ctx, card, (XentSize) {DLG_MIN_W, DLG_MIN_H});
	xent_setmaxsize(ctx, card, (XentSize) {DLG_MAX_W, DLG_MAX_H});
	xent_setproto(ctx, card, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, card, XENT_FLEX_COLUMN);
	xent_setitems(ctx, card, XENT_FLEX_ALIGN_STRETCH);
	return card;
}

static XentNodeId dialog_make_padded_region(
  XentCtx *ctx, FluxStore *store, XentNodeId card, FluxControlType type, XentFlexDirection dir, float gap
) {
	XentNodeId region = flux_factory_create_node(ctx, store, card, type);
	xent_setproto(ctx, region, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, region, dir);
	xent_setgap(ctx, region, gap);
	xent_setp(ctx, region, (XentInsets) {DLG_PADDING, DLG_PADDING, DLG_PADDING, DLG_PADDING});
	return region;
}

static void dialog_add_button(
  FluxDialogRuntime *rt, XentNodeId command_area, char const *label, FluxDialogResult result, int slot
) {
	rt->buttons [slot].node = XENT_NODE_INVALID;
	if (!label || !label [0]) return;
	rt->buttons [slot].runtime = rt;
	rt->buttons [slot].result  = result;

	FluxButtonDesc bi          = {rt->ctx, rt->store, command_area, label, dialog_button_click, &rt->buttons [slot]};
	XentNodeId     n           = flux_button_init(&bi);
	if (n == XENT_NODE_INVALID) return;
	rt->buttons [slot].node = n;
	if (n == dialog_default_node(rt)) flux_prop_button_style(rt->store, n, FLUX_BUTTON_ACCENT);
	xent_setgrow(rt->ctx, n, 1.0f);
	if (rt->default_focus == XENT_NODE_INVALID) rt->default_focus = n;
}

typedef struct DialogDims {
	float w;
	float inner_w;
	float card_h;
	float content_h;
	float command_h;
} DialogDims;

static DialogDims dialog_dims(FluxContentDialogDesc const *info) {
	float w           = info->width > 0.0f ? info->width : DLG_MIN_W;
	w                 = w < DLG_MIN_W ? DLG_MIN_W : (w > DLG_MAX_W ? DLG_MAX_W : w);
	float title_block = info->title ? DLG_TITLE_H + DLG_TITLE_GAP : 0.0f;
	float command_h   = DLG_PADDING + DLG_BTN_H + DLG_PADDING;
	float card_h      = DLG_PADDING + title_block + info->content_height + DLG_PADDING + command_h;
	card_h            = card_h < DLG_MIN_H ? DLG_MIN_H : (card_h > DLG_MAX_H ? DLG_MAX_H : card_h);
	return (DialogDims) {w, w - DLG_PADDING * 2.0f, card_h, card_h - command_h, command_h};
}

static XentNodeId dialog_build_content_region(FluxContentDialogDesc const *info, XentNodeId card, DialogDims const *d) {
	XentNodeId region = dialog_make_padded_region(
	  info->ctx, info->store, card, FLUX_CONTROL_DIALOG_CONTENT, XENT_FLEX_COLUMN, DLG_TITLE_GAP
	);
	xent_setsize(info->ctx, region, (XentSize) {d->w, d->content_h});
	if (info->title) {
		FluxTextDesc ti = {info->ctx, info->store, region, info->title, DLG_TITLE_SIZE};
		XentNodeId   tn = flux_text_init(&ti);
		flux_prop_text_weight(info->store, tn, FLUX_FONT_SEMI_BOLD);
		xent_setsize(info->ctx, tn, (XentSize) {d->inner_w, DLG_TITLE_H});
	}
	return region;
}

static void dialog_init_runtime(FluxDialogRuntime *rt, XentNodeId root, FluxContentDialogDesc const *info) {
	rt->root           = root;
	rt->overlay_parent = info->overlay_parent;
	rt->ctx            = info->ctx;
	rt->store          = info->store;
	rt->input          = info->input;
	rt->window         = info->window;
	rt->on_result      = info->on_result;
	rt->on_result_ctx  = info->userdata;
	rt->default_focus  = XENT_NODE_INVALID;
	rt->title          = flux_str_dup(info->title);
	rt->primary        = flux_str_dup(info->primary_text);
	rt->secondary      = flux_str_dup(info->secondary_text);
	rt->close          = flux_str_dup(info->close_text);
	rt->width          = info->width;
	rt->content_height = info->content_height;
}

static void               dialog_build_card(FluxDialogRuntime *rt);

static FluxDialogRuntime *dialog_create_runtime(FluxContentDialogDesc const *info, XentNodeId root) {
	XentNodeId layer = dialog_make_layer(info);
	if (layer == XENT_NODE_INVALID) return NULL;
	FluxDialogRuntime *rt = calloc(1, sizeof(*rt));
	if (!rt) {
		xent_node_destroy(info->ctx, layer);
		return NULL;
	}
	dialog_init_runtime(rt, root, info);
	rt->layer                        = layer;
	FluxNodeData *layer_nd           = flux_store_find(info->store, layer);
	layer_nd->component_data         = rt;
	layer_nd->uia                    = &dialog_layer_uia;
	layer_nd->visuals.dialog_surface = FLUX_DIALOG_SURFACE_SMOKE;
	return rt;
}

XentNodeId flux_dialog_init(FluxContentDialogDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	XentNodeId root = flux_factory_create_node(info->ctx, info->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTENT_DIALOG);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	FluxNodeData *nd = flux_store_find(info->store, root);
	if (!nd) return root;
	FluxDialogRuntime *rt = dialog_create_runtime(info, root);
	if (!rt) return root;
	xent_setcollapsed(info->ctx, root, true);

	rt->content = flux_factory_create_node(info->ctx, info->store, rt->layer, FLUX_CONTROL_CONTAINER);
	dialog_build_card(rt);

	nd->component_data         = rt;
	nd->destroy_component_data = dialog_destroy;

	if (info->out_content) *info->out_content = rt->content;
	dialog_park(rt);
	return root;
}

void flux_dialog_show(FluxStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt || rt->open) return;
	rt->return_focus = flux_input_focus(rt->input);

	dialog_ensure_on_overlay(rt);
	xent_node_move(rt->ctx, rt->overlay_parent, rt->layer, XENT_NODE_INVALID);
	dialog_set_modal(rt);
	rt->open = true;
	dialog_anim_start(rt);
	dialog_repaint(rt);
	XentNodeId start = dialog_default_node(rt);
	if (flux_input_focuswithin(rt->input, rt->content, true)) return;
	if (start == XENT_NODE_INVALID || !xent_sem_enabled(rt->ctx, start)) start = rt->default_focus;
	flux_input_setfocus(rt->input, start, true);
}

void flux_dialog_hide(FluxStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (rt) dialog_close(rt, FLUX_DIALOG_NONE, XTK_EVENT_CAUSE_PROGRAM);
}

void flux_dialog_unmount(FluxStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt) return;
	dialog_close(rt, FLUX_DIALOG_NONE, XTK_EVENT_CAUSE_PROGRAM);
	xent_node_destroy(rt->ctx, rt->layer);
	rt->layer = rt->card = rt->content = rt->default_focus = XENT_NODE_INVALID;
}

void flux_dialog_retarget(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *nd = flux_store_find(store, dst);
	if (!nd || !nd->component_data) return;
	FluxDialogRuntime *rt = ( FluxDialogRuntime * ) nd->component_data;
	rt->root              = dst;
	rt->ctx               = ctx;
	rt->store             = store;
}

void flux_dialog_sync_host(FluxStore *store, XentNodeId dialog, bool open) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt) return;
	if (open)
		if (!rt->open) flux_dialog_show(store, dialog);
		else dialog_ensure_on_overlay(rt);
	else if (rt->open) flux_dialog_hide(store, dialog);
	else dialog_park(rt);
}

static void dialog_build_card(FluxDialogRuntime *rt) {
	FluxContentDialogDesc info = {
	  .ctx            = rt->ctx,
	  .store          = rt->store,
	  .title          = rt->title,
	  .primary_text   = rt->primary,
	  .secondary_text = rt->secondary,
	  .close_text     = rt->close,
	  .width          = rt->width,
	  .content_height = rt->content_height,
	};
	DialogDims d = dialog_dims(&info);
	rt->card     = dialog_make_card(rt->ctx, rt->store, rt->layer);
	xent_setsize(rt->ctx, rt->card, (XentSize) {d.w, d.card_h});
	XentNodeId content_region = dialog_build_content_region(&info, rt->card, &d);
	xent_node_append(rt->ctx, content_region, rt->content);
	xent_setsize(rt->ctx, rt->content, (XentSize) {d.inner_w, rt->content_height});
	XentNodeId command_area
	  = dialog_make_padded_region(rt->ctx, rt->store, rt->card, FLUX_CONTROL_CONTAINER, XENT_FLEX_ROW, DLG_BTN_SPACING);
	xent_setsize(rt->ctx, command_area, (XentSize) {d.w, d.command_h});
	rt->default_focus = XENT_NODE_INVALID;
	dialog_add_button(rt, command_area, rt->primary, FLUX_DIALOG_PRIMARY, 0);
	dialog_add_button(rt, command_area, rt->secondary, FLUX_DIALOG_SECONDARY, 1);
	dialog_add_button(rt, command_area, rt->close, FLUX_DIALOG_NONE, 2);
}

static bool dialog_text_equal(char const *a, char const *b) {
	if (!a || !a [0]) a = NULL;
	if (!b || !b [0]) b = NULL;
	return a == b || (a && b && strcmp(a, b) == 0);
}

void flux_dialog_configure(
  FluxStore *store, XentNodeId dialog, char const *title, char const *primary, char const *secondary, char const *close,
  float width, float content_height
) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt) return;
	if (dialog_text_equal(rt->title, title)
	    && dialog_text_equal(rt->primary, primary)
	    && dialog_text_equal(rt->secondary, secondary)
	    && dialog_text_equal(rt->close, close)
	    && rt->width == width
	    && rt->content_height == content_height)
		return;
	flux_str_replace(&rt->title, title);
	flux_str_replace(&rt->primary, primary);
	flux_str_replace(&rt->secondary, secondary);
	flux_str_replace(&rt->close, close);
	rt->width          = width;
	rt->content_height = content_height;
	xent_node_remove(rt->ctx, xent_node_parent(rt->ctx, rt->content), rt->content);
	xent_node_destroy(rt->ctx, rt->card);
	dialog_build_card(rt);
	if (!rt->open) return;
	dialog_ensure_on_overlay(rt);
	dialog_set_modal(rt);
}

XentNodeId flux_dialogbody_node(FluxStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	return rt ? rt->content : XENT_NODE_INVALID;
}
