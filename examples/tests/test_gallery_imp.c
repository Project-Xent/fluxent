#include "app.h"
#include "render/flux_render_snapshot.h"
#include "store/flux_node_data.h"
#include "window/flux_window_internal.h"

#include <controls/data/flux_button_data.h>
#include <controls/data/flux_combo_box_data.h>
#include <controls/data/flux_info_badge_data.h>
#include <controls/data/flux_list_view_data.h>
#include <controls/data/flux_slider_data.h>
#include <controls/data/flux_textbox_data.h>
#include <controls/behavior/flux_expander_internal.h>
#include <controls/draw/flux_control_draw.h>
#include <host/flux_prop_apply.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

typedef struct PageEvents {
	int const *tags;
	int        count;
} PageEvents;

typedef struct RetainedCase {
	int    page;
	XtkMsg msg;
} RetainedCase;

#define EVENT_SET(name, ...) static int const name [] = {__VA_ARGS__}
#define EVENT_ROW(name)      {name, ( int ) (sizeof(name) / sizeof(name [0]))}

EVENT_SET(button_events, MSG_CLICK, MSG_BUTTON_DISABLE);
EVENT_SET(split_events, MSG_SPLIT_PRIMARY, MSG_SPLIT_MENU);
EVENT_SET(checkbox_events, MSG_CHECK_A, MSG_CHECK_B);
EVENT_SET(radio_events, MSG_RADIO_0, MSG_RADIO_1, MSG_RADIO_2);
EVENT_SET(combo_events, MSG_COMBO, MSG_COMBO_FONT);
EVENT_SET(slider_events, MSG_SLIDER_BASIC, MSG_SLIDER_STEP, MSG_SLIDER_TICK);
EVENT_SET(toggle_events, MSG_LIGHT);
EVENT_SET(radio_buttons_events, MSG_RADIO_BUTTONS);
EVENT_SET(rating_events, MSG_RATING);
EVENT_SET(toggle_split_events, MSG_TOGGLE_SPLIT, MSG_TOGGLE_SPLIT_CLICK, MSG_TOGGLE_SPLIT_MENU);
EVENT_SET(textbox_events, MSG_NAME, MSG_PASSWORD);
EVENT_SET(auto_suggest_events, MSG_ASB_TEXT, MSG_ASB_QUERY, MSG_ASB_CHOSEN);
EVENT_SET(numberbox_events, MSG_QUANTITY);
EVENT_SET(info_bar_events, MSG_BAR_CLOSE, MSG_BARS_RESET);
EVENT_SET(badge_events, MSG_BADGE_DELTA);
EVENT_SET(progress_events, MSG_PROGRESS, MSG_INDETERMINATE);
EVENT_SET(person_events, MSG_PP_BADGE, MSG_PP_GROUP);
EVENT_SET(flip_events, MSG_FLIP_SELECT);
EVENT_SET(pager_events, MSG_PAGER_SELECT);
EVENT_SET(menu_events, MSG_MENU, MSG_REPEAT);
EVENT_SET(tab_events, MSG_TAB_SELECT, MSG_TAB_CLOSE, MSG_TAB_ADD);
EVENT_SET(expander_events, MSG_EXPAND, MSG_CHECK_A);
EVENT_SET(breadcrumb_events, MSG_BREADCRUMB);
EVENT_SET(selector_events, MSG_SELECTOR_BAR);
EVENT_SET(list_events, MSG_LIST_SELECT, MSG_MULTI_SELECT);
EVENT_SET(grid_events, MSG_GRID_SELECT);
EVENT_SET(listbox_events, MSG_LISTBOX_SELECT);
EVENT_SET(tree_events, MSG_TREE_SELECT, MSG_TREE_INVOKE, MSG_TREE_EXPAND);
EVENT_SET(items_events, MSG_ITEMS_INVOKE);
EVENT_SET(refresh_events, MSG_REFRESH, MSG_REFRESH_DONE);
EVENT_SET(split_view_events, MSG_SPLIT_TOGGLE, MSG_SPLIT_MODE, MSG_SPLIT_PLACE);
EVENT_SET(title_bar_events, MSG_TB_BACK, MSG_TB_PANE);
EVENT_SET(dialog_events, MSG_DIALOG_OPEN, MSG_DIALOG_RESULT);
EVENT_SET(tip_events, MSG_TIP_OPEN, MSG_TIP_ACTION, MSG_TIP_CLOSE);

