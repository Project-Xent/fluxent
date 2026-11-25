#include "controls/factory/flux_factory.h"
#include "a11y/flux_uia_ids.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"
#include "runtime/flux_str.h"
#include "controls/factory/flux_init_nav.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"
#include "input/flux_input_internal.h"
#include "a11y/flux_uia_app.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_textbox.h"
#include "controls/factory/flux_control_build.h"
#include "host/flux_prop_apply.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static FluxPagerData *pager_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_PAGER || !nd->component_data) return NULL;
	return ( FluxPagerData * ) nd->component_data;
}

static int pager_nav(FluxPagerData const *d, int slot) {
	uint8_t const vis [4] = {d->first_vis, d->prev_vis, d->next_vis, d->last_vis};
	return flux_pager_nav_state(slot, vis [slot], d->selected, d->count);
}

static int pager_count(int count) { return count < 0 ? -1 : count; }

static int pager_clamp(int count, int index) {
	if (count == 0) return 0;
	int last = count < 0 ? INT32_MAX - 1 : count - 1;
	if (index > last) return last;
	return index < 0 ? 0 : index;
}

float flux_pager_cell_width(FluxText *text, int32_t page) {
	if (!text || page < 10000) return FLUX_PAGER_BTN;
	char label [16];
	snprintf(label, sizeof(label), "%d", page);
	FluxTextStyle style = {.font_size = 14.0f, .font_weight = XENT_FONT_WEIGHT_DEFAULT};
	return fmaxf(FLUX_PAGER_BTN, ceilf(flux_text_measure(text, label, &style, 0.0f).w));
}

static int pager_elems(FluxPagerData const *d, FluxPagerElem *elems, float *tw) {
	int32_t cells [FLUX_PAGER_MAX_CELLS];
	int     cc = flux_pager_build_cells(d->count, d->selected, !d->hide_ends, cells);
	return flux_pager_layout(
	  d->display_mode, pager_nav(d, 0), pager_nav(d, 1), pager_nav(d, 2), pager_nav(d, 3), cells, cc, d->box_w, elems,
	  tw, d->text
	);
}

static void pager_apply_size(FluxPagerData *d) {
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	float         tw = 0.0f;
	pager_elems(d, elems, &tw);
	xent_setsize(d->ctx, d->node, (XentSize) {tw, FLUX_PAGER_HEIGHT});
}

static int pager_hit(FluxPagerData const *d, float x, float y) {
	if (y < 0.0f || y > FLUX_PAGER_HEIGHT) return -1;
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	float         tw = 0.0f;
	int           n  = pager_elems(d, elems, &tw);
	for (int i = 0; i < n; i++)
		if (x >= elems [i].x && x < elems [i].x + elems [i].w) return i;
	return -1;
}

static void pager_parts_sync(FluxPagerData *d);
static void pager_box_select(FluxPagerData *d);

static void pager_select(FluxPagerData *d, int index) {
	index = pager_clamp(d->count, index);
	if (index == d->selected) return;
	flux_uia_noteprop(d->store, d->node, FLUX_UIA_PROP_SELECTION, d->selected, index);
	d->selected = index;
	pager_apply_size(d);
	pager_parts_sync(d);
	pager_box_select(d);
	XtkHost *host = flux_store_host(d->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_SELECT,
		  .target         = ( XtkCtrl ) d->node,
		  .source         = ( XtkCtrl ) d->node,
		  .current_target = ( XtkCtrl ) d->node,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
		};
		flux_host_route(host, &event);
	}
	if (d->on_select) d->on_select(d->on_select_ctx, index);
}

static XentNodeId pager_elem_part(FluxPagerData const *d, FluxPagerElem const *e, int slot) {
	switch (e->kind) {
	case FLUX_PAGER_EL_FIRST : return d->nav [0];
	case FLUX_PAGER_EL_PREV  : return d->nav [1];
	case FLUX_PAGER_EL_NEXT  : return d->nav [2];
	case FLUX_PAGER_EL_LAST  : return d->nav [3];
	case FLUX_PAGER_EL_CELL  : return slot < FLUX_PAGER_MAX_CELLS ? d->cells [slot] : XENT_NODE_INVALID;
	default                  : return XENT_NODE_INVALID;
	}
}

