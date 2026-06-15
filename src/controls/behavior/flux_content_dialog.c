#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI ContentDialog (ContentDialog_themeresources.xaml): card MinWidth 320,
 * MaxWidth 548, MinHeight 184, MaxHeight 756, corner 8, padding 24, button spacing
 * 8, title 20/SemiBold. The card is centered in a full-window scrim overlay; the
 * scrim node is rendered by flux_draw_content_dialog and the card/children are real
 * nodes. SurfaceStrokeColorDefault = #66757575. */
#define DLG_MIN_W       320.0f
#define DLG_MAX_W       548.0f
#define DLG_MIN_H       184.0f
#define DLG_MAX_H       756.0f
#define DLG_CORNER      8.0f
#define DLG_PADDING     24.0f
#define DLG_BTN_SPACING 8.0f
#define DLG_TITLE_GAP   12.0f
#define DLG_TITLE_SIZE  20.0f
#define DLG_TITLE_H     28.0f /* 20pt SemiBold line box (ContentDialogTitleMargin handles the 12 gap). */
#define DLG_BTN_H       32.0f /* standard button height; xent has no wrap-content so sizes are explicit. */

typedef struct FluxDialogRuntime FluxDialogRuntime;

typedef struct FluxDialogButton {
	FluxDialogRuntime *runtime;
	FluxDialogResult   result;
} FluxDialogButton;

struct FluxDialogRuntime {
	XentNodeId        root;
	XentNodeId        card;
	XentNodeId        content;
	XentNodeId        overlay_parent;
	XentNodeId        default_focus;
	XentContext      *ctx;
	FluxNodeStore    *store;
	FluxInput        *input;
	FluxWindow       *window;
	FluxThemeManager *theme;
	void              (*on_result)(void *ctx, FluxDialogResult result);
	void             *on_result_ctx;
	FluxDialogButton  buttons [3];
	bool              open;
	DWORD             anim_start; /**< Entrance animation start tick (GetTickCount). */
};

/* Entrance animation (DialogShowing): card scale 1.05->1.0 over 250ms with a
 * FastOutSlowIn (ease-out cubic) curve, opacity 0->1 over 83ms. Driven by a shared
 * thread timer because the dialog has no per-frame hook of its own. */
#define DLG_ANIM_MS  250.0f
#define DLG_FADE_MS  83.0f
#define DLG_MAX_ANIM 4

static FluxDialogRuntime *g_dlg_anim [DLG_MAX_ANIM];
static int                g_dlg_anim_count;
static UINT_PTR           g_dlg_timer;

static FluxDialogRuntime *dialog_runtime(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_CONTENT_DIALOG) return NULL;
	return ( FluxDialogRuntime * ) nd->component_data;
}

