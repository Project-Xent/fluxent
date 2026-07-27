#include <stdio.h>
#include <string.h>
#include <fluxent/flux_host.h>
#include <fluxent/flux_menu.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>
#include <xtk/xtk_nav_model.h>

#include "bridge/flux_internal.h"
#include "controls/data/flux_button_data.h"
#include "controls/data/flux_combo_box_data.h"
#include "controls/data/flux_menu_bar_data.h"
#include "controls/data/flux_nav_view_data.h"
#include "controls/data/flux_person_picture_data.h"
#include "controls/data/flux_progress_data.h"
#include "controls/data/flux_split_view_data.h"
#include "controls/data/flux_tab_view_data.h"
#include "controls/data/flux_title_bar_data.h"
#include "controls/behavior/flux_expander_internal.h"
#include "controls/behavior/flux_host_menu.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_popup.h"
#include "popup/flux_menu_internal.h"
#include "controls/textbox/tb_internal.h"
#include "render/flux_dl_replay.h"

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static uint32_t g_clicks;
static int      g_nav_selection = -1;

typedef struct DropComboProbe {
	FluxStore *store;
	XentNodeId node;
	int        selected;
} DropComboProbe;

typedef struct DropMenuProbe {
	FluxMenu *menu;
	int       clicks;
} DropMenuProbe;

typedef struct ComboProbe {
	int calls;
	int selected;
} ComboProbe;

typedef struct DropNavProbe {
	FluxStore *store;
	XentNodeId node;
} DropNavProbe;

static uint32_t on_invoke(void *ud, XtkEvent const *ev) {
	( void ) ud;
	( void ) ev;
	g_clicks++;
	return XTK_EVENT_HANDLED;
}

static uint32_t on_nav_select(void *ud, XtkEvent const *ev) {
	( void ) ud;
	if (ev->payload.kind == XTK_EVENT_PAYLOAD_I32) g_nav_selection = ev->payload.u.i32;
	return XTK_EVENT_HANDLED;
}

static XtkBatchResult *commit_ok(XtkHost *host, XtkBatch *batch) {
	XtkPrepare  *prepare = NULL;
	XtkBatchDiag diagnostic;
	if (host->vt->prepare(host, batch, &prepare, &diagnostic) != XTK_BATCH_OK || !prepare) {
		printf("FAIL: prepare %d op %u\n", ( int ) diagnostic.error, diagnostic.failed_op);
		return NULL;
	}
	XtkBatchResult *result = host->vt->commit(host, prepare, &diagnostic);
	if (!result) printf("FAIL: commit %d\n", ( int ) diagnostic.error);
	return result;
}

static XtkCtrl mapped_control(XtkBatchResult const *result, XtkLocalId local) {
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == local) return result->controls [i].control;
	return XTK_CTRL_INVALID;
}