static PageEvents const expected_events [PAGE_COUNT] = {
  [PAGE_BUTTON]         = EVENT_ROW(button_events),
  [PAGE_SPLIT]          = EVENT_ROW(split_events),
  [PAGE_CHECKBOX]       = EVENT_ROW(checkbox_events),
  [PAGE_RADIO]          = EVENT_ROW(radio_events),
  [PAGE_COMBO]          = EVENT_ROW(combo_events),
  [PAGE_SLIDER]         = EVENT_ROW(slider_events),
  [PAGE_TOGGLE]         = EVENT_ROW(toggle_events),
  [PAGE_RADIO_BUTTONS]  = EVENT_ROW(radio_buttons_events),
  [PAGE_RATING]         = EVENT_ROW(rating_events),
  [PAGE_TOGGLE_SPLIT]   = EVENT_ROW(toggle_split_events),
  [PAGE_TEXTBOX]        = EVENT_ROW(textbox_events),
  [PAGE_AUTOSUGGEST]    = EVENT_ROW(auto_suggest_events),
  [PAGE_NUMBERBOX]      = EVENT_ROW(numberbox_events),
  [PAGE_INFOBAR]        = EVENT_ROW(info_bar_events),
  [PAGE_BADGE]          = EVENT_ROW(badge_events),
  [PAGE_PROGRESS]       = EVENT_ROW(progress_events),
  [PAGE_PERSON_PICTURE] = EVENT_ROW(person_events),
  [PAGE_FLIPVIEW]       = EVENT_ROW(flip_events),
  [PAGE_PAGER]          = EVENT_ROW(pager_events),
  [PAGE_MENUS]          = EVENT_ROW(menu_events),
  [PAGE_TABVIEW]        = EVENT_ROW(tab_events),
  [PAGE_EXPANDER]       = EVENT_ROW(expander_events),
  [PAGE_BREADCRUMB]     = EVENT_ROW(breadcrumb_events),
  [PAGE_SELECTOR_BAR]   = EVENT_ROW(selector_events),
  [PAGE_LISTVIEW]       = EVENT_ROW(list_events),
  [PAGE_GRIDVIEW]       = EVENT_ROW(grid_events),
  [PAGE_LISTBOX]        = EVENT_ROW(listbox_events),
  [PAGE_TREE_VIEW]      = EVENT_ROW(tree_events),
  [PAGE_ITEMS_VIEW]     = EVENT_ROW(items_events),
  [PAGE_PULL_REFRESH]   = EVENT_ROW(refresh_events),
  [PAGE_SPLIT_VIEW]     = EVENT_ROW(split_view_events),
  [PAGE_TITLE_BAR]      = EVENT_ROW(title_bar_events),
  [PAGE_DIALOG]         = EVENT_ROW(dialog_events),
  [PAGE_TEACHING_TIP]   = EVENT_ROW(tip_events),
};

static FluxControlType const featured [PAGE_COUNT] = {
  [PAGE_HOME]           = FLUX_CONTROL_INFO_BAR,
  [PAGE_BUTTON]         = FLUX_CONTROL_BUTTON,
  [PAGE_SPLIT]          = FLUX_CONTROL_SPLIT_BUTTON,
  [PAGE_CHECKBOX]       = FLUX_CONTROL_CHECKBOX,
  [PAGE_RADIO]          = FLUX_CONTROL_RADIO,
  [PAGE_COMBO]          = FLUX_CONTROL_COMBO_BOX,
  [PAGE_SLIDER]         = FLUX_CONTROL_SLIDER,
  [PAGE_TOGGLE]         = FLUX_CONTROL_SWITCH,
  [PAGE_RADIO_BUTTONS]  = FLUX_CONTROL_RADIO_BUTTONS,
  [PAGE_RATING]         = FLUX_CONTROL_RATING,
  [PAGE_TOGGLE_SPLIT]   = FLUX_CONTROL_TOGGLE_SPLIT_BUTTON,
  [PAGE_TEXTBOX]        = FLUX_CONTROL_TEXT_INPUT,
  [PAGE_AUTOSUGGEST]    = FLUX_CONTROL_AUTO_SUGGEST,
  [PAGE_NUMBERBOX]      = FLUX_CONTROL_NUMBER_BOX,
  [PAGE_TYPOGRAPHY]     = FLUX_CONTROL_TEXT,
  [PAGE_INFOBAR]        = FLUX_CONTROL_INFO_BAR,
  [PAGE_BADGE]          = FLUX_CONTROL_INFO_BADGE,
  [PAGE_PROGRESS]       = FLUX_CONTROL_PROGRESS,
  [PAGE_TOOLTIP]        = FLUX_CONTROL_BUTTON,
  [PAGE_IMAGE]          = FLUX_CONTROL_IMAGE,
  [PAGE_PERSON_PICTURE] = FLUX_CONTROL_PERSON_PICTURE,
  [PAGE_FLIPVIEW]       = FLUX_CONTROL_FLIP_VIEW,
  [PAGE_PAGER]          = FLUX_CONTROL_PAGER,
  [PAGE_MENUS]          = FLUX_CONTROL_MENU_BAR,
  [PAGE_TABVIEW]        = FLUX_CONTROL_TAB_VIEW,
  [PAGE_EXPANDER]       = FLUX_CONTROL_EXPANDER,
  [PAGE_BREADCRUMB]     = FLUX_CONTROL_BREADCRUMB_BAR,
  [PAGE_SELECTOR_BAR]   = FLUX_CONTROL_SELECTOR_BAR,
  [PAGE_LISTVIEW]       = FLUX_CONTROL_LIST,
  [PAGE_GRIDVIEW]       = FLUX_CONTROL_GRID_VIEW,
  [PAGE_LISTBOX]        = FLUX_CONTROL_LIST_BOX,
  [PAGE_TREE_VIEW]      = FLUX_CONTROL_TREE_VIEW,
  [PAGE_ITEMS_VIEW]     = FLUX_CONTROL_ITEMS_VIEW,
  [PAGE_PULL_REFRESH]   = FLUX_CONTROL_REFRESH,
  [PAGE_SPLIT_VIEW]     = FLUX_CONTROL_SPLIT_VIEW,
  [PAGE_TITLE_BAR]      = FLUX_CONTROL_TITLE_BAR,
  [PAGE_DIALOG]         = FLUX_CONTROL_CONTENT_DIALOG,
  [PAGE_TEACHING_TIP]   = FLUX_CONTROL_TEACHING_TIP,
  [PAGE_SETTINGS]       = FLUX_CONTROL_HYPERLINK,
};