static void dialog_repaint(FluxDialogRuntime *rt) {
	HWND h = flux_window_hwnd(rt->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float dialog_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* WinUI animates ScaleTransform on the card (BackgroundElement) but Opacity on
 * LayoutRoot — i.e. the scrim fades in with the card, not just the card. So scale
 * goes on the card and opacity on the dialog root (whose subtree is scrim + card). */
static void  dialog_set_card_transform(FluxDialogRuntime *rt, float scale, float opacity) {
	FluxNodeData *card = flux_node_store_get(rt->store, rt->card);
	FluxNodeData *root = flux_node_store_get(rt->store, rt->root);
	if (card) card->render_scale = scale;
	if (root) root->render_opacity = opacity;
}

static void dialog_anim_remove(FluxDialogRuntime *rt) {
	for (int i = 0; i < g_dlg_anim_count; i++) {
		if (g_dlg_anim [i] != rt) continue;
		g_dlg_anim [i] = g_dlg_anim [--g_dlg_anim_count];
		break;
	}
	if (g_dlg_anim_count == 0 && g_dlg_timer) {
		KillTimer(NULL, g_dlg_timer);
		g_dlg_timer = 0;
	}
}

static void CALLBACK dialog_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD now) {
	( void ) hwnd;
	( void ) msg;
	( void ) id;
	( void ) now;
	for (int i = g_dlg_anim_count - 1; i >= 0; i--) {
		FluxDialogRuntime *rt      = g_dlg_anim [i];
		float              elapsed = ( float ) (GetTickCount() - rt->anim_start);
		float              t       = dialog_clamp01(elapsed / DLG_ANIM_MS);
		float              ease    = 1.0f - powf(1.0f - t, 3.0f);
		float              fade    = dialog_clamp01(elapsed / DLG_FADE_MS);
		dialog_set_card_transform(rt, 1.05f - 0.05f * ease, fade);
		dialog_repaint(rt);
		if (elapsed >= DLG_ANIM_MS) {
			dialog_set_card_transform(rt, 1.0f, 1.0f);
			dialog_anim_remove(rt);
		}
	}
}

static void dialog_anim_start(FluxDialogRuntime *rt) {
	dialog_set_card_transform(rt, 1.05f, 0.0f);
	rt->anim_start = GetTickCount();
	if (g_dlg_anim_count < DLG_MAX_ANIM) g_dlg_anim [g_dlg_anim_count++] = rt;
	if (!g_dlg_timer) g_dlg_timer = SetTimer(NULL, 0, 16, dialog_timer_proc);
}

static void dialog_close(FluxDialogRuntime *rt, FluxDialogResult result) {
	if (!rt->open) return;
	rt->open = false;
	dialog_anim_remove(rt);
	dialog_set_card_transform(rt, 1.0f, 1.0f);
	flux_input_set_modal(rt->input, XENT_NODE_INVALID, NULL, NULL);
	xent_remove_child(rt->ctx, rt->overlay_parent, rt->root);
	dialog_repaint(rt);
	if (rt->on_result) rt->on_result(rt->on_result_ctx, result);
}

static void dialog_escape(void *ctx) { dialog_close(( FluxDialogRuntime * ) ctx, FLUX_DIALOG_NONE); }

static void dialog_button_click(void *ctx) {
	FluxDialogButton *b = ( FluxDialogButton * ) ctx;
	if (b && b->runtime) dialog_close(b->runtime, b->result);
}

static void dialog_destroy(void *component_data) {
	dialog_anim_remove(( FluxDialogRuntime * ) component_data);
	free(component_data);
}

static void dialog_setup_root(XentContext *ctx, XentNodeId root) {
	/* Full-viewport overlay: absolute origin so an absolute-protocol host stacks it
	 * over the page rather than in flow; the scrim then dims the page behind it. */
	xent_set_absolute_position(ctx, root, (XentPoint) {0.0f, 0.0f});
	xent_set_size_percent(ctx, root, (XentSize) {1.0f, 1.0f});
	xent_set_protocol(ctx, root, XENT_PROTOCOL_GRID);
	XentGridSizeMode tri_mode [] = {XENT_GRID_STAR, XENT_GRID_AUTO, XENT_GRID_STAR};
	float            tri_val []  = {1.0f, 0.0f, 1.0f};
	xent_set_grid_rows(ctx, root, tri_mode, tri_val, 3);
	xent_set_grid_columns(ctx, root, tri_mode, tri_val, 3);
}

static XentNodeId dialog_make_card(XentContext *ctx, FluxNodeStore *store, XentNodeId root) {
	XentNodeId card = flux_factory_create_node(ctx, store, root, FLUX_CONTROL_CARD);
	xent_set_grid_row(ctx, card, 1);
	xent_set_grid_column(ctx, card, 1);
	xent_set_min_size(ctx, card, (XentSize) {DLG_MIN_W, DLG_MIN_H});
	xent_set_max_size(ctx, card, (XentSize) {DLG_MAX_W, DLG_MAX_H});
	xent_set_protocol(ctx, card, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, card, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, card, XENT_FLEX_ALIGN_STRETCH);
	return card;
}

static XentNodeId dialog_make_padded_region(
  XentContext *ctx, FluxNodeStore *store, XentNodeId card, FluxControlType type, XentFlexDirection dir, float gap
) {
	XentNodeId region = flux_factory_create_node(ctx, store, card, type);
	xent_set_protocol(ctx, region, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, region, dir);
	xent_set_gap(ctx, region, gap);
	xent_set_padding(ctx, region, (XentInsets) {DLG_PADDING, DLG_PADDING, DLG_PADDING, DLG_PADDING});
	return region;
}

static void dialog_add_button(
  FluxDialogRuntime *rt, XentNodeId command_area, char const *label, FluxDialogResult result, int slot, bool accent
) {
	if (!label) return;
	rt->buttons [slot].runtime = rt;
	rt->buttons [slot].result  = result;

	FluxButtonCreateInfo bi    = {rt->ctx, rt->store, command_area, label, dialog_button_click, &rt->buttons [slot]};
	XentNodeId           n     = flux_create_button(&bi);
	if (n == XENT_NODE_INVALID) return;
	if (accent) flux_button_set_style(rt->store, n, FLUX_BUTTON_ACCENT);
	xent_set_flex_grow(rt->ctx, n, 1.0f);
	if (rt->default_focus == XENT_NODE_INVALID) rt->default_focus = n;
}

/* xent has no wrap-content (auto-sized flex fills available space), so the card and
 * every region get explicit sizes. Width clamps to [320,548]; height is the content
 * region (padding + title + body) plus the fixed command bar, clamped to [184,756]. */
typedef struct DialogDims {
	float w;         /**< Card width. */
	float inner_w;   /**< Width inside the 24px padding. */
	float card_h;    /**< Total card height. */
	float content_h; /**< Content region height (card minus command bar). */
	float command_h; /**< Fixed command bar height. */
} DialogDims;

static DialogDims dialog_dims(FluxContentDialogCreateInfo const *info) {
	float w           = info->width > 0.0f ? info->width : DLG_MIN_W;
	w                 = w < DLG_MIN_W ? DLG_MIN_W : (w > DLG_MAX_W ? DLG_MAX_W : w);
	float title_block = info->title ? DLG_TITLE_H + DLG_TITLE_GAP : 0.0f;
	float command_h   = DLG_PADDING + DLG_BTN_H + DLG_PADDING;
	float card_h      = DLG_PADDING + title_block + info->content_height + DLG_PADDING + command_h;
	card_h            = card_h < DLG_MIN_H ? DLG_MIN_H : (card_h > DLG_MAX_H ? DLG_MAX_H : card_h);
	return (DialogDims) {w, w - DLG_PADDING * 2.0f, card_h, card_h - command_h, command_h};
}

/* Build the padded content region (optional title + caller's body host) under the card.
 * Explicit height (not flex-grow): grow absorbs xent's slightly-too-large free space and
 * shoves the command bar past the card bottom; a definite height keeps the bar flush. */
static XentNodeId
dialog_build_content_region(FluxContentDialogCreateInfo const *info, XentNodeId card, DialogDims const *d) {
	XentNodeId region = dialog_make_padded_region(
	  info->ctx, info->store, card, FLUX_CONTROL_DIALOG_CONTENT, XENT_FLEX_COLUMN, DLG_TITLE_GAP
	);
	xent_set_size(info->ctx, region, (XentSize) {d->w, d->content_h});
	if (info->title) {
		FluxTextCreateInfo ti = {info->ctx, info->store, region, info->title, DLG_TITLE_SIZE};
		XentNodeId         tn = flux_create_text(&ti);
		flux_text_set_weight(info->store, tn, FLUX_FONT_SEMI_BOLD);
		xent_set_size(info->ctx, tn, (XentSize) {d->inner_w, DLG_TITLE_H});
	}
	return region;
}

static void dialog_init_runtime(FluxDialogRuntime *rt, XentNodeId root, FluxContentDialogCreateInfo const *info) {
	rt->root           = root;
	rt->overlay_parent = info->overlay_parent;
	rt->ctx            = info->ctx;
	rt->store          = info->store;
	rt->input          = info->input;
	rt->window         = info->window;
	rt->theme          = info->theme;
	rt->on_result      = info->on_result;
	rt->on_result_ctx  = info->userdata;
	rt->default_focus  = XENT_NODE_INVALID;
}

XentNodeId flux_create_content_dialog(FluxContentDialogCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTENT_DIALOG);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, root);
	FluxDialogRuntime *rt = nd ? ( FluxDialogRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return root;
	}
	dialog_init_runtime(rt, root, info);

	DialogDims d = dialog_dims(info);
	dialog_setup_root(info->ctx, root);
	rt->card = dialog_make_card(info->ctx, info->store, root);
	xent_set_size(info->ctx, rt->card, (XentSize) {d.w, d.card_h});

	XentNodeId content_region = dialog_build_content_region(info, rt->card, &d);
	rt->content = flux_factory_create_node(info->ctx, info->store, content_region, FLUX_CONTROL_CONTAINER);
	xent_set_size(info->ctx, rt->content, (XentSize) {d.inner_w, info->content_height});

	XentNodeId command_area = dialog_make_padded_region(
	  info->ctx, info->store, rt->card, FLUX_CONTROL_CONTAINER, XENT_FLEX_ROW, DLG_BTN_SPACING
	);
	xent_set_size(info->ctx, command_area, (XentSize) {d.w, d.command_h});
	dialog_add_button(rt, command_area, info->primary_text, FLUX_DIALOG_PRIMARY, 0, true);
	dialog_add_button(rt, command_area, info->secondary_text, FLUX_DIALOG_SECONDARY, 1, false);
	dialog_add_button(rt, command_area, info->close_text, FLUX_DIALOG_NONE, 2, false);

	nd->component_data         = rt;
	nd->destroy_component_data = dialog_destroy;

	if (info->out_content) *info->out_content = rt->content;
	return root;
}

void flux_content_dialog_show(FluxNodeStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (!rt || rt->open) return;

	FluxThemeColors const *t = rt->theme ? flux_theme_colors(rt->theme) : flux_theme_default_colors();
	flux_node_set_background(store, rt->card, t->solid_background);
	flux_node_set_border(store, rt->card, flux_color_rgba(0x75, 0x75, 0x75, 0x66), 1.0f);
	flux_node_set_corner_radius(store, rt->card, DLG_CORNER);

	xent_append_child(rt->ctx, rt->overlay_parent, rt->root);
	flux_input_set_modal(rt->input, rt->card, dialog_escape, rt);
	if (rt->default_focus != XENT_NODE_INVALID) flux_input_set_focus(rt->input, rt->default_focus);
	rt->open = true;
	dialog_anim_start(rt);
	dialog_repaint(rt);
}

void flux_content_dialog_hide(FluxNodeStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	if (rt) dialog_close(rt, FLUX_DIALOG_NONE);
}

XentNodeId flux_content_dialog_content_node(FluxNodeStore *store, XentNodeId dialog) {
	FluxDialogRuntime *rt = dialog_runtime(store, dialog);
	return rt ? rt->content : XENT_NODE_INVALID;
}