static int test_retained_menu(XtkHost *host, FluxApp *app) {
	XtkPropPatch drop_prop = {
	  .id    = XTK_PROP_TEXT,
	  .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Actions", .len = 7u}},
	};
	XtkPropPatch first_props [] = {
	  {.id = XTK_PROP_TEXT,    .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Open", .len = 4u}}},
	  {.id = XTK_PROP_ENABLED, .value = {.kind = XTK_VALUE_BOOL, .u.b = true}                              },
	};
	XtkPropPatch second_props [] = {
	  {.id = XTK_PROP_TEXT,    .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Locked", .len = 6u}}},
	  {.id = XTK_PROP_ENABLED, .value = {.kind = XTK_VALUE_BOOL, .u.b = false}                               },
	};
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "retained menu batch");
	XtkLocalId drop  = xtk_batch_create(batch, XTK_CTRL_DROPDOWN, xtk_ref_none(), xtk_ref_none(), &drop_prop, 1u);
	XtkLocalId fly   = xtk_batch_create(batch, XTK_CTRL_MENUFLY, xtk_ref_local(drop), xtk_ref_none(), NULL, 0u);
	XtkLocalId first = xtk_batch_create(batch, XTK_CTRL_MENUITEM, xtk_ref_local(fly), xtk_ref_none(), first_props, 2u);
	XtkLocalId second
	  = xtk_batch_create(batch, XTK_CTRL_MENUITEM, xtk_ref_local(fly), xtk_ref_none(), second_props, 2u);
	EXPECT(drop != XTK_LOCAL_INVALID && second != XTK_LOCAL_INVALID, "retained menu creates");
	EXPECT(
	  xtk_batch_on(batch, xtk_ref_local(first), XTK_EVENT_INVOKE, XTK_EVENT_PHASE_MASK_TARGET, on_invoke, NULL)
		!= XTK_SUB_TOKEN_INVALID,
	  "retained menu subscription"
	);
	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 4u, "retained menu commit");

	XentNodeId    drop_node = ( XentNodeId ) mapped_control(result, drop);
	XentNodeId    fly_node  = ( XentNodeId ) mapped_control(result, fly);
	FluxNodeData *drop_data = flux_store_find(flux_app_store(app), drop_node);
	FluxMenu     *menu      = flux_host_menufly_menu(flux_app_store(app), fly_node);
	EXPECT(drop_data && drop_data->behavior.on_click && menu, "dropdown bound to retained menu");
	EXPECT(menu->item_count == 2, "retained menu item count");
	EXPECT(menu->items [0].enabled && !menu->items [1].enabled, "retained menu enabled state");
	EXPECT(strcmp(menu->items [0].label, "Open") == 0, "retained menu label");

	g_clicks = 0u;
	activate_item(menu, 1);
	EXPECT(g_clicks == 0u, "disabled retained menu item cannot invoke");
	activate_item(menu, 0);
	EXPECT(g_clicks == 1u, "enabled retained menu item routes invoke");

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup && xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) drop_node)), "retained menu cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "retained menu cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);

	XtkPropPatch fly_prop = {
	  .id    = XTK_PROP_TEXT,
	  .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "File", .len = 4u}},
	};
	batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "retained menu bar batch");
	XtkLocalId bar = xtk_batch_create(batch, XTK_CTRL_MENUBAR, xtk_ref_none(), xtk_ref_none(), NULL, 0u);
	fly            = xtk_batch_create(batch, XTK_CTRL_MENUFLY, xtk_ref_local(bar), xtk_ref_none(), &fly_prop, 1u);
	first          = xtk_batch_create(batch, XTK_CTRL_MENUITEM, xtk_ref_local(fly), xtk_ref_none(), first_props, 2u);
	EXPECT(bar != XTK_LOCAL_INVALID && first != XTK_LOCAL_INVALID, "retained menu bar creates");
	result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 3u, "retained menu bar commit");
	XentNodeId bar_node       = ( XentNodeId ) mapped_control(result, bar);
	fly_node                  = ( XentNodeId ) mapped_control(result, fly);
	FluxNodeData    *bar_data = flux_store_find(flux_app_store(app), bar_node);
	FluxMenuBarData *menu_bar = bar_data ? bar_data->component_data : NULL;
	menu                      = flux_host_menufly_menu(flux_app_store(app), fly_node);
	EXPECT(menu_bar && menu_bar->count == 1, "retained menu bar item count");
	EXPECT(menu_bar->items [0].flyout == menu && !menu_bar->items [0].owns_flyout, "menu bar borrows retained flyout");

	cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup && xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) bar_node)), "menu bar cleanup");
	cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "menu bar cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_progress_range(XtkHost *host, FluxApp *app) {
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_MIN_F32,   .value = {.kind = XTK_VALUE_F32, .u.f32 = 25.0f} },
	  {.id = XTK_PROP_MAX_F32,   .value = {.kind = XTK_VALUE_F32, .u.f32 = 125.0f}},
	  {.id = XTK_PROP_VALUE_F32, .value = {.kind = XTK_VALUE_F32, .u.f32 = 75.0f} },
	};
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "progress range batch");
	XtkLocalId bar  = xtk_batch_create(batch, XTK_CTRL_PROGRESS, xtk_ref_none(), xtk_ref_none(), props, 3u);
	XtkLocalId ring = xtk_batch_create(batch, XTK_CTRL_PROGRESSRING, xtk_ref_none(), xtk_ref_none(), props, 3u);
	EXPECT(bar != XTK_LOCAL_INVALID && ring != XTK_LOCAL_INVALID, "progress range creates");
	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 2u, "progress range commit");

	XentNodeId            bar_node  = ( XentNodeId ) mapped_control(result, bar);
	XentNodeId            ring_node = ( XentNodeId ) mapped_control(result, ring);
	FluxNodeData         *bar_nd    = flux_store_find(flux_app_store(app), bar_node);
	FluxNodeData         *ring_nd   = flux_store_find(flux_app_store(app), ring_node);
	FluxProgressData     *bar_data  = bar_nd ? ( FluxProgressData * ) bar_nd->component_data : NULL;
	FluxProgressRingData *ring_data = ring_nd ? ( FluxProgressRingData * ) ring_nd->component_data : NULL;
	EXPECT(
	  bar_data && bar_data->min_value == 25.0f && bar_data->max_value == 125.0f && bar_data->value == 75.0f,
	  "progress bar keeps full range"
	);
	EXPECT(
	  ring_data && ring_data->min_value == 25.0f && ring_data->max_value == 125.0f && ring_data->value == 75.0f,
	  "progress ring keeps full range"
	);
	float value;
	float min;
	float max;
	EXPECT(
	  xent_sem_value(flux_app_ctx(app), bar_node, &value, &min, &max)
		&& value == 75.0f
		&& min == 25.0f
		&& max == 125.0f,
	  "progress semantics keep full range"
	);

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup, "progress cleanup batch");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) bar_node)), "progress bar cleanup");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) ring_node)), "progress ring cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "progress cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_text_inputs_bind_app(XtkHost *host, FluxApp *app) {
	XtkCtrlId const types [] = {XTK_CTRL_TEXTBOX, XTK_CTRL_PASSWORD, XTK_CTRL_NUMBER};
	XtkBatch       *batch    = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "text input batch");
	for (uint32_t i = 0; i < sizeof(types) / sizeof(types [0]); i++)
		EXPECT(
		  xtk_batch_create(batch, types [i], xtk_ref_none(), xtk_ref_none(), NULL, 0u) != XTK_LOCAL_INVALID,
		  "text input create"
		);

	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 3u, "text input commit");
	for (uint32_t i = 0; i < result->control_count; i++) {
		XentNodeId            node = ( XentNodeId ) result->controls [i].control;
		FluxNodeData         *nd   = flux_store_find(flux_app_store(app), node);
		FluxTextBoxInputData *tb   = nd ? ( FluxTextBoxInputData * ) nd->component_data : NULL;
		EXPECT(tb && tb->app == app, "declarative text input keeps app services");
	}

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup, "text input cleanup batch");
	for (uint32_t i = 0; i < result->control_count; i++)
		EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(result->controls [i].control)), "text input cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "text input cleanup commit");

	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_hyperlink_projects_url(XtkHost *host, FluxApp *app) {
	XtkPropPatch props [] = {
	  {
       .id    = XTK_PROP_TEXT,
       .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Project", .len = 7u}},
	   },
	  {
       .id    = XTK_PROP_URL,
       .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "https://xent.dev", .len = 16u}},
	   },
	};
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "hyperlink batch");
	XtkLocalId link = xtk_batch_create(batch, XTK_CTRL_LINK, xtk_ref_none(), xtk_ref_none(), props, 2u);
	EXPECT(link != XTK_LOCAL_INVALID, "hyperlink create");
	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 1u, "hyperlink commit");
	XentNodeId         node = ( XentNodeId ) mapped_control(result, link);
	FluxNodeData      *nd   = flux_store_find(flux_app_store(app), node);
	FluxHyperlinkData *data = nd ? nd->component_data : NULL;
	EXPECT(data && !strcmp(data->label, "Project"), "hyperlink label projection");
	EXPECT(data->url && !strcmp(data->url, "https://xent.dev"), "hyperlink url projection");

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup && xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) node)), "hyperlink cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "hyperlink cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_suggest_projects_to_inner_textbox(XtkHost *host, FluxApp *app) {
	static char const items [] = "Amsterdam\0Athens\0";
	XtkPropPatch      props [] = {
	  {
       .id    = XTK_PROP_PLACEHOLDER,
       .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Search cities", .len = 13u}},
	   },
	  {
       .id    = XTK_PROP_TEXT,
       .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Am", .len = 2u}},
	   },
	  {
       .id = XTK_PROP_ITEMS,
       .value
		= {.kind = XTK_VALUE_BYTES, .u.bytes = {.data = ( uint8_t * ) ( uintptr_t ) items, .len = sizeof(items) - 1u}},
	   },
	  {
       .id    = XTK_PROP_ICON,
       .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Find", .len = 4u}},
	   },
	  {
       .id    = XTK_PROP_ENABLED,
       .value = {.kind = XTK_VALUE_BOOL, .u.b = true},
	   },
	};
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "suggest batch");
	EXPECT(
	  xtk_batch_create(
		batch, XTK_CTRL_SUGGEST, xtk_ref_none(), xtk_ref_none(), props, ( uint32_t ) (sizeof(props) / sizeof(props [0]))
	  ) != XTK_LOCAL_INVALID,
	  "suggest create"
	);

	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 1u, "suggest commit");
	XentNodeId root    = ( XentNodeId ) result->controls [0].control;
	XentNodeId textbox = XENT_NODE_INVALID;
	XentNodeId button  = XENT_NODE_INVALID;
	for (XentNodeId child = xent_node_first(flux_app_ctx(app), root); child != XENT_NODE_INVALID;
	  child               = xent_node_next(flux_app_ctx(app), child))
	{
		FluxNodeData *nd = flux_store_find(flux_app_store(app), child);
		if (!nd) continue;
		if (nd->component_type == FLUX_CONTROL_TEXT_INPUT) textbox = child;
		if (nd->component_type == FLUX_CONTROL_BUTTON) button = child;
	}
	FluxNodeData    *textbox_node = flux_store_find(flux_app_store(app), textbox);
	FluxTextBoxData *textbox_data = textbox_node ? textbox_node->component_data : NULL;
	EXPECT(textbox_data && strcmp(textbox_data->placeholder, "Search cities") == 0, "suggest placeholder");
	EXPECT(strcmp(textbox_data->content, "Am") == 0, "suggest content");
	EXPECT(button != XENT_NODE_INVALID, "suggest query button");

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup, "suggest cleanup batch");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(result->controls [0].control)), "suggest cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "suggest cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static void drop_combo_on_select(void *ctx, int index) {
	DropComboProbe *probe = ( DropComboProbe * ) ctx;
	probe->selected       = index;
	flux_tree_drop(probe->store, probe->node);
}