static int pager_nav_slot(uint8_t kind) {
	switch (kind) {
	case FLUX_PAGER_EL_FIRST : return 0;
	case FLUX_PAGER_EL_PREV  : return 1;
	case FLUX_PAGER_EL_NEXT  : return 2;
	case FLUX_PAGER_EL_LAST  : return 3;
	default                  : return -1;
	}
}

static bool pager_number_elem(FluxPagerElem const *e) {
	return e->kind == FLUX_PAGER_EL_CELL && e->page != FLUX_PAGER_ELLIPSIS_PAGE;
}

static void pager_activate(FluxPagerData *d, FluxPagerElem const *e) {
	if (pager_number_elem(e)) {
		pager_select(d, e->page - 1);
		return;
	}
	int slot = pager_nav_slot(e->kind);
	if (slot < 0 || pager_nav(d, slot) != FLUX_PAGER_NAV_ENABLED) return;
	int const target [4] = {0, d->selected - 1, d->selected + 1, d->count - 1};
	pager_select(d, target [slot]);
}

static void pager_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	d->pressed_elem  = pager_hit(d, x, y);
}

static void pager_pointer_focus(FluxPagerData *d, XentNodeId part) {
	FluxInput *input = flux_store_input(d->store);
	if (input && part != XENT_NODE_INVALID && xent_get_focusable(d->ctx, part)) flux_input_setfocus(input, part, true);
}

static bool pager_chrome_hit(void *ctx, float x, float y) {
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	XentRect       r = {0}, p = {0};
	( void ) y;
	if (!d->box || xent_collapsed(d->ctx, d->box)) return true;
	xent_layout_rect(d->ctx, d->box, &r);
	xent_layout_rect(d->ctx, d->node, &p);
	return x < r.x - p.x || x >= r.x - p.x + r.w;
}

static void pager_click(void *ctx) {
	FluxPagerData *d       = ( FluxPagerData * ) ctx;
	FluxNodeData  *nd      = flux_store_find(d->store, d->node);
	int            hit     = nd ? pager_hit(d, nd->hover_local_x, nd->hover_local_y) : -1;
	int            pressed = d->pressed_elem;
	d->pressed_elem        = -1;
	if (hit < 0 || hit != pressed) return;
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	int           n = pager_elems(d, elems, NULL);
	if (hit >= n || elems [hit].page == FLUX_PAGER_ELLIPSIS_PAGE) return;
	int slot = 0;
	for (int i = 0; i < hit; i++) slot += pager_number_elem(&elems [i]);
	XentNodeId part = pager_elem_part(d, &elems [hit], slot);
	if (part != XENT_NODE_INVALID) {
		flux_uia_note(d->store, part, -1, FLUX_UIA_EVT_INVOKED);
		pager_pointer_focus(d, part);
	}
	pager_activate(d, &elems [hit]);
}

static void pager_cancel(void *ctx) {
	FluxPagerData *d = ( FluxPagerData * ) ctx;
	d->pressed_elem  = -1;
}

static void pager_destroy(void *component_data) {
	FluxPagerData *d = ( FluxPagerData * ) component_data;
	if (!d) return;
	flux_str_free(d->prefix);
	flux_str_free(d->suffix);
	free(d);
}

static FluxPagerData *pager_of_part(FluxStore *store, XentNodeId part, int *index) {
	XentNodeId owner = flux_uia_part_owner(store, part, index);
	return owner != XENT_NODE_INVALID ? pager_data(store, owner) : NULL;
}

static bool pager_uia_go(FluxStore *store, XentNodeId part) {
	int            index = 0;
	FluxPagerData *d     = pager_of_part(store, part, &index);
	if (!d) return false;
	if (index >= 0) pager_select(d, index);
	else
		pager_activate(
		  d, &(FluxPagerElem) {
			   ( uint8_t ) (index == -1   ? FLUX_PAGER_EL_FIRST
			                : index == -2 ? FLUX_PAGER_EL_PREV
			                : index == -3 ? FLUX_PAGER_EL_NEXT
			                              : FLUX_PAGER_EL_LAST),
			   0, 0.0f, 0.0f}
		);
	return true;
}