static void mark_events(PageEvents expected, bool tags [MSG_COUNT]) {
	for (int i = 0; i < expected.count; i++) tags [expected.tags [i]] = true;
}

static bool check_events(GalleryApp const *g, int page) {
	bool want [MSG_COUNT] = {0};
	bool got [MSG_COUNT]  = {0};
	mark_events(expected_events [page], want);
	for (int i = 0; i < g->bind_n; i++) {
		int tag = gallery_bind_tag(g, i);
		if (tag <= MSG_NONE || tag >= MSG_COUNT) {
			printf("page %d invalid binding %d at %d/%d\n", page, tag, i, g->bind_n);
			return false;
		}
		got [tag] = true;
	}
	for (int tag = MSG_NONE + 1; tag < MSG_COUNT; tag++) {
		if (want [tag] == got [tag]) continue;
		printf("page %d tag %d: want %d, got %d\n", page, tag, want [tag], got [tag]);
		return false;
	}
	return true;
}

static bool scan_page(GalleryApp *g, XentNodeId node, bool present [XTK_CTRL_TYPE_COUNT], int *node_count) {
	FluxNodeData *data = flux_store_find(gallery_store(g), node);
	if (!data || ( unsigned ) data->component_type >= XTK_CTRL_TYPE_COUNT) {
		printf("invalid node %llu type %u\n", ( unsigned long long ) node, data ? data->component_type : UINT32_MAX);
		return false;
	}
	present [data->component_type] = true;
	(*node_count)++;
	for (XentNodeId child = xent_node_first(gallery_ctx(g), node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(gallery_ctx(g), child))
		if (!scan_page(g, child, present, node_count)) return false;
	return true;
}

static XentNodeId find_type(GalleryApp *g, XentNodeId node, FluxControlType type) {
	FluxNodeData *data = flux_store_find(gallery_store(g), node);
	if (data && data->component_type == type) return node;
	for (XentNodeId child = xent_node_first(gallery_ctx(g), node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(gallery_ctx(g), child))
	{
		XentNodeId found = find_type(g, child, type);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId find_text(GalleryApp *g, XentNodeId node, char const *text) {
	char const *content = xent_node_text(gallery_ctx(g), node);
	if (content && strcmp(content, text) == 0) return node;
	for (XentNodeId child = xent_node_first(gallery_ctx(g), node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(gallery_ctx(g), child))
	{
		XentNodeId found = find_text(g, child, text);
		if (found != XENT_NODE_INVALID) return found;
	}
	return XENT_NODE_INVALID;
}

static XentNodeId control_for_text(GalleryApp *g, char const *text, FluxControlType type) {
	XentNodeId node = find_text(g, g->page_root, text);
	while (node != XENT_NODE_INVALID && flux_node_type(gallery_store(g), node) != type)
		node = xent_node_parent(gallery_ctx(g), node);
	return node;
}

static bool window_layout(GalleryApp *g, float width, float height) {
	FluxWindow *window = flux_app_window(g->flux);
	HWND        hwnd   = flux_app_hwnd(g->flux);
	if (!window || !hwnd || !window->on_render) return false;
	float scale = flux_window_dpi(window).dpi_x / 96.0f;
	if (scale <= 0.0f) return false;
	SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(( int ) (width * scale), ( int ) (height * scale)));
	window->on_render(window->render_ctx);
	return true;
}

static bool card_responsive(GalleryApp *g, XentNodeId control, XentNodeId output, bool single) {
	XentNodeId items        = xent_node_parent(gallery_ctx(g), control);
	XentRect   control_rect = {0};
	XentRect   items_rect   = {0};
	XentRect   output_rect  = {0};
	if (control == XENT_NODE_INVALID
		|| output == XENT_NODE_INVALID
		|| items == XENT_NODE_INVALID
		|| !window_layout(g, 1100.0f, 760.0f))
		return false;
	if (!xent_layout_rect(gallery_ctx(g), control, &control_rect)
		|| !xent_layout_rect(gallery_ctx(g), items, &items_rect)
		|| !xent_layout_rect(gallery_ctx(g), output, &output_rect))
		return false;
	XentRect aligned        = single ? control_rect : items_rect;
	float    control_center = aligned.y + aligned.h * 0.5f;
	float    output_center  = output_rect.y + output_rect.h * 0.5f;
	if (fabsf(control_center - output_center) > 1.0f) return false;
	if (!window_layout(g, 600.0f, 760.0f)) return false;
	if (!xent_layout_rect(gallery_ctx(g), items, &items_rect)
		|| !xent_layout_rect(gallery_ctx(g), output, &output_rect))
		return false;
	bool wrapped = output_rect.y >= items_rect.y + items_rect.h;
	return window_layout(g, 1100.0f, 760.0f) && wrapped;
}

static bool button_row_wraps(GalleryApp *g) {
	XentNodeId first      = control_for_text(g, "Accent style button", FLUX_CONTROL_BUTTON);
	XentNodeId last       = control_for_text(g, "Text style button", FLUX_CONTROL_BUTTON);
	XentRect   first_rect = {0};
	XentRect   last_rect  = {0};
	if (first == XENT_NODE_INVALID
		|| last == XENT_NODE_INVALID
		|| !window_layout(g, 1100.0f, 760.0f)
		|| !xent_layout_rect(gallery_ctx(g), first, &first_rect)
		|| !xent_layout_rect(gallery_ctx(g), last, &last_rect)
		|| fabsf(first_rect.y - last_rect.y) > 1.0f
		|| !window_layout(g, 600.0f, 760.0f)
		|| !xent_layout_rect(gallery_ctx(g), first, &first_rect)
		|| !xent_layout_rect(gallery_ctx(g), last, &last_rect))
		return false;
	bool wrapped = last_rect.y >= first_rect.y + first_rect.h;
	return window_layout(g, 1100.0f, 760.0f) && wrapped;
}

static bool check_slider_ranges(GalleryApp *g, XentNodeId node, int *index) {
	static float const min [] = {0.0f, 500.0f, 0.0f};
	static float const max [] = {100.0f, 1000.0f, 100.0f};
	FluxNodeData      *data   = flux_store_find(gallery_store(g), node);
	if (data && data->component_type == FLUX_CONTROL_SLIDER) {
		if (*index >= 3 || !data->component_data) return false;
		FluxSliderData *slider = ( FluxSliderData * ) data->component_data;
		if (slider->min_value != min [*index] || slider->max_value != max [*index]) return false;
		(*index)++;
	}
	for (XentNodeId child = xent_node_first(gallery_ctx(g), node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(gallery_ctx(g), child))
		if (!check_slider_ranges(g, child, index)) return false;
	return true;
}

static bool scan_button_styles(GalleryApp *g, XentNodeId node, bool present [4], int *count) {
	FluxNodeData *data = flux_store_find(gallery_store(g), node);
	if (data && data->component_type == FLUX_CONTROL_BUTTON) {
		FluxButtonData *button = data->component_data;
		if (!button || ( unsigned ) button->style >= 4u) return false;
		present [button->style] = true;
		(*count)++;
	}
	for (XentNodeId child = xent_node_first(gallery_ctx(g), node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(gallery_ctx(g), child))
		if (!scan_button_styles(g, child, present, count)) return false;
	return true;
}

static bool build_page(GalleryApp *g, int page) {
	g->model.page = page;
	if (page == PAGE_DIALOG) g->model.dialog_open = true;
	gallery_rebuild_page(g);
	if (!xent_node_valid(gallery_ctx(g), g->page_root)) {
		printf("page %d invalid root\n", page);
		return false;
	}
	if (!xent_layout(gallery_ctx(g), flux_app_root(g->flux), 1100.0f, 760.0f)) {
		printf("page %d layout failed\n", page);
		return false;
	}
	bool present [XTK_CTRL_TYPE_COUNT] = {0};
	int  node_count                    = 0;
	if (!scan_page(g, g->page_root, present, &node_count) || node_count < 2) {
		printf("page %d invalid tree (%d nodes)\n", page, node_count);
		return false;
	}
	FluxControlType type = featured [page];
	if (page == PAGE_DIALOG && type && !present [type])
		present [type] = find_type(g, flux_app_overlay(g->flux), type) != XENT_NODE_INVALID;
	if (type && !present [type]) {
		printf("page %d missing featured control %d\n", page, type);
		return false;
	}
	return check_events(g, page);
}

static bool hyperlink_keeps_natural_width(GalleryApp *g) {
	if (!build_page(g, PAGE_MENUS)) return false;
	XentNodeId         link    = find_type(g, g->page_root, FLUX_CONTROL_HYPERLINK);
	XentNodeId         card    = xent_node_parent(gallery_ctx(g), link);
	XentRect           lr      = {0};
	XentRect           cr      = {0};
	XentResolvedInsets padding = {0};
	return link != XENT_NODE_INVALID
	    && card != XENT_NODE_INVALID
	    && xent_layout_rect(gallery_ctx(g), link, &lr)
	    && xent_layout_rect(gallery_ctx(g), card, &cr)
	    && xent_resolved_p(gallery_ctx(g), link, XENT_AXIS_HORIZONTAL, &padding)
	    && lr.w < cr.w
	    && padding.main_start == 11.0f
	    && padding.main_end == 11.0f;
}

static int check_routed_behavior(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_BUTTON), "button page");
	bool button_styles [4] = {0};
	int  button_count      = 0;
	EXPECT(scan_button_styles(g, g->page_root, button_styles, &button_count), "button style data");
	EXPECT(button_count >= 8, "button gallery controls projected");
	EXPECT(
	  button_styles [XTK_BUTTON_STANDARD]
		&& button_styles [XTK_BUTTON_SUBTLE]
		&& button_styles [XTK_BUTTON_TEXT]
		&& button_styles [XTK_BUTTON_ACCENT],
	  "all button styles projected"
	);
	EXPECT(button_row_wraps(g), "button style row wraps only at narrow width");
	XentNodeId button = find_type(g, g->page_root, FLUX_CONTROL_BUTTON);
	EXPECT(button != XENT_NODE_INVALID, "button node");
	EXPECT(
	  card_responsive(g, button, xent_node_parent(gallery_ctx(g), g->live_output [MSG_CLICK]), false),
	  "button card keeps its control group intact while wrapping"
	);
	int clicks = g->model.clicks;
	flux_input_route_activation(g->host, gallery_store(g), button, XTK_EVENT_CAUSE_PROGRAM);
	gallery_flush(g);
	EXPECT(g->model.clicks == clicks + 1, "button event updates model");

	EXPECT(build_page(g, PAGE_SLIDER), "slider page");
	int slider_count = 0;
	EXPECT(check_slider_ranges(g, g->page_root, &slider_count) && slider_count == 3, "slider ranges projected");
	XentNodeId slider = find_type(g, g->page_root, FLUX_CONTROL_SLIDER);
	XtkEvent   event  = {
	  .id             = XTK_EVENT_CHANGE_F32,
	  .target         = ( XtkCtrl ) slider,
	  .source         = ( XtkCtrl ) slider,
	  .current_target = ( XtkCtrl ) slider,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_PROGRAM,
	  .payload        = {.kind = XTK_EVENT_PAYLOAD_F32, .u.f32 = 37.0f},
	};
	EXPECT(flux_host_route(g->host, &event) & XTK_EVENT_HANDLED, "slider event routed");
	gallery_flush(g);
	EXPECT(g->model.slider_basic == 37.0f, "slider event updates model");
	return 0;
}

static int check_combo_retention(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_COMBO), "combo page");
	XentNodeId root  = g->page_root;
	XentNodeId combo = find_type(g, root, FLUX_CONTROL_COMBO_BOX);
	EXPECT(combo != XENT_NODE_INVALID, "combo node");

	FluxNodeData *node = flux_store_find(gallery_store(g), combo);
	EXPECT(node && node->behavior.on_key, "combo keyboard behavior");
	EXPECT(node->behavior.on_key(node->behavior.on_key_ctx, FLUX_KEY_DOWN, true), "combo keyboard selection");
	EXPECT(g->pending_head != NULL, "combo selection queued");
	gallery_flush(g);

	EXPECT(g->page_root == root, "combo event preserves page");
	EXPECT(xent_node_valid(gallery_ctx(g), combo), "combo event preserves control");
	FluxComboBoxData *data = node->component_data;
	EXPECT(data && g->model.combo_sel == data->selected_index, "combo model follows retained control");
	return 0;
}

static int check_textbox_retention(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_TEXTBOX), "textbox page");
	XentNodeId root     = g->page_root;
	XentNodeId box      = find_type(g, root, FLUX_CONTROL_TEXT_INPUT);
	XentNodeId password = find_type(g, root, FLUX_CONTROL_PASSWORD_BOX);
	EXPECT(box != XENT_NODE_INVALID, "textbox node");
	EXPECT(password != XENT_NODE_INVALID, "password node");

	FluxNodeData *node = flux_store_find(gallery_store(g), box);
	EXPECT(node && node->behavior.on_char, "textbox character behavior");
	node->behavior.on_char(node->behavior.on_char_ctx, L'x');
	EXPECT(g->pending_head != NULL, "textbox edit queued");
	gallery_flush(g);

	EXPECT(g->page_root == root, "textbox edit preserves page");
	EXPECT(xent_node_valid(gallery_ctx(g), box), "textbox edit preserves control");
	FluxTextBoxData *data = node->component_data;
	EXPECT(data && !strcmp(data->content, "x"), "textbox retains edited content");
	EXPECT(g->model.name && !strcmp(g->model.name, "x"), "textbox edit updates model");
	EXPECT(!strcmp(xent_node_text(gallery_ctx(g), g->live_output [MSG_NAME]), "Hello, x!"), "textbox output updates");
	FluxNodeData  *output = flux_store_find(gallery_store(g), g->live_output [MSG_NAME]);
	FluxRenderSnap snap;
	flux_snapshot_build(&snap, gallery_ctx(g), g->live_output [MSG_NAME], output);
	EXPECT(
	  snap.u.text.text_content && !strcmp(snap.u.text.text_content, "Hello, x!"), "textbox output reaches renderer"
	);
	FluxNodeData *password_node = flux_store_find(gallery_store(g), password);
	EXPECT(password_node && password_node->behavior.on_char, "password character behavior");
	password_node->behavior.on_char(password_node->behavior.on_char_ctx, L'a');
	gallery_flush(g);
	EXPECT(g->model.pass_chars == 1, "password string event updates shared model");
	EXPECT(g->page_root == root && xent_node_valid(gallery_ctx(g), password), "password edit preserves page");
	EXPECT(
	  card_responsive(g, box, xent_node_parent(gallery_ctx(g), g->live_output [MSG_NAME]), true),
	  "textbox card aligns wide content and wraps narrow output"
	);
	return 0;
}