static void combo_selected(void *ctx, int index) {
	ComboProbe *probe = ctx;
	probe->calls++;
	probe->selected = index;
}

static void drop_nav_on_select(void *ctx, int index) {
	( void ) index;
	DropNavProbe *probe = ctx;
	flux_tree_drop(probe->store, probe->node);
}

static int test_nav_callback_can_drop_nav(XtkHost *host, FluxApp *app) {
	DropNavProbe probe = {.store = flux_app_store(app)};
	probe.node         = flux_nav_init(&(FluxNavViewDesc) {
	  .ctx                  = flux_app_ctx(app),
	  .store                = probe.store,
	  .parent               = flux_app_root(app),
	  .window               = flux_app_window(app),
	  .text                 = flux_app_text(app),
	  .theme                = flux_app_theme(app),
	  .width                = 800.0f,
	  .height               = 600.0f,
	  .on_selection_changed = drop_nav_on_select,
	  .userdata             = &probe,
	});
	EXPECT(probe.node != XENT_NODE_INVALID, "drop nav");
	EXPECT(flux_nav_add_item(probe.store, probe.node, NULL, "One") == 0, "drop nav first item");
	EXPECT(flux_nav_add_item(probe.store, probe.node, NULL, "Two") == 1, "drop nav second item");
	FluxNodeData    *nd   = flux_store_find(probe.store, probe.node);
	FluxNavViewData *data = nd ? nd->component_data : NULL;
	EXPECT(data && data->count == 2, "drop nav runtime");
	flux_input_route_activation(host, probe.store, data->items [1].node, XTK_EVENT_CAUSE_USER);
	EXPECT(!xent_node_valid(flux_app_ctx(app), probe.node), "nav callback may destroy its source");
	return 0;
}

