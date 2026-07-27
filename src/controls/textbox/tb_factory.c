#include "tb_internal.h"
#include "tb_metrics.h"
#include "runtime/flux_str.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_init_textbox.h"
#include "render/flux_engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef void (*TbChangeFn)(void *, char const *);

typedef struct TbTextControlSpec {
	XentCtx        *ctx;
	FluxStore      *store;
	XentNodeId      parent;
	FluxControlType type;
	char const     *placeholder;
	TbChangeFn      on_change;
	void           *userdata;
} TbTextControlSpec;

typedef struct TbAppTextControlSpec {
	FluxApp    *app;
	XentNodeId  parent;
	XentNodeId  (*create)(FluxTextBoxDesc const *);
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
	XentCtx    *ctx;
	XentNodeId  node;
	FluxStore  *store;
	char const *placeholder;
	TbChangeFn  on_change;
	void       *userdata;
} TbBaseSpec;

static XentNodeId tb_create_node_with_parent(XentCtx *ctx, FluxStore *store, XentNodeId parent, FluxControlType type) {
	XentNodeId node = xent_node_create(ctx);
	if (node == XENT_NODE_INVALID) return node;

	flux_node_settype(store, node, type);

	if (parent != XENT_NODE_INVALID) xent_node_append(ctx, parent, node);

	FluxNodeData *nd = flux_store_ensure(store, node);
	if (nd) nd->component_type = type;
	xent_set_focusable(ctx, node, true);
	xent_setsize(ctx, node, (XentSize) {NAN, 32.0f});
	xent_setminsize(ctx, node, (XentSize) {64.0f, 32.0f});
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
	nd->behavior.on_click               = tb_on_pointer_finish;
	nd->behavior.on_click_ctx           = tb;
	nd->behavior.on_cancel              = tb_on_pointer_finish;
	nd->behavior.on_cancel_ctx          = tb;
	nd->behavior.on_ime_composition     = tb_on_ime_composition;
	nd->behavior.on_ime_composition_ctx = tb;
	nd->behavior.on_context_menu        = tb_on_context_menu;
	nd->behavior.on_context_menu_ctx    = tb;
}

static void tb_attach_app(FluxStore *store, XentNodeId node, FluxApp *app) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd && nd->component_data) (( FluxTextBoxInputData * ) nd->component_data)->app = app;
}

void tb_destroy(void *component_data) {
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) component_data;
	if (!tb) return;

	tb_free_undo_redo(tb);
	flux_str_free(tb->base.placeholder);
	flux_str_free(tb->base.font_family);
	free(tb->buffer);
	free(tb->flat_buffer);
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
	tb->flat_cap    = FLUX_TEXTBOX_INITIAL_CAP;
	tb->flat_buffer = ( char * ) calloc(1, tb->flat_cap);
	if (!tb->flat_buffer) {
		free(tb->buffer);
		free(tb);
		return NULL;
	}
	tb->buf_len            = 0;
	tb->gap_start          = 0;
	tb->gap_end            = tb->buf_cap;
	tb->buffer [0]         = '\0';
	tb->flat_buffer [0]    = '\0';
	tb->base.content       = tb->flat_buffer;
	tb->base.placeholder   = flux_str_dup(spec->placeholder);
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

	XentNodeId node = tb_create_node_with_parent(spec->ctx, spec->store, spec->parent, spec->type);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd         = flux_store_find(spec->store, node);
	TbBaseSpec    base_spec  = {spec->ctx, node, spec->store, spec->placeholder, spec->on_change, spec->userdata};
	FluxTextBoxInputData *tb = nd ? tb_alloc_base(&base_spec) : NULL;
	if (nd && tb) tb_wire_behaviors(nd, tb);

	xent_sem_setrole(spec->ctx, node, XENT_SEM_ROLE_CUSTOM);
	if (spec->placeholder) xent_sem_setlabel(spec->ctx, node, spec->placeholder);

	return node;
}

XentNodeId flux_textbox_init(FluxTextBoxDesc const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbTextControlSpec spec = {
	  info->ctx, info->store, info->parent, FLUX_CONTROL_TEXT_INPUT, info->placeholder, info->on_change,
	  info->userdata};
	return tb_create_text_control(&spec);
}

void flux_textbox_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxNodeData         *nd = flux_store_find(store, node);
	FluxTextBoxInputData *tb = nd ? ( FluxTextBoxInputData * ) nd->component_data : NULL;
	if (!tb) return;
	tb->ctx   = ctx;
	tb->node  = node;
	tb->store = store;
}

