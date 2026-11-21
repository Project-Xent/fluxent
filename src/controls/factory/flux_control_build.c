#include "a11y/flux_uia_ids.h"
#include "controls/flux_control_uia.h"
#include "controls/factory/flux_control_build.h"
#include "controls/flux_module.h"
#include "controls/flux_module_props.h"
#include "controls/behavior/flux_colheader.h"
#include "controls/behavior/flux_cmdbar.h"
#include "render/flux_render_snapshot.h"
#include "host/flux_prop_apply.h"
#include "controls/factory/flux_init_textbox.h"
#include "store/flux_node_data.h"

#include "controls/behavior/flux_flyout_binding.h"
#include "controls/behavior/flux_host_menu.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_media.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_picker.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_textbox.h"
#include "controls/factory/flux_init_value.h"
#include "controls/factory/flux_factory.h"
#include "controls/behavior/flux_breadcrumb_internal.h"
#include "controls/behavior/flux_expander_internal.h"
#include "controls/behavior/flux_flip_internal.h"
#include "controls/behavior/flux_refresh_internal.h"
#include "controls/behavior/flux_slider.h"
#include "controls/behavior/flux_calendar.h"
#include "controls/behavior/flux_color_picker.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/data/flux_combo_box_data.h"
#include "controls/data/flux_info_bar_data.h"
#include "controls/data/flux_pager_data.h"
#include "controls/data/flux_menu_bar_data.h"
#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_person_picture_data.h"
#include "controls/data/flux_split_view_data.h"
#include "controls/data/flux_tab_view_data.h"
#include "controls/data/flux_title_bar_data.h"
#include "fluxent/flux_input.h"
#include <xtk/xtk_catalog.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static XtkValue const *mount_value(FluxControlMount const *m, XtkPropertyId id, XtkValueKind kind) {
	for (uint32_t i = 0; i < m->prop_count; i++)
		if (m->props [i].id == id && m->props [i].value.kind == kind) return &m->props [i].value;
	return NULL;
}

static bool mount_bool(FluxControlMount const *m, XtkPropertyId id, bool fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_BOOL);
	return v ? v->u.b : fallback;
}

static int mount_i32(FluxControlMount const *m, XtkPropertyId id, int fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_I32);
	return v ? v->u.i32 : fallback;
}

static float mount_f32(FluxControlMount const *m, XtkPropertyId id, float fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_F32);
	return v ? v->u.f32 : fallback;
}

static double mount_f64(FluxControlMount const *m, XtkPropertyId id, double fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_F64);
	return v ? v->u.f64 : fallback;
}

static uint32_t mount_enum(FluxControlMount const *m, XtkPropertyId id, uint32_t fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_ENUM);
	return v ? v->u.u32 : fallback;
}

static char const *mount_string(FluxControlMount const *m, XtkPropertyId id, char const *fallback) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_STRING);
	return v ? v->u.string.data : fallback;
}

static void adopt_children(XentCtx *ctx, XentNodeId dst, XentNodeId src) {
	for (;;) {
		XentNodeId child = xent_node_first(ctx, src);
		if (child == XENT_NODE_INVALID) break;
		xent_node_remove(ctx, src, child);
		xent_node_append(ctx, dst, child);
	}
}

static void copy_semantics(XentCtx *ctx, XentNodeId dst, XentNodeId src) {
	char const *label = xent_sem_label(ctx, src);
	float       v = 0, lo = 0, hi = 0;

	xent_sem_setrole(ctx, dst, xent_sem_role(ctx, src));
	if (label) xent_sem_setlabel(ctx, dst, label);
	if (xent_sem_value(ctx, src, &v, &lo, &hi)) xent_sem_setvalue(ctx, dst, v, lo, hi);
	xent_sem_setchecked(ctx, dst, xent_sem_checked(ctx, src));
	xent_set_focusable(ctx, dst, xent_get_focusable(ctx, src));
}

static void transfer(XentCtx *ctx, FluxStore *store, XentNodeId dst, XentNodeId src) {
	FluxNodeData *d = flux_store_find(store, dst);
	FluxNodeData *s = flux_store_find(store, src);
	if (!d || !s) {
		if (src != XENT_NODE_INVALID) flux_tree_drop(store, src);
		return;
	}
	( void ) xent_copystyle(ctx, dst, src);
	adopt_children(ctx, dst, src);
	if (d->component_data && d->destroy_component_data) d->destroy_component_data(d->component_data);
	d->component_data         = s->component_data;
	d->destroy_component_data = s->destroy_component_data;
	d->behavior               = s->behavior;
	d->clips_children         = s->clips_children;
	s->component_data         = NULL;
	s->destroy_component_data = NULL;
	memset(&s->behavior, 0, sizeof(s->behavior));
	copy_semantics(ctx, dst, src);
	flux_tree_drop(store, src);
}

static void layout_card(XentCtx *ctx, XentNodeId node) {
	xent_setproto(ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, node, XENT_FLEX_COLUMN);
	xent_setp(ctx, node, (XentInsets) {16.0f, 16.0f, 16.0f, 16.0f});
	xent_setgap(ctx, node, 8.0f);
	xent_setfit(ctx, node, false, true);
}

static void layout_divider(XentCtx *ctx, XentNodeId node) { xent_setsize(ctx, node, (XentSize) {NAN, 1.0f}); }

static void layout_flex_column(XentCtx *ctx, XentNodeId node) {
	xent_setproto(ctx, node, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, node, XENT_FLEX_COLUMN);
	xent_setitems(ctx, node, XENT_FLEX_ALIGN_STRETCH);
}

static char const *button_label(FluxNodeData *d) {
	return d->component_data ? (( FluxButtonData * ) d->component_data)->label : NULL;
}

static char const *toggle_label(FluxNodeData *d) {
	return d->component_data ? (( FluxCheckboxData * ) d->component_data)->label : NULL;
}

static void retarget_button(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  ctx, dst, button_label(d), {11.0f, 5.0f, 11.0f, 6.0f},
         {0.0f, 32.0f},
         {NAN, 32.0f}
    });
	xent_set_focusable(ctx, dst, true);
}

static void retarget_repeat(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	flux_repeat_retarget(store, dst);
	retarget_button(ctx, store, dst);
}

static void retarget_hyperlink(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	retarget_button(ctx, store, dst);
	xent_setself(ctx, dst, XENT_FLEX_ALIGN_START);
}

static void retarget_dropdown(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  ctx, dst, button_label(d), {11.0f, 5.0f, 31.0f, 6.0f},
         {0.0f, 32.0f},
         {32.0f, 32.0f}
    });
	xent_set_focusable(ctx, dst, true);
}

static void retarget_split(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_split_retarget(store, dst);
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  ctx, dst, button_label(d), {11.0f, 5.0f, 11.0f + FLUX_SPLIT_DIVIDER_W + FLUX_SPLIT_SECONDARY_W, 6.0f},
	  {0.0f, 32.0f},
         {68.0f, 32.0f}
    });
}

static void retarget_slider(XentCtx *ctx, FluxStore *store, XentNodeId dst) { flux_slider_retarget(ctx, store, dst); }

static void retarget_scroll_bar(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	flux_scroll_bar_retarget(ctx, store, dst);
}

static void retarget_picker(XentCtx *ctx, FluxStore *store, XentNodeId dst) { flux_picker_retarget(ctx, store, dst); }

static void retarget_looping(XentCtx *ctx, FluxStore *store, XentNodeId dst) { flux_looping_retarget(ctx, store, dst); }

static void retarget_rating(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) ctx;
	flux_rating_retarget(store, dst);
}

static void retarget_checkbox(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_toggle_applymetrics(ctx, dst, FLUX_CONTROL_CHECKBOX, toggle_label(d));
	xent_set_focusable(ctx, dst, true);
}

static void retarget_radio(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_toggle_applymetrics(ctx, dst, FLUX_CONTROL_RADIO, toggle_label(d));
	xent_set_focusable(ctx, dst, true);
}

static void retarget_switch(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  ctx, dst, NULL, {0},
         {0.0f, 32.0f},
         {40.0f, 32.0f}
    });
	xent_set_focusable(ctx, dst, true);
}

static void retarget_scroll(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) store;
	layout_flex_column(ctx, dst);
}

static void retarget_list(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d) return;
	if (d->component_data) {
		FluxListViewData *ld = ( FluxListViewData * ) d->component_data;
		ld->root             = dst;
		ld->ctx              = ctx;
		ld->store            = store;
	}
	layout_flex_column(ctx, dst);
}

static void retarget_radio_buttons(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxRadioButtonsData *g = ( FluxRadioButtonsData * ) d->component_data;
	g->root                 = dst;
	g->ctx                  = ctx;
	g->store                = store;
}

static void retarget_tree(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) ctx;
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	(( FluxTreeViewData * ) d->component_data)->root = dst;
}

static void retarget_refresh(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) ctx;
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	(( FluxRefreshData * ) d->component_data)->root = dst;
}

static void retarget_selector(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) ctx;
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	(( FluxSelectorBarData * ) d->component_data)->root = dst;
}

static void retarget_breadcrumb(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	( void ) ctx;
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	(( FluxBreadcrumbBarData * ) d->component_data)->root = dst;
}

static void retarget_nav(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxNavViewData *nav = ( FluxNavViewData * ) d->component_data;
	nav->root            = dst;
	nav->ctx             = ctx;
	nav->store           = store;
}

static void retarget_tab(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxTabViewData *tv = ( FluxTabViewData * ) d->component_data;
	tv->root            = dst;
	tv->ctx             = ctx;
	tv->store           = store;
}

static void retarget_menu_bar(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxMenuBarData *mb = ( FluxMenuBarData * ) d->component_data;
	mb->root            = dst;
	mb->ctx             = ctx;
	mb->store           = store;
}

static void retarget_expander(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxExpanderData *ex = ( FluxExpanderData * ) d->component_data;
	ex->root             = dst;
	ex->ctx              = ctx;
	ex->store            = store;
}

static void retarget_combo(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	typedef struct {
		FluxComboBoxData model;
		FluxStore       *store;
		XentNodeId       node;
		XentCtx         *ctx;
	} ComboHead;

	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	ComboHead *h = ( ComboHead * ) d->component_data;
	h->node      = dst;
	h->store     = store;
	h->ctx       = ctx;
}

static void retarget_textbox(XentCtx *ctx, FluxStore *store, XentNodeId dst) { flux_textbox_retarget(ctx, store, dst); }

static void retarget_info_bar(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	typedef struct {
		FluxStore *store;
		XentNodeId node;
		XentCtx   *ctx;
	} IbBind;

	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxInfoBarData *ib = ( FluxInfoBarData * ) d->component_data;
	ib->layout_ctx      = ctx;
	ib->layout_node     = dst;
	IbBind *b           = ( IbBind * ) d->behavior.on_click_ctx;
	if (!b) return;
	b->node  = dst;
	b->ctx   = ctx;
	b->store = store;
}

static void retarget_flip(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxFlipViewData *fv = ( FluxFlipViewData * ) d->component_data;
	fv->root             = dst;
	fv->ctx              = ctx;
	fv->store            = store;
}

static void retarget_pager(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxPagerData *pd = ( FluxPagerData * ) d->component_data;
	pd->node          = dst;
	pd->ctx           = ctx;
	pd->store         = store;
	flux_pager_configure(store, dst, pd->count, pd->selected, pd->display_mode);
}