static int test_combo_keyboard_contract(FluxApp *app) {
	static char const *const items [] = {"A", "B", "C"};
	ComboProbe               probe    = {.selected = -1};
	XentNodeId               combo    = flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = flux_app_ctx(app),
	  .store          = flux_app_store(app),
	  .parent         = flux_app_root(app),
	  .window         = flux_app_window(app),
	  .text           = flux_app_text(app),
	  .theme          = flux_app_theme(app),
	  .items          = items,
	  .item_count     = 3,
	  .selected_index = 0,
	  .on_select      = combo_selected,
	  .userdata       = &probe,
	});
	EXPECT(combo != XENT_NODE_INVALID, "keyboard combo");
	FluxNodeData     *node = flux_store_find(flux_app_store(app), combo);
	FluxComboBoxData *data = node ? node->component_data : NULL;
	EXPECT(data && node->behavior.on_key, "keyboard combo behavior");

	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_ENTER, true), "combo opens");
	EXPECT(data->open, "combo open state");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_DOWN, true), "combo highlight moves");
	EXPECT(data->selected_index == 0 && probe.calls == 0, "open navigation previews only");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_ESCAPE, true), "combo escape");
	EXPECT(!data->open && data->selected_index == 0, "escape cancels preview");

	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_ENTER, true), "combo reopens");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_DOWN, true), "combo second highlight");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_ENTER, true), "combo commits");
	EXPECT(!data->open && data->selected_index == 1, "combo commit closes");
	EXPECT(probe.calls == 1 && probe.selected == 1, "combo commit notifies once");
	flux_tree_drop(flux_app_store(app), combo);
	return 0;
}