void flux_textbox_sync_ime(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || !nd->component_data) return;
	if (nd->component_type != FLUX_CONTROL_TEXT_INPUT
		&& nd->component_type != FLUX_CONTROL_PASSWORD_BOX
		&& nd->component_type != FLUX_CONTROL_NUMBER_BOX)
		return;
	tb_update_ime_position(( FluxTextBoxInputData * ) nd->component_data);
}

static XentNodeId tb_create_app_control(TbAppTextControlSpec const *spec) {
	if (!spec->app) return XENT_NODE_INVALID;

	XentCtx   *ctx   = flux_app_ctx(spec->app);
	FluxStore *store = flux_app_store(spec->app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	FluxTextBoxDesc create_info = {ctx, store, spec->parent, spec->placeholder, spec->on_change, spec->userdata};
	XentNodeId      node        = spec->create(&create_info);
	if (node == XENT_NODE_INVALID) return node;

	tb_attach_app(store, node, spec->app);

	return node;
}

XentNodeId flux_app_create_textbox(FluxAppTextBoxDesc const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbAppTextControlSpec spec
	  = {info->app, info->parent, flux_textbox_init, info->placeholder, info->on_change, info->userdata};
	return tb_create_app_control(&spec);
}

static void tb_configure_password_grid(XentCtx *ctx, XentNodeId node);

XentNodeId  flux_password_init(FluxTextBoxDesc const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbTextControlSpec spec = {
	  info->ctx, info->store, info->parent, FLUX_CONTROL_PASSWORD_BOX, info->placeholder, info->on_change,
	  info->userdata};
	XentNodeId node = tb_create_text_control(&spec);
	if (node == XENT_NODE_INVALID) return node;

	tb_configure_password_grid(info->ctx, node);
	return node;
}

XentNodeId flux_app_create_password(FluxAppTextBoxDesc const *info) {
	if (!info) return XENT_NODE_INVALID;
	TbAppTextControlSpec spec
	  = {info->app, info->parent, flux_password_init, info->placeholder, info->on_change, info->userdata};
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

static void tb_configure_text_grid(XentCtx *ctx, XentNodeId node, float const *col_vals, uint32_t col_count) {
	xent_setproto(ctx, node, XENT_PROTOCOL_GRID);
	XentGridSizeMode row_modes []  = {XENT_GRID_STAR};
	float            row_vals []   = {1.0f};
	XentGridSizeMode col_modes [3] = {XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL};
	xent_grid_setrows(ctx, node, row_modes, row_vals, 1);
	xent_grid_setcols(ctx, node, col_modes, col_vals, col_count);
}

static void tb_configure_password_grid(XentCtx *ctx, XentNodeId node) {
	float col_vals [] = {1.0f, FLUX_PASSWORD_REVEAL_BTN_W};
	tb_configure_text_grid(ctx, node, col_vals, 2);
}

static void tb_configure_number_grid(XentCtx *ctx, XentNodeId node) {
	float col_vals [] = {1.0f, FLUX_NUMBER_BOX_DELETE_BTN_W, FLUX_NUMBER_BOX_SPIN_W};
	tb_configure_text_grid(ctx, node, col_vals, 3);
}

XentNodeId flux_number_init(FluxNumberBoxDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = tb_create_node_with_parent(info->ctx, info->store, info->parent, FLUX_CONTROL_NUMBER_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData         *nd        = flux_store_find(info->store, node);
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

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CUSTOM);
	xent_sem_setvalue(info->ctx, node, ( float ) info->min_value, ( float ) info->min_value, ( float ) info->max_value);
	xent_sem_setexpanded(info->ctx, node, true);
	xent_set_focusable(info->ctx, node, true);

	tb_configure_number_grid(info->ctx, node);
	xent_setminsize(info->ctx, node, (XentSize) {64.0f + FLUX_NUMBER_BOX_DELETE_BTN_W + FLUX_NUMBER_BOX_SPIN_W, 32.0f});

	return node;
}

XentNodeId flux_app_create_number(FluxAppNumberBoxDesc const *info) {
	if (!info || !info->app) return XENT_NODE_INVALID;

	XentCtx   *ctx   = flux_app_ctx(info->app);
	FluxStore *store = flux_app_store(info->app);
	if (!ctx || !store) return XENT_NODE_INVALID;

	FluxNumberBoxDesc create_info
	  = {ctx, store, info->parent, info->min_value, info->max_value, info->step, info->on_value_change, info->userdata};
	XentNodeId node = flux_number_init(&create_info);
	if (node == XENT_NODE_INVALID) return node;

	tb_attach_app(store, node, info->app);

	return node;
}