static void retarget_pips(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxPipsPagerData *pd = ( FluxPipsPagerData * ) d->component_data;
	pd->node              = dst;
	pd->ctx               = ctx;
	pd->store             = store;
	flux_pips_configure(store, dst, pd->count, pd->selected, pd->max_visible, ( int ) pd->nav_vis);
}

static void retarget_person(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxPersonPictureData *person = ( FluxPersonPictureData * ) d->component_data;
	person->ctx                   = ctx;
	person->store                 = store;
	person->node                  = dst;
}

static void retarget_split_view(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxSplitViewData *split = ( FluxSplitViewData * ) d->component_data;
	split->ctx               = ctx;
	split->store             = store;
	split->node              = dst;
}

static void retarget_title_bar(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	FluxTitleBarData *bar = ( FluxTitleBarData * ) d->component_data;
	bar->ctx              = ctx;
	bar->store            = store;
	bar->node             = dst;
}

static void retarget_auto_suggest(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	typedef struct {
		FluxStore *store;
		XentCtx   *ctx;
		XentNodeId root;
	} AsbHead;

	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	AsbHead *h = ( AsbHead * ) d->component_data;
	h->root    = dst;
	h->ctx     = ctx;
	h->store   = store;
}

static void retarget_teaching_tip(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	flux_teachtip_retarget(ctx, store, dst);
}

#define SEED_NONE XENT_NODE_INVALID

static XentNodeId seed_date_picker(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_datepicker_init(&(FluxDatePickerDesc) {
	  .ctx     = ctx,
	  .store   = store,
	  .parent  = SEED_NONE,
	  .input   = m->env->input,
	  .window  = m->env->window,
	  .overlay = m->env->overlay});
}

static XentNodeId seed_time_picker(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_timepicker_init(&(FluxTimePickerDesc) {
	  .ctx     = ctx,
	  .store   = store,
	  .parent  = SEED_NONE,
	  .input   = m->env->input,
	  .window  = m->env->window,
	  .overlay = m->env->overlay,
	  .minutes = -1});
}

static XentNodeId seed_image(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_image_init(&(FluxImageDesc) {ctx, store, SEED_NONE, NULL, FLUX_IMAGE_UNIFORM});
}

static XentNodeId seed_scroll(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	XentNodeId    node = flux_scroll_init(&(FluxContainerDesc) {ctx, store, SEED_NONE});
	FluxNodeData *nd   = node != XENT_NODE_INVALID ? flux_store_find(store, node) : NULL;
	if (nd) (( FluxScrollData * ) nd->component_data)->reports = 1;
	return node;
}

static XentNodeId seed_flat(FluxStore *store, XentNodeId node) {
	flux_list_uia_flatten(store, node);
	return node;
}

static XentNodeId seed_list(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return seed_flat(
	  store, flux_list_init(&(FluxListViewDesc) {
			   .ctx = ctx, .store = store, .parent = SEED_NONE, .input = m->env->input, .realize_events = true})
	);
}

static XentNodeId seed_list_box(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return seed_flat(
	  store, flux_list_init(&(FluxListViewDesc) {
			   .ctx            = ctx,
			   .store          = store,
			   .parent         = SEED_NONE,
			   .kind           = XTK_LIST_KIND_LIST_BOX,
			   .input          = m->env->input,
			   .realize_events = true})
	);
}

static XentNodeId seed_grid(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return seed_flat(
	  store, flux_list_init(&(FluxListViewDesc) {
			   .ctx            = ctx,
			   .store          = store,
			   .parent         = SEED_NONE,
			   .kind           = XTK_LIST_KIND_GRID,
			   .input          = m->env->input,
			   .realize_events = true})
	);
}

static XentNodeId seed_repeater(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_list_init(&(FluxListViewDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .kind           = XTK_LIST_KIND_REPEATER,
	  .input          = m->env->input,
	  .realize_events = true});
}

static XentNodeId seed_list_item(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_listitem_init(&(FluxListItemDesc) {ctx, store, SEED_NONE});
}

static XentNodeId seed_group_header(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_groupheader_init(&(FluxListItemDesc) {ctx, store, SEED_NONE});
}

static XentNodeId seed_items_view(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_itemsview_init(&(FluxItemsViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .input = m->env->input});
}

static XentNodeId seed_tree(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_tree_init(&(FluxTreeViewDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_radio_buttons(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_radiogroup_init(&(FluxRadioButtonsDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_selector(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_selector_init(&(FluxSelectorBarDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_breadcrumb(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_breadcrumb_init(&(FluxBreadcrumbBarDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = m->env->window,
	  .text   = m->env->text,
	  .theme  = m->env->theme,
	  .input  = m->env->input});
}

static XentNodeId seed_teaching_tip(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_teachtip_init(&(FluxTeachingTipDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = m->env->window,
	  .text   = m->env->text,
	  .theme  = m->env->theme,
	  .input  = m->env->input});
}

static XentNodeId seed_person(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_avatar_init(&(FluxPersonPictureDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_menu_bar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_menubar_init(&(FluxMenuBarDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = m->env->window,
	  .text   = m->env->text,
	  .theme  = m->env->theme});
}

static XentNodeId seed_flip(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_flip_init(&(FluxFlipViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = m->env->window});
}

static XentNodeId seed_auto_suggest(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_suggest_init(&(FluxAutoSuggestDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .app    = m->env->app,
	  .window = m->env->window,
	  .text   = m->env->text,
	  .theme  = m->env->theme});
}

typedef struct MountStrings {
	char const **items;
	int          count;
} MountStrings;

static bool mount_strings_open(FluxControlMount const *m, XtkPropertyId id, MountStrings *out) {
	XtkValue const *v = mount_value(m, id, XTK_VALUE_BYTES);
	if (!v) return true;
	uint32_t at = 0u;
	int      n  = 0;
	while (at < v->u.bytes.len) {
		uint8_t const *end = memchr(v->u.bytes.data + at, 0, v->u.bytes.len - at);
		if (!end || n == INT_MAX) return false;
		at = ( uint32_t ) (end - v->u.bytes.data) + 1u;
		n++;
	}
	char const **items = n ? ( char const ** ) malloc(sizeof(*items) * ( size_t ) n) : NULL;
	if (n && !items) return false;
	at = 0u;
	for (int i = 0; i < n; i++) {
		items [i]  = ( char const * ) v->u.bytes.data + at;
		at        += ( uint32_t ) strlen(items [i]) + 1u;
	}
	*out = (MountStrings) {.items = items, .count = n};
	return true;
}

static void mount_strings_close(MountStrings *strings) {
	free(strings->items);
	*strings = (MountStrings) {0};
}

static XentNodeId mount_combo(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	MountStrings strings = {0};
	if (!mount_strings_open(m, XTK_PROP_ITEMS, &strings)) return XENT_NODE_INVALID;
	XentNodeId node = flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .window         = m->env->window,
	  .text           = m->env->text,
	  .theme          = m->env->theme,
	  .app            = m->env->app,
	  .input          = m->env->input,
	  .items          = strings.items,
	  .item_count     = strings.count,
	  .selected_index = mount_i32(m, XTK_PROP_SELECTED_INDEX, -1),
	  .placeholder    = mount_string(m, XTK_PROP_PLACEHOLDER, NULL)});
	mount_strings_close(&strings);
	return node;
}

static XentNodeId mount_button(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_button_init(&(FluxButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), NULL, NULL});
}

static XentNodeId mount_repeat(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_repeat_init(&(FluxButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), NULL, NULL});
}

static XentNodeId mount_dropdown(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_dropdown_init(&(FluxDropDownButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), mount_string(m, XTK_PROP_ICON, NULL)});
}

static XentNodeId mount_split(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_split_init(&(FluxSplitButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), mount_string(m, XTK_PROP_ICON, NULL), NULL, NULL});
}

static XentNodeId mount_toggle_split(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_togglesplit_init(&(FluxToggleSplitButtonDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .label     = mount_string(m, XTK_PROP_TEXT, NULL),
	  .icon_name = mount_string(m, XTK_PROP_ICON, NULL),
	  .checked   = mount_bool(m, XTK_PROP_CHECKED, false)});
}

static XentNodeId mount_hyperlink(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_link_init(&(FluxHyperlinkDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), mount_string(m, XTK_PROP_URL, NULL), NULL, NULL});
}

static XentNodeId mount_text(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_text_init(&(FluxTextDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, ""), mount_f32(m, XTK_PROP_FONT_SIZE, 14.0f),
	  m->env->text});
}

static XentNodeId
mount_toggle(XentCtx *ctx, FluxStore *store, FluxControlMount const *m, XentNodeId (*init)(FluxToggleDesc const *)) {
	return init(&(FluxToggleDesc) {
	  ctx, store, SEED_NONE, mount_string(m, XTK_PROP_TEXT, NULL), mount_bool(m, XTK_PROP_CHECKED, false), NULL, NULL});
}

#define MOUNT_TOGGLE(name, init)                                                        \
	static XentNodeId name(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) { \
		return mount_toggle(ctx, store, m, init);                                       \
	}

MOUNT_TOGGLE(mount_checkbox, flux_checkbox_init)
MOUNT_TOGGLE(mount_radio, flux_radio_init)
MOUNT_TOGGLE(mount_switch, flux_switch_init)

static XentNodeId mount_slider(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_slider_init(&(FluxSliderDesc) {
	  ctx, store, SEED_NONE, mount_f32(m, XTK_PROP_MIN_F32, 0.0f), mount_f32(m, XTK_PROP_MAX_F32, 1.0f),
	  mount_f32(m, XTK_PROP_VALUE_F32, 0.0f), NULL, NULL});
}

static XentNodeId mount_progress(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_progress_init(&(FluxProgressDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .value     = mount_f32(m, XTK_PROP_VALUE_F32, 0.0f),
	  .max_value = mount_f32(m, XTK_PROP_MAX_F32, 100.0f),
	  .min_value = mount_f32(m, XTK_PROP_MIN_F32, 0.0f)});
}

static XentNodeId mount_progress_ring(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_progressring_init(&(FluxProgressDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .value     = mount_f32(m, XTK_PROP_VALUE_F32, 0.0f),
	  .max_value = mount_f32(m, XTK_PROP_MAX_F32, 100.0f),
	  .min_value = mount_f32(m, XTK_PROP_MIN_F32, 0.0f)});
}

static XentNodeId mount_badge(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_badge_init(&(FluxInfoBadgeDesc) {
	  ctx, store, SEED_NONE, ( FluxInfoBadgeMode ) mount_enum(m, XTK_PROP_BADGE_MODE, 0u),
	  mount_i32(m, XTK_PROP_BADGE_VALUE, 0)});
}

static XentNodeId mount_looping(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	MountStrings strings = {0};
	if (!mount_strings_open(m, XTK_PROP_ITEMS, &strings)) return XENT_NODE_INVALID;
	XentNodeId node = flux_looping_init(&(FluxLoopingDesc) {
	  .ctx         = ctx,
	  .store       = store,
	  .parent      = SEED_NONE,
	  .items       = strings.items,
	  .item_count  = strings.count,
	  .selected    = mount_i32(m, XTK_PROP_SELECTED_INDEX, -1),
	  .item_height = mount_f32(m, XTK_PROP_ITEM_HEIGHT, 0.0f),
	  .item_width  = mount_f32(m, XTK_PROP_ITEM_WIDTH, 0.0f),
	  .should_loop = mount_bool(m, XTK_PROP_SHOULD_LOOP, false)});
	mount_strings_close(&strings);
	return node;
}

static XentNodeId mount_scroll_bar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_scroll_bar_init(&(FluxScrollBarDesc) {
	  .ctx           = ctx,
	  .store         = store,
	  .parent        = SEED_NONE,
	  .orientation   = ( FluxScrollBarOrientation ) mount_enum(m, XTK_PROP_ORIENTATION, FLUX_SCROLLBAR_VERTICAL),
	  .minimum       = mount_f64(m, XTK_PROP_MIN_F64, 0.0),
	  .maximum       = mount_f64(m, XTK_PROP_MAX_F64, 1.0),
	  .value         = mount_f64(m, XTK_PROP_VALUE_F64, 0.0),
	  .viewport_size = mount_f64(m, XTK_PROP_VIEWPORT_SIZE, 0.0)});
}

static XentNodeId mount_rating(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_rating_init(&(FluxRatingDesc) {
	  .ctx               = ctx,
	  .store             = store,
	  .parent            = SEED_NONE,
	  .value             = mount_f64(m, XTK_PROP_VALUE_F64, -1.0),
	  .max_rating        = ( int ) mount_f64(m, XTK_PROP_MAX_F64, 5.0),
	  .placeholder_value = mount_f64(m, XTK_PROP_PLACEHOLDER_F64, -1.0),
	  .initial_set_value = mount_i32(m, XTK_PROP_INITIAL_VALUE, 1),
	  .is_clear_enabled  = mount_bool(m, XTK_PROP_CLEAR_ENABLED, false),
	  .is_read_only      = mount_bool(m, XTK_PROP_READ_ONLY, false),
	  .caption           = mount_string(m, XTK_PROP_TEXT, NULL)});
}

static XentNodeId mount_expander(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_expander_init(&(FluxExpanderDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .window         = m->env->window,
	  .header         = mount_string(m, XTK_PROP_TEXT, NULL),
	  .expanded       = mount_bool(m, XTK_PROP_EXPANDED, false),
	  .width          = mount_f32(m, XTK_PROP_WIDTH, 0.0f),
	  .content_height = mount_f32(m, XTK_PROP_CONTENT_HEIGHT, 0.0f)});
}

static XentNodeId mount_refresh(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_refresh_init(&(FluxRefreshContainerDesc) {
	  .ctx             = ctx,
	  .store           = store,
	  .parent          = SEED_NONE,
	  .window          = m->env->window,
	  .direction       = ( FluxPullDirection ) mount_enum(m, XTK_PROP_PULL_DIRECTION, 0u),
	  .visualizer_size = mount_f32(m, XTK_PROP_VISUALIZER_SIZE, 100.0f),
	  .threshold_ratio = mount_f32(m, XTK_PROP_THRESHOLD_RATIO, 0.8f)});
}

static XentNodeId mount_info_bar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_infobar_init(&(FluxInfoBarDesc) {
	  .ctx         = ctx,
	  .store       = store,
	  .parent      = SEED_NONE,
	  .severity    = ( FluxInfoBarSeverity ) mount_enum(m, XTK_PROP_SEVERITY, 0u),
	  .title       = mount_string(m, XTK_PROP_TEXT, NULL),
	  .message     = mount_string(m, XTK_PROP_MESSAGE, NULL),
	  .is_closable = mount_bool(m, XTK_PROP_CLOSABLE, false)});
}

static XentNodeId mount_dialog(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_dialog_init(&(FluxContentDialogDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .overlay_parent = m->env->overlay,
	  .input          = m->env->input,
	  .window         = m->env->window,
	  .theme          = m->env->theme,
	  .title          = mount_string(m, XTK_PROP_TEXT, NULL),
	  .primary_text   = mount_string(m, XTK_PROP_PRIMARY_TEXT, NULL),
	  .secondary_text = mount_string(m, XTK_PROP_SECONDARY_TEXT, NULL),
	  .close_text     = mount_string(m, XTK_PROP_CLOSE_TEXT, NULL),
	  .width          = mount_f32(m, XTK_PROP_WIDTH, 400.0f),
	  .content_height = mount_f32(m, XTK_PROP_CONTENT_HEIGHT, 120.0f)});
}

static XentNodeId mount_nav(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	if (!m->env->window) return XENT_NODE_INVALID;
	return flux_nav_init(&(FluxNavViewDesc) {
	  .ctx      = ctx,
	  .store    = store,
	  .parent   = SEED_NONE,
	  .window   = m->env->window,
	  .text     = m->env->text,
	  .theme    = m->env->theme,
	  .width    = mount_f32(m, XTK_PROP_WIDTH, 0.0f),
	  .height   = mount_f32(m, XTK_PROP_HEIGHT, 0.0f),
	  .adaptive = true});
}

static XentNodeId mount_textbox(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	char const *placeholder = mount_string(m, XTK_PROP_PLACEHOLDER, NULL);
	if (m->env->app)
		return flux_app_create_textbox(&(FluxAppTextBoxDesc) {m->env->app, SEED_NONE, placeholder, NULL, NULL});
	return flux_textbox_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, placeholder, NULL, NULL});
}