static void pager_uia_name(FluxStore *store, XentNodeId part, char *out, size_t size) {
	static char const *const nav [4] = {"First page", "Previous page", "Next page", "Last page"};
	int                      index   = 0;
	( void ) pager_of_part(store, part, &index);
	if (index >= 0) snprintf(out, size, "Page %d", index + 1);
	else if (index >= -4) snprintf(out, size, "%s", nav [-index - 1]);
}

static bool pager_uia_setinfo(FluxStore *store, XentNodeId part, int *pos, int *size, int *level) {
	( void ) level;
	int            index = 0;
	FluxPagerData *d     = pager_of_part(store, part, &index);
	if (!d || index < 0) return false;
	*pos  = index + 1;
	*size = d->count < 0 ? 0 : d->count;
	return true;
}

static FluxUiaHooks const pager_uia_button = {
  .role     = FLUX_UIA_BUTTON,
  .patterns = FLUX_UIA_P_INVOKE,
  .invoked  = true,
  .klass    = L"Button",
  .invoke   = pager_uia_go,
  .name     = pager_uia_name};
static FluxUiaHooks const pager_uia_cell = {
  .role     = FLUX_UIA_BUTTON,
  .patterns = FLUX_UIA_P_INVOKE,
  .invoked  = true,
  .klass    = L"Button",
  .invoke   = pager_uia_go,
  .name     = pager_uia_name,
  .setinfo  = pager_uia_setinfo};
static FluxUiaHooks const pager_uia_group = {.role = FLUX_UIA_GROUP};

static XentNodeId         pager_root(XentCtx *ctx, XentNodeId node) {
	XentNodeId up = xent_node_parent(ctx, node);
	while (up != XENT_NODE_INVALID) {
		node = up;
		up   = xent_node_parent(ctx, node);
	}
	return node;
}

static bool pager_root_key(FluxPagerData *d, FluxKey key) {
	FluxInput *input = flux_store_input(d->store);
	if (!input || (key != FLUX_KEY_LEFT && key != FLUX_KEY_RIGHT)) return false;
	XentNodeId focus = flux_input_focus(input);
	bool       rtl   = focus != XENT_NODE_INVALID && xent_resolved_dir(d->ctx, focus) == XENT_DIRECTION_RTL;
	if (rtl) key = key == FLUX_KEY_LEFT ? FLUX_KEY_RIGHT : FLUX_KEY_LEFT;
	flux_input_xyfocus(input, pager_root(d->ctx, d->node), key);
	return true;
}

static bool pager_part_key(void *ctx, FluxKey key, bool down) {
	return down && pager_root_key(( FluxPagerData * ) ctx, key);
}

static bool pager_box_key(void *ctx, FluxKey key, bool down) {
	FluxPagerData *d     = ( FluxPagerData * ) ctx;
	FluxInput     *input = flux_store_input(d->store);
	XentNodeId     focus = input ? flux_input_focus(input) : XENT_NODE_INVALID;
	int            slot  = focus == d->combo ? 1 : 0;
	if (d->box_key [slot] && d->box_key [slot](d->box_key_ctx [slot], key, down)) return true;
	return down && pager_root_key(d, key);
}

static void pager_box_hook(FluxPagerData *d, XentNodeId node, int slot) {
	FluxNodeData *nd = flux_store_find(d->store, node);
	if (!nd) return;
	d->box_key [slot]       = nd->behavior.on_key;
	d->box_key_ctx [slot]   = nd->behavior.on_key_ctx;
	nd->behavior.on_key     = pager_box_key;
	nd->behavior.on_key_ctx = d;
}

static void pager_part_click(void *ctx) {
	FluxPagerData *d     = ( FluxPagerData * ) ctx;
	FluxInput     *input = flux_store_input(d->store);
	if (input) pager_uia_go(d->store, flux_input_focus(input));
}

