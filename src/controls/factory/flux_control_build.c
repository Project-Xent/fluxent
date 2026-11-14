#include "controls/factory/flux_control_build.h"
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

typedef XentNodeId (*FluxSeedFn)(XentCtx *ctx, FluxStore *store);
typedef XentNodeId (*FluxEnvSeedFn)(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env);
typedef XentNodeId (*FluxMountSeedFn)(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
);
typedef void           (*FluxRetargetFn)(XentCtx *ctx, FluxStore *store, XentNodeId dst);
typedef void           (*FluxLayoutFn)(XentCtx *ctx, XentNodeId node);
typedef void           (*FluxUnmountFn)(FluxStore *store, XentNodeId node);

static XtkValue const *mount_value(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, XtkValueKind kind) {
	for (uint32_t i = 0; i < count; i++)
		if (props [i].id == id && props [i].value.kind == kind) return &props [i].value;
	return NULL;
}

static bool mount_bool(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, bool fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_BOOL);
	return v ? v->u.b : fallback;
}

static int mount_i32(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, int fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_I32);
	return v ? v->u.i32 : fallback;
}

static float mount_f32(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, float fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_F32);
	return v ? v->u.f32 : fallback;
}

static double mount_f64(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, double fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_F64);
	return v ? v->u.f64 : fallback;
}

static uint32_t mount_enum(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, uint32_t fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_ENUM);
	return v ? v->u.u32 : fallback;
}

static char const *mount_string(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, char const *fallback) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_STRING);
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