static int check_numberbox_projection(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_NUMBERBOX), "numberbox page");
	EXPECT(g->pending_head == NULL, "numberbox model projection is silent");
	EXPECT(xent_node_valid(gallery_ctx(g), g->live_control [MSG_QUANTITY]), "numberbox projected control");
	return 0;
}

static FluxListViewData *page_list_data(GalleryApp *g, FluxControlType type) {
	XentNodeId    node = find_type(g, g->page_root, type);
	FluxNodeData *nd   = flux_store_find(gallery_store(g), node);
	return nd ? ( FluxListViewData * ) nd->component_data : NULL;
}

static int check_list_cell_geometry(GalleryApp *g, int page, FluxControlType type, float width, float height) {
	EXPECT(build_page(g, page), "collection page");
	FluxListViewData *ld = page_list_data(g, type);
	EXPECT(ld && ld->host != XENT_NODE_INVALID, "collection host");
	XentNodeId cell = xent_node_first(gallery_ctx(g), ld->host);
	EXPECT(cell != XENT_NODE_INVALID, "collection cell");
	XentRect r = {0}, host = {0};
	EXPECT(xent_layout_rect(gallery_ctx(g), cell, &r), "collection cell rect");
	EXPECT(xent_layout_rect(gallery_ctx(g), ld->host, &host), "collection host rect");
	EXPECT((width > 0.0f ? r.w == width : r.w == host.w) && r.h == height, "collection cell has slot size");
	return 0;
}

