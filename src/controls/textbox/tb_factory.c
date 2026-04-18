/**
 * @file tb_factory.c
 * @brief Factory functions for TextBox, PasswordBox, and NumberBox.
 */
#include "tb_internal.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_engine.h"
#include <stdlib.h>
#include <string.h>

/* External renderer declarations */
extern void
flux_draw_textbox(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void flux_draw_password_box(
  FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *
);
extern void
flux_draw_number_box(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);

/* Helper to create a node with parent */
static XentNodeId
tb_create_node_with_parent(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, XentControlType type) {
	XentNodeId node = xent_create_node(ctx);
	if (node == XENT_NODE_INVALID) return node;

	xent_set_control_type(ctx, node, type);

	if (parent != XENT_NODE_INVALID) xent_append_child(ctx, parent, node);

	flux_node_store_get_or_create(store, node);
	xent_set_userdata(ctx, node, flux_node_store_get(store, node));

	xent_set_focusable(ctx, node, true);
	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   TextBox
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_textbox(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *placeholder,
  void (*on_change)(void *, char const *), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_TEXT_INPUT, flux_draw_textbox, NULL);

	XentNodeId node = tb_create_node_with_parent(ctx, store, parent, XENT_CONTROL_TEXT_INPUT);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) calloc(1, sizeof(FluxTextBoxInputData));
				if (tb) {
					tb->buf_cap            = FLUX_TEXTBOX_INITIAL_CAP;
					tb->buffer             = ( char * ) calloc(1, tb->buf_cap);
					tb->buf_len            = 0;
					tb->buffer [0]         = '\0';
					tb->base.content       = tb->buffer;
					tb->base.placeholder   = placeholder;
					tb->base.font_size     = 14.0f;
					tb->base.enabled       = true;
					tb->base.on_change     = on_change;
					tb->base.on_change_ctx = userdata;
					tb->ctx                = ctx;
					tb->node               = node;
					tb->store              = store;
					tb->app                = NULL;
				}
			nd->component_data                  = &tb->base;
			nd->behavior.on_pointer_move        = tb_on_pointer_move;
			nd->behavior.on_pointer_move_ctx    = tb;
			nd->behavior.on_focus               = tb_on_focus;
			nd->behavior.on_focus_ctx           = tb;
			nd->behavior.on_blur                = tb_on_blur;
			nd->behavior.on_blur_ctx            = tb;
			nd->behavior.on_key                 = tb_on_key;
			nd->behavior.on_key_ctx             = tb;
			nd->behavior.on_char                = tb_on_char;
			nd->behavior.on_char_ctx            = tb;
			nd->behavior.on_pointer_down        = tb_on_pointer_down;
			nd->behavior.on_pointer_down_ctx    = tb;
			nd->behavior.on_ime_composition     = tb_on_ime_composition;
			nd->behavior.on_ime_composition_ctx = tb;
			nd->behavior.on_context_menu        = tb_on_context_menu;
			nd->behavior.on_context_menu_ctx    = tb;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
	if (placeholder) xent_set_semantic_label(ctx, node, placeholder);

	return node;
}

XentNodeId flux_app_create_textbox(
  FluxApp *app, XentNodeId parent, char const *placeholder, void (*on_change)(void *, char const *), void *userdata
) {
	if (!app) return XENT_NODE_INVALID;

	XentContext   *ctx   = flux_app_get_context(app);
	FluxNodeStore *store = flux_app_get_store(app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	XentNodeId node = flux_create_textbox(ctx, store, parent, placeholder, on_change, userdata);
	if (node == XENT_NODE_INVALID) return node;

	/* Wire the app pointer so text renderer is accessible */
	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd && nd->component_data) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) nd->component_data;
			tb->app                  = app;
		}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   PasswordBox
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_password_box(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *placeholder,
  void (*on_change)(void *, char const *), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_PASSWORD_BOX, flux_draw_password_box, NULL);

	XentNodeId node = tb_create_node_with_parent(ctx, store, parent, XENT_CONTROL_PASSWORD_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) calloc(1, sizeof(FluxTextBoxInputData));
				if (tb) {
					tb->buf_cap            = FLUX_TEXTBOX_INITIAL_CAP;
					tb->buffer             = ( char * ) calloc(1, tb->buf_cap);
					tb->buf_len            = 0;
					tb->buffer [0]         = '\0';
					tb->base.content       = tb->buffer;
					tb->base.placeholder   = placeholder;
					tb->base.font_size     = 14.0f;
					tb->base.enabled       = true;
					tb->base.on_change     = on_change;
					tb->base.on_change_ctx = userdata;
					tb->ctx                = ctx;
					tb->node               = node;
					tb->store              = store;
					tb->app                = NULL;
				}
			/* For PasswordBox, we wrap the component_data in a FluxPasswordBoxData.
			   But since the renderer uses flux_draw_textbox and casts to FluxTextBoxData*,
			   we keep using FluxTextBoxInputData and just set the control type. The
			   mask rendering is handled specially in the renderer. */
			nd->component_data                  = &tb->base;
			nd->behavior.on_pointer_move        = tb_on_pointer_move;
			nd->behavior.on_pointer_move_ctx    = tb;
			nd->behavior.on_focus               = tb_on_focus;
			nd->behavior.on_focus_ctx           = tb;
			nd->behavior.on_blur                = tb_on_blur;
			nd->behavior.on_blur_ctx            = tb;
			nd->behavior.on_key                 = tb_on_key;
			nd->behavior.on_key_ctx             = tb;
			nd->behavior.on_char                = tb_on_char;
			nd->behavior.on_char_ctx            = tb;
			nd->behavior.on_pointer_down        = tb_on_pointer_down;
			nd->behavior.on_pointer_down_ctx    = tb;
			nd->behavior.on_ime_composition     = tb_on_ime_composition;
			nd->behavior.on_ime_composition_ctx = tb;
			nd->behavior.on_context_menu        = tb_on_context_menu;
			nd->behavior.on_context_menu_ctx    = tb;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
	if (placeholder) xent_set_semantic_label(ctx, node, placeholder);
	xent_set_focusable(ctx, node, true);

	/* Grid layout: WinUI PasswordBox template
	   3 rows (Auto, *, Auto) simplified to 1 row Star (no Header/Description yet)
	   2 columns: *(text area), Auto(RevealButton Width=30) → Pixel(30) since no child nodes */
	xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
	{
		XentGridSizeMode row_modes [] = {XENT_GRID_STAR};
		float            row_vals []  = {1.0f};
		xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

		XentGridSizeMode col_modes [] = {XENT_GRID_STAR, XENT_GRID_PIXEL};
		float            col_vals []  = {1.0f, 30.0f};
		xent_set_grid_columns(ctx, node, col_modes, col_vals, 2);
	}

	return node;
}

