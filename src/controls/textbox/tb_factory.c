#include "tb_internal.h"
#include "controls/draw/flux_control_draw.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_engine.h"
#include <stdlib.h>
#include <string.h>

typedef void (*TbDrawFn)(
  FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *
);

typedef void (*TbChangeFn)(void *, char const *);

typedef struct TbTextControlSpec {
	XentContext    *ctx;
	FluxNodeStore  *store;
	XentNodeId      parent;
	XentControlType type;
	TbDrawFn        draw;
	char const     *placeholder;
	TbChangeFn      on_change;
	void           *userdata;
} TbTextControlSpec;

typedef struct TbAppTextControlSpec {
	FluxApp    *app;
	XentNodeId  parent;
	XentNodeId  (*create)(FluxTextBoxCreateInfo const *);
	char const *placeholder;
	TbChangeFn  on_change;
	void       *userdata;
} TbAppTextControlSpec;

typedef struct TbNumberBoxSpec {
	double minimum;
	double maximum;
	double step;
	void   (*on_value_change)(void *, double);
	void  *userdata;
} TbNumberBoxSpec;

typedef struct TbBaseSpec {
	XentContext   *ctx;
	XentNodeId     node;
	FluxNodeStore *store;
	char const    *placeholder;
	TbChangeFn     on_change;
	void          *userdata;
} TbBaseSpec;

static XentNodeId
tb_create_node_with_parent(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, XentControlType type) {
	XentNodeId node = xent_create_node(ctx);
	if (node == XENT_NODE_INVALID) return node;

	xent_set_control_type(ctx, node, type);

	if (parent != XENT_NODE_INVALID) xent_append_child(ctx, parent, node);

	FluxNodeData *nd = flux_node_store_get_or_create(store, node);
	if (nd) nd->component_type = type;
	xent_set_userdata(ctx, node, nd);
	xent_set_focusable(ctx, node, true);
	return node;
}

static void tb_wire_behaviors(FluxNodeData *nd, FluxTextBoxInputData *tb) {
	nd->component_data                  = &tb->base;
	nd->destroy_component_data          = tb_destroy;
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

static void tb_attach_app(FluxNodeStore *store, XentNodeId node, FluxApp *app) {
	FluxNodeData *nd = flux_node_store_get(store, node);
	if (nd && nd->component_data) (( FluxTextBoxInputData * ) nd->component_data)->app = app;
}

void tb_destroy(void *component_data) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) component_data;
	if (!tb) return;

	tb_free_undo_redo(tb);
	free(tb->buffer);
	free(tb->ime_buf);
	free(tb->nb);
	free(tb);
}

static FluxTextBoxInputData *tb_alloc_base(TbBaseSpec const *spec) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) calloc(1, sizeof(FluxTextBoxInputData));
	if (!tb) return NULL;
	tb->buf_cap = FLUX_TEXTBOX_INITIAL_CAP;
	tb->buffer  = ( char * ) calloc(1, tb->buf_cap);
	if (!tb->buffer) {
		free(tb);
		return NULL;
	}
	tb->buf_len            = 0;
	tb->buffer [0]         = '\0';
	tb->base.content       = tb->buffer;
	tb->base.placeholder   = spec->placeholder;
	tb->base.font_size     = 14.0f;
	tb->base.enabled       = true;
	tb->base.on_change     = spec->on_change;
	tb->base.on_change_ctx = spec->userdata;
	tb->ctx                = spec->ctx;
	tb->node               = spec->node;
	tb->store              = spec->store;
	tb->app                = NULL;
	return tb;
}