static void drop_menu_on_click(void *ctx) {
	DropMenuProbe *probe = ctx;
	probe->clicks++;
	flux_menu_destroy(probe->menu);
	probe->menu = NULL;
}

static int test_menu_callback_can_destroy_menu(FluxApp *app) {
	DropMenuProbe probe = {.menu = flux_menu_create(flux_app_window(app))};
	EXPECT(probe.menu, "drop menu");
	EXPECT(
	  flux_menu_additem(
		probe.menu,
		&(FluxMenuItemDef) {
		  .type         = FLUX_MENU_ITEM_NORMAL,
		  .label        = "Drop",
		  .enabled      = true,
		  .on_click     = drop_menu_on_click,
		  .on_click_ctx = &probe,
		}
	  ) == 0,
	  "drop menu item"
	);
	flux_menu_showinput(
	  probe.menu, (FluxRect) {0.0f, 0.0f, 20.0f, 20.0f}, FLUX_PLACEMENT_BOTTOM, FLUX_MENU_INPUT_KEYBOARD
	);
	EXPECT(flux_menu_onkey(probe.menu, FLUX_KEY_DOWN), "drop menu selection");
	EXPECT(flux_menu_onkey(probe.menu, FLUX_KEY_ENTER), "drop menu invoke");
	EXPECT(probe.clicks == 1 && !probe.menu, "menu callback may destroy menu");
	EXPECT(!flux_app_menu(app), "destroyed menu clears active menu");
	return 0;
}

static int test_combo_callback_can_drop_node(FluxApp *app) {
	static char const *const items [] = {"A", "B"};
	DropComboProbe           probe    = {.store = flux_app_store(app), .selected = -1};
	probe.node                        = flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = flux_app_ctx(app),
	  .store          = probe.store,
	  .parent         = flux_app_root(app),
	  .window         = flux_app_window(app),
	  .text           = flux_app_text(app),
	  .theme          = flux_app_theme(app),
	  .items          = items,
	  .item_count     = 2,
	  .selected_index = -1,
	  .on_select      = drop_combo_on_select,
	  .userdata       = &probe,
	});
	EXPECT(probe.node != XENT_NODE_INVALID, "drop combo");
	FluxNodeData *node = flux_store_find(probe.store, probe.node);
	EXPECT(node && node->behavior.on_key, "drop combo behavior");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_DOWN, true), "drop combo key");
	EXPECT(probe.selected == 0, "drop combo selection");
	EXPECT(!xent_node_valid(flux_app_ctx(app), probe.node), "combo callback may drop itself");
	return 0;
}