static void pager_part_behavior(FluxPagerData *d, XentNodeId part) {
	FluxNodeData *nd = flux_store_find(d->store, part);
	if (!nd) return;
	nd->behavior.on_key       = pager_part_key;
	nd->behavior.on_key_ctx   = d;
	nd->behavior.on_click     = pager_part_click;
	nd->behavior.on_click_ctx = d;
}

int flux_pager_focus(FluxPagerData const *d, int *page) {
	static uint8_t const kinds [4] = {FLUX_PAGER_EL_FIRST, FLUX_PAGER_EL_PREV, FLUX_PAGER_EL_NEXT, FLUX_PAGER_EL_LAST};
	for (int k = 0; k < 4; k++) {
		FluxNodeData const *nd = flux_store_find(d->store, d->nav [k]);
		if (nd && nd->state.focused && !nd->state.suppress_focus) return kinds [k];
	}
	for (int i = 0; i < FLUX_PAGER_MAX_CELLS; i++) {
		FluxNodeData const *nd = flux_store_find(d->store, d->cells [i]);
		if (!nd || !nd->state.focused || nd->state.suppress_focus) continue;
		*page = nd->uia_part + 1;
		return FLUX_PAGER_EL_CELL;
	}
	return -1;
}

static XentRect pager_cell_span(FluxPagerElem const *elems, int n) {
	float x0 = -1.0f, x1 = 0.0f;
	for (int i = 0; i < n; i++) {
		if (elems [i].kind != FLUX_PAGER_EL_CELL) continue;
		x0 = x0 < 0.0f ? elems [i].x : x0;
		x1 = elems [i].x + elems [i].w;
	}
	return x0 < 0.0f ? (XentRect) {0} : (XentRect) {x0, 0.0f, x1 - x0, FLUX_PAGER_HEIGHT};
}

static void pager_place_cell(FluxPagerData *d, XentNodeId node, FluxPagerElem const *e, float group_x) {
	FluxNodeData *nd = flux_store_find(d->store, node);
	if (nd) nd->uia_part = e->page - 1;
	XentRect r = {e->x - group_x, 0.0f, e->w, FLUX_PAGER_HEIGHT};
	xent_set_focusable(d->ctx, node, true);
	flux_uia_part_place(d->ctx, node, r, (XentPoint) {0});
}

static void pager_place_cells(FluxPagerData *d, FluxPagerElem const *elems, int n, XentRect *span) {
	int slot = 0;
	*span    = pager_cell_span(elems, n);
	for (int i = 0; i < n; i++)
		if (pager_number_elem(&elems [i])) pager_place_cell(d, d->cells [slot++], &elems [i], span->x);
	for (; slot < FLUX_PAGER_MAX_CELLS; slot++) {
		xent_set_focusable(d->ctx, d->cells [slot], false);
		flux_uia_part_place(d->ctx, d->cells [slot], (XentRect) {0}, (XentPoint) {0});
	}
}

static void pager_place_navs(FluxPagerData *d, FluxPagerElem const *elems, int n) {
	XentRect at [4] = {{0}};
	bool     on [4] = {false, false, false, false};
	for (int i = 0; i < n; i++) {
		int k = pager_nav_slot(elems [i].kind);
		if (k >= 0) at [k] = (XentRect) {elems [i].x, 0.0f, elems [i].w, FLUX_PAGER_HEIGHT};
	}
	for (int k = 0; k < 4; k++) {
		on [k] = pager_nav(d, k) == FLUX_PAGER_NAV_ENABLED;
		flux_uia_part_place(d->ctx, d->nav [k], at [k], (XentPoint) {0});
		flux_uia_part_label(d->ctx, d->nav [k], NULL, on [k]);
		xent_set_focusable(d->ctx, d->nav [k], on [k] && at [k].w > 0.0f);
	}
}