XentNodeId flux_app_create_password_box(
  FluxApp *app, XentNodeId parent, char const *placeholder, void (*on_change)(void *, char const *), void *userdata
) {
	if (!app) return XENT_NODE_INVALID;

	XentContext   *ctx   = flux_app_get_context(app);
	FluxNodeStore *store = flux_app_get_store(app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	XentNodeId node = flux_create_password_box(ctx, store, parent, placeholder, on_change, userdata);
	if (node == XENT_NODE_INVALID) return node;

	/* Wire the app pointer so text renderer is accessible */
	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd && nd->component_data) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) nd->component_data;
			tb->app                  = app;
		}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   NumberBox
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_number_box(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, double min_val, double max_val, double step,
  void (*on_value_change)(void *, double), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_NUMBER_BOX, flux_draw_number_box, NULL);

	XentNodeId node = tb_create_node_with_parent(ctx, store, parent, XENT_CONTROL_NUMBER_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) calloc(1, sizeof(FluxTextBoxInputData));
				if (tb) {
					tb->buf_cap                = FLUX_TEXTBOX_INITIAL_CAP;
					tb->buffer                 = ( char * ) calloc(1, tb->buf_cap);
					tb->buf_len                = 0;
					tb->base.font_size         = 14.0f;
					tb->base.enabled           = true;
					tb->ctx                    = ctx;
					tb->node                   = node;
					tb->store                  = store;
					tb->app                    = NULL;

					/* NumberBox-specific initialization (WinUI defaults) */
					tb->nb_minimum             = min_val;
					tb->nb_maximum             = max_val;
					tb->nb_small_change        = step;
					tb->nb_large_change        = step * 10.0;
					tb->nb_value               = min_val; /* start at minimum (not NaN) */
					tb->nb_is_wrap_enabled     = false;
					tb->nb_spin_placement      = 2;       /* FLUX_NB_SPIN_INLINE — visible by default for our API */
					tb->nb_validation          = 0;       /* InvalidInputOverwritten */
					tb->nb_on_value_change     = on_value_change;
					tb->nb_on_value_change_ctx = userdata;

					/* Initialize text from value */
					nb_update_text_to_value(tb);
				}
			nd->component_data                  = &tb->base;
			nd->behavior.on_pointer_move        = tb_on_pointer_move;
			nd->behavior.on_pointer_move_ctx    = tb;
			nd->behavior.on_focus               = tb_on_focus;
			nd->behavior.on_focus_ctx           = tb;
			nd->behavior.on_blur                = tb_on_blur;
			nd->behavior.on_blur_ctx            = tb;
			nd->behavior.on_key                 = tb_on_key;
			nd->behavior.on_key_ctx             = tb;
			nd->behavior.on_char                = tb_on_char;
			nd->behavior.on_char_ctx            = tb;
			nd->behavior.on_pointer_down        = tb_on_pointer_down;
			nd->behavior.on_pointer_down_ctx    = tb;
			nd->behavior.on_ime_composition     = tb_on_ime_composition;
			nd->behavior.on_ime_composition_ctx = tb;
			nd->behavior.on_context_menu        = tb_on_context_menu;
			nd->behavior.on_context_menu_ctx    = tb;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
	xent_set_semantic_value(ctx, node, ( float ) min_val, ( float ) min_val, ( float ) max_val);
	xent_set_semantic_expanded(ctx, node, true); /* Inline spin visible */
	xent_set_focusable(ctx, node, true);

	/* Grid layout */
	xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
	{
		XentGridSizeMode row_modes [] = {XENT_GRID_STAR};
		float            row_vals []  = {1.0f};
		xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

		XentGridSizeMode col_modes [] = {XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL};
		float            col_vals []  = {1.0f, 40.0f, 36.0f};
		xent_set_grid_columns(ctx, node, col_modes, col_vals, 3);
	}

	return node;
}

XentNodeId flux_app_create_number_box(
  FluxApp *app, XentNodeId parent, double min_val, double max_val, double step, void (*on_value_change)(void *, double),
  void *userdata
) {
	if (!app) return XENT_NODE_INVALID;

	XentContext   *ctx   = flux_app_get_context(app);
	FluxNodeStore *store = flux_app_get_store(app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	XentNodeId node = flux_create_number_box(ctx, store, parent, min_val, max_val, step, on_value_change, userdata);
	if (node == XENT_NODE_INVALID) return node;

	/* Wire the app pointer so text renderer is accessible */
	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd && nd->component_data) {
			FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) nd->component_data;
			tb->app                  = app;
		}

	return node;
}