static int check_collection_geometry(GalleryApp *g) {
	if (check_list_cell_geometry(g, PAGE_LISTVIEW, FLUX_CONTROL_LIST, 0.0f, 40.0f)) return 1;
	if (check_list_cell_geometry(g, PAGE_LISTBOX, FLUX_CONTROL_LIST_BOX, 260.0f, 40.0f)) return 1;
	if (check_list_cell_geometry(g, PAGE_ITEMS_VIEW, FLUX_CONTROL_ITEMS_VIEW, 140.0f, 96.0f)) return 1;

	EXPECT(build_page(g, PAGE_GRIDVIEW), "grid page");
	FluxListViewData *grid    = page_list_data(g, FLUX_CONTROL_GRID_VIEW);
	FluxNodeData     *grid_nd = grid ? flux_store_find(gallery_store(g), grid->root) : NULL;
	EXPECT(grid && grid_nd, "grid data");
	if (flux_list_window_stale(gallery_ctx(g), grid_nd)) flux_list_notify_stale(gallery_store(g), grid->root);
	EXPECT(xent_layout(gallery_ctx(g), flux_app_root(g->flux), 1100.0f, 760.0f), "grid relayout");
	EXPECT(grid->cols > 1, "grid resolves auto columns");
	XentNodeId stable_cell = xent_node_first(gallery_ctx(g), grid->host);
	if (flux_list_window_stale(gallery_ctx(g), grid_nd)) flux_list_notify_stale(gallery_store(g), grid->root);
	EXPECT(xent_node_first(gallery_ctx(g), grid->host) == stable_cell, "grid auto columns converge");
	XentNodeId cell = xent_node_first(gallery_ctx(g), grid->host);
	XentRect   r    = {0};
	EXPECT(cell != XENT_NODE_INVALID && xent_layout_rect(gallery_ctx(g), cell, &r), "grid cell rect");
	EXPECT(r.w == 152.0f && r.h == 104.0f, "grid cell has slot size");

	EXPECT(build_page(g, PAGE_PULL_REFRESH), "refresh page");
	FluxListViewData *refresh_list = page_list_data(g, FLUX_CONTROL_LIST);
	EXPECT(refresh_list, "refresh nested list");
	cell = xent_node_first(gallery_ctx(g), refresh_list->host);
	EXPECT(cell != XENT_NODE_INVALID && xent_layout_rect(gallery_ctx(g), cell, &r), "refresh row rect");
	EXPECT(r.h == 40.0f, "refresh row has slot height");
	return 0;
}