static XentNodeId mount_password(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	char const *placeholder = mount_string(m, XTK_PROP_PLACEHOLDER, NULL);
	if (m->env->app)
		return flux_app_create_password(&(FluxAppTextBoxDesc) {m->env->app, SEED_NONE, placeholder, NULL, NULL});
	return flux_password_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, placeholder, NULL, NULL});
}

static XentNodeId mount_number(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	double min  = mount_f64(m, XTK_PROP_MIN_F64, 0.0);
	double max  = mount_f64(m, XTK_PROP_MAX_F64, 100.0);
	double step = mount_f64(m, XTK_PROP_STEP_F64, 1.0);
	if (m->env->app)
		return flux_app_create_number(&(FluxAppNumberBoxDesc) {m->env->app, SEED_NONE, min, max, step, NULL, NULL});
	return flux_number_init(&(FluxNumberBoxDesc) {ctx, store, SEED_NONE, min, max, step, NULL, NULL});
}

static XentNodeId mount_split_view(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_splitview_init(&(FluxSplitViewDesc) {
	  .ctx                 = ctx,
	  .store               = store,
	  .parent              = SEED_NONE,
	  .display_mode        = ( int ) mount_enum(m, XTK_PROP_DISPLAY_MODE, 0u),
	  .pane_placement      = ( int ) mount_enum(m, XTK_PROP_PANE_PLACEMENT, 0u),
	  .pane_open           = mount_bool(m, XTK_PROP_OPEN, false),
	  .open_pane_length    = mount_f32(m, XTK_PROP_OPEN_PANE_LENGTH, 0.0f),
	  .compact_pane_length = mount_f32(m, XTK_PROP_COMPACT_PANE_LENGTH, 0.0f)});
}

static XentNodeId mount_title_bar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_titlebar_init(&(FluxTitleBarDesc) {
	  .ctx              = ctx,
	  .store            = store,
	  .parent           = SEED_NONE,
	  .window           = m->env->window,
	  .title            = mount_string(m, XTK_PROP_TEXT, NULL),
	  .subtitle         = mount_string(m, XTK_PROP_SUBTITLE, NULL),
	  .icon             = mount_string(m, XTK_PROP_ICON, NULL),
	  .show_back        = mount_bool(m, XTK_PROP_SHOW_BACK, false),
	  .back_disabled    = mount_bool(m, XTK_PROP_BACK_DISABLED, false),
	  .show_pane_toggle = mount_bool(m, XTK_PROP_SHOW_PANE_TOGGLE, false),
	  .integrate_window = false});
}

static XentNodeId mount_pager(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_pager_init(&(FluxPagerDesc) {
	  .ctx          = ctx,
	  .store        = store,
	  .parent       = SEED_NONE,
	  .count        = mount_i32(m, XTK_PROP_ITEM_COUNT, 1),
	  .selected     = mount_i32(m, XTK_PROP_SELECTED_INDEX, 0),
	  .display_mode = ( int ) mount_enum(m, XTK_PROP_DISPLAY_MODE, 0u),
	  .first_button = ( int ) mount_enum(m, XTK_PROP_FIRST_BUTTON, 0u),
	  .prev_button  = ( int ) mount_enum(m, XTK_PROP_PREVIOUS_BUTTON, 0u),
	  .next_button  = ( int ) mount_enum(m, XTK_PROP_NEXT_BUTTON, 0u),
	  .last_button  = ( int ) mount_enum(m, XTK_PROP_LAST_BUTTON, 0u),
	  .prefix       = mount_string(m, XTK_PROP_PREFIX, NULL),
	  .suffix       = mount_string(m, XTK_PROP_SUFFIX, NULL)});
}

static XentNodeId mount_pips(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	XentNodeId node = flux_pips_init(&(FluxPipsPagerDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .vertical = mount_enum(m, XTK_PROP_ORIENTATION, 0u) != 0u});
	if (node == XENT_NODE_INVALID) return node;
	flux_pips_configure(
	  store, node, mount_i32(m, XTK_PROP_ITEM_COUNT, 0), mount_i32(m, XTK_PROP_SELECTED_INDEX, 0),
	  mount_i32(m, XTK_PROP_MAX_VISIBLE, 5), ( int ) mount_enum(m, XTK_PROP_NAV_VISIBILITY, 0u)
	);
	return node;
}

static bool mount_tab_values(FluxStore *store, XentNodeId node, FluxControlMount const *m) {
	MountStrings strings = {0};
	if (!mount_strings_open(m, XTK_PROP_ITEMS, &strings)) return false;
	if ((strings.count & 1) != 0 || strings.count / 2 > FLUX_TAB_VIEW_MAX_TABS) {
		mount_strings_close(&strings);
		return false;
	}
	char const *icons [FLUX_TAB_VIEW_MAX_TABS];
	char const *labels [FLUX_TAB_VIEW_MAX_TABS];
	for (int i = 0; i < strings.count / 2; i++) {
		icons [i]  = strings.items [i * 2][0] ? strings.items [i * 2] : NULL;
		labels [i] = strings.items [i * 2 + 1];
	}
	bool ok = flux_tabs_mount_items(
	  store, node, icons, labels, strings.count / 2, mount_i32(m, XTK_PROP_SELECTED_INDEX, -1),
	  mount_bool(m, XTK_PROP_ADD_VISIBLE, false)
	);
	mount_strings_close(&strings);
	return ok;
}

static XentNodeId mount_tab(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	XentNodeId node = flux_tabs_init(&(FluxTabViewDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = m->env->window,
	  .text   = m->env->text,
	  .input  = m->env->input});
	if (node == XENT_NODE_INVALID || mount_tab_values(store, node, m)) return node;
	flux_tree_drop(store, node);
	return XENT_NODE_INVALID;
}

static XentNodeId seed_colheader(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_colheader_init(ctx, store, m->env->window);
}

static XentNodeId seed_colitem(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_colitem_init(ctx, store, m->env->window);
}

static XentNodeId seed_commandbar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_cmdbar_init(ctx, store, m->env);
}

static XentNodeId seed_appbarbutton(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_appbar_init(ctx, store, m->env->text, FLUX_APPBAR_BUTTON);
}

static XentNodeId seed_appbartoggle(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_appbar_init(ctx, store, m->env->text, FLUX_APPBAR_TOGGLE);
}

static XentNodeId seed_appbarsep(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_appbar_init(ctx, store, m->env->text, FLUX_APPBAR_SEPARATOR);
}

static XentNodeId seed_calendar(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	( void ) m;
	return flux_calendar_init(ctx, store);
}