static XentNodeId tb_create_text_control(TbTextControlSpec const *spec) {
	if (!spec->ctx || !spec->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(spec->store, spec->type, spec->draw, NULL);

	XentNodeId node = tb_create_node_with_parent(spec->ctx, spec->store, spec->parent, spec->type);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd         = flux_node_store_get(spec->store, node);
	TbBaseSpec    base_spec  = {spec->ctx, node, spec->store, spec->placeholder, spec->on_change, spec->userdata};
	FluxTextBoxInputData *tb = nd ? tb_alloc_base(&base_spec) : NULL;
	if (nd && tb) tb_wire_behaviors(nd, tb);

	xent_set_semantic_role(spec->ctx, node, XENT_SEMANTIC_CUSTOM);
	if (spec->placeholder) xent_set_semantic_label(spec->ctx, node, spec->placeholder);

	return node;
}

XentNodeId flux_create_textbox(FluxTextBoxCreateInfo const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbTextControlSpec spec = {
	  info->ctx, info->store, info->parent, XENT_CONTROL_TEXT_INPUT, flux_draw_textbox, info->placeholder,
	  info->on_change, info->userdata};
	return tb_create_text_control(&spec);
}

static XentNodeId tb_create_app_control(TbAppTextControlSpec const *spec) {
	if (!spec->app) return XENT_NODE_INVALID;

	XentContext   *ctx   = flux_app_get_context(spec->app);
	FluxNodeStore *store = flux_app_get_store(spec->app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	FluxTextBoxCreateInfo create_info = {ctx, store, spec->parent, spec->placeholder, spec->on_change, spec->userdata};
	XentNodeId            node        = spec->create(&create_info);
	if (node == XENT_NODE_INVALID) return node;

	tb_attach_app(store, node, spec->app);

	return node;
}

XentNodeId flux_app_create_textbox(FluxAppTextBoxCreateInfo const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbAppTextControlSpec spec
	  = {info->app, info->parent, flux_create_textbox, info->placeholder, info->on_change, info->userdata};
	return tb_create_app_control(&spec);
}

static void tb_configure_password_grid(XentContext *ctx, XentNodeId node);

XentNodeId  flux_create_password_box(FluxTextBoxCreateInfo const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbTextControlSpec spec = {
	  info->ctx, info->store, info->parent, XENT_CONTROL_PASSWORD_BOX, flux_draw_password_box, info->placeholder,
	  info->on_change, info->userdata};
	XentNodeId node = tb_create_text_control(&spec);
	if (node == XENT_NODE_INVALID) return node;

	tb_configure_password_grid(info->ctx, node);
	return node;
}

XentNodeId flux_app_create_password_box(FluxAppTextBoxCreateInfo const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbAppTextControlSpec spec
	  = {info->app, info->parent, flux_create_password_box, info->placeholder, info->on_change, info->userdata};
	return tb_create_app_control(&spec);
}

static FluxNBExt *tb_create_number_ext(TbNumberBoxSpec const *spec) {
	FluxNBExt *nb = ( FluxNBExt * ) calloc(1, sizeof(FluxNBExt));
	if (!nb) return NULL;

	nb->minimum             = spec->minimum;
	nb->maximum             = spec->maximum;
	nb->small_change        = spec->step;
	nb->large_change        = spec->step * 10.0;
	nb->value               = spec->minimum;
	nb->spin_placement      = 2;
	nb->on_value_change     = spec->on_value_change;
	nb->on_value_change_ctx = spec->userdata;
	return nb;
}

static void tb_configure_text_grid(XentContext *ctx, XentNodeId node, float const *col_vals, uint32_t col_count) {
	xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
	XentGridSizeMode row_modes []  = {XENT_GRID_STAR};
	float            row_vals []   = {1.0f};
	XentGridSizeMode col_modes [3] = {XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL};
	xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);
	xent_set_grid_columns(ctx, node, col_modes, col_vals, col_count);
}

static void tb_configure_password_grid(XentContext *ctx, XentNodeId node) {
	float col_vals [] = {1.0f, 30.0f};
	tb_configure_text_grid(ctx, node, col_vals, 2);
}

static void tb_configure_number_grid(XentContext *ctx, XentNodeId node) {
	float col_vals [] = {1.0f, 40.0f, 36.0f};
	tb_configure_text_grid(ctx, node, col_vals, 3);
}

XentNodeId flux_create_number_box(FluxNumberBoxCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_NUMBER_BOX, flux_draw_number_box, NULL);

	XentNodeId node = tb_create_node_with_parent(info->ctx, info->store, info->parent, XENT_CONTROL_NUMBER_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd        = flux_node_store_get(info->store, node);
	TbBaseSpec            base_spec = {info->ctx, node, info->store, NULL, NULL, NULL};
	FluxTextBoxInputData *tb        = nd ? tb_alloc_base(&base_spec) : NULL;
	if (tb) {
		TbNumberBoxSpec spec = {info->min_value, info->max_value, info->step, info->on_value_change, info->userdata};
		FluxNBExt      *nb   = tb_create_number_ext(&spec);
		if (!nb) {
			tb_destroy(tb);
			return node;
		}
		tb->nb = nb;
		nb_update_text_to_value(tb);
		tb_wire_behaviors(nd, tb);
	}

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_CUSTOM);
	xent_set_semantic_value(
	  info->ctx, node, ( float ) info->min_value, ( float ) info->min_value, ( float ) info->max_value
	);
	xent_set_semantic_expanded(info->ctx, node, true);
	xent_set_focusable(info->ctx, node, true);

	tb_configure_number_grid(info->ctx, node);

	return node;
}

XentNodeId flux_app_create_number_box(FluxAppNumberBoxCreateInfo const *info) {
	if (!info || !info->app) return XENT_NODE_INVALID;

	XentContext   *ctx   = flux_app_get_context(info->app);
	FluxNodeStore *store = flux_app_get_store(info->app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	FluxNumberBoxCreateInfo create_info
	  = {ctx, store, info->parent, info->min_value, info->max_value, info->step, info->on_value_change, info->userdata};
	XentNodeId node = flux_create_number_box(&create_info);
	if (node == XENT_NODE_INVALID) return node;

	tb_attach_app(store, node, info->app);

	return node;
}