static int check_expander_geometry(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_EXPANDER), "expander page");
	XentNodeId        root = find_type(g, g->page_root, FLUX_CONTROL_EXPANDER);
	FluxNodeData     *nd   = flux_store_find(gallery_store(g), root);
	FluxExpanderData *d    = nd ? ( FluxExpanderData * ) nd->component_data : NULL;
	EXPECT(d && isnan(d->width) && isnan(d->content_height), "expander defaults are content-sized");
	flux_prop_expander_expanded(gallery_store(g), root, gallery_ctx(g), true);
	EXPECT(xent_layout(gallery_ctx(g), flux_app_root(g->flux), 1100.0f, 760.0f), "expanded expander relayout");
	XentRect expander = {0};
	XentRect header   = {0};
	EXPECT(xent_layout_rect(gallery_ctx(g), root, &expander), "expander rect");
	EXPECT(xent_layout_rect(gallery_ctx(g), d->header, &header), "expander header rect");
	EXPECT(header.w == expander.w && header.h == FLUX_EXPANDER_MIN_HEIGHT, "expander header geometry");
	XentNodeId body_text = xent_node_first(gallery_ctx(g), d->content);
	XentRect   body      = {0};
	XentRect   text      = {0};
	EXPECT(xent_layout_rect(gallery_ctx(g), d->content, &body), "expander body rect");
	EXPECT(
	  body_text != XENT_NODE_INVALID && xent_layout_rect(gallery_ctx(g), body_text, &text), "expander body text rect"
	);
	EXPECT(body.w == expander.w && text.w > 100.0f, "expander body keeps usable content width");
	return 0;
}