static void pager_parts_sync(FluxPagerData *d) {
	FluxPagerElem elems [FLUX_PAGER_MAX_CELLS + 4];
	float         tw   = 0.0f;
	int           n    = pager_elems(d, elems, &tw);
	XentRect      span = {0};
	if (!d->group) return;
	pager_place_navs(d, elems, n);
	pager_place_cells(d, elems, n, &span);
	flux_uia_part_place(d->ctx, d->group, span, (XentPoint) {0});
	for (int i = 0; d->box && i < n; i++)
		if (elems [i].kind == FLUX_PAGER_EL_BOX) xent_setpos(d->ctx, d->box, (XentPoint) {elems [i].x, 0.0f});
}

static char const *pager_prefix(FluxPagerData const *d) { return d->prefix && d->prefix [0] ? d->prefix : "Page"; }

static char const *pager_suffix(FluxPagerData const *d) { return d->suffix && d->suffix [0] ? d->suffix : "of"; }

static void        pager_box_select(FluxPagerData *d) {
	int entries = d->count < 0 ? FLUX_PAGER_INFINITE_ENTRIES : d->count;
	if (d->combo && d->selected < entries) flux_prop_combo_selected(d->store, d->combo, d->selected);
	if (d->number) flux_prop_numberbox_value(d->store, d->number, ( double ) d->selected + 1.0);
}

static void pager_combo_fill(FluxPagerData *d) {
	int    n     = d->count < 0 ? FLUX_PAGER_INFINITE_ENTRIES : d->count;
	size_t width = 12u;
	if (( size_t ) n > SIZE_MAX / width - 1u) return;
	char        *pool  = ( char * ) malloc((( size_t ) n + 1u) * width);
	char const **items = ( char const ** ) malloc((( size_t ) n + 1u) * sizeof(*items));
	for (int i = 0; pool && items && i < n; i++) {
		char *label = pool + ( size_t ) i * width;
		snprintf(label, width, "%d", i + 1);
		items [i] = label;
	}
	if (pool && items) flux_prop_combo_items(d->store, d->combo, items, n);
	free(( void * ) items);
	free(pool);
}

static void pager_box_sync(FluxPagerData *d) {
	char total [16];
	if (!d->box || !d->combo || !d->number) return;
	bool boxed = flux_pager_boxed(d->display_mode);
	bool combo = flux_pager_combo(d->display_mode, d->count);
	xent_setcollapsed(d->ctx, d->box, !boxed);
	xent_setcollapsed(d->ctx, d->combo, !combo);
	xent_setcollapsed(d->ctx, d->number, combo);
	xent_setcollapsed(d->ctx, d->suffix_label, d->count < 0);
	xent_setcollapsed(d->ctx, d->total_label, d->count < 0);
	snprintf(total, sizeof(total), "%d", d->count);
	flux_prop_text_content(d->store, d->prefix_label, pager_prefix(d));
	flux_prop_text_content(d->store, d->suffix_label, pager_suffix(d));
	flux_prop_text_content(d->store, d->total_label, total);
	if (combo) pager_combo_fill(d);
	else flux_prop_combo_items(d->store, d->combo, NULL, 0);
	flux_prop_numberbox_range(d->store, d->number, 1.0, d->count < 0 ? INT32_MAX : ( double ) d->count);
	pager_box_select(d);
	if (!boxed) d->box_w = 0.0f;
}

void flux_pager_sync(XentNodeId node, FluxNodeData *nd) {
	FluxPagerData *d = ( FluxPagerData * ) nd->component_data;
	XentRect       r = {0};
	if (!d || !d->box || d->node != node) return;
	xent_layout_rect(d->ctx, d->box, &r);
	float w = flux_pager_boxed(d->display_mode) ? r.w : 0.0f;
	if (fabsf(w - d->box_w) < 0.5f) return;
	d->box_w = w;
	pager_apply_size(d);
	pager_parts_sync(d);
}

static void pager_number_changed(void *ctx, double value) {
	if (isfinite(value)) pager_select(( FluxPagerData * ) ctx, ( int ) fmin(fmax(value, 1.0), INT32_MAX) - 1);
}