static int test_combo_model(XtkHost *host, FluxApp *app) {
	static uint8_t const items []       = {'B', 'l', 'u', 'e', 0, 'G', 'r', 'e', 'e', 'n', 0};
	XtkPropPatch         combo_props [] = {
	  {.id = XTK_PROP_PLACEHOLDER,    .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Pick", .len = 4u}}},
	  {.id = XTK_PROP_SELECTED_INDEX, .value = {.kind = XTK_VALUE_I32, .u.i32 = 1}                                },
	  {.id     = XTK_PROP_ITEMS,
       .value = {.kind = XTK_VALUE_BYTES, .u.bytes = {.data = ( uint8_t * ) items, .len = sizeof(items)}}         },
	};
	XtkBatch  *batch = xtk_batch_alloc(host->vt->revision(host));
	XtkLocalId combo = xtk_batch_create(batch, XTK_CTRL_COMBO, xtk_ref_none(), xtk_ref_none(), combo_props, 3u);
	EXPECT(combo != XTK_LOCAL_INVALID, "combo create");
	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 1u, "combo commit");
	XentNodeId        id   = ( XentNodeId ) result->controls [0].control;
	FluxNodeData     *node = flux_store_find(flux_app_store(app), id);
	FluxComboBoxData *data = node ? ( FluxComboBoxData * ) node->component_data : NULL;
	EXPECT(data && data->item_count == 2, "combo items projected");
	EXPECT(strcmp(data->items [0], "Blue") == 0 && strcmp(data->items [1], "Green") == 0, "combo item labels");
	EXPECT(data->selected_index == 1 && strcmp(data->placeholder, "Pick") == 0, "combo state projected");

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup && xtk_batch_drop(cleanup, xtk_ref_existing(result->controls [0].control)), "combo cleanup");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "combo cleanup commit");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_mount_materializes_initial_state(XtkHost *host, FluxApp *app) {
	static uint8_t const tab_items [] = {
	  0,
	  'O',
	  'n',
	  'e',
	  0,
	  0,
	  'T',
	  'w',
	  'o',
	  0,
	};
	XtkPropPatch expander_props [] = {
	  {.id = XTK_PROP_TEXT,           .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Details", .len = 7u}}},
	  {.id = XTK_PROP_EXPANDED,       .value = {.kind = XTK_VALUE_BOOL, .u.b = true}                                 },
	  {.id = XTK_PROP_CONTENT_HEIGHT, .value = {.kind = XTK_VALUE_F32, .u.f32 = 80.0f}                               },
	};
	XtkPropPatch tab_props [] = {
	  {.id = XTK_PROP_ITEMS,
       .value
		= {.kind = XTK_VALUE_BYTES, .u.bytes = {.data = ( uint8_t * ) ( uintptr_t ) tab_items, .len = sizeof(tab_items)}}},
	  {.id = XTK_PROP_SELECTED_INDEX, .value = {.kind = XTK_VALUE_I32, .u.i32 = 1}                                       },
	  {.id = XTK_PROP_ADD_VISIBLE,    .value = {.kind = XTK_VALUE_BOOL, .u.b = true}                                     },
	};
	XtkPropPatch split_props [] = {
	  {.id = XTK_PROP_OPEN,             .value = {.kind = XTK_VALUE_BOOL, .u.b = true}   },
	  {.id = XTK_PROP_OPEN_PANE_LENGTH, .value = {.kind = XTK_VALUE_F32, .u.f32 = 240.0f}},
	};
	XtkPropPatch title_props [] = {
	  {.id = XTK_PROP_TEXT,      .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "Window", .len = 6u}}},
	  {.id = XTK_PROP_SHOW_BACK, .value = {.kind = XTK_VALUE_BOOL, .u.b = true}                                },
	};
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(batch, "mount-state batch");
	XtkLocalId expander = xtk_batch_create(
	  batch, XTK_CTRL_EXPANDER, xtk_ref_none(), xtk_ref_none(), expander_props,
	  ( uint32_t ) (sizeof(expander_props) / sizeof(expander_props [0]))
	);
	XtkLocalId tabs = xtk_batch_create(
	  batch, XTK_CTRL_TABS, xtk_ref_none(), xtk_ref_none(), tab_props,
	  ( uint32_t ) (sizeof(tab_props) / sizeof(tab_props [0]))
	);
	XtkLocalId split = xtk_batch_create(
	  batch, XTK_CTRL_SPLITVIEW, xtk_ref_none(), xtk_ref_none(), split_props,
	  ( uint32_t ) (sizeof(split_props) / sizeof(split_props [0]))
	);
	XtkLocalId title = xtk_batch_create(
	  batch, XTK_CTRL_TITLEBAR, xtk_ref_none(), xtk_ref_none(), title_props,
	  ( uint32_t ) (sizeof(title_props) / sizeof(title_props [0]))
	);
	XtkLocalId avatar = xtk_batch_create(batch, XTK_CTRL_AVATAR, xtk_ref_none(), xtk_ref_none(), NULL, 0u);
	EXPECT(
	  expander != XTK_LOCAL_INVALID
		&& tabs != XTK_LOCAL_INVALID
		&& split != XTK_LOCAL_INVALID
		&& title != XTK_LOCAL_INVALID
		&& avatar != XTK_LOCAL_INVALID,
	  "mount-state creates"
	);

	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 5u, "mount-state commit");
	XentNodeId        expander_node = ( XentNodeId ) mapped_control(result, expander);
	XentNodeId        tabs_node     = ( XentNodeId ) mapped_control(result, tabs);
	XentNodeId        split_node    = ( XentNodeId ) mapped_control(result, split);
	XentNodeId        title_node    = ( XentNodeId ) mapped_control(result, title);
	XentNodeId        avatar_node   = ( XentNodeId ) mapped_control(result, avatar);
	FluxNodeData     *expander_nd   = flux_store_find(flux_app_store(app), expander_node);
	FluxExpanderData *expander_data = expander_nd ? expander_nd->component_data : NULL;
	EXPECT(expander_data && expander_data->expanded, "expander mounts expanded");
	EXPECT(!expander_data->anim_active, "expander mount does not animate from defaults");
	EXPECT(
	  xent_node_parent(flux_app_ctx(app), expander_data->content) == expander_node,
	  "expander content is attached during construction"
	);

	FluxNodeData    *tabs_nd   = flux_store_find(flux_app_store(app), tabs_node);
	FluxTabViewData *tabs_data = tabs_nd ? tabs_nd->component_data : NULL;
	EXPECT(tabs_data && tabs_data->count == 2 && tabs_data->selected == 1, "tabs mount complete model");
	EXPECT(tabs_data->add_visible && tabs_data->add_node != XENT_NODE_INVALID, "tabs mount add button");
	EXPECT(!tabs_data->anim_active, "tabs mount does not animate from an empty strip");
	for (int i = 0; i < tabs_data->count; i++) EXPECT(!tabs_data->tabs [i].w_anim, "tabs mount widths are settled");

	FluxNodeData          *split_nd    = flux_store_find(flux_app_store(app), split_node);
	FluxNodeData          *title_nd    = flux_store_find(flux_app_store(app), title_node);
	FluxNodeData          *avatar_nd   = flux_store_find(flux_app_store(app), avatar_node);
	FluxSplitViewData     *split_data  = split_nd ? split_nd->component_data : NULL;
	FluxTitleBarData      *title_data  = title_nd ? title_nd->component_data : NULL;
	FluxPersonPictureData *avatar_data = avatar_nd ? avatar_nd->component_data : NULL;
	EXPECT(split_data && split_data->node == split_node && split_data->pane_open, "split view retargets live owner");
	EXPECT(title_data && title_data->node == title_node && title_data->show_back, "title bar retargets live owner");
	EXPECT(avatar_data && avatar_data->node == avatar_node, "person picture retargets live owner");

	XtkBatch *cleanup = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(cleanup, "mount-state cleanup batch");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) expander_node)), "drop mounted expander");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) tabs_node)), "drop mounted tabs");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) split_node)), "drop mounted split view");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) title_node)), "drop mounted title bar");
	EXPECT(xtk_batch_drop(cleanup, xtk_ref_existing(( XtkCtrl ) avatar_node)), "drop mounted avatar");
	XtkBatchResult *cleanup_result = commit_ok(host, cleanup);
	EXPECT(cleanup_result, "mount-state cleanup");
	xtk_batch_result_destroy(cleanup_result);
	xtk_batch_free(cleanup);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