static XentNodeId seed_colorpicker(XentCtx *ctx, FluxStore *store, FluxControlMount const *m) {
	return flux_colorpicker_init(ctx, store, m->env);
}

static bool shell_menuitem(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlEnv const *env) {
	( void ) env;
	return flux_host_menuitem_build(ctx, store, node);
}

static bool absorb_seed(XentCtx *ctx, FluxStore *store, XentNodeId node, XentNodeId seed, FluxControlModule const *m) {
	if (seed == XENT_NODE_INVALID) return false;
	transfer(ctx, store, node, seed);
	if (m->retarget) m->retarget(ctx, store, node);
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_data;
}

bool flux_control_build(
  XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlType type, FluxControlMount const *mount
) {
	if (!mount || !mount->env) return false;
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return false;
	if (nd->component_data || ( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return true;
	FluxControlModule const *m = flux_module_get(type);
	if (!m) return false;
	if (m->shell) return m->shell(ctx, store, node, mount->env);
	if (m->layout) m->layout(ctx, node);
	return m->seed ? absorb_seed(ctx, store, node, m->seed(ctx, store, mount), m) : true;
}

void flux_control_unmount(FluxStore *store, XentNodeId node, FluxControlType type) {
	FluxControlModule const *m = flux_module_get(type);
	if (m && m->unmount) m->unmount(store, node);
}

static uint32_t frame_list(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) store;
	( void ) node;
	( void ) flux_list_sync_layout(nd);
	return flux_list_window_stale(ctx, nd) ? FLUX_FRAME_DEFER : 0u;
}

static uint32_t frame_scroll(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) store;
	return flux_scroll_sync(ctx, node, nd) ? FLUX_FRAME_DEFER : 0u;
}

static uint32_t frame_looping(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) ctx;
	( void ) store;
	return flux_looping_sync(node, nd) ? FLUX_FRAME_REDRAW : 0u;
}

static uint32_t frame_tabs(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) ctx;
	( void ) nd;
	flux_tabs_sync_layout(store, node);
	return 0u;
}

#define FRAME_CTX(name, fn)                                                                   \
	static uint32_t name(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) { \
		( void ) store;                                                                       \
		fn(ctx, node, nd);                                                                    \
		return 0u;                                                                            \
	}

#define FRAME_NODE(name, fn)                                                                  \
	static uint32_t name(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) { \
		( void ) ctx;                                                                         \
		( void ) store;                                                                       \
		fn(node, nd);                                                                         \
		return 0u;                                                                            \
	}

FRAME_CTX(frame_flip, flux_flip_sync)
FRAME_CTX(frame_splitview, flux_splitview_sync)
FRAME_CTX(frame_breadcrumb, flux_breadcrumb_sync)
FRAME_CTX(collect_titlebar, flux_titlebar_sync)
FRAME_NODE(frame_picker, flux_picker_sync)
FRAME_NODE(frame_colorpicker, flux_colorpicker_sync)

static void touch_self(FluxTouch *touch, XentNodeId node) { flux_touch_note(touch, node); }

static void touch_listitem(FluxTouch *touch, XentNodeId node) {
	XentNodeId owner = node;
	for (int i = 0; i < 3 && owner != XENT_NODE_INVALID; i++) owner = xent_node_parent(touch->ctx, owner);
	if (owner != XENT_NODE_INVALID && xtk_ctrl_islist(flux_node_type(touch->store, owner)))
		flux_touch_note(touch, owner);
}

static void touch_radio(FluxTouch *touch, XentNodeId node) {
	XentNodeId parent = xent_node_parent(touch->ctx, node);
	if (parent != XENT_NODE_INVALID && flux_node_type(touch->store, parent) == FLUX_CONTROL_RADIO_BUTTONS)
		flux_touch_note(touch, parent);
}

static void touch_menufly(FluxTouch *touch, XentNodeId fly) {
	flux_touch_note(touch, fly);
	XentNodeId parent = flux_host_menufly_parent(touch->store, fly);
	if (parent != XENT_NODE_INVALID && flux_node_type(touch->store, parent) == FLUX_CONTROL_MENU_BAR)
		flux_touch_note(touch, parent);
}

static void touch_menuitem(FluxTouch *touch, XentNodeId node) {
	touch_menufly(touch, xent_node_parent(touch->ctx, node));
}

static FluxTreeHooks const tree_list = {
  .content  = flux_list_content_node,
  .slot     = flux_list_slot,
  .touch    = touch_self,
  .commit   = flux_list_apply_pending,
  .frame    = frame_list,
  .deferred = flux_list_notify_stale,
  .collect  = flux_list_stick,
};
static FluxTreeHooks const tree_listitem = {.placed = fluxlistitem_bind_owner, .touch = touch_listitem};
static FluxTreeHooks const tree_scroll   = {.frame = frame_scroll, .deferred = flux_scroll_report};
static FluxTreeHooks const tree_nav      = {.content = flux_nav_content_node};
static FluxTreeHooks const tree_expander = {.content = flux_expbody_node};
static FluxTreeHooks const tree_dialog   = {.content = flux_dialogbody_node};
static FluxTreeHooks const tree_flip     = {.content = flux_flip_content_node, .frame = frame_flip};
static FluxTreeHooks const tree_refresh  = {.content = flux_refresh_content_node};
static FluxTreeHooks const tree_tabs     = {.content = flux_tabs_content_node, .frame = frame_tabs};
static FluxTreeHooks const tree_menubar
  = {.content = flux_menubar_content_node, .touch = touch_self, .commit = flux_host_menu_sync_bar};
static FluxTreeHooks const tree_menufly    = {.touch = touch_menufly, .commit = flux_host_menu_sync_fly};
static FluxTreeHooks const tree_menuitem   = {.touch = touch_menuitem};
static FluxTreeHooks const tree_radiogroup = {.touch = touch_self, .commit = flux_radiogroup_sync_children};
static FluxTreeHooks const tree_radio      = {.touch = touch_radio};
static FluxTreeHooks const tree_splitview  = {.frame = frame_splitview};
static FluxTreeHooks const tree_breadcrumb = {.frame = frame_breadcrumb};
static FluxTreeHooks const tree_looping    = {.frame = frame_looping};
static FluxTreeHooks const tree_commandbar
  = {.content = flux_cmdbar_content, .slot = flux_cmdbar_slot, .frame = flux_cmdbar_frame};
static FluxTreeHooks const tree_picker      = {.frame = frame_picker};
static FluxTreeHooks const tree_colorpicker = {.frame = frame_colorpicker};
static FluxTreeHooks const tree_textbox     = {.focus = flux_textbox_sync_ime};
static FluxTreeHooks const tree_titlebar    = {.collect = collect_titlebar};

#define WHEEL(name, fn)                                                                  \
	static bool name(FluxStore *store, XentNodeId node, float delta, float *remaining) { \
		( void ) remaining;                                                              \
		return fn(store, node, delta);                                                   \
	}

WHEEL(wheel_combo, flux_combo_handle_wheel)
WHEEL(wheel_flip, flux_flip_handle_wheel)
WHEEL(wheel_calendar, flux_calendar_handle_wheel)

static bool wheel_textbox(FluxStore *store, XentNodeId node, float delta, float *remaining) {
	( void ) delta;
	*remaining -= flux_textbox_wheel(store, node, *remaining);
	return *remaining == 0.0f;
}

static FluxInputHooks const input_combo    = {.wheel = wheel_combo};
static FluxInputHooks const input_flip     = {.wheel = wheel_flip};
static FluxInputHooks const input_calendar = {.wheel = wheel_calendar};
static FluxInputHooks const input_textbox  = {.wheel = wheel_textbox};
static FluxInputHooks const input_looping  = {.scroll_wheel = flux_looping_handle_wheel};
static FluxInputHooks const input_refresh
  = {.pull = flux_refresh_touch_pull, .release = flux_refresh_touch_release, .cancel = flux_refresh_touch_cancel};

static void range_slider(FluxStore *store, XentNodeId node, double value) {
	flux_prop_slider_value(store, node, ( float ) value);
}

static FluxUiaHooks const uia_button_invoke        = {.role = FLUX_UIA_BUTTON, .patterns = FLUX_UIA_P_INVOKE};
static FluxUiaHooks const uia_button_toggle        = {.role = FLUX_UIA_BUTTON, .patterns = FLUX_UIA_P_TOGGLE};
static FluxUiaHooks const uia_checkbox_toggle      = {.role = FLUX_UIA_CHECKBOX, .patterns = FLUX_UIA_P_TOGGLE};
static FluxUiaHooks const uia_group                = {.role = FLUX_UIA_GROUP, .patterns = 0};
static FluxUiaHooks const uia_header               = {.role = FLUX_UIA_HEADER, .patterns = 0};
static FluxUiaHooks const uia_headeritem           = {.role = FLUX_UIA_HEADERITEM, .patterns = 0};
static FluxUiaHooks const uia_hidden               = {.role = FLUX_UIA_HIDDEN, .patterns = 0};
static FluxUiaHooks const uia_hyperlink_invoke     = {.role = FLUX_UIA_HYPERLINK, .patterns = FLUX_UIA_P_INVOKE};
static FluxUiaHooks const uia_image                = {.role = FLUX_UIA_IMAGE, .patterns = 0};
static FluxUiaHooks const uia_list                 = {.role = FLUX_UIA_LIST, .patterns = 0};
static FluxUiaHooks const uia_menu                 = {.role = FLUX_UIA_MENU, .patterns = 0};
static FluxUiaHooks const uia_menubar              = {.role = FLUX_UIA_MENUBAR, .patterns = 0};
static FluxUiaHooks const uia_pane                 = {.role = FLUX_UIA_PANE, .patterns = 0};
static FluxUiaHooks const uia_progressbar_range    = {.role = FLUX_UIA_PROGRESSBAR, .patterns = FLUX_UIA_P_RANGE};
static FluxUiaHooks const uia_radio_selection_item = {.role = FLUX_UIA_RADIO, .patterns = FLUX_UIA_P_SELECTION_ITEM};
static FluxUiaHooks const uia_separator            = {.role = FLUX_UIA_SEPARATOR, .patterns = 0};
static FluxUiaHooks const uia_slider_role          = {.role = FLUX_UIA_SLIDER, .patterns = 0};
static FluxUiaHooks const uia_statusbar            = {.role = FLUX_UIA_STATUSBAR, .patterns = 0};
static FluxUiaHooks const uia_tab                  = {.role = FLUX_UIA_TAB, .patterns = 0};
static FluxUiaText const  uia_text_doc             = {.view = flux_text_uia_view, .select = flux_text_uia_select};
static FluxUiaHooks const uia_text        = {.role = FLUX_UIA_TEXT, .patterns = FLUX_UIA_P_TEXT, .doc = &uia_text_doc};
static FluxUiaHooks const uia_titlebar    = {.role = FLUX_UIA_TITLEBAR, .patterns = 0};
static FluxUiaHooks const uia_tooltip     = {.role = FLUX_UIA_TOOLTIP, .patterns = 0};
static FluxUiaHooks const uia_window      = {.role = FLUX_UIA_WINDOW, .patterns = 0};
static FluxUiaHooks const uia_scroll_pane = {.role = FLUX_UIA_PANE, .patterns = FLUX_UIA_P_SCROLL};
static FluxUiaHooks const uia_dataitem_scroll = {.role = FLUX_UIA_DATAITEM, .patterns = FLUX_UIA_P_SCROLL_ITEM};
static FluxUiaHooks const uia_expander        = {
  .role          = FLUX_UIA_BUTTON,
  .patterns      = FLUX_UIA_P_EXPAND,
  .expand        = flux_expander_uia_expand,
  .toggle_expand = flux_expander_uia_toggle};