static void pager_combo_changed(void *ctx, int index) {
	if (index >= 0) pager_select(( FluxPagerData * ) ctx, index);
}

static XentNodeId pager_label(FluxPagerData *d, XentInsets margin) {
	XentNodeId n
	  = flux_text_init(&(FluxTextDesc) {.ctx = d->ctx, .store = d->store, .parent = d->box, .font_size = 14.0f});
	xent_setm(d->ctx, n, margin);
	xent_setself(d->ctx, n, XENT_FLEX_ALIGN_CENTER);
	return n;
}

static XentNodeId pager_number(FluxPagerData *d, FluxControlEnv const *env) {
	double     max = ( double ) d->count;
	XentNodeId n
	  = env->app
	    ? flux_app_create_number(&(FluxAppNumberBoxDesc) {env->app, d->box, 1.0, max, 1.0, pager_number_changed, d})
	    : flux_number_init(&(FluxNumberBoxDesc) {d->ctx, d->store, d->box, 1.0, max, 1.0, pager_number_changed, d});
	flux_prop_numberbox_spin_enabled(d->store, n, false);
	flux_prop_numberbox_changes(d->store, n, 1.0, 5.0);
	return n;
}

static XentNodeId pager_combo(FluxPagerData *d, FluxControlEnv const *env) {
	return flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = d->ctx,
	  .store          = d->store,
	  .parent         = d->box,
	  .window         = env->window,
	  .text           = env->text,
	  .theme          = env->theme,
	  .app            = env->app,
	  .input          = env->input,
	  .selected_index = -1,
	  .on_select      = pager_combo_changed,
	  .userdata       = d});
}

static void pager_box_make(FluxPagerData *d, FluxControlEnv const *env) {
	if (!env) return;
	d->box = flux_factory_create_node(d->ctx, d->store, d->node, FLUX_CONTROL_CONTAINER);
	if (d->box == XENT_NODE_INVALID) return;
	xent_setproto(d->ctx, d->box, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->box, XENT_FLEX_ROW);
	xent_setitems(d->ctx, d->box, XENT_FLEX_ALIGN_CENTER);
	xent_setsize(d->ctx, d->box, (XentSize) {NAN, FLUX_PAGER_HEIGHT});
	xent_setfit(d->ctx, d->box, true, false);
	d->prefix_label = pager_label(d, (XentInsets) {5.0f, 0.0f, 5.0f, 0.0f});
	d->number       = pager_number(d, env);
	d->combo        = pager_combo(d, env);
	d->suffix_label = pager_label(d, (XentInsets) {5.0f, 0.0f, 0.0f, 0.0f});
	d->total_label  = pager_label(d, (XentInsets) {5.0f, 0.0f, 5.0f, 0.0f});
	flux_uia_settext(d->store, d->number, FLUX_A11Y_NAME, "Page");
	flux_uia_settext(d->store, d->combo, FLUX_A11Y_NAME, "Page");
	pager_box_hook(d, d->number, 0);
	pager_box_hook(d, d->combo, 1);
	pager_box_sync(d);
}

static void pager_parts_make(FluxPagerData *d, FluxControlEnv const *env) {
	d->nav [0] = flux_uia_part(d->ctx, d->store, d->node, d->node, -1);
	d->nav [1] = flux_uia_part(d->ctx, d->store, d->node, d->node, -2);
	d->group   = flux_uia_part(d->ctx, d->store, d->node, d->node, -6);
	pager_box_make(d, env);
	d->nav [2] = flux_uia_part(d->ctx, d->store, d->node, d->node, -3);
	d->nav [3] = flux_uia_part(d->ctx, d->store, d->node, d->node, -4);
	for (int k = 0; k < 4; k++) flux_uia_part_hooks(d->store, d->nav [k], &pager_uia_button);
	for (int k = 0; k < 4; k++) pager_part_behavior(d, d->nav [k]);
	flux_uia_part_hooks(d->store, d->group, &pager_uia_group);
	for (int i = 0; d->group && i < FLUX_PAGER_MAX_CELLS; i++) {
		d->cells [i] = flux_uia_part(d->ctx, d->store, d->node, d->group, i);
		flux_uia_part_hooks(d->store, d->cells [i], &pager_uia_cell);
		pager_part_behavior(d, d->cells [i]);
	}
	pager_parts_sync(d);
}