static int test_nav_model(XtkHost *host, FluxApp *app) {
	XtkNavItemDesc items [] = {
	  {XTK_NAV_ITEM,   "Home",     "Home"    },
	  {XTK_NAV_CHILD,  NULL,       "Child"   },
	  {XTK_NAV_FOOTER, "Settings", "Settings"},
	};
	XtkNavDesc desc = {.items = items, .nitem = 3, .sel = 0, .window_title_bar = true, .app_title = "Host Nav"};
	uint32_t   size = xtk_nav_model_size(&desc);
	uint8_t    model [256];
	EXPECT(size <= sizeof(model) && xtk_nav_model_write(&desc, model, size), "nav model encode");
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_NAV_MODEL,      .value = {.kind = XTK_VALUE_BYTES, .u.bytes = {.data = model, .len = size}}},
	  {.id = XTK_PROP_SELECTED_INDEX, .value = {.kind = XTK_VALUE_I32, .u.i32 = 0}                               },
	};
	XtkBatch  *batch = xtk_batch_alloc(host->vt->revision(host));
	XtkLocalId local = xtk_batch_create(batch, XTK_CTRL_NAV, xtk_ref_none(), xtk_ref_none(), props, 2u);
	EXPECT(local != XTK_LOCAL_INVALID, "nav create op");
	EXPECT(
	  xtk_batch_on(batch, xtk_ref_local(local), XTK_EVENT_SELECT, XTK_EVENT_PHASE_MASK_TARGET, on_nav_select, NULL)
		!= XTK_SUB_TOKEN_INVALID,
	  "nav select subscription"
	);
	XtkBatchResult *result = commit_ok(host, batch);
	EXPECT(result && result->control_count == 1u, "nav commit");
	XentNodeId nav = ( XentNodeId ) result->controls [0].control;
	EXPECT(strcmp(flux_nav_item_label(flux_app_store(app), nav, 0), "Home") == 0, "nav menu item");
	EXPECT(strcmp(flux_nav_item_label(flux_app_store(app), nav, 1), "Child") == 0, "nav child item");
	EXPECT(strcmp(flux_nav_item_label(flux_app_store(app), nav, 2), "Settings") == 0, "nav footer item");
	EXPECT(xent_layout(flux_app_ctx(app), flux_app_root(app), 800.0f, 600.0f), "nav layout");
	FluxNavViewData *data = ( FluxNavViewData * ) flux_store_find(flux_app_store(app), nav)->component_data;
	XentRect         rect;
	EXPECT(data && data->count == 3, "nav retained model");
	EXPECT(
	  xent_layout_rect(flux_app_ctx(app), data->items [0].node, &rect)
		&& rect.w > 0.0f
		&& rect.h > 0.0f
		&& rect.y < 100.0f,
	  "nav item starts inside the menu viewport"
	);
	XtkBatch *repeat = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(repeat, "repeat nav batch");
	EXPECT(
	  xtk_batch_set(
		repeat, xtk_ref_existing(( XtkCtrl ) nav), XTK_PROP_NAV_MODEL,
		(XtkValue) {
		  .kind = XTK_VALUE_BYTES, .u.bytes = {.data = model, .len = size}
    },
		(XtkChangeStamp) {0}
	  ),
	  "repeat nav model op"
	);
	XtkBatchResult *repeat_result = commit_ok(host, repeat);
	EXPECT(repeat_result && data->count == 3, "nav model replaces instead of appending");
	EXPECT(strcmp(flux_nav_item_label(flux_app_store(app), nav, 1), "Child") == 0, "nav replace labels");
	xtk_batch_result_destroy(repeat_result);
	xtk_batch_free(repeat);
	g_nav_selection = -1;
	flux_input_route_activation(host, flux_app_store(app), data->items [2].node, XTK_EVENT_CAUSE_USER);
	EXPECT(g_nav_selection == 2, "nav activation routes selected index");
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return 0;
}