static FluxUiaHooks const uia_dropdown = {
  .role          = FLUX_UIA_BUTTON,
  .patterns      = FLUX_UIA_P_INVOKE | FLUX_UIA_P_EXPAND,
  .expand        = flux_dropdown_uia_expand,
  .toggle_expand = flux_dropdown_uia_toggle};
static FluxUiaHooks const uia_split = {
  .role          = FLUX_UIA_SPLITBUTTON,
  .patterns      = FLUX_UIA_P_INVOKE | FLUX_UIA_P_EXPAND,
  .expand        = flux_split_uia_expand,
  .toggle_expand = flux_split_uia_toggle};
static FluxUiaHooks const uia_togglesplit = {
  .role          = FLUX_UIA_SPLITBUTTON,
  .patterns      = FLUX_UIA_P_TOGGLE | FLUX_UIA_P_EXPAND,
  .expand        = flux_split_uia_expand,
  .toggle_expand = flux_split_uia_toggle};
static FluxUiaHooks const uia_menubaritem = {
  .role          = FLUX_UIA_MENUITEM,
  .patterns      = FLUX_UIA_P_INVOKE | FLUX_UIA_P_EXPAND,
  .expand        = flux_menubar_uia_expand,
  .toggle_expand = flux_menubar_uia_toggle};
static FluxUiaHooks const uia_commandbar = {
  .role          = FLUX_UIA_APPBAR,
  .patterns      = FLUX_UIA_P_EXPAND,
  .expand        = flux_cmdbar_uia_expand,
  .toggle_expand = flux_cmdbar_uia_toggle};

static unsigned uia_mode_required(FluxStore *store, XentNodeId node) {
	( void ) store;
	( void ) node;
	return FLUX_UIA_SEL_REQUIRED;
}

static FluxUiaHooks const uia_list_sel
  = {.role = FLUX_UIA_LIST, .patterns = FLUX_UIA_P_SELECTION, .select_mode = flux_list_uia_mode};
static FluxUiaSub const uia_list_virtual = {
  .kind     = flux_list_uia_kind,
  .rect     = flux_list_uia_rect,
  .name     = flux_list_uia_name,
  .selected = flux_list_uia_subselected,
  .select   = flux_list_uia_subselect,
  .scroll   = flux_list_scrollto,
  .count    = flux_list_uia_count,
  .index    = flux_list_uia_index,
  .realized = flux_list_uia_realized,
  .realize  = flux_list_scrollto};
static FluxUiaHooks const uia_list_items = {
  .role        = FLUX_UIA_LIST,
  .patterns    = FLUX_UIA_P_SELECTION | FLUX_UIA_P_SCROLL | FLUX_UIA_P_ITEMS,
  .select_mode = flux_list_uia_mode,
  .sub         = &uia_list_virtual,
  .scroller    = flux_list_uia_scroller};
static FluxUiaHooks const uia_listitem_sel = {
  .role     = FLUX_UIA_LISTITEM,
  .patterns = FLUX_UIA_P_SCROLL_ITEM | FLUX_UIA_P_SELECTION_ITEM,
  .selected = flux_list_uia_selected,
  .select   = flux_list_uia_select,
  .name     = flux_listitem_uia_name};
static FluxUiaHooks const uia_tree_sel
  = {.role = FLUX_UIA_TREE, .patterns = FLUX_UIA_P_SELECTION, .select_mode = flux_tree_uia_mode};
static FluxUiaHooks const uia_treeitem_sel = {
  .role          = FLUX_UIA_TREEITEM,
  .patterns      = FLUX_UIA_P_SCROLL_ITEM | FLUX_UIA_P_EXPAND | FLUX_UIA_P_SELECTION_ITEM,
  .expand        = flux_tree_uia_expand,
  .toggle_expand = flux_tree_uia_toggle,
  .selected      = flux_tree_uia_selected,
  .select        = flux_tree_uia_select};
static FluxUiaHooks const uia_tabs_sel
  = {.role = FLUX_UIA_TAB, .patterns = FLUX_UIA_P_SELECTION, .select_mode = uia_mode_required};
static FluxUiaHooks const uia_tabitem_sel
  = {.role = FLUX_UIA_TABITEM, .patterns = FLUX_UIA_P_SCROLL_ITEM | FLUX_UIA_P_SELECTION_ITEM};
static FluxUiaHooks const uia_nav_sel     = {.role = FLUX_UIA_CUSTOM, .patterns = FLUX_UIA_P_SELECTION};
static FluxUiaHooks const uia_navitem_sel = {
  .role          = FLUX_UIA_LISTITEM,
  .patterns      = FLUX_UIA_P_EXPAND | FLUX_UIA_P_SELECTION_ITEM,
  .expand        = flux_nav_uia_expand,
  .toggle_expand = flux_nav_uia_toggle};
static FluxUiaHooks const uia_selector_sel = {.role = FLUX_UIA_LIST, .patterns = FLUX_UIA_P_SELECTION};
static FluxUiaHooks const uia_selectoritem_sel
  = {.role = FLUX_UIA_LISTITEM, .patterns = FLUX_UIA_P_SELECTION_ITEM, .selected = flux_selector_uia_selected};
static FluxUiaSub const uia_looping_virtual = {
  .kind     = flux_looping_uia_kind,
  .rect     = flux_list_uia_rect,
  .name     = flux_looping_uia_name,
  .selected = flux_looping_uia_subselected,
  .select   = flux_looping_uia_subselect,
  .scroll   = flux_looping_uia_scrollto,
  .count    = flux_looping_uia_count,
  .index    = flux_looping_uia_index,
  .realized = flux_looping_uia_realized};
static FluxUiaHooks const uia_looping_sel = {
  .role        = FLUX_UIA_LIST,
  .patterns    = FLUX_UIA_P_SELECTION | FLUX_UIA_P_SCROLL | FLUX_UIA_P_ITEMS,
  .select_mode = uia_mode_required,
  .sub         = &uia_looping_virtual,
  .scroll_info = flux_looping_uia_scrollinfo,
  .scroll_by   = flux_looping_uia_scrollby,
  .scroll_set  = flux_looping_uia_scrollset};
static FluxUiaHooks const uia_loopitem_sel = {
  .role     = FLUX_UIA_LISTITEM,
  .patterns = FLUX_UIA_P_SCROLL_ITEM | FLUX_UIA_P_SELECTION_ITEM,
  .selected = flux_looping_uia_selected,
  .name     = flux_loopitem_uia_name};
static FluxUiaHooks const uia_splitpane
  = {.role = FLUX_UIA_WINDOW, .patterns = FLUX_UIA_P_WINDOW, .live = flux_splitpane_uia_live, .modal = true};
static FluxUiaSub const uia_slider_thumbs = {
  .kind    = flux_slider_uia_kind,
  .span    = flux_slider_uia_span,
  .rect    = flux_slider_uia_rect,
  .name    = flux_slider_uia_name,
  .focused = flux_slider_uia_focused};
static FluxUiaSub const uia_calendar_cells = {
  .kind     = flux_calendar_uia_kind,
  .span     = flux_calendar_uia_span,
  .rect     = flux_calendar_uia_rect,
  .name     = flux_calendar_uia_name,
  .focused  = flux_calendar_uia_focused,
  .invoke   = flux_calendar_uia_invoke,
  .selected = flux_calendar_uia_selected,
  .select   = flux_calendar_uia_select,
  .heads    = flux_calendar_uia_heads,
  .grid     = flux_calendar_uia_grid,
  .cell     = flux_calendar_uia_cell,
  .row_head = flux_calendar_uia_rowhead,
  .scroll   = flux_calendar_uia_scroll};
static FluxUiaHooks const uia_calendar = {
  .role        = FLUX_UIA_CALENDAR,
  .patterns    = FLUX_UIA_P_SELECTION | FLUX_UIA_P_GRID | FLUX_UIA_P_TABLE | FLUX_UIA_P_VALUE,
  .select_mode = flux_calendar_uia_mode,
  .sub         = &uia_calendar_cells,
  .text        = flux_calendar_uia_value};
static FluxUiaHooks const uia_suggestitem = {
  .role     = FLUX_UIA_LISTITEM,
  .patterns = FLUX_UIA_P_SELECTION_ITEM | FLUX_UIA_P_SCROLL_ITEM,
  .selected = flux_suggestitem_uia_selected,
  .select   = flux_suggestitem_uia_select};
static FluxUiaHooks const uia_comboitem = {
  .role     = FLUX_UIA_LISTITEM,
  .patterns = FLUX_UIA_P_SELECTION_ITEM | FLUX_UIA_P_SCROLL_ITEM,
  .selected = flux_comboitem_uia_selected,
  .select   = flux_comboitem_uia_select};
static FluxUiaHooks const uia_combo = {
  .role          = FLUX_UIA_COMBOBOX,
  .patterns      = FLUX_UIA_P_VALUE | FLUX_UIA_P_EXPAND | FLUX_UIA_P_SELECTION,
  .expand        = flux_combo_uia_expand,
  .toggle_expand = flux_combo_uia_toggle,
  .text          = flux_combo_text,
  .set_text      = flux_prop_combo_text};
static FluxUiaHooks const uia_picker
  = {.role = FLUX_UIA_BUTTON, .patterns = FLUX_UIA_P_INVOKE | FLUX_UIA_P_VALUE, .text = flux_picker_valuetext};
static FluxUiaText const uia_textbox_doc
  = {.view = flux_textbox_uia_view, .select = flux_textbox_uia_select, .reveal = flux_textbox_uia_reveal};
static FluxUiaHooks const uia_textbox = {
  .role     = FLUX_UIA_EDIT,
  .patterns = FLUX_UIA_P_VALUE | FLUX_UIA_P_TEXT | FLUX_UIA_P_TEXT2,
  .set_text = flux_prop_textbox_content,
  .doc      = &uia_textbox_doc};
static FluxUiaHooks const uia_password
  = {.role = FLUX_UIA_EDIT, .patterns = FLUX_UIA_P_VALUE, .set_text = flux_prop_password_content};
static FluxUiaHooks const uia_number
  = {.role = FLUX_UIA_SPINNER, .patterns = FLUX_UIA_P_RANGE | FLUX_UIA_P_VALUE, .set_range = flux_prop_numberbox_value};
static FluxUiaHooks const uia_scrollbar
  = {.role = FLUX_UIA_SCROLLBAR, .patterns = FLUX_UIA_P_RANGE, .set_range = flux_scroll_bar_setvalue};
static FluxUiaHooks const uia_slider = {
  .role      = FLUX_UIA_SLIDER,
  .patterns  = FLUX_UIA_P_RANGE,
  .set_range = range_slider,
  .thumb     = flux_slider_thumb,
  .set_thumb = flux_slider_userthumb,
  .sub       = &uia_slider_thumbs};

#define MODULE(name)        FluxControlModule const flux_ctrl_##name
#define CONSUMES(name, ...) static XtkPropertyId const name##_consumed [] = {__VA_ARGS__, XTK_PROP_ID_INVALID}
#define DEPS(name, ...)     static FluxControlModule const *const name##_deps [] = {__VA_ARGS__, NULL}