XentNodeId flux_pager_init(FluxPagerDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PAGER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData  *nd = flux_store_find(info->store, node);
	FluxPagerData *d  = nd ? ( FluxPagerData * ) calloc(1, sizeof(*d)) : NULL;
	if (!nd || !d) {
		free(d);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	d->ctx                           = info->ctx;
	d->store                         = info->store;
	d->text                          = info->env ? info->env->text : NULL;
	d->node                          = node;
	d->count                         = pager_count(info->count);
	d->selected                      = pager_clamp(d->count, info->selected);
	d->hide_ends                     = info->hide_ends;
	d->display_mode                  = ( uint8_t ) info->display_mode;
	d->first_vis                     = ( uint8_t ) info->first_button;
	d->prev_vis                      = ( uint8_t ) info->prev_button;
	d->next_vis                      = ( uint8_t ) info->next_button;
	d->last_vis                      = ( uint8_t ) info->last_button;
	d->prefix                        = info->prefix ? flux_str_dup(info->prefix) : NULL;
	d->suffix                        = info->suffix ? flux_str_dup(info->suffix) : NULL;
	d->pressed_elem                  = -1;
	d->on_select                     = info->on_select;
	d->on_select_ctx                 = info->userdata;

	nd->component_data               = d;
	nd->destroy_component_data       = pager_destroy;
	nd->behavior.on_pointer_down     = pager_pointer_down;
	nd->behavior.on_pointer_down_ctx = d;
	nd->behavior.on_click            = pager_click;
	nd->behavior.on_click_ctx        = d;
	nd->behavior.on_cancel           = pager_cancel;
	nd->behavior.on_cancel_ctx       = d;
	nd->behavior.chrome_hit          = pager_chrome_hit;
	nd->behavior.chrome_hit_ctx      = d;

	xent_set_focusable(info->ctx, node, true);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	pager_apply_size(d);
	pager_parts_make(d, info->env);
	return node;
}

void flux_pager_configure(FluxStore *store, XentNodeId id, int count, int selected, int display_mode) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	count = pager_count(count);
	if (count < selected && count > -1) selected = count - 1;
	selected = pager_clamp(count, selected);
	if (selected != d->selected) flux_uia_noteprop(store, id, FLUX_UIA_PROP_SELECTION, d->selected, selected);
	d->count        = count;
	d->selected     = selected;
	d->display_mode = ( uint8_t ) display_mode;
	pager_box_sync(d);
	pager_apply_size(d);
	pager_parts_sync(d);
}

void flux_pager_set_selected(FluxStore *store, XentNodeId id, int index) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	index = pager_clamp(d->count, index);
	if (index != d->selected) flux_uia_noteprop(store, id, FLUX_UIA_PROP_SELECTION, d->selected, index);
	d->selected = index;
	pager_apply_size(d);
	pager_parts_sync(d);
	pager_box_select(d);
}

void flux_pager_setbuttons(FluxStore *store, XentNodeId id, int first, int prev, int next, int last) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	d->first_vis = ( uint8_t ) first;
	d->prev_vis  = ( uint8_t ) prev;
	d->next_vis  = ( uint8_t ) next;
	d->last_vis  = ( uint8_t ) last;
	pager_apply_size(d);
	pager_parts_sync(d);
}

void flux_pager_settext(FluxStore *store, XentNodeId id, char const *prefix, char const *suffix) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	flux_str_replace(&d->prefix, prefix);
	flux_str_replace(&d->suffix, suffix);
	pager_box_sync(d);
	pager_apply_size(d);
	pager_parts_sync(d);
}

void flux_pager_setends(FluxStore *store, XentNodeId id, bool hide) {
	FluxPagerData *d = pager_data(store, id);
	if (!d) return;
	d->hide_ends = hide;
	pager_apply_size(d);
	pager_parts_sync(d);
}