int main(void) {
	XtkHost *host = flux_host_create();
	EXPECT(host, "host");

	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	XtkValue  text  = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "Go", .len = 2u}
    };
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_TEXT, .value = text, .change = {0}}
    };
	XtkLocalId button = xtk_batch_create(batch, XTK_CTRL_BUTTON, xtk_ref_none(), xtk_ref_none(), props, 1u);
	XtkSubRef  token
	  = xtk_batch_on(batch, xtk_ref_local(button), XTK_EVENT_INVOKE, XTK_EVENT_PHASE_MASK_TARGET, on_invoke, NULL);
	EXPECT(button != XTK_LOCAL_INVALID && token != XTK_SUB_TOKEN_INVALID, "ops");

	XtkBatchResult *result = commit_ok(host, batch);
	if (!result) return 1;
	EXPECT(result->control_count == 1u, "mapping");
	XtkCtrl btn = result->controls [0].control;
	EXPECT(host->vt->live(host, btn), "live");
	EXPECT(host->vt->type(host, btn) == XTK_CTRL_BUTTON, "type");

	XtkValueView view;
	EXPECT(host->vt->prop(host, btn, XTK_PROP_TEXT, &view, NULL), "get text");
	EXPECT(view.kind == XTK_VALUE_STRING && view.u.string.len == 2u, "text");

	g_clicks = 0u;
	XtkEvent event
	  = {.id = XTK_EVENT_INVOKE, .target = btn, .current_target = btn, .source = btn, .phase = XTK_EVENT_PHASE_TARGET};
	EXPECT(flux_host_route(host, &event) & XTK_EVENT_HANDLED, "routed");
	EXPECT(g_clicks == 1u, "click");

	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);

	FluxAppCfg cfg = {0};
	FluxApp   *app = flux_app_create(&cfg);
	EXPECT(app, "app");
	FluxBackendCtx bctx    = {.ctx = flux_app_ctx(app), .store = flux_app_store(app), .app = app};
	XtkHost       *adopted = flux_host_adopt(bctx.ctx, bctx.store, flux_app_root(app));
	EXPECT(adopted, "adopt");
	XtkBackend *backend = flux_xtk_backend_create(&bctx, adopted);
	EXPECT(backend && backend->vt->host(backend) == adopted, "backend host");
	flux_app_sethost(app, adopted, backend);
	if (test_text_inputs_bind_app(adopted, app)) return 1;
	if (test_hyperlink_projects_url(adopted, app)) return 1;
	if (test_suggest_projects_to_inner_textbox(adopted, app)) return 1;
	if (test_combo_keyboard_contract(app)) return 1;
	if (test_combo_callback_can_drop_node(app)) return 1;
	if (test_nav_callback_can_drop_nav(adopted, app)) return 1;
	if (test_menu_callback_can_destroy_menu(app)) return 1;
	if (test_retained_menu(adopted, app)) return 1;
	if (test_combo_model(adopted, app)) return 1;
	if (test_mount_materializes_initial_state(adopted, app)) return 1;
	if (test_progress_range(adopted, app)) return 1;
	if (test_nav_model(adopted, app)) return 1;
	flux_host_destroy(adopted);
	flux_app_destroy(app);

	flux_host_destroy(host);
	printf("PASS: flux host batch / route\n");
	return 0;
}