CONSUMES(text, XTK_PROP_TEXT, XTK_PROP_FONT_SIZE);
CONSUMES(label, XTK_PROP_TEXT);
CONSUMES(check, XTK_PROP_TEXT, XTK_PROP_CHECKED);
CONSUMES(range, XTK_PROP_MIN_F32, XTK_PROP_MAX_F32, XTK_PROP_VALUE_F32);
CONSUMES(placeholder, XTK_PROP_PLACEHOLDER);
CONSUMES(number, XTK_PROP_MIN_F64, XTK_PROP_MAX_F64, XTK_PROP_STEP_F64);
CONSUMES(link, XTK_PROP_TEXT, XTK_PROP_URL);
CONSUMES(badge, XTK_PROP_BADGE_MODE, XTK_PROP_BADGE_VALUE);
CONSUMES(labelicon, XTK_PROP_TEXT, XTK_PROP_ICON);
CONSUMES(togglesplit, XTK_PROP_TEXT, XTK_PROP_ICON, XTK_PROP_CHECKED);
CONSUMES(combo, XTK_PROP_ITEMS, XTK_PROP_SELECTED_INDEX, XTK_PROP_PLACEHOLDER);
CONSUMES(expander, XTK_PROP_TEXT, XTK_PROP_EXPANDED, XTK_PROP_WIDTH, XTK_PROP_CONTENT_HEIGHT);
CONSUMES(infobar, XTK_PROP_TEXT, XTK_PROP_MESSAGE, XTK_PROP_SEVERITY, XTK_PROP_CLOSABLE);
CONSUMES(
  dialog, XTK_PROP_TEXT, XTK_PROP_PRIMARY_TEXT, XTK_PROP_SECONDARY_TEXT, XTK_PROP_CLOSE_TEXT, XTK_PROP_WIDTH,
  XTK_PROP_CONTENT_HEIGHT
);
CONSUMES(nav, XTK_PROP_WIDTH, XTK_PROP_HEIGHT);
CONSUMES(tabs, XTK_PROP_ITEMS, XTK_PROP_SELECTED_INDEX, XTK_PROP_ADD_VISIBLE);
CONSUMES(
  pips, XTK_PROP_ORIENTATION, XTK_PROP_ITEM_COUNT, XTK_PROP_SELECTED_INDEX, XTK_PROP_MAX_VISIBLE,
  XTK_PROP_NAV_VISIBILITY
);
CONSUMES(
  rating, XTK_PROP_VALUE_F64, XTK_PROP_MAX_F64, XTK_PROP_PLACEHOLDER_F64, XTK_PROP_INITIAL_VALUE,
  XTK_PROP_CLEAR_ENABLED, XTK_PROP_READ_ONLY, XTK_PROP_TEXT
);
CONSUMES(refresh, XTK_PROP_PULL_DIRECTION, XTK_PROP_VISUALIZER_SIZE, XTK_PROP_THRESHOLD_RATIO);
CONSUMES(
  pager, XTK_PROP_ITEM_COUNT, XTK_PROP_SELECTED_INDEX, XTK_PROP_DISPLAY_MODE, XTK_PROP_FIRST_BUTTON,
  XTK_PROP_PREVIOUS_BUTTON, XTK_PROP_NEXT_BUTTON, XTK_PROP_LAST_BUTTON, XTK_PROP_PREFIX, XTK_PROP_SUFFIX
);
CONSUMES(
  splitview, XTK_PROP_DISPLAY_MODE, XTK_PROP_PANE_PLACEMENT, XTK_PROP_OPEN, XTK_PROP_OPEN_PANE_LENGTH,
  XTK_PROP_COMPACT_PANE_LENGTH
);
CONSUMES(
  titlebar, XTK_PROP_TEXT, XTK_PROP_SUBTITLE, XTK_PROP_ICON, XTK_PROP_SHOW_BACK, XTK_PROP_BACK_DISABLED,
  XTK_PROP_SHOW_PANE_TOGGLE
);

DEPS(list, &flux_ctrl_scroll, &flux_ctrl_listitem, &flux_ctrl_groupheader);
DEPS(tree, &flux_ctrl_list, &flux_ctrl_treeitem);
DEPS(combo, &flux_ctrl_textbox, &flux_ctrl_scroll, &flux_ctrl_comboitem);
DEPS(suggest, &flux_ctrl_textbox, &flux_ctrl_button, &flux_ctrl_scroll, &flux_ctrl_suggestitem);
DEPS(expander, &flux_ctrl_exphead, &flux_ctrl_expbody, &flux_ctrl_text);
DEPS(dialog, &flux_ctrl_dialogbody, &flux_ctrl_card, &flux_ctrl_button, &flux_ctrl_text);
DEPS(menubar, &flux_ctrl_menuitem);
DEPS(nav, &flux_ctrl_navitem, &flux_ctrl_scroll, &flux_ctrl_titlebar);
DEPS(tabs, &flux_ctrl_tabitem, &flux_ctrl_scroll);
DEPS(radiogroup, &flux_ctrl_radio, &flux_ctrl_text);
DEPS(selector, &flux_ctrl_selectoritem, &flux_ctrl_scroll);
DEPS(breadcrumb, &flux_ctrl_crumb);
DEPS(refresh, &flux_ctrl_scroll);
DEPS(splitview, &flux_ctrl_splitpane, &flux_ctrl_splitbody);
DEPS(looping, &flux_ctrl_scroll, &flux_ctrl_loopitem);
DEPS(pickerflyout, &flux_ctrl_looping, &flux_ctrl_button);
DEPS(picker, &flux_ctrl_pickerflyout);
DEPS(colheader, &flux_ctrl_colitem);
DEPS(colitem, &flux_ctrl_colbutton);
DEPS(commandbar, &flux_ctrl_cmdmore);
DEPS(colorpicker, &flux_ctrl_combo, &flux_ctrl_slider, &flux_ctrl_textbox);

MODULE(container)
  = {.type = FLUX_CONTROL_CONTAINER, .layout = layout_flex_column, .record = flux_container_record, .uia = &uia_group};
MODULE(text) = {
  .type     = FLUX_CONTROL_TEXT,
  .snapshot = flux_snap_text,
  .props    = flux_props_text,
  .seed     = mount_text,
  .consumed = text_consumed,
  .record   = flux_text_record,
  .uia      = &uia_text};
MODULE(button) = {
  .type     = FLUX_CONTROL_BUTTON,
  .snapshot = flux_snap_button,
  .props    = flux_props_button,
  .seed     = mount_button,
  .retarget = retarget_button,
  .consumed = label_consumed,
  .record   = flux_button_record,
  .uia      = &uia_button_invoke};
MODULE(toggle) = {
  .type     = FLUX_CONTROL_TOGGLE_BUTTON,
  .snapshot = flux_snap_button,
  .record   = flux_toggle_record,
  .uia      = &uia_button_toggle};
MODULE(checkbox) = {
  .type     = FLUX_CONTROL_CHECKBOX,
  .snapshot = flux_snap_checkbox_like,
  .props    = flux_props_checkbox,
  .seed     = mount_checkbox,
  .retarget = retarget_checkbox,
  .consumed = check_consumed,
  .record   = flux_checkbox_record,
  .uia      = &uia_checkbox_toggle};
MODULE(radio) = {
  .type     = FLUX_CONTROL_RADIO,
  .snapshot = flux_snap_checkbox_like,
  .tree     = &tree_radio,
  .props    = flux_props_radio,
  .seed     = mount_radio,
  .retarget = retarget_radio,
  .consumed = check_consumed,
  .record   = flux_radio_record,
  .uia      = &uia_radio_selection_item};
MODULE(switch) = {
  .type     = FLUX_CONTROL_SWITCH,
  .snapshot = flux_snap_switch,
  .props    = flux_props_switch,
  .seed     = mount_switch,
  .retarget = retarget_switch,
  .consumed = check_consumed,
  .record   = flux_switch_record,
  .uia      = &uia_button_toggle};
MODULE(slider) = {
  .type     = FLUX_CONTROL_SLIDER,
  .snapshot = flux_snap_slider,
  .uia      = &uia_slider,
  .props    = flux_props_slider,
  .seed     = mount_slider,
  .retarget = retarget_slider,
  .consumed = range_consumed,
  .record   = flux_slider_record,
};
MODULE(textbox) = {
  .type     = FLUX_CONTROL_TEXT_INPUT,
  .snapshot = flux_snap_textbox,
  .tree     = &tree_textbox,
  .input    = &input_textbox,
  .uia      = &uia_textbox,
  .props    = flux_props_textbox,
  .seed     = mount_textbox,
  .retarget = retarget_textbox,
  .consumed = placeholder_consumed,
  .record   = flux_textbox_record,
};
MODULE(scroll) = {
  .type     = FLUX_CONTROL_SCROLL,
  .snapshot = flux_snap_scroll,
  .tree     = &tree_scroll,
  .props    = flux_props_scroll,
  .seed     = seed_scroll,
  .retarget = retarget_scroll,
  .record   = flux_scroll_record,
  .overlay  = flux_scroll_overlay_record,
  .uia      = &uia_scroll_pane};
static FluxTreeHooks const tree_image = {.frame = flux_image_sync};
MODULE(image)                         = {
  .type     = FLUX_CONTROL_IMAGE,
  .tree     = &tree_image,
  .snapshot = flux_snap_image,
  .props    = flux_props_image,
  .seed     = seed_image,
  .record   = flux_image_record,
  .uia      = &uia_image};
MODULE(progress) = {
  .type     = FLUX_CONTROL_PROGRESS,
  .snapshot = flux_snap_progress,
  .props    = flux_props_progress,
  .seed     = mount_progress,
  .consumed = range_consumed,
  .record   = flux_progress_record,
  .uia      = &uia_progressbar_range};
MODULE(list) = {
  .type     = FLUX_CONTROL_LIST,
  .tree     = &tree_list,
  .props    = flux_props_list,
  .deps     = list_deps,
  .seed     = seed_list,
  .retarget = retarget_list,
  .snapshot = flux_snap_list,
  .overlay  = flux_list_overlay_record,
  .record   = flux_container_record,
  .uia      = &uia_list_items};
MODULE(tab)  = {.type = FLUX_CONTROL_TAB, .record = flux_container_record, .uia = &uia_tab};
MODULE(card) = {.type = FLUX_CONTROL_CARD, .layout = layout_card, .record = flux_card_record, .uia = &uia_group};
MODULE(divider)
  = {.type = FLUX_CONTROL_DIVIDER, .layout = layout_divider, .record = flux_divider_record, .uia = &uia_separator};
MODULE(canvas)   = {.type = FLUX_CONTROL_CANVAS, .record = flux_container_record, .uia = &uia_pane};
MODULE(password) = {
  .type     = FLUX_CONTROL_PASSWORD_BOX,
  .snapshot = flux_snap_password_box,
  .tree     = &tree_textbox,
  .uia      = &uia_password,
  .props    = flux_props_password,
  .seed     = mount_password,
  .retarget = retarget_textbox,
  .consumed = placeholder_consumed,
  .record   = flux_password_record,
};
MODULE(number) = {
  .type     = FLUX_CONTROL_NUMBER_BOX,
  .snapshot = flux_snap_number_box,
  .tree     = &tree_textbox,
  .uia      = &uia_number,
  .props    = flux_props_number,
  .seed     = mount_number,
  .retarget = retarget_textbox,
  .consumed = number_consumed,
  .record   = flux_number_record,
};
MODULE(link) = {
  .type     = FLUX_CONTROL_HYPERLINK,
  .snapshot = flux_snap_hyperlink,
  .props    = flux_props_link,
  .seed     = mount_hyperlink,
  .retarget = retarget_hyperlink,
  .consumed = link_consumed,
  .record   = flux_link_record,
  .uia      = &uia_hyperlink_invoke};