static int check_badge_binding(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_BADGE), "badge page");
	XentNodeId         node  = g->live_control [MSG_BADGE_DELTA];
	FluxNodeData      *nd    = flux_store_find(gallery_store(g), node);
	FluxInfoBadgeData *badge = nd ? ( FluxInfoBadgeData * ) nd->component_data : NULL;
	EXPECT(badge && badge->mode == FLUX_BADGE_NUMBER, "badge counter targets numeric badge");
	gallery_dispatch(g, (XtkMsg) {.tag = MSG_BADGE_DELTA, .i = 1});
	gallery_flush(g);
	EXPECT(badge->value == g->model.badge_value, "badge counter updates numeric badge");
	return 0;
}

static int check_progress_geometry(GalleryApp *g) {
	EXPECT(build_page(g, PAGE_PROGRESS), "progress page");
	XentNodeId bar       = find_type(g, g->page_root, FLUX_CONTROL_PROGRESS);
	XentNodeId ring      = find_type(g, g->page_root, FLUX_CONTROL_PROGRESS_RING);
	XentRect   bar_rect  = {0};
	XentRect   ring_rect = {0};
	EXPECT(xent_layout_rect(gallery_ctx(g), bar, &bar_rect), "progress bar rect");
	EXPECT(xent_layout_rect(gallery_ctx(g), ring, &ring_rect), "progress ring rect");
	EXPECT(bar_rect.w > 300.0f && bar_rect.h == 3.0f, "progress bar stretches at WinUI height");
	EXPECT(ring_rect.w == 32.0f && ring_rect.h == 32.0f, "progress ring keeps default size");
	EXPECT(ring_rect.y >= bar_rect.y + bar_rect.h + 15.0f, "progress controls keep page spacing");
	return 0;
}