static void retarget_dialog(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	typedef struct {
		XentNodeId root;
		XentNodeId card;
		XentNodeId content;
		XentNodeId overlay_parent;
		XentNodeId default_focus;
		XentCtx   *ctx;
		FluxStore *store;
	} DialogHead;

	FluxNodeData *d = flux_store_find(store, dst);
	if (!d || !d->component_data) return;
	DialogHead *h = ( DialogHead * ) d->component_data;
	h->root       = dst;
	h->ctx        = ctx;
	h->store      = store;
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

static XentNodeId seed_button(XentCtx *ctx, FluxStore *store) {
	return flux_button_init(&(FluxButtonDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL});
}

static XentNodeId seed_dropdown(XentCtx *ctx, FluxStore *store) {
	return flux_dropdown_init(&(FluxDropDownButtonDesc) {ctx, store, SEED_NONE, NULL, NULL});
}

static XentNodeId seed_repeat(XentCtx *ctx, FluxStore *store) {
	return flux_repeat_init(&(FluxButtonDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL});
}

static XentNodeId seed_hyperlink(XentCtx *ctx, FluxStore *store) {
	return flux_link_init(&(FluxHyperlinkDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL, NULL});
}

static XentNodeId seed_text(XentCtx *ctx, FluxStore *store) {
	return flux_text_init(&(FluxTextDesc) {ctx, store, SEED_NONE, "", 14.0f});
}

static XentNodeId seed_checkbox(XentCtx *ctx, FluxStore *store) {
	return flux_checkbox_init(&(FluxToggleDesc) {ctx, store, SEED_NONE, NULL, false, NULL, NULL});
}

static XentNodeId seed_radio(XentCtx *ctx, FluxStore *store) {
	return flux_radio_init(&(FluxToggleDesc) {ctx, store, SEED_NONE, NULL, false, NULL, NULL});
}

static XentNodeId seed_switch(XentCtx *ctx, FluxStore *store) {
	return flux_switch_init(&(FluxToggleDesc) {ctx, store, SEED_NONE, NULL, false, NULL, NULL});
}

static XentNodeId seed_scroll_bar(XentCtx *ctx, FluxStore *store) {
	return flux_scroll_bar_init(&(FluxScrollBarDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE, .maximum = 1.0});
}

static XentNodeId seed_date_picker(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_datepicker_init(&(FluxDatePickerDesc) {
	  .ctx     = ctx,
	  .store   = store,
	  .parent  = SEED_NONE,
	  .input   = env->input,
	  .window  = env->window,
	  .overlay = env->overlay});
}

static XentNodeId seed_time_picker(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_timepicker_init(&(FluxTimePickerDesc) {
	  .ctx     = ctx,
	  .store   = store,
	  .parent  = SEED_NONE,
	  .input   = env->input,
	  .window  = env->window,
	  .overlay = env->overlay,
	  .minutes = -1});
}

static XentNodeId seed_looping(XentCtx *ctx, FluxStore *store) {
	return flux_looping_init(&(FluxLoopingDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE, .selected = -1});
}

static XentNodeId seed_slider(XentCtx *ctx, FluxStore *store) {
	return flux_slider_init(&(FluxSliderDesc) {ctx, store, SEED_NONE, 0.0f, 1.0f, 0.0f, NULL, NULL});
}

static XentNodeId seed_image(XentCtx *ctx, FluxStore *store) {
	return flux_image_init(&(FluxImageDesc) {ctx, store, SEED_NONE, NULL, FLUX_IMAGE_UNIFORM});
}

static XentNodeId seed_scroll(XentCtx *ctx, FluxStore *store) {
	XentNodeId    node = flux_scroll_init(&(FluxContainerDesc) {ctx, store, SEED_NONE});
	FluxNodeData *nd   = node != XENT_NODE_INVALID ? flux_store_find(store, node) : NULL;
	if (nd) (( FluxScrollData * ) nd->component_data)->reports = 1;
	return node;
}

static XentNodeId seed_progress(XentCtx *ctx, FluxStore *store) {
	return flux_progress_init(&(FluxProgressDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .value = 0.0f, .max_value = 100.0f, .min_value = 0.0f});
}

static XentNodeId seed_progress_ring(XentCtx *ctx, FluxStore *store) {
	return flux_progressring_init(&(FluxProgressDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .value = 0.0f, .max_value = 100.0f, .min_value = 0.0f});
}

static XentNodeId seed_info_badge(XentCtx *ctx, FluxStore *store) {
	return flux_badge_init(&(FluxInfoBadgeDesc) {ctx, store, SEED_NONE, FLUX_BADGE_NUMBER, 0});
}

static XentNodeId seed_split(XentCtx *ctx, FluxStore *store) {
	return flux_split_init(&(FluxSplitButtonDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL, NULL});
}

static XentNodeId seed_toggle_split(XentCtx *ctx, FluxStore *store) {
	return flux_togglesplit_init(&(FluxToggleSplitButtonDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_list(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_list_init(&(FluxListViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .input = env->input, .realize_events = true});
}

static XentNodeId seed_list_box(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_list_init(&(FluxListViewDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .kind           = XTK_LIST_KIND_LIST_BOX,
	  .input          = env->input,
	  .realize_events = true});
}

static XentNodeId seed_grid(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_list_init(&(FluxListViewDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .kind           = XTK_LIST_KIND_GRID,
	  .input          = env->input,
	  .realize_events = true});
}

static XentNodeId seed_repeater(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_list_init(&(FluxListViewDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .kind           = XTK_LIST_KIND_REPEATER,
	  .input          = env->input,
	  .realize_events = true});
}

static XentNodeId seed_list_item(XentCtx *ctx, FluxStore *store) {
	return flux_listitem_init(&(FluxListItemDesc) {ctx, store, SEED_NONE});
}

static XentNodeId seed_items_view(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_itemsview_init(&(FluxItemsViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .input = env->input});
}

static XentNodeId seed_tree(XentCtx *ctx, FluxStore *store) {
	return flux_tree_init(&(FluxTreeViewDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_radio_buttons(XentCtx *ctx, FluxStore *store) {
	return flux_radiogroup_init(&(FluxRadioButtonsDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_rating(XentCtx *ctx, FluxStore *store) {
	return flux_rating_init(&(FluxRatingDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_selector(XentCtx *ctx, FluxStore *store) {
	return flux_selector_init(&(FluxSelectorBarDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_breadcrumb(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_breadcrumb_init(&(FluxBreadcrumbBarDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = env->window,
	  .text   = env->text,
	  .theme  = env->theme,
	  .input  = env->input});
}

static XentNodeId seed_teaching_tip(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_teachtip_init(&(FluxTeachingTipDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = env->window,
	  .text   = env->text,
	  .theme  = env->theme,
	  .input  = env->input});
}

static XentNodeId seed_refresh(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_refresh_init(&(FluxRefreshContainerDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window});
}

static XentNodeId seed_pager(XentCtx *ctx, FluxStore *store) {
	return flux_pager_init(&(FluxPagerDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_person(XentCtx *ctx, FluxStore *store) {
	return flux_avatar_init(&(FluxPersonPictureDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_split_view(XentCtx *ctx, FluxStore *store) {
	return flux_splitview_init(&(FluxSplitViewDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_title_bar(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_titlebar_init(&(FluxTitleBarDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window});
}

static XentNodeId seed_textbox(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	if (env && env->app) return flux_app_create_textbox(&(FluxAppTextBoxDesc) {env->app, SEED_NONE, NULL, NULL, NULL});
	return flux_textbox_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL});
}

static XentNodeId seed_password(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	if (env && env->app) return flux_app_create_password(&(FluxAppTextBoxDesc) {env->app, SEED_NONE, NULL, NULL, NULL});
	return flux_password_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, NULL, NULL, NULL});
}

static XentNodeId seed_number(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	if (env && env->app)
		return flux_app_create_number(&(FluxAppNumberBoxDesc) {env->app, SEED_NONE, 0.0, 100.0, 1.0, NULL, NULL});
	return flux_number_init(&(FluxNumberBoxDesc) {ctx, store, SEED_NONE, 0.0, 100.0, 1.0, NULL, NULL});
}

static XentNodeId seed_combo(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .window = env->window,
	  .text   = env->text,
	  .theme  = env->theme,
	  .app    = env->app,
	  .input  = env->input});
}

static XentNodeId seed_info_bar(XentCtx *ctx, FluxStore *store) {
	return flux_infobar_init(&(FluxInfoBarDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_expander(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_expander_init(&(FluxExpanderDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window});
}

static XentNodeId seed_dialog(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_dialog_init(&(FluxContentDialogDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .overlay_parent = env->overlay,
	  .input          = env->input,
	  .window         = env->window,
	  .theme          = env->theme});
}

static XentNodeId seed_menu_bar(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_menubar_init(&(FluxMenuBarDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window, .text = env->text, .theme = env->theme});
}

static XentNodeId seed_nav(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	if (!env || !env->window) return XENT_NODE_INVALID;
	return flux_nav_init(&(FluxNavViewDesc) {
	  .ctx      = ctx,
	  .store    = store,
	  .parent   = SEED_NONE,
	  .window   = env->window,
	  .text     = env->text,
	  .theme    = env->theme,
	  .adaptive = true});
}

static XentNodeId seed_tab(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_tabs_init(&(FluxTabViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window, .text = env->text, .input = env->input});
}

static XentNodeId seed_flip(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_flip_init(&(FluxFlipViewDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window});
}

static XentNodeId seed_pips(XentCtx *ctx, FluxStore *store) {
	return flux_pips_init(&(FluxPipsPagerDesc) {.ctx = ctx, .store = store, .parent = SEED_NONE});
}

static XentNodeId seed_auto_suggest(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	return flux_suggest_init(&(FluxAutoSuggestDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .app    = env->app,
	  .window = env->window,
	  .text   = env->text,
	  .theme  = env->theme});
}

typedef struct MountStrings {
	char const **items;
	int          count;
} MountStrings;

static bool mount_strings_open(XtkPropPatch const *props, uint32_t count, XtkPropertyId id, MountStrings *out) {
	XtkValue const *v = mount_value(props, count, id, XTK_VALUE_BYTES);
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

static XentNodeId
mount_combo(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	MountStrings strings = {0};
	if (!mount_strings_open(props, prop_count, XTK_PROP_ITEMS, &strings)) return XENT_NODE_INVALID;
	XentNodeId node = flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .window         = env->window,
	  .text           = env->text,
	  .theme          = env->theme,
	  .app            = env->app,
	  .input          = env->input,
	  .items          = strings.items,
	  .item_count     = strings.count,
	  .selected_index = mount_i32(props, prop_count, XTK_PROP_SELECTED_INDEX, -1),
	  .placeholder    = mount_string(props, prop_count, XTK_PROP_PLACEHOLDER, NULL)});
	mount_strings_close(&strings);
	return node;
}

static XentNodeId mount_button(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_button_init(&(FluxButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL), NULL, NULL});
}

static XentNodeId mount_repeat(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_repeat_init(&(FluxButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL), NULL, NULL});
}

static XentNodeId mount_dropdown(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_dropdown_init(&(FluxDropDownButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  mount_string(props, prop_count, XTK_PROP_ICON, NULL)});
}

static XentNodeId
mount_split(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	( void ) env;
	return flux_split_init(&(FluxSplitButtonDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  mount_string(props, prop_count, XTK_PROP_ICON, NULL), NULL, NULL});
}

static XentNodeId mount_toggle_split(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_togglesplit_init(&(FluxToggleSplitButtonDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .label     = mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  .icon_name = mount_string(props, prop_count, XTK_PROP_ICON, NULL),
	  .checked   = mount_bool(props, prop_count, XTK_PROP_CHECKED, false)});
}

static XentNodeId mount_hyperlink(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_link_init(&(FluxHyperlinkDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  mount_string(props, prop_count, XTK_PROP_URL, NULL), NULL, NULL});
}

static XentNodeId
mount_text(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	return flux_text_init(&(FluxTextDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, ""),
	  mount_f32(props, prop_count, XTK_PROP_FONT_SIZE, 14.0f), env->text});
}

static XentNodeId mount_toggle(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count,
  XentNodeId (*init)(FluxToggleDesc const *)
) {
	( void ) env;
	return init(&(FluxToggleDesc) {
	  ctx, store, SEED_NONE, mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  mount_bool(props, prop_count, XTK_PROP_CHECKED, false), NULL, NULL});
}

#define MOUNT_TOGGLE(name, init)                                                                                \
	static XentNodeId name(                                                                                     \
	  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count \
	) {                                                                                                         \
		return mount_toggle(ctx, store, env, props, prop_count, init);                                          \
	}

MOUNT_TOGGLE(mount_checkbox, flux_checkbox_init)
MOUNT_TOGGLE(mount_radio, flux_radio_init)
MOUNT_TOGGLE(mount_switch, flux_switch_init)

static XentNodeId mount_slider(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_slider_init(&(FluxSliderDesc) {
	  ctx, store, SEED_NONE, mount_f32(props, prop_count, XTK_PROP_MIN_F32, 0.0f),
	  mount_f32(props, prop_count, XTK_PROP_MAX_F32, 1.0f), mount_f32(props, prop_count, XTK_PROP_VALUE_F32, 0.0f),
	  NULL, NULL});
}

static XentNodeId mount_progress(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_progress_init(&(FluxProgressDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .value     = mount_f32(props, prop_count, XTK_PROP_VALUE_F32, 0.0f),
	  .max_value = mount_f32(props, prop_count, XTK_PROP_MAX_F32, 100.0f),
	  .min_value = mount_f32(props, prop_count, XTK_PROP_MIN_F32, 0.0f)});
}

static XentNodeId mount_progress_ring(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_progressring_init(&(FluxProgressDesc) {
	  .ctx       = ctx,
	  .store     = store,
	  .parent    = SEED_NONE,
	  .value     = mount_f32(props, prop_count, XTK_PROP_VALUE_F32, 0.0f),
	  .max_value = mount_f32(props, prop_count, XTK_PROP_MAX_F32, 100.0f),
	  .min_value = mount_f32(props, prop_count, XTK_PROP_MIN_F32, 0.0f)});
}

static XentNodeId
mount_badge(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	( void ) env;
	return flux_badge_init(&(FluxInfoBadgeDesc) {
	  ctx, store, SEED_NONE, ( FluxInfoBadgeMode ) mount_enum(props, prop_count, XTK_PROP_BADGE_MODE, 0u),
	  mount_i32(props, prop_count, XTK_PROP_BADGE_VALUE, 0)});
}

static XentNodeId mount_looping(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	MountStrings strings = {0};
	if (!mount_strings_open(props, prop_count, XTK_PROP_ITEMS, &strings)) return XENT_NODE_INVALID;
	XentNodeId node = flux_looping_init(&(FluxLoopingDesc) {
	  .ctx         = ctx,
	  .store       = store,
	  .parent      = SEED_NONE,
	  .items       = strings.items,
	  .item_count  = strings.count,
	  .selected    = mount_i32(props, prop_count, XTK_PROP_SELECTED_INDEX, -1),
	  .item_height = mount_f32(props, prop_count, XTK_PROP_ITEM_HEIGHT, 0.0f),
	  .item_width  = mount_f32(props, prop_count, XTK_PROP_ITEM_WIDTH, 0.0f),
	  .should_loop = mount_bool(props, prop_count, XTK_PROP_SHOULD_LOOP, false)});
	mount_strings_close(&strings);
	return node;
}

static XentNodeId mount_scroll_bar(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_scroll_bar_init(&(FluxScrollBarDesc) {
	  .ctx    = ctx,
	  .store  = store,
	  .parent = SEED_NONE,
	  .orientation
	  = ( FluxScrollBarOrientation ) mount_enum(props, prop_count, XTK_PROP_ORIENTATION, FLUX_SCROLLBAR_VERTICAL),
	  .minimum       = mount_f64(props, prop_count, XTK_PROP_MIN_F64, 0.0),
	  .maximum       = mount_f64(props, prop_count, XTK_PROP_MAX_F64, 1.0),
	  .value         = mount_f64(props, prop_count, XTK_PROP_VALUE_F64, 0.0),
	  .viewport_size = mount_f64(props, prop_count, XTK_PROP_VIEWPORT_SIZE, 0.0)});
}

static XentNodeId mount_rating(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_rating_init(&(FluxRatingDesc) {
	  .ctx               = ctx,
	  .store             = store,
	  .parent            = SEED_NONE,
	  .value             = mount_f64(props, prop_count, XTK_PROP_VALUE_F64, -1.0),
	  .max_rating        = ( int ) mount_f64(props, prop_count, XTK_PROP_MAX_F64, 5.0),
	  .placeholder_value = mount_f64(props, prop_count, XTK_PROP_PLACEHOLDER_F64, -1.0),
	  .initial_set_value = mount_i32(props, prop_count, XTK_PROP_INITIAL_VALUE, 1),
	  .is_clear_enabled  = mount_bool(props, prop_count, XTK_PROP_CLEAR_ENABLED, false),
	  .is_read_only      = mount_bool(props, prop_count, XTK_PROP_READ_ONLY, false),
	  .caption           = mount_string(props, prop_count, XTK_PROP_TEXT, NULL)});
}

static XentNodeId mount_expander(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	return flux_expander_init(&(FluxExpanderDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .parent         = SEED_NONE,
	  .window         = env->window,
	  .header         = mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  .expanded       = mount_bool(props, prop_count, XTK_PROP_EXPANDED, false),
	  .width          = mount_f32(props, prop_count, XTK_PROP_WIDTH, 0.0f),
	  .content_height = mount_f32(props, prop_count, XTK_PROP_CONTENT_HEIGHT, 0.0f)});
}

static XentNodeId mount_refresh(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	return flux_refresh_init(&(FluxRefreshContainerDesc) {
	  .ctx             = ctx,
	  .store           = store,
	  .parent          = SEED_NONE,
	  .window          = env->window,
	  .direction       = ( FluxPullDirection ) mount_enum(props, prop_count, XTK_PROP_PULL_DIRECTION, 0u),
	  .visualizer_size = mount_f32(props, prop_count, XTK_PROP_VISUALIZER_SIZE, 100.0f),
	  .threshold_ratio = mount_f32(props, prop_count, XTK_PROP_THRESHOLD_RATIO, 0.8f)});
}

static XentNodeId mount_info_bar(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_infobar_init(&(FluxInfoBarDesc) {
	  .ctx         = ctx,
	  .store       = store,
	  .parent      = SEED_NONE,
	  .severity    = ( FluxInfoBarSeverity ) mount_enum(props, prop_count, XTK_PROP_SEVERITY, 0u),
	  .title       = mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  .message     = mount_string(props, prop_count, XTK_PROP_MESSAGE, NULL),
	  .is_closable = mount_bool(props, prop_count, XTK_PROP_CLOSABLE, false)});
}

static XentNodeId mount_dialog(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	return flux_dialog_init(&(FluxContentDialogDesc) {
	  .ctx            = ctx,
	  .store          = store,
	  .overlay_parent = env->overlay,
	  .input          = env->input,
	  .window         = env->window,
	  .theme          = env->theme,
	  .title          = mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  .primary_text   = mount_string(props, prop_count, XTK_PROP_PRIMARY_TEXT, NULL),
	  .secondary_text = mount_string(props, prop_count, XTK_PROP_SECONDARY_TEXT, NULL),
	  .close_text     = mount_string(props, prop_count, XTK_PROP_CLOSE_TEXT, NULL),
	  .width          = mount_f32(props, prop_count, XTK_PROP_WIDTH, 400.0f),
	  .content_height = mount_f32(props, prop_count, XTK_PROP_CONTENT_HEIGHT, 120.0f)});
}

static XentNodeId
mount_nav(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	if (!env || !env->window) return XENT_NODE_INVALID;
	return flux_nav_init(&(FluxNavViewDesc) {
	  .ctx      = ctx,
	  .store    = store,
	  .parent   = SEED_NONE,
	  .window   = env->window,
	  .text     = env->text,
	  .theme    = env->theme,
	  .width    = mount_f32(props, prop_count, XTK_PROP_WIDTH, 0.0f),
	  .height   = mount_f32(props, prop_count, XTK_PROP_HEIGHT, 0.0f),
	  .adaptive = true});
}

static XentNodeId mount_textbox(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	char const *placeholder = mount_string(props, prop_count, XTK_PROP_PLACEHOLDER, NULL);
	if (env && env->app)
		return flux_app_create_textbox(&(FluxAppTextBoxDesc) {env->app, SEED_NONE, placeholder, NULL, NULL});
	return flux_textbox_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, placeholder, NULL, NULL});
}

static XentNodeId mount_password(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	char const *placeholder = mount_string(props, prop_count, XTK_PROP_PLACEHOLDER, NULL);
	if (env && env->app)
		return flux_app_create_password(&(FluxAppTextBoxDesc) {env->app, SEED_NONE, placeholder, NULL, NULL});
	return flux_password_init(&(FluxTextBoxDesc) {ctx, store, SEED_NONE, placeholder, NULL, NULL});
}

static XentNodeId mount_number(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	double min  = mount_f64(props, prop_count, XTK_PROP_MIN_F64, 0.0);
	double max  = mount_f64(props, prop_count, XTK_PROP_MAX_F64, 100.0);
	double step = mount_f64(props, prop_count, XTK_PROP_STEP_F64, 1.0);
	if (env && env->app)
		return flux_app_create_number(&(FluxAppNumberBoxDesc) {env->app, SEED_NONE, min, max, step, NULL, NULL});
	return flux_number_init(&(FluxNumberBoxDesc) {ctx, store, SEED_NONE, min, max, step, NULL, NULL});
}

static XentNodeId mount_split_view(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	( void ) env;
	return flux_splitview_init(&(FluxSplitViewDesc) {
	  .ctx                 = ctx,
	  .store               = store,
	  .parent              = SEED_NONE,
	  .display_mode        = ( int ) mount_enum(props, prop_count, XTK_PROP_DISPLAY_MODE, 0u),
	  .pane_placement      = ( int ) mount_enum(props, prop_count, XTK_PROP_PANE_PLACEMENT, 0u),
	  .pane_open           = mount_bool(props, prop_count, XTK_PROP_OPEN, false),
	  .open_pane_length    = mount_f32(props, prop_count, XTK_PROP_OPEN_PANE_LENGTH, 0.0f),
	  .compact_pane_length = mount_f32(props, prop_count, XTK_PROP_COMPACT_PANE_LENGTH, 0.0f)});
}

static XentNodeId mount_title_bar(
  XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count
) {
	return flux_titlebar_init(&(FluxTitleBarDesc) {
	  .ctx              = ctx,
	  .store            = store,
	  .parent           = SEED_NONE,
	  .window           = env->window,
	  .title            = mount_string(props, prop_count, XTK_PROP_TEXT, NULL),
	  .subtitle         = mount_string(props, prop_count, XTK_PROP_SUBTITLE, NULL),
	  .icon             = mount_string(props, prop_count, XTK_PROP_ICON, NULL),
	  .show_back        = mount_bool(props, prop_count, XTK_PROP_SHOW_BACK, false),
	  .back_disabled    = mount_bool(props, prop_count, XTK_PROP_BACK_DISABLED, false),
	  .show_pane_toggle = mount_bool(props, prop_count, XTK_PROP_SHOW_PANE_TOGGLE, false),
	  .integrate_window = false});
}

static XentNodeId
mount_pager(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	( void ) env;
	return flux_pager_init(&(FluxPagerDesc) {
	  .ctx          = ctx,
	  .store        = store,
	  .parent       = SEED_NONE,
	  .count        = mount_i32(props, prop_count, XTK_PROP_ITEM_COUNT, 1),
	  .selected     = mount_i32(props, prop_count, XTK_PROP_SELECTED_INDEX, 0),
	  .display_mode = ( int ) mount_enum(props, prop_count, XTK_PROP_DISPLAY_MODE, 0u),
	  .first_button = ( int ) mount_enum(props, prop_count, XTK_PROP_FIRST_BUTTON, 0u),
	  .prev_button  = ( int ) mount_enum(props, prop_count, XTK_PROP_PREVIOUS_BUTTON, 0u),
	  .next_button  = ( int ) mount_enum(props, prop_count, XTK_PROP_NEXT_BUTTON, 0u),
	  .last_button  = ( int ) mount_enum(props, prop_count, XTK_PROP_LAST_BUTTON, 0u),
	  .prefix       = mount_string(props, prop_count, XTK_PROP_PREFIX, NULL),
	  .suffix       = mount_string(props, prop_count, XTK_PROP_SUFFIX, NULL)});
}

static XentNodeId
mount_pips(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	( void ) env;
	XentNodeId node = flux_pips_init(&(FluxPipsPagerDesc) {
	  .ctx      = ctx,
	  .store    = store,
	  .parent   = SEED_NONE,
	  .vertical = mount_enum(props, prop_count, XTK_PROP_ORIENTATION, 0u) != 0u});
	if (node == XENT_NODE_INVALID) return node;
	flux_pips_configure(
	  store, node, mount_i32(props, prop_count, XTK_PROP_ITEM_COUNT, 0),
	  mount_i32(props, prop_count, XTK_PROP_SELECTED_INDEX, 0), mount_i32(props, prop_count, XTK_PROP_MAX_VISIBLE, 5),
	  ( int ) mount_enum(props, prop_count, XTK_PROP_NAV_VISIBILITY, 0u)
	);
	return node;
}

static bool mount_tab_values(FluxStore *store, XentNodeId node, XtkPropPatch const *props, uint32_t prop_count) {
	MountStrings strings = {0};
	if (!mount_strings_open(props, prop_count, XTK_PROP_ITEMS, &strings)) return false;
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
	  store, node, icons, labels, strings.count / 2, mount_i32(props, prop_count, XTK_PROP_SELECTED_INDEX, -1),
	  mount_bool(props, prop_count, XTK_PROP_ADD_VISIBLE, false)
	);
	mount_strings_close(&strings);
	return ok;
}

static XentNodeId
mount_tab(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env, XtkPropPatch const *props, uint32_t prop_count) {
	XentNodeId node = flux_tabs_init(&(FluxTabViewDesc) {
	  .ctx = ctx, .store = store, .parent = SEED_NONE, .window = env->window, .text = env->text, .input = env->input});
	if (node == XENT_NODE_INVALID || mount_tab_values(store, node, props, prop_count)) return node;
	flux_tree_drop(store, node);
	return XENT_NODE_INVALID;
}

typedef struct FluxMountProperty {
	FluxControlType type;
	XtkPropertyId   property;
} FluxMountProperty;

#define MOUNT_PROP(type, property) {FLUX_CONTROL_##type, XTK_PROP_##property}

static FluxMountProperty const mount_properties [] = {
  MOUNT_PROP(BUTTON, TEXT),
  MOUNT_PROP(REPEAT_BUTTON, TEXT),
  MOUNT_PROP(DROPDOWN_BUTTON, TEXT),
  MOUNT_PROP(DROPDOWN_BUTTON, ICON),
  MOUNT_PROP(SPLIT_BUTTON, TEXT),
  MOUNT_PROP(SPLIT_BUTTON, ICON),
  MOUNT_PROP(TOGGLE_SPLIT_BUTTON, TEXT),
  MOUNT_PROP(TOGGLE_SPLIT_BUTTON, ICON),
  MOUNT_PROP(TOGGLE_SPLIT_BUTTON, CHECKED),
  MOUNT_PROP(HYPERLINK, TEXT),
  MOUNT_PROP(HYPERLINK, URL),
  MOUNT_PROP(TEXT, TEXT),
  MOUNT_PROP(TEXT, FONT_SIZE),
  MOUNT_PROP(CHECKBOX, TEXT),
  MOUNT_PROP(CHECKBOX, CHECKED),
  MOUNT_PROP(RADIO, TEXT),
  MOUNT_PROP(RADIO, CHECKED),
  MOUNT_PROP(SWITCH, TEXT),
  MOUNT_PROP(SWITCH, CHECKED),
  MOUNT_PROP(SLIDER, MIN_F32),
  MOUNT_PROP(SLIDER, MAX_F32),
  MOUNT_PROP(SLIDER, VALUE_F32),
  MOUNT_PROP(PROGRESS, MAX_F32),
  MOUNT_PROP(PROGRESS, MIN_F32),
  MOUNT_PROP(PROGRESS, VALUE_F32),
  MOUNT_PROP(PROGRESS_RING, MAX_F32),
  MOUNT_PROP(PROGRESS_RING, MIN_F32),
  MOUNT_PROP(PROGRESS_RING, VALUE_F32),
  MOUNT_PROP(INFO_BADGE, BADGE_MODE),
  MOUNT_PROP(INFO_BADGE, BADGE_VALUE),
  MOUNT_PROP(RATING, VALUE_F64),
  MOUNT_PROP(RATING, MAX_F64),
  MOUNT_PROP(RATING, PLACEHOLDER_F64),
  MOUNT_PROP(RATING, INITIAL_VALUE),
  MOUNT_PROP(RATING, CLEAR_ENABLED),
  MOUNT_PROP(RATING, READ_ONLY),
  MOUNT_PROP(RATING, TEXT),
  MOUNT_PROP(COMBO_BOX, ITEMS),
  MOUNT_PROP(COMBO_BOX, SELECTED_INDEX),
  MOUNT_PROP(COMBO_BOX, PLACEHOLDER),
  MOUNT_PROP(EXPANDER, TEXT),
  MOUNT_PROP(EXPANDER, EXPANDED),
  MOUNT_PROP(EXPANDER, WIDTH),
  MOUNT_PROP(EXPANDER, CONTENT_HEIGHT),
  MOUNT_PROP(REFRESH, PULL_DIRECTION),
  MOUNT_PROP(REFRESH, VISUALIZER_SIZE),
  MOUNT_PROP(REFRESH, THRESHOLD_RATIO),
  MOUNT_PROP(INFO_BAR, TEXT),
  MOUNT_PROP(INFO_BAR, MESSAGE),
  MOUNT_PROP(INFO_BAR, SEVERITY),
  MOUNT_PROP(INFO_BAR, CLOSABLE),
  MOUNT_PROP(CONTENT_DIALOG, TEXT),
  MOUNT_PROP(CONTENT_DIALOG, PRIMARY_TEXT),
  MOUNT_PROP(CONTENT_DIALOG, SECONDARY_TEXT),
  MOUNT_PROP(CONTENT_DIALOG, CLOSE_TEXT),
  MOUNT_PROP(CONTENT_DIALOG, WIDTH),
  MOUNT_PROP(CONTENT_DIALOG, CONTENT_HEIGHT),
  MOUNT_PROP(NAV_VIEW, WIDTH),
  MOUNT_PROP(NAV_VIEW, HEIGHT),
  MOUNT_PROP(TEXT_INPUT, PLACEHOLDER),
  MOUNT_PROP(PASSWORD_BOX, PLACEHOLDER),
  MOUNT_PROP(NUMBER_BOX, MIN_F64),
  MOUNT_PROP(NUMBER_BOX, MAX_F64),
  MOUNT_PROP(NUMBER_BOX, STEP_F64),
  MOUNT_PROP(SPLIT_VIEW, DISPLAY_MODE),
  MOUNT_PROP(SPLIT_VIEW, PANE_PLACEMENT),
  MOUNT_PROP(SPLIT_VIEW, OPEN),
  MOUNT_PROP(SPLIT_VIEW, OPEN_PANE_LENGTH),
  MOUNT_PROP(SPLIT_VIEW, COMPACT_PANE_LENGTH),
  MOUNT_PROP(TITLE_BAR, TEXT),
  MOUNT_PROP(TITLE_BAR, SUBTITLE),
  MOUNT_PROP(TITLE_BAR, ICON),
  MOUNT_PROP(TITLE_BAR, SHOW_BACK),
  MOUNT_PROP(TITLE_BAR, BACK_DISABLED),
  MOUNT_PROP(TITLE_BAR, SHOW_PANE_TOGGLE),
  MOUNT_PROP(PAGER, ITEM_COUNT),
  MOUNT_PROP(PAGER, SELECTED_INDEX),
  MOUNT_PROP(PAGER, DISPLAY_MODE),
  MOUNT_PROP(PAGER, FIRST_BUTTON),
  MOUNT_PROP(PAGER, PREVIOUS_BUTTON),
  MOUNT_PROP(PAGER, NEXT_BUTTON),
  MOUNT_PROP(PAGER, LAST_BUTTON),
  MOUNT_PROP(PAGER, PREFIX),
  MOUNT_PROP(PAGER, SUFFIX),
  MOUNT_PROP(PIPS_PAGER, ORIENTATION),
  MOUNT_PROP(PIPS_PAGER, ITEM_COUNT),
  MOUNT_PROP(PIPS_PAGER, SELECTED_INDEX),
  MOUNT_PROP(PIPS_PAGER, MAX_VISIBLE),
  MOUNT_PROP(PIPS_PAGER, NAV_VISIBILITY),
  MOUNT_PROP(TAB_VIEW, ITEMS),
  MOUNT_PROP(TAB_VIEW, SELECTED_INDEX),
  MOUNT_PROP(TAB_VIEW, ADD_VISIBLE),
};

bool flux_control_mount_consumed(FluxControlType type, XtkPropertyId property) {
	for (size_t i = 0; i < sizeof(mount_properties) / sizeof(mount_properties [0]); i++)
		if (mount_properties [i].type == type && mount_properties [i].property == property) return true;
	return false;
}

static FluxRetargetFn const k_retarget [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_BUTTON]              = retarget_button,
  [FLUX_CONTROL_REPEAT_BUTTON]       = retarget_repeat,
  [FLUX_CONTROL_HYPERLINK]           = retarget_hyperlink,
  [FLUX_CONTROL_DROPDOWN_BUTTON]     = retarget_dropdown,
  [FLUX_CONTROL_SPLIT_BUTTON]        = retarget_split,
  [FLUX_CONTROL_TOGGLE_SPLIT_BUTTON] = retarget_split,
  [FLUX_CONTROL_SLIDER]              = retarget_slider,
  [FLUX_CONTROL_SCROLL_BAR]          = retarget_scroll_bar,
  [FLUX_CONTROL_LOOPING_SELECTOR]    = retarget_looping,
  [FLUX_CONTROL_DATE_PICKER]         = retarget_picker,
  [FLUX_CONTROL_TIME_PICKER]         = retarget_picker,
  [FLUX_CONTROL_CALENDAR_VIEW]       = flux_calendar_retarget,
  [FLUX_CONTROL_COLOR_PICKER]        = flux_colorpicker_retarget,
  [FLUX_CONTROL_RATING]              = retarget_rating,
  [FLUX_CONTROL_CHECKBOX]            = retarget_checkbox,
  [FLUX_CONTROL_RADIO]               = retarget_radio,
  [FLUX_CONTROL_SWITCH]              = retarget_switch,
  [FLUX_CONTROL_SCROLL]              = retarget_scroll,
  [FLUX_CONTROL_LIST]                = retarget_list,
  [FLUX_CONTROL_LIST_BOX]            = retarget_list,
  [FLUX_CONTROL_GRID_VIEW]           = retarget_list,
  [FLUX_CONTROL_ITEMS_REPEATER]      = retarget_list,
  [FLUX_CONTROL_ITEMS_VIEW]          = retarget_list,
  [FLUX_CONTROL_RADIO_BUTTONS]       = retarget_radio_buttons,
  [FLUX_CONTROL_TREE_VIEW]           = retarget_tree,
  [FLUX_CONTROL_REFRESH]             = retarget_refresh,
  [FLUX_CONTROL_SELECTOR_BAR]        = retarget_selector,
  [FLUX_CONTROL_BREADCRUMB_BAR]      = retarget_breadcrumb,
  [FLUX_CONTROL_NAV_VIEW]            = retarget_nav,
  [FLUX_CONTROL_TAB_VIEW]            = retarget_tab,
  [FLUX_CONTROL_MENU_BAR]            = retarget_menu_bar,
  [FLUX_CONTROL_EXPANDER]            = retarget_expander,
  [FLUX_CONTROL_COMBO_BOX]           = retarget_combo,
  [FLUX_CONTROL_CONTENT_DIALOG]      = retarget_dialog,
  [FLUX_CONTROL_TEXT_INPUT]          = retarget_textbox,
  [FLUX_CONTROL_PASSWORD_BOX]        = retarget_textbox,
  [FLUX_CONTROL_NUMBER_BOX]          = retarget_textbox,
  [FLUX_CONTROL_INFO_BAR]            = retarget_info_bar,
  [FLUX_CONTROL_FLIP_VIEW]           = retarget_flip,
  [FLUX_CONTROL_PAGER]               = retarget_pager,
  [FLUX_CONTROL_PIPS_PAGER]          = retarget_pips,
  [FLUX_CONTROL_PERSON_PICTURE]      = retarget_person,
  [FLUX_CONTROL_SPLIT_VIEW]          = retarget_split_view,
  [FLUX_CONTROL_TITLE_BAR]           = retarget_title_bar,
  [FLUX_CONTROL_AUTO_SUGGEST]        = retarget_auto_suggest,
  [FLUX_CONTROL_TEACHING_TIP]        = retarget_teaching_tip,
};

static void apply_retarget(XentCtx *ctx, FluxStore *store, XentNodeId dst, FluxControlType type) {
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return;
	if (k_retarget [type]) k_retarget [type](ctx, store, dst);
}

static FluxMountSeedFn const k_mount_seed [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_BUTTON]              = mount_button,
  [FLUX_CONTROL_REPEAT_BUTTON]       = mount_repeat,
  [FLUX_CONTROL_DROPDOWN_BUTTON]     = mount_dropdown,
  [FLUX_CONTROL_SPLIT_BUTTON]        = mount_split,
  [FLUX_CONTROL_TOGGLE_SPLIT_BUTTON] = mount_toggle_split,
  [FLUX_CONTROL_HYPERLINK]           = mount_hyperlink,
  [FLUX_CONTROL_TEXT]                = mount_text,
  [FLUX_CONTROL_CHECKBOX]            = mount_checkbox,
  [FLUX_CONTROL_RADIO]               = mount_radio,
  [FLUX_CONTROL_SWITCH]              = mount_switch,
  [FLUX_CONTROL_SLIDER]              = mount_slider,
  [FLUX_CONTROL_SCROLL_BAR]          = mount_scroll_bar,
  [FLUX_CONTROL_LOOPING_SELECTOR]    = mount_looping,
  [FLUX_CONTROL_PROGRESS]            = mount_progress,
  [FLUX_CONTROL_PROGRESS_RING]       = mount_progress_ring,
  [FLUX_CONTROL_INFO_BADGE]          = mount_badge,
  [FLUX_CONTROL_RATING]              = mount_rating,
  [FLUX_CONTROL_COMBO_BOX]           = mount_combo,
  [FLUX_CONTROL_EXPANDER]            = mount_expander,
  [FLUX_CONTROL_REFRESH]             = mount_refresh,
  [FLUX_CONTROL_INFO_BAR]            = mount_info_bar,
  [FLUX_CONTROL_CONTENT_DIALOG]      = mount_dialog,
  [FLUX_CONTROL_NAV_VIEW]            = mount_nav,
  [FLUX_CONTROL_TEXT_INPUT]          = mount_textbox,
  [FLUX_CONTROL_PASSWORD_BOX]        = mount_password,
  [FLUX_CONTROL_NUMBER_BOX]          = mount_number,
  [FLUX_CONTROL_SPLIT_VIEW]          = mount_split_view,
  [FLUX_CONTROL_TITLE_BAR]           = mount_title_bar,
  [FLUX_CONTROL_PAGER]               = mount_pager,
  [FLUX_CONTROL_PIPS_PAGER]          = mount_pips,
  [FLUX_CONTROL_TAB_VIEW]            = mount_tab,
};

static FluxLayoutFn const k_layout [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_CONTAINER]          = layout_flex_column,
  [FLUX_CONTROL_CARD]               = layout_card,
  [FLUX_CONTROL_DIVIDER]            = layout_divider,
  [FLUX_CONTROL_SPLIT_VIEW_CONTENT] = layout_flex_column,
  [FLUX_CONTROL_SPLIT_VIEW_PANE]    = layout_flex_column,
};

static FluxSeedFn const k_seed [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_BUTTON]              = seed_button,
  [FLUX_CONTROL_DROPDOWN_BUTTON]     = seed_dropdown,
  [FLUX_CONTROL_REPEAT_BUTTON]       = seed_repeat,
  [FLUX_CONTROL_HYPERLINK]           = seed_hyperlink,
  [FLUX_CONTROL_TEXT]                = seed_text,
  [FLUX_CONTROL_CHECKBOX]            = seed_checkbox,
  [FLUX_CONTROL_RADIO]               = seed_radio,
  [FLUX_CONTROL_SWITCH]              = seed_switch,
  [FLUX_CONTROL_SLIDER]              = seed_slider,
  [FLUX_CONTROL_SCROLL_BAR]          = seed_scroll_bar,
  [FLUX_CONTROL_LOOPING_SELECTOR]    = seed_looping,
  [FLUX_CONTROL_IMAGE]               = seed_image,
  [FLUX_CONTROL_SCROLL]              = seed_scroll,
  [FLUX_CONTROL_PROGRESS]            = seed_progress,
  [FLUX_CONTROL_PROGRESS_RING]       = seed_progress_ring,
  [FLUX_CONTROL_INFO_BADGE]          = seed_info_badge,
  [FLUX_CONTROL_CALENDAR_VIEW]       = flux_calendar_init,
  [FLUX_CONTROL_SPLIT_BUTTON]        = seed_split,
  [FLUX_CONTROL_TOGGLE_SPLIT_BUTTON] = seed_toggle_split,
  [FLUX_CONTROL_LIST_ITEM]           = seed_list_item,
  [FLUX_CONTROL_TREE_VIEW]           = seed_tree,
  [FLUX_CONTROL_RADIO_BUTTONS]       = seed_radio_buttons,
  [FLUX_CONTROL_RATING]              = seed_rating,
  [FLUX_CONTROL_SELECTOR_BAR]        = seed_selector,
  [FLUX_CONTROL_PAGER]               = seed_pager,
  [FLUX_CONTROL_PERSON_PICTURE]      = seed_person,
  [FLUX_CONTROL_SPLIT_VIEW]          = seed_split_view,
  [FLUX_CONTROL_INFO_BAR]            = seed_info_bar,
  [FLUX_CONTROL_PIPS_PAGER]          = seed_pips,
};

static FluxEnvSeedFn const k_env_seed [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_DATE_PICKER]    = seed_date_picker,
  [FLUX_CONTROL_TIME_PICKER]    = seed_time_picker,
  [FLUX_CONTROL_COLOR_PICKER]   = flux_colorpicker_init,
  [FLUX_CONTROL_COMBO_BOX]      = seed_combo,
  [FLUX_CONTROL_AUTO_SUGGEST]   = seed_auto_suggest,
  [FLUX_CONTROL_BREADCRUMB_BAR] = seed_breadcrumb,
  [FLUX_CONTROL_TEACHING_TIP]   = seed_teaching_tip,
  [FLUX_CONTROL_CONTENT_DIALOG] = seed_dialog,
  [FLUX_CONTROL_MENU_BAR]       = seed_menu_bar,
  [FLUX_CONTROL_NAV_VIEW]       = seed_nav,
  [FLUX_CONTROL_TAB_VIEW]       = seed_tab,
  [FLUX_CONTROL_TITLE_BAR]      = seed_title_bar,
  [FLUX_CONTROL_TEXT_INPUT]     = seed_textbox,
  [FLUX_CONTROL_PASSWORD_BOX]   = seed_password,
  [FLUX_CONTROL_NUMBER_BOX]     = seed_number,
  [FLUX_CONTROL_LIST]           = seed_list,
  [FLUX_CONTROL_LIST_BOX]       = seed_list_box,
  [FLUX_CONTROL_GRID_VIEW]      = seed_grid,
  [FLUX_CONTROL_ITEMS_REPEATER] = seed_repeater,
  [FLUX_CONTROL_ITEMS_VIEW]     = seed_items_view,
  [FLUX_CONTROL_REFRESH]        = seed_refresh,
  [FLUX_CONTROL_EXPANDER]       = seed_expander,
  [FLUX_CONTROL_FLIP_VIEW]      = seed_flip,
};

static FluxUnmountFn const k_unmount [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_CONTENT_DIALOG] = flux_dialog_hide,
  [FLUX_CONTROL_DATE_PICKER]    = flux_picker_close,
  [FLUX_CONTROL_TIME_PICKER]    = flux_picker_close,
};

void flux_control_unmount(FluxStore *store, XentNodeId node, FluxControlType type) {
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT || !k_unmount [type]) return;
	k_unmount [type](store, node);
}

static bool build_menu_shell(
  XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlType type, FluxControlEnv const *env, bool *handled
) {
	*handled = true;
	if (type == FLUX_CONTROL_MENU_FLYOUT) return flux_host_menufly_build(ctx, store, node, env);
	if (type == FLUX_CONTROL_MENU_BAR_ITEM) return flux_host_menuitem_build(ctx, store, node);
	*handled = false;
	return true;
}

static XentNodeId
control_seed(XentCtx *ctx, FluxStore *store, FluxControlType type, FluxControlMount const *mount, bool *handled) {
	*handled = true;
	if (k_mount_seed [type]) return k_mount_seed [type](ctx, store, mount->env, mount->props, mount->prop_count);
	if (k_env_seed [type]) return k_env_seed [type](ctx, store, mount->env);
	if (k_seed [type]) return k_seed [type](ctx, store);
	*handled = false;
	return XENT_NODE_INVALID;
}

static bool absorb_seed(XentCtx *ctx, FluxStore *store, XentNodeId node, XentNodeId seed, FluxControlType type) {
	if (seed == XENT_NODE_INVALID) return false;
	transfer(ctx, store, node, seed);
	apply_retarget(ctx, store, node, type);
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_data;
}

bool flux_control_build(
  XentCtx *ctx, FluxStore *store, XentNodeId node, FluxControlType type, FluxControlMount const *mount
) {
	if (!mount || !mount->env) return false;
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return false;
	if (nd->component_data) return true;
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return true;
	bool handled = false;
	bool ok      = build_menu_shell(ctx, store, node, type, mount->env, &handled);
	if (handled) return ok;
	if (k_layout [type]) {
		k_layout [type](ctx, node);
		return true;
	}
	XentNodeId seed = control_seed(ctx, store, type, mount, &handled);
	return handled ? absorb_seed(ctx, store, node, seed, type) : true;
}