MODULE(repeat) = {
  .type     = FLUX_CONTROL_REPEAT_BUTTON,
  .snapshot = flux_snap_repeat_button,
  .props    = flux_props_repeat,
  .seed     = mount_repeat,
  .retarget = retarget_repeat,
  .consumed = label_consumed,
  .record   = flux_repeat_record,
  .uia      = &uia_button_invoke};
MODULE(progressring) = {
  .type     = FLUX_CONTROL_PROGRESS_RING,
  .snapshot = flux_snap_progress_ring,
  .props    = flux_props_progressring,
  .seed     = mount_progress_ring,
  .consumed = range_consumed,
  .record   = flux_progressring_record,
  .uia      = &uia_progressbar_range};
MODULE(badge) = {
  .type     = FLUX_CONTROL_INFO_BADGE,
  .snapshot = flux_snap_info_badge,
  .props    = flux_props_badge,
  .seed     = mount_badge,
  .consumed = badge_consumed,
  .record   = flux_badge_record,
  .uia      = &uia_hidden};
MODULE(tooltip) = {.type = FLUX_CONTROL_TOOLTIP, .uia = &uia_tooltip};
MODULE(flyout)  = {.type = FLUX_CONTROL_FLYOUT, .uia = &uia_pane};
MODULE(menufly) = {
  .type  = FLUX_CONTROL_MENU_FLYOUT,
  .tree  = &tree_menufly,
  .props = flux_props_menufly,
  .shell = flux_host_menufly_build,
  .uia   = &uia_menu};
MODULE(dropdown) = {
  .type     = FLUX_CONTROL_DROPDOWN_BUTTON,
  .snapshot = flux_snap_button,
  .props    = flux_props_button,
  .seed     = mount_dropdown,
  .retarget = retarget_dropdown,
  .consumed = labelicon_consumed,
  .record   = flux_dropdown_record,
  .uia      = &uia_dropdown};
MODULE(split) = {
  .type     = FLUX_CONTROL_SPLIT_BUTTON,
  .snapshot = flux_snap_button,
  .props    = flux_props_button,
  .seed     = mount_split,
  .retarget = retarget_split,
  .consumed = labelicon_consumed,
  .record   = flux_split_record,
  .uia      = &uia_split};
MODULE(combo) = {
  .type     = FLUX_CONTROL_COMBO_BOX,
  .snapshot = flux_snap_combo_box,
  .input    = &input_combo,
  .uia      = &uia_combo,
  .props    = flux_props_combo,
  .deps     = combo_deps,
  .seed     = mount_combo,
  .retarget = retarget_combo,
  .consumed = combo_consumed,
  .record   = flux_combo_record,
};
MODULE(expander) = {
  .type     = FLUX_CONTROL_EXPANDER,
  .tree     = &tree_expander,
  .props    = flux_props_expander,
  .deps     = expander_deps,
  .seed     = mount_expander,
  .retarget = retarget_expander,
  .consumed = expander_consumed,
  .uia      = &uia_expander};
MODULE(exphead) = {
  .type     = FLUX_CONTROL_EXPANDER_HEADER,
  .snapshot = flux_snap_expander_header,
  .record   = flux_exphead_record,
  .uia      = &uia_hidden};
MODULE(expbody) = {.type = FLUX_CONTROL_EXPANDER_CONTENT, .record = flux_expbody_record, .uia = &uia_hidden};
MODULE(infobar) = {
  .type     = FLUX_CONTROL_INFO_BAR,
  .snapshot = flux_snap_info_bar,
  .props    = flux_props_infobar,
  .seed     = mount_info_bar,
  .retarget = retarget_info_bar,
  .consumed = infobar_consumed,
  .record   = flux_infobar_record,
  .uia      = &uia_statusbar};
MODULE(dialog) = {
  .type     = FLUX_CONTROL_CONTENT_DIALOG,
  .tree     = &tree_dialog,
  .props    = flux_props_dialog,
  .deps     = dialog_deps,
  .seed     = mount_dialog,
  .retarget = flux_dialog_retarget,
  .unmount  = flux_dialog_unmount,
  .consumed = dialog_consumed,
  .uia      = &uia_window};
MODULE(dialogbody) = {.type = FLUX_CONTROL_DIALOG_CONTENT, .record = flux_dialogbody_record, .uia = &uia_hidden};
MODULE(menubar)    = {
  .type     = FLUX_CONTROL_MENU_BAR,
  .tree     = &tree_menubar,
  .deps     = menubar_deps,
  .seed     = seed_menu_bar,
  .retarget = retarget_menu_bar,
  .uia      = &uia_menubar};
MODULE(menuitem) = {
  .type     = FLUX_CONTROL_MENU_BAR_ITEM,
  .snapshot = flux_snap_menu_bar_item,
  .tree     = &tree_menuitem,
  .props    = flux_props_menuitem,
  .shell    = shell_menuitem,
  .record   = flux_menuitem_record,
  .uia      = &uia_menubaritem};
MODULE(nav) = {
  .type     = FLUX_CONTROL_NAV_VIEW,
  .snapshot = flux_snap_nav_view,
  .tree     = &tree_nav,
  .props    = flux_props_nav,
  .deps     = nav_deps,
  .seed     = mount_nav,
  .retarget = retarget_nav,
  .consumed = nav_consumed,
  .record   = flux_nav_record,
  .overlay  = flux_nav_overlay_record,
  .uia      = &uia_nav_sel};
MODULE(navitem) = {
  .type     = FLUX_CONTROL_NAV_VIEW_ITEM,
  .snapshot = flux_snap_nav_view_item,
  .record   = flux_navitem_record,
  .uia      = &uia_navitem_sel};
MODULE(tabs) = {
  .type     = FLUX_CONTROL_TAB_VIEW,
  .tree     = &tree_tabs,
  .props    = flux_props_tabs,
  .deps     = tabs_deps,
  .seed     = mount_tab,
  .retarget = retarget_tab,
  .consumed = tabs_consumed,
  .record   = flux_tabs_record,
  .uia      = &uia_tabs_sel};
MODULE(tabitem) = {
  .type     = FLUX_CONTROL_TAB_VIEW_ITEM,
  .snapshot = flux_snap_tab_view_item,
  .record   = flux_tabitem_record,
  .uia      = &uia_tabitem_sel};
MODULE(listitem) = {
  .type     = FLUX_CONTROL_LIST_ITEM,
  .snapshot = flux_snap_list_item,
  .tree     = &tree_listitem,
  .props    = flux_props_listitem,
  .seed     = seed_list_item,
  .record   = flux_listitem_record,
  .uia      = &uia_listitem_sel};
MODULE(listbox) = {
  .type     = FLUX_CONTROL_LIST_BOX,
  .tree     = &tree_list,
  .props    = flux_props_list,
  .deps     = list_deps,
  .seed     = seed_list_box,
  .retarget = retarget_list,
  .snapshot = flux_snap_list,
  .overlay  = flux_list_overlay_record,
  .record   = flux_listbox_record,
  .uia      = &uia_list_items};
MODULE(gridview) = {
  .type     = FLUX_CONTROL_GRID_VIEW,
  .tree     = &tree_list,
  .props    = flux_props_list,
  .deps     = list_deps,
  .seed     = seed_grid,
  .retarget = retarget_list,
  .snapshot = flux_snap_list,
  .overlay  = flux_list_overlay_record,
  .record   = flux_container_record,
  .uia      = &uia_list_items};
MODULE(repeater) = {
  .type     = FLUX_CONTROL_ITEMS_REPEATER,
  .tree     = &tree_list,
  .props    = flux_props_list,
  .deps     = list_deps,
  .seed     = seed_repeater,
  .retarget = retarget_list,
  .snapshot = flux_snap_list,
  .overlay  = flux_list_overlay_record,
  .record   = flux_container_record,
  .uia      = &uia_group};
MODULE(itemsview) = {
  .type     = FLUX_CONTROL_ITEMS_VIEW,
  .tree     = &tree_list,
  .props    = flux_props_itemsview,
  .deps     = list_deps,
  .seed     = seed_items_view,
  .retarget = retarget_list,
  .snapshot = flux_snap_list,
  .overlay  = flux_list_overlay_record,
  .record   = flux_container_record,
  .uia      = &uia_list_sel};
MODULE(flip) = {
  .type     = FLUX_CONTROL_FLIP_VIEW,
  .snapshot = flux_snap_flip_view,
  .tree     = &tree_flip,
  .input    = &input_flip,
  .props    = flux_props_flip,
  .seed     = seed_flip,
  .retarget = retarget_flip,
  .record   = flux_flip_record,
  .overlay  = flux_flip_overlay_record,
  .uia      = &uia_list};
MODULE(pips) = {
  .type     = FLUX_CONTROL_PIPS_PAGER,
  .snapshot = flux_snap_pips_pager,
  .props    = flux_props_pips,
  .seed     = mount_pips,
  .retarget = retarget_pips,
  .consumed = pips_consumed,
  .record   = flux_pips_record,
  .uia      = &uia_menu};
MODULE(suggest) = {
  .type     = FLUX_CONTROL_AUTO_SUGGEST,
  .props    = flux_props_suggest,
  .deps     = suggest_deps,
  .seed     = seed_auto_suggest,
  .retarget = retarget_auto_suggest,
  .record   = flux_container_record,
  .uia      = &uia_group};
MODULE(togglesplit) = {
  .type     = FLUX_CONTROL_TOGGLE_SPLIT_BUTTON,
  .snapshot = flux_snap_button,
  .props    = flux_props_togglesplit,
  .seed     = mount_toggle_split,
  .retarget = retarget_split,
  .consumed = togglesplit_consumed,
  .record   = flux_split_record,
  .uia      = &uia_togglesplit};
MODULE(radiogroup) = {
  .type     = FLUX_CONTROL_RADIO_BUTTONS,
  .tree     = &tree_radiogroup,
  .props    = flux_props_radiogroup,
  .deps     = radiogroup_deps,
  .seed     = seed_radio_buttons,
  .retarget = retarget_radio_buttons,
  .record   = flux_container_record,
  .uia      = &uia_group};
MODULE(rating) = {
  .type     = FLUX_CONTROL_RATING,
  .snapshot = flux_snap_rating,
  .props    = flux_props_rating,
  .seed     = mount_rating,
  .retarget = retarget_rating,
  .consumed = rating_consumed,
  .record   = flux_rating_record,
  .uia      = &uia_slider_role};
MODULE(selector) = {
  .type     = FLUX_CONTROL_SELECTOR_BAR,
  .props    = flux_props_selector,
  .deps     = selector_deps,
  .seed     = seed_selector,
  .retarget = retarget_selector,
  .record   = flux_container_record,
  .uia      = &uia_selector_sel};
MODULE(selectoritem) = {
  .type     = FLUX_CONTROL_SELECTOR_BAR_ITEM,
  .snapshot = flux_snap_selector_item,
  .record   = flux_selectoritem_record,
  .uia      = &uia_selectoritem_sel};
MODULE(breadcrumb) = {
  .type     = FLUX_CONTROL_BREADCRUMB_BAR,
  .tree     = &tree_breadcrumb,
  .props    = flux_props_breadcrumb,
  .deps     = breadcrumb_deps,
  .seed     = seed_breadcrumb,
  .retarget = retarget_breadcrumb,
  .uia      = &uia_group};