static int check_page_retention(GalleryApp *g) {
#define RETAIN(page_id, event)    \
	{                             \
		page_id, { .tag = event } \
	}
#define RETAIN_I(page_id, event, value)       \
	{                                         \
		page_id, { .tag = event, .i = value } \
	}
#define RETAIN_B(page_id, event, value)       \
	{                                         \
		page_id, { .tag = event, .b = value } \
	}
#define RETAIN_F(page_id, event, value)       \
	{                                         \
		page_id, { .tag = event, .f = value } \
	}
#define RETAIN_D(page_id, event, value)       \
	{                                         \
		page_id, { .tag = event, .d = value } \
	}
#define RETAIN_S(page_id, event, value)                    \
	{                                                      \
		page_id, { .tag = event, .ptr = ( void * ) value } \
	}
	static RetainedCase const cases [] = {
	  RETAIN(PAGE_BUTTON, MSG_CLICK),
	  RETAIN(PAGE_SPLIT, MSG_SPLIT_PRIMARY),
	  RETAIN_B(PAGE_CHECKBOX, MSG_CHECK_A, true),
	  RETAIN_B(PAGE_RADIO, MSG_RADIO_2, true),
	  RETAIN_I(PAGE_COMBO, MSG_COMBO, 1),
	  RETAIN_F(PAGE_SLIDER, MSG_SLIDER_BASIC, 37.0f),
	  RETAIN_B(PAGE_TOGGLE, MSG_LIGHT, true),
	  RETAIN_I(PAGE_RADIO_BUTTONS, MSG_RADIO_BUTTONS, 1),
	  RETAIN_D(PAGE_RATING, MSG_RATING, 3.0),
	  RETAIN_B(PAGE_TOGGLE_SPLIT, MSG_TOGGLE_SPLIT, true),
	  RETAIN_S(PAGE_TEXTBOX, MSG_NAME, "retained"),
	  RETAIN_S(PAGE_AUTOSUGGEST, MSG_ASB_TEXT, "retained"),
	  RETAIN_D(PAGE_NUMBERBOX, MSG_QUANTITY, 3.0),
	  RETAIN_I(PAGE_INFOBAR, MSG_BAR_CLOSE, 1),
	  RETAIN_I(PAGE_BADGE, MSG_BADGE_DELTA, 1),
	  RETAIN_F(PAGE_PROGRESS, MSG_PROGRESS, 37.0f),
	  RETAIN_I(PAGE_PERSON_PICTURE, MSG_PP_BADGE, 1),
	  RETAIN_I(PAGE_FLIPVIEW, MSG_FLIP_SELECT, 1),
	  RETAIN_I(PAGE_PAGER, MSG_PAGER_SELECT, 1),
	  RETAIN(PAGE_MENUS, MSG_REPEAT),
	  RETAIN_B(PAGE_EXPANDER, MSG_EXPAND, true),
	  RETAIN_I(PAGE_BREADCRUMB, MSG_BREADCRUMB, 1),
	  RETAIN_I(PAGE_SELECTOR_BAR, MSG_SELECTOR_BAR, 1),
	  RETAIN_I(PAGE_LISTVIEW, MSG_LIST_SELECT, 1),
	  RETAIN_I(PAGE_GRIDVIEW, MSG_GRID_SELECT, 1),
	  RETAIN_I(PAGE_LISTBOX, MSG_LISTBOX_SELECT, 1),
	  RETAIN_I(PAGE_TREE_VIEW, MSG_TREE_SELECT, 1),
	  RETAIN_I(PAGE_ITEMS_VIEW, MSG_ITEMS_INVOKE, 1),
	  RETAIN(PAGE_PULL_REFRESH, MSG_REFRESH),
	  RETAIN(PAGE_SPLIT_VIEW, MSG_SPLIT_TOGGLE),
	  RETAIN(PAGE_TITLE_BAR, MSG_TB_BACK),
	  RETAIN_I(PAGE_DIALOG, MSG_DIALOG_RESULT, 1),
	  RETAIN(PAGE_TEACHING_TIP, MSG_TIP_OPEN),
	};
#undef RETAIN
#undef RETAIN_I
#undef RETAIN_B
#undef RETAIN_F
#undef RETAIN_D
#undef RETAIN_S
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases [0]); i++) {
		EXPECT(build_page(g, cases [i].page), "retained page builds");
		XentNodeId root = g->page_root;
		gallery_dispatch(g, cases [i].msg);
		gallery_flush(g);
		EXPECT(g->page_root == root, "control event preserves page root");
		EXPECT(xent_node_valid(gallery_ctx(g), root), "retained page root remains live");
	}
	return 0;
}

int main(void) {
	GalleryApp g;
	EXPECT(gallery_app_create(&g), "gallery create");
	for (int page = 0; page < PAGE_COUNT; page++) {
		if (build_page(&g, page)) continue;
		printf("FAIL: page %d behavior contract\n", page);
		gallery_app_destroy(&g);
		return 1;
	}
	if (check_routed_behavior(&g)) {
		gallery_app_destroy(&g);
		return 1;
	}
	if (check_combo_retention(&g) || check_textbox_retention(&g) || check_numberbox_projection(&g)) {
		gallery_app_destroy(&g);
		return 1;
	}
	if (check_collection_geometry(&g)
		|| check_expander_geometry(&g)
		|| check_badge_binding(&g)
		|| check_progress_geometry(&g)
		|| !hyperlink_keeps_natural_width(&g))
	{
		gallery_app_destroy(&g);
		return 1;
	}
	if (check_page_retention(&g)) {
		gallery_app_destroy(&g);
		return 1;
	}
	gallery_app_destroy(&g);
	printf("PASS: imperative gallery pages / bindings / routed behavior\n");
	return 0;
}