MODULE(crumb) = {
  .type     = FLUX_CONTROL_BREADCRUMB_ITEM,
  .snapshot = flux_snap_breadcrumb_item,
  .record   = flux_crumb_record,
  .uia      = &uia_button_invoke};
MODULE(teachtip) = {
  .type     = FLUX_CONTROL_TEACHING_TIP,
  .snapshot = flux_snap_teachtip,
  .props    = flux_props_teachtip,
  .seed     = seed_teaching_tip,
  .retarget = retarget_teaching_tip,
  .record   = flux_teachtip_record,
  .uia      = &uia_hidden};
MODULE(tree) = {
  .type     = FLUX_CONTROL_TREE_VIEW,
  .props    = flux_props_tree,
  .deps     = tree_deps,
  .seed     = seed_tree,
  .retarget = retarget_tree,
  .record   = flux_container_record,
  .uia      = &uia_tree_sel};
MODULE(treeitem) = {
  .type     = FLUX_CONTROL_TREE_ITEM,
  .snapshot = flux_snap_tree_item,
  .props    = flux_props_treeitem,
  .record   = flux_treeitem_record,
  .uia      = &uia_treeitem_sel};
MODULE(refresh) = {
  .type     = FLUX_CONTROL_REFRESH,
  .snapshot = flux_snap_refresh,
  .tree     = &tree_refresh,
  .input    = &input_refresh,
  .props    = flux_props_refresh,
  .deps     = refresh_deps,
  .seed     = mount_refresh,
  .retarget = retarget_refresh,
  .consumed = refresh_consumed,
  .record   = flux_refresh_record,
  .overlay  = flux_refresh_overlay_record,
  .uia      = &uia_hidden};
MODULE(avatar) = {
  .type     = FLUX_CONTROL_PERSON_PICTURE,
  .snapshot = flux_snap_person_picture,
  .props    = flux_props_avatar,
  .seed     = seed_person,
  .retarget = retarget_person,
  .record   = flux_avatar_record,
  .uia      = &uia_text};
MODULE(pager) = {
  .type     = FLUX_CONTROL_PAGER,
  .snapshot = flux_snap_pager,
  .props    = flux_props_pager,
  .seed     = mount_pager,
  .retarget = retarget_pager,
  .consumed = pager_consumed,
  .record   = flux_pager_record,
  .uia      = &uia_menu};
MODULE(splitview) = {
  .type     = FLUX_CONTROL_SPLIT_VIEW,
  .tree     = &tree_splitview,
  .props    = flux_props_splitview,
  .deps     = splitview_deps,
  .seed     = mount_split_view,
  .retarget = retarget_split_view,
  .consumed = splitview_consumed,
  .record   = flux_container_record,
  .uia      = &uia_hidden};
MODULE(splitpane) = {
  .type     = FLUX_CONTROL_SPLIT_VIEW_PANE,
  .snapshot = flux_snap_split_pane,
  .layout   = layout_flex_column,
  .record   = flux_splitpane_record,
  .uia      = &uia_splitpane};
MODULE(splitbody) = {
  .type   = FLUX_CONTROL_SPLIT_VIEW_CONTENT,
  .layout = layout_flex_column,
  .record = flux_container_record,
  .uia    = &uia_hidden};
MODULE(titlebar) = {
  .type     = FLUX_CONTROL_TITLE_BAR,
  .snapshot = flux_snap_title_bar,
  .tree     = &tree_titlebar,
  .props    = flux_props_titlebar,
  .seed     = mount_title_bar,
  .retarget = retarget_title_bar,
  .consumed = titlebar_consumed,
  .record   = flux_titlebar_record,
  .uia      = &uia_titlebar};
MODULE(scrollbar) = {
  .type     = FLUX_CONTROL_SCROLL_BAR,
  .snapshot = flux_snap_scroll_bar,
  .uia      = &uia_scrollbar,
  .props    = flux_props_scrollbar,
  .seed     = mount_scroll_bar,
  .retarget = retarget_scroll_bar,
  .record   = flux_scroll_bar_record,
};
MODULE(looping) = {
  .type     = FLUX_CONTROL_LOOPING_SELECTOR,
  .snapshot = flux_snap_looping,
  .tree     = &tree_looping,
  .input    = &input_looping,
  .props    = flux_props_looping,
  .deps     = looping_deps,
  .seed     = mount_looping,
  .retarget = retarget_looping,
  .overlay  = flux_looping_record,
  .uia      = &uia_looping_sel};
MODULE(loopitem) = {
  .type     = FLUX_CONTROL_LOOPING_ITEM,
  .snapshot = flux_snap_looping_item,
  .record   = flux_looping_item_record,
  .uia      = &uia_loopitem_sel};
MODULE(datepicker) = {
  .type     = FLUX_CONTROL_DATE_PICKER,
  .snapshot = flux_snap_picker,
  .tree     = &tree_picker,
  .uia      = &uia_picker,
  .props    = flux_props_datepicker,
  .deps     = picker_deps,
  .seed     = seed_date_picker,
  .retarget = retarget_picker,
  .unmount  = flux_picker_close,
  .record   = flux_picker_record,
};
MODULE(timepicker) = {
  .type     = FLUX_CONTROL_TIME_PICKER,
  .snapshot = flux_snap_picker,
  .tree     = &tree_picker,
  .uia      = &uia_picker,
  .props    = flux_props_timepicker,
  .deps     = picker_deps,
  .seed     = seed_time_picker,
  .retarget = retarget_picker,
  .unmount  = flux_picker_close,
  .record   = flux_picker_record,
};
MODULE(pickerflyout) = {
  .type     = FLUX_CONTROL_PICKER_FLYOUT,
  .snapshot = flux_snap_picker_card,
  .deps     = pickerflyout_deps,
  .record   = flux_pickercard_record,
  .uia      = &uia_pane};
MODULE(calendar) = {
  .type     = FLUX_CONTROL_CALENDAR_VIEW,
  .snapshot = flux_snap_calendar,
  .input    = &input_calendar,
  .props    = flux_props_calendar,
  .seed     = seed_calendar,
  .retarget = flux_calendar_retarget,
  .record   = flux_calendar_record,
  .uia      = &uia_calendar};
MODULE(calday)      = {.type = FLUX_CONTROL_CALENDAR_DAY, .uia = &uia_dataitem_scroll};
MODULE(colorpicker) = {
  .type     = FLUX_CONTROL_COLOR_PICKER,
  .snapshot = flux_snap_colorpicker,
  .tree     = &tree_colorpicker,
  .props    = flux_props_colorpicker,
  .deps     = colorpicker_deps,
  .seed     = seed_colorpicker,
  .retarget = flux_colorpicker_retarget,
  .record   = flux_colorpicker_record,
  .uia      = &uia_hidden};
MODULE(colorspectrum) = {.type = FLUX_CONTROL_COLOR_SPECTRUM, .uia = &uia_slider_role};
MODULE(colorslider)   = {.type = FLUX_CONTROL_COLOR_SLIDER, .uia = &uia_slider_role};
MODULE(colheader)     = {
  .type     = FLUX_CONTROL_COLHEADER,
  .deps     = colheader_deps,
  .snapshot = flux_snap_colheader,
  .seed     = seed_colheader,
  .retarget = flux_colheader_retarget,
  .record   = flux_colheader_record,
  .overlay  = flux_colheader_overlay_record,
  .uia      = &uia_header};
MODULE(colitem) = {
  .type     = FLUX_CONTROL_COLITEM,
  .deps     = colitem_deps,
  .props    = flux_props_colitem,
  .snapshot = flux_snap_colitem,
  .seed     = seed_colitem,
  .retarget = flux_colitem_retarget,
  .record   = flux_colitem_record,
  .uia      = &uia_headeritem};
MODULE(commandbar) = {
  .type     = FLUX_CONTROL_COMMANDBAR,
  .deps     = commandbar_deps,
  .tree     = &tree_commandbar,
  .props    = flux_props_commandbar,
  .snapshot = flux_snap_cmdbar,
  .seed     = seed_commandbar,
  .retarget = flux_cmdbar_retarget,
  .record   = flux_cmdbar_record,
  .uia      = &uia_commandbar};
MODULE(appbarbutton) = {
  .type     = FLUX_CONTROL_APPBARBUTTON,
  .props    = flux_props_appbar,
  .snapshot = flux_snap_appbar,
  .seed     = seed_appbarbutton,
  .retarget = flux_appbar_retarget,
  .record   = flux_appbar_record,
  .uia      = &uia_button_invoke};
MODULE(appbartoggle) = {
  .type     = FLUX_CONTROL_APPBARTOGGLE,
  .props    = flux_props_appbar,
  .snapshot = flux_snap_appbar,
  .seed     = seed_appbartoggle,
  .retarget = flux_appbar_retarget,
  .record   = flux_appbar_record,
  .uia      = &uia_button_toggle};
MODULE(appbarsep) = {
  .type     = FLUX_CONTROL_APPBARSEP,
  .props    = flux_props_appbar,
  .snapshot = flux_snap_appbar,
  .seed     = seed_appbarsep,
  .retarget = flux_appbar_retarget,
  .record   = flux_appbar_record,
  .uia      = &uia_hidden};
MODULE(cmdmore) = {
  .type     = FLUX_CONTROL_CMDMORE,
  .snapshot = flux_snap_cmdmore,
  .record   = flux_cmdmore_record,
  .uia      = &uia_button_invoke};
MODULE(colbutton) = {
  .type = FLUX_CONTROL_COLBUTTON, .snapshot = flux_snap_colbutton, .record = flux_colbutton_record, .uia = &uia_hidden};
MODULE(comboitem) = {
  .type     = FLUX_CONTROL_COMBO_ITEM,
  .snapshot = flux_snap_comboitem,
  .record   = flux_comboitem_record,
  .uia      = &uia_comboitem};
MODULE(suggestitem) = {
  .type     = FLUX_CONTROL_SUGGEST_ITEM,
  .snapshot = flux_snap_suggestitem,
  .record   = flux_suggestitem_record,
  .uia      = &uia_suggestitem};
static FluxInputHooks const input_menupresenter = {.wheel = flux_menu_wheel};
static FluxUiaHooks const   uia_menupresenter   = {.role = FLUX_UIA_MENU, .patterns = 0};
MODULE(menupresenter)                           = {
  .type     = FLUX_CONTROL_MENU_PRESENTER,
  .snapshot = flux_snap_menupresenter,
  .input    = &input_menupresenter,
  .record   = flux_menupresenter_record,
  .uia      = &uia_menupresenter};
MODULE(menurow) = {.type = FLUX_CONTROL_MENU_ROW, .snapshot = flux_snap_menurow, .record = flux_menurow_record};
static FluxInputHooks const input_cmdovf = {.wheel = flux_cmdovf_wheel};
MODULE(cmdovf)                           = {
  .type     = FLUX_CONTROL_CMDBAR_OVERFLOW,
  .snapshot = flux_snap_cmdovf,
  .input    = &input_cmdovf,
  .record   = flux_cmdovf_record,
  .uia      = &uia_hidden};
MODULE(cmdovfrow) = {.type = FLUX_CONTROL_CMDBAR_ROW, .snapshot = flux_snap_cmdovfrow, .record = flux_cmdovfrow_record};
MODULE(groupheader) = {
  .type   = FLUX_CONTROL_GROUP_HEADER,
  .tree   = &tree_listitem,
  .seed   = seed_group_header,
  .record = flux_groupheader_record,
  .uia    = &uia_group};
