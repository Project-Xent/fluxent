#include "fluxent/flux_node_store.h"
#include "fluxent/fluxent.h"
#include <xent/xent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RADIO_GROUP_MAX 8

typedef struct {
	FluxNodeStore *store;
	XentNodeId     nodes [RADIO_GROUP_MAX];
	int            count;
} RadioGroup;

typedef struct {
	RadioGroup *group;
	int         index;
} RadioGroupMember;

typedef struct {
	int            count;
	XentNodeId     label_node;
	FluxNodeStore *store;
	char           buf [64];
} CounterState;

typedef struct {
	int            count;
	XentNodeId     label_node;
	FluxNodeStore *store;
	char           buf [64];
} RepeatCounterState;

typedef struct {
	bool           toggled;
	XentNodeId     node;
	FluxNodeStore *store;
	XentContext   *ctx;
} ToggleState;

typedef struct {
	XentContext        *ctx;
	FluxNodeStore      *store;
	FluxApp            *app;
	XentNodeId          scroll_root;
	XentNodeId          root;
	FluxNodeData       *scroll_nd;
	XentNodeId          footer_div;
	XentNodeId          footer;
	XentNodeId          spacer;
	CounterState       *counter;
	RepeatCounterState *repeat;
	ToggleState        *toggle;
	RadioGroup         *radio;
	RadioGroupMember   *members;
	FluxFlyout         *flyout;
	FluxMenuFlyout     *menu;
} Demo;

static void radio_group_trampoline(void *ctx) {
	RadioGroupMember *m = ( RadioGroupMember * ) ctx;
	if (!m || !m->group) return;

	RadioGroup *g = m->group;
	for (int i = 0; i < g->count; i++) {
		FluxNodeData *nd = flux_node_store_get(g->store, g->nodes [i]);
		if (!nd || !nd->component_data) continue;

		FluxCheckboxData *cd = ( FluxCheckboxData * ) nd->component_data;
		if (!cd->enabled) continue;

		FluxCheckState state = (i == m->index) ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
		cd->state            = state;
		if (i == m->index && cd->on_change) cd->on_change(cd->on_change_ctx, state);
	}
}

static void on_counter_click(void *ctx) {
	CounterState *cs = ( CounterState * ) ctx;
	cs->count++;
	snprintf(cs->buf, sizeof(cs->buf), "Clicked: %d", cs->count);
	flux_text_set_content(cs->store, cs->label_node, cs->buf);
}

static void on_repeat_click(void *ctx) {
	RepeatCounterState *rs = ( RepeatCounterState * ) ctx;
	rs->count++;
	snprintf(rs->buf, sizeof(rs->buf), "Repeated: %d", rs->count);
	flux_text_set_content(rs->store, rs->label_node, rs->buf);
}

static void on_toggle_click(void *ctx) {
	ToggleState *ts  = ( ToggleState * ) ctx;
	ts->toggled      = !ts->toggled;

	FluxNodeData *nd = flux_node_store_get(ts->store, ts->node);
	if (nd && nd->component_data) {
		FluxButtonData *bd = ( FluxButtonData * ) nd->component_data;
		bd->is_checked     = ts->toggled;
	}
	xent_set_semantic_checked(ts->ctx, ts->node, ts->toggled ? 1 : 0);
}

static void on_check_change(void *ctx, FluxCheckState s) {
	( void ) ctx;
	( void ) s;
}

static void on_switch_change(void *ctx, FluxCheckState s) {
	( void ) ctx;
	( void ) s;
}

static void on_radio_change(void *ctx, FluxCheckState s) {
	( void ) ctx;
	( void ) s;
}

static void on_slider_change(void *ctx, float v) {
	( void ) ctx;
	( void ) v;
}

static void on_textbox_change(void *ctx, char const *t) {
	( void ) ctx;
	( void ) t;
}

static void on_password_change(void *ctx, char const *t) {
	( void ) ctx;
	( void ) t;
}

static void on_number_change(void *ctx, double v) {
	( void ) ctx;
	( void ) v;
}

static void       on_hyperlink_click(void *ctx) { ( void ) ctx; }

static XentNodeId demo_button(Demo *d, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata) {
	FluxButtonCreateInfo info = {d->ctx, d->store, parent, label, on_click, userdata};
	return flux_create_button(&info);
}

static XentNodeId
demo_repeat_button(Demo *d, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata) {
	FluxButtonCreateInfo info = {d->ctx, d->store, parent, label, on_click, userdata};
	return flux_create_repeat_button(&info);
}

static XentNodeId demo_hyperlink(
  Demo *d, XentNodeId parent, char const *label, char const *url, void (*on_click)(void *), void *userdata
) {
	FluxHyperlinkCreateInfo info = {d->ctx, d->store, parent, label, url, on_click, userdata};
	return flux_create_hyperlink(&info);
}

static XentNodeId demo_toggle(
  Demo *d, XentNodeId parent, XentControlType type, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
) {
	FluxToggleCreateInfo info = {d->ctx, d->store, parent, label, checked, on_change, userdata};
	if (type == XENT_CONTROL_RADIO) return flux_create_radio(&info);
	if (type == XENT_CONTROL_SWITCH) return flux_create_switch(&info);
	return flux_create_checkbox(&info);
}

static XentNodeId demo_slider(
  Demo *d, XentNodeId parent, float min, float max, float value, void (*on_change)(void *, float), void *userdata
) {
	FluxSliderCreateInfo info = {d->ctx, d->store, parent, min, max, value, on_change, userdata};
	return flux_create_slider(&info);
}

static XentNodeId demo_progress(Demo *d, XentNodeId parent, float value, float max_value) {
	FluxProgressCreateInfo info = {d->ctx, d->store, parent, value, max_value};
	return flux_create_progress(&info);
}

static XentNodeId demo_progress_ring(Demo *d, XentNodeId parent, float value, float max_value) {
	FluxProgressCreateInfo info = {d->ctx, d->store, parent, value, max_value};
	return flux_create_progress_ring(&info);
}

static XentNodeId demo_badge(Demo *d, XentNodeId parent, FluxInfoBadgeMode mode, int32_t value) {
	FluxInfoBadgeCreateInfo info = {d->ctx, d->store, parent, mode, value};
	return flux_create_info_badge(&info);
}

static XentNodeId make_row(XentContext *ctx, XentNodeId parent, float gap, float h) {
	XentNodeId row = xent_create_node(ctx);
	xent_set_protocol(ctx, row, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, row, XENT_FLEX_ROW);
	xent_set_flex_align_items(ctx, row, XENT_FLEX_ALIGN_CENTER);
	xent_set_gap(ctx, row, gap);
	xent_set_size(ctx, row, 480, h);
	xent_append_child(ctx, parent, row);
	return row;
}

static XentNodeId make_section(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *text) {
	XentNodeId t = flux_create_text(ctx, store, parent, text, 16.0f);
	xent_set_size(ctx, t, 480, 24);
	flux_text_set_weight(store, t, FLUX_FONT_SEMI_BOLD);
	return t;
}

static void add_divider(Demo *d) {
	XentNodeId div = flux_create_divider(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, div, 480, 1);
}

static bool demo_alloc_state(Demo *d) {
	d->counter = ( CounterState * ) calloc(1, sizeof(*d->counter));
	d->repeat  = ( RepeatCounterState * ) calloc(1, sizeof(*d->repeat));
	d->toggle  = ( ToggleState * ) calloc(1, sizeof(*d->toggle));
	d->radio   = ( RadioGroup * ) calloc(1, sizeof(*d->radio));
	d->members = ( RadioGroupMember * ) calloc(RADIO_GROUP_MAX, sizeof(*d->members));
	if (!d->counter || !d->repeat || !d->toggle || !d->radio || !d->members) return false;

	d->counter->store = d->store;
	snprintf(d->counter->buf, sizeof(d->counter->buf), "Clicked: 0");
	d->repeat->store = d->store;
	snprintf(d->repeat->buf, sizeof(d->repeat->buf), "Repeated: 0");
	d->toggle->store = d->store;
	d->toggle->ctx   = d->ctx;
	d->radio->store  = d->store;
	return true;
}

static bool demo_init(Demo *d) {
	XentConfig xcfg       = {0};
	xcfg.initial_capacity = 512;
	d->ctx                = xent_create_context(&xcfg);
	if (!d->ctx) return false;

	d->store = flux_node_store_create(512);
	if (!d->store) return false;
	return demo_alloc_state(d);
}

static void demo_make_scroll_root(Demo *d) {
	d->scroll_root = xent_create_node(d->ctx);
	xent_set_control_type(d->ctx, d->scroll_root, XENT_CONTROL_SCROLL);
	xent_set_protocol(d->ctx, d->scroll_root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->scroll_root, XENT_FLEX_COLUMN);

	d->scroll_nd = flux_node_store_get_or_create(d->store, d->scroll_root);
	xent_set_userdata(d->ctx, d->scroll_root, d->scroll_nd);
	if (!d->scroll_nd) return;

	FluxScrollData *sd = ( FluxScrollData * ) calloc(1, sizeof(*sd));
	if (!sd) return;
	sd->v_vis                    = FLUX_SCROLL_AUTO;
	sd->h_vis                    = FLUX_SCROLL_AUTO;
	d->scroll_nd->component_data = sd;
}

static void demo_make_root(Demo *d) {
	d->root = xent_create_node(d->ctx);
	xent_set_protocol(d->ctx, d->root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, d->root, XENT_FLEX_ALIGN_CENTER);
	xent_set_padding(d->ctx, d->root, 32, 32, 32, 32);
	xent_set_gap(d->ctx, d->root, 14);
	xent_set_size(d->ctx, d->root, NAN, 2200);
	xent_append_child(d->ctx, d->scroll_root, d->root);
}

static void demo_add_title(Demo *d) {
	XentNodeId title = flux_create_text(d->ctx, d->store, d->root, "Hello, Fluxent!", 28.0f);
	xent_set_size(d->ctx, title, 480, 40);
	flux_text_set_weight(d->store, title, FLUX_FONT_BOLD);
	flux_text_set_alignment(d->store, title, FLUX_TEXT_CENTER);

	XentNodeId subtitle
	  = flux_create_text(d->ctx, d->store, d->root, "A pure-C Fluent Design UI - All Controls Showcase", 13.0f);
	xent_set_size(d->ctx, subtitle, 480, 20);
	flux_text_set_alignment(d->store, subtitle, FLUX_TEXT_CENTER);
	add_divider(d);
}

static void demo_add_button_variants(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	XentNodeId b1  = demo_button(d, row, "Standard", NULL, NULL);
	xent_set_size(d->ctx, b1, 110, 32);
	flux_node_set_tooltip(d->store, b1, "This is a standard button");

	XentNodeId b2 = demo_button(d, row, "Accent", NULL, NULL);
	xent_set_size(d->ctx, b2, 110, 32);
	flux_button_set_style(d->store, b2, FLUX_BUTTON_ACCENT);
	flux_node_set_tooltip(d->store, b2, "This is an accent-styled button for primary actions");

	XentNodeId b3 = demo_button(d, row, "Subtle", NULL, NULL);
	xent_set_size(d->ctx, b3, 110, 32);
	flux_button_set_style(d->store, b3, FLUX_BUTTON_SUBTLE);
	flux_node_set_tooltip(d->store, b3, "Subtle buttons blend into the background");
}

static void demo_add_disabled_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	for (int i = 0; i < 3; i++) {
		XentNodeId btn = demo_button(d, row, "Disabled", NULL, NULL);
		xent_set_size(d->ctx, btn, 110, 32);
		if (i == 1) flux_button_set_style(d->store, btn, FLUX_BUTTON_ACCENT);
		if (i == 2) flux_button_set_style(d->store, btn, FLUX_BUTTON_SUBTLE);
		flux_button_set_enabled(d->store, btn, false);
	}
}

static void demo_add_icon_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	XentNodeId ib1 = demo_button(d, row, "Settings", NULL, NULL);
	xent_set_size(d->ctx, ib1, 120, 32);
	flux_button_set_icon(d->store, ib1, "Settings");

	XentNodeId ib2 = demo_button(d, row, "Search", NULL, NULL);
	xent_set_size(d->ctx, ib2, 110, 32);
	flux_button_set_icon(d->store, ib2, "Search");
	flux_button_set_style(d->store, ib2, FLUX_BUTTON_ACCENT);
}

static void demo_add_counter_button(Demo *d) {
	XentNodeId row  = make_row(d->ctx, d->root, 12, 32);
	XentNodeId cbtn = demo_button(d, row, "Count", on_counter_click, d->counter);
	xent_set_size(d->ctx, cbtn, 100, 32);
	flux_button_set_style(d->store, cbtn, FLUX_BUTTON_ACCENT);
	flux_button_set_icon(d->store, cbtn, "Add");
	flux_node_set_tooltip(d->store, cbtn, "Click to increment the counter");

	XentNodeId clbl = flux_create_text(d->ctx, d->store, row, d->counter->buf, 14.0f);
	xent_set_size(d->ctx, clbl, 140, 32);
	d->counter->label_node = clbl;
}

static void demo_add_buttons(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Button");
	demo_add_button_variants(d);
	demo_add_disabled_buttons(d);
	demo_add_icon_buttons(d);
	demo_add_counter_button(d);
	add_divider(d);
}

static void demo_add_toggle(Demo *d) {
	make_section(d->ctx, d->store, d->root, "ToggleButton");
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId tb1 = demo_button(d, row, "Toggle Me", on_toggle_click, d->toggle);
	xent_set_size(d->ctx, tb1, 130, 32);
	xent_set_control_type(d->ctx, tb1, XENT_CONTROL_TOGGLE_BUTTON);
	d->toggle->node = tb1;

	XentNodeId tb2  = demo_button(d, row, "Toggled On", NULL, NULL);
	xent_set_size(d->ctx, tb2, 130, 32);
	xent_set_control_type(d->ctx, tb2, XENT_CONTROL_TOGGLE_BUTTON);
	FluxNodeData *nd = flux_node_store_get(d->store, tb2);
	if (nd && nd->component_data) (( FluxButtonData * ) nd->component_data)->is_checked = true;
	add_divider(d);
}

static void demo_add_repeat(Demo *d) {
	make_section(d->ctx, d->store, d->root, "RepeatButton (hold to repeat)");
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId rb  = demo_repeat_button(d, row, "Hold Me", on_repeat_click, d->repeat);
	xent_set_size(d->ctx, rb, 120, 32);

	XentNodeId rlbl = flux_create_text(d->ctx, d->store, row, d->repeat->buf, 14.0f);
	xent_set_size(d->ctx, rlbl, 160, 32);
	d->repeat->label_node = rlbl;
	add_divider(d);
}

static void demo_add_hyperlinks(Demo *d) {
	make_section(d->ctx, d->store, d->root, "HyperlinkButton");
	XentNodeId row = make_row(d->ctx, d->root, 16, 32);
	XentNodeId h1  = demo_hyperlink(d, row, "Visit GitHub", "https://github.com", on_hyperlink_click, NULL);
	xent_set_size(d->ctx, h1, 130, 32);
	XentNodeId h2 = demo_hyperlink(d, row, "xmake.io", "https://xmake.io", on_hyperlink_click, NULL);
	xent_set_size(d->ctx, h2, 100, 32);
	XentNodeId h3 = demo_hyperlink(d, row, "Disabled Link", NULL, NULL, NULL);
	xent_set_size(d->ctx, h3, 120, 32);
	flux_hyperlink_set_enabled(d->store, h3, false);
	add_divider(d);
}

static void demo_add_check_switch(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Checkbox & Switch");
	XentNodeId row  = make_row(d->ctx, d->root, 20, 32);
	XentNodeId chk1 = demo_toggle(d, row, XENT_CONTROL_CHECKBOX, "Enable feature", false, on_check_change, NULL);
	xent_set_size(d->ctx, chk1, 160, 32);
	XentNodeId chk2 = demo_toggle(d, row, XENT_CONTROL_CHECKBOX, "Checked", true, on_check_change, NULL);
	xent_set_size(d->ctx, chk2, 110, 32);
	XentNodeId sw1 = demo_toggle(d, row, XENT_CONTROL_SWITCH, "Wi-Fi", true, on_switch_change, NULL);
	xent_set_size(d->ctx, sw1, 110, 32);

	row              = make_row(d->ctx, d->root, 20, 32);
	XentNodeId chk_d = demo_toggle(d, row, XENT_CONTROL_CHECKBOX, "Disabled", false, on_check_change, NULL);
	xent_set_size(d->ctx, chk_d, 120, 32);
	flux_checkbox_set_enabled(d->store, chk_d, false);
	XentNodeId sw_d = demo_toggle(d, row, XENT_CONTROL_SWITCH, "Disabled", false, on_switch_change, NULL);
	xent_set_size(d->ctx, sw_d, 120, 32);
	xent_set_semantic_enabled(d->ctx, sw_d, false);
	add_divider(d);
}

static void demo_wire_radio_group(Demo *d) {
	for (int i = 0; i < d->radio->count; i++) {
		d->members [i].group = d->radio;
		d->members [i].index = i;
		FluxNodeData *nd     = flux_node_store_get(d->store, d->radio->nodes [i]);
		if (!nd) continue;
		nd->behavior.on_click     = radio_group_trampoline;
		nd->behavior.on_click_ctx = &d->members [i];
	}
}

static void demo_add_radio(Demo *d) {
	make_section(d->ctx, d->store, d->root, "RadioButton (mutually exclusive)");
	XentNodeId row      = make_row(d->ctx, d->root, 16, 32);
	d->radio->nodes [0] = demo_toggle(d, row, XENT_CONTROL_RADIO, "Option A", true, on_radio_change, NULL);
	d->radio->nodes [1] = demo_toggle(d, row, XENT_CONTROL_RADIO, "Option B", false, on_radio_change, NULL);
	d->radio->nodes [2] = demo_toggle(d, row, XENT_CONTROL_RADIO, "Option C", false, on_radio_change, NULL);
	d->radio->count     = 3;
	for (int i = 0; i < d->radio->count; i++) xent_set_size(d->ctx, d->radio->nodes [i], 110, 32);
	demo_wire_radio_group(d);
	add_divider(d);
}

static void demo_add_slider(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Slider");
	XentNodeId sld = demo_slider(d, d->root, 0.0f, 100.0f, 50.0f, on_slider_change, NULL);
	xent_set_size(d->ctx, sld, 480, 32);
	add_divider(d);
}

static void demo_add_progress(Demo *d) {
	make_section(d->ctx, d->store, d->root, "ProgressBar");
	XentNodeId prog1 = demo_progress(d, d->root, 65.0f, 100.0f);
	xent_set_size(d->ctx, prog1, 480, 20);
	XentNodeId prog2 = demo_progress(d, d->root, 0.0f, 100.0f);
	xent_set_size(d->ctx, prog2, 480, 20);
	flux_progress_set_indeterminate(d->store, prog2, true);

	make_section(d->ctx, d->store, d->root, "ProgressRing");
	XentNodeId row   = make_row(d->ctx, d->root, 20, 48);
	XentNodeId ring1 = demo_progress_ring(d, row, 75.0f, 100.0f);
	XentNodeId ring2 = demo_progress_ring(d, row, 0.0f, 0.0f);
	XentNodeId ring3 = demo_progress_ring(d, row, 40.0f, 100.0f);
	xent_set_size(d->ctx, ring1, 32, 32);
	xent_set_size(d->ctx, ring2, 32, 32);
	xent_set_size(d->ctx, ring3, 48, 48);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "75%", 12.0f), 35, 20);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "Loading...", 12.0f), 72, 20);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "40%", 12.0f), 35, 20);
	add_divider(d);
}

static void demo_add_badge(Demo *d) {
	make_section(d->ctx, d->store, d->root, "InfoBadge");
	XentNodeId row = make_row(d->ctx, d->root, 14, 24);
	XentNodeId bd  = demo_badge(d, row, FLUX_BADGE_DOT, 0);
	xent_set_size(d->ctx, bd, 8, 8);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "Dot", 12.0f), 28, 20);
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 3), 16, 16);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "= 3", 12.0f), 28, 20);
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 42), 24, 16);
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "= 42", 12.0f), 36, 20);
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 999), 28, 16);
	XentNodeId bi = demo_badge(d, row, FLUX_BADGE_ICON, 0);
	xent_set_size(d->ctx, bi, 16, 16);
	flux_info_badge_set_icon(d->store, bi, "Info");
	xent_set_size(d->ctx, flux_create_text(d->ctx, d->store, row, "Icon", 12.0f), 36, 20);
	add_divider(d);
}

static void demo_add_card(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Card");
	XentNodeId card = flux_create_card(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, card, 480, 148);
	xent_set_protocol(d->ctx, card, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, card, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, card, XENT_FLEX_ALIGN_START);
	xent_set_padding(d->ctx, card, 16, 16, 16, 16);
	xent_set_gap(d->ctx, card, 8);

	XentNodeId ct = flux_create_text(d->ctx, d->store, card, "This is a Card", 16.0f);
	xent_set_size(d->ctx, ct, 440, 24);
	flux_text_set_weight(d->store, ct, FLUX_FONT_SEMI_BOLD);
	XentNodeId body = flux_create_text(
	  d->ctx, d->store, card, "Cards group related content with a subtle background and border.", 13.0f
	);
	xent_set_size(d->ctx, body, 440, 44);

	XentNodeId row = make_row(d->ctx, card, 8, 32);
	xent_set_size(d->ctx, row, 440, 32);
	XentNodeId action = demo_button(d, row, "Action", NULL, NULL);
	xent_set_size(d->ctx, action, 100, 32);
	flux_button_set_style(d->store, action, FLUX_BUTTON_ACCENT);
	xent_set_size(d->ctx, demo_button(d, row, "Cancel", NULL, NULL), 100, 32);
	add_divider(d);
}

static void demo_add_footer_placeholder(Demo *d) {
	make_section(d->ctx, d->store, d->root, "TextBox / PasswordBox / NumberBox");
	d->footer_div = flux_create_divider(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, d->footer_div, 480, 1);
	d->footer
	  = flux_create_text(d->ctx, d->store, d->root, "xent-core + fluxent - Mica backdrop - All controls demo", 11.0f);
	xent_set_size(d->ctx, d->footer, 480, 20);
	flux_text_set_alignment(d->store, d->footer, FLUX_TEXT_CENTER);
	d->spacer = xent_create_node(d->ctx);
	xent_set_size(d->ctx, d->spacer, 1, 32);
	xent_append_child(d->ctx, d->root, d->spacer);
}

static void demo_build_static_content(Demo *d) {
	demo_make_scroll_root(d);
	demo_make_root(d);
	demo_add_title(d);
	demo_add_buttons(d);
	demo_add_toggle(d);
	demo_add_repeat(d);
	demo_add_hyperlinks(d);
	demo_add_check_switch(d);
	demo_add_radio(d);
	demo_add_slider(d);
	demo_add_progress(d);
	demo_add_badge(d);
	demo_add_card(d);
	demo_add_footer_placeholder(d);
}

static bool demo_create_app(Demo *d) {
	FluxAppConfig cfg = {0};
	cfg.title         = L"Hello Fluxent - All Controls";
	cfg.width         = 640;
	cfg.height        = 760;
	cfg.dark_mode     = flux_theme_system_is_dark();
	cfg.backdrop      = FLUX_BACKDROP_MICA;

	HRESULT hr        = flux_app_create(&cfg, &d->app);
	if (FAILED(hr)) return false;
	flux_app_set_root(d->app, d->ctx, d->scroll_root, d->store);

	if (!d->scroll_nd || !d->scroll_nd->component_data) return true;
	FluxScrollData *sd = ( FluxScrollData * ) d->scroll_nd->component_data;
	sd->content_h      = 2200.0f;
	sd->content_w      = 640.0f;
	return true;
}

static void demo_move_footer_after_inputs(Demo *d) {
	xent_remove_child(d->ctx, d->root, d->footer_div);
	xent_remove_child(d->ctx, d->root, d->footer);
	xent_remove_child(d->ctx, d->root, d->spacer);
}

static void demo_restore_footer(Demo *d) {
	xent_append_child(d->ctx, d->root, d->footer_div);
	xent_append_child(d->ctx, d->root, d->footer);
	xent_append_child(d->ctx, d->root, d->spacer);
}

static void demo_add_text_inputs(Demo *d) {
	demo_move_footer_after_inputs(d);
	FluxAppTextBoxCreateInfo text_info = {d->app, d->root, "Type something here...", on_textbox_change, NULL};
	XentNodeId               textbox   = flux_app_create_textbox(&text_info);
	xent_set_size(d->ctx, textbox, 480, 32);
	FluxAppTextBoxCreateInfo pass_info = {d->app, d->root, "Enter password...", on_password_change, NULL};
	XentNodeId               passbox   = flux_app_create_password_box(&pass_info);
	xent_set_size(d->ctx, passbox, 480, 32);
	FluxAppNumberBoxCreateInfo num_info = {d->app, d->root, -100.0, 100.0, 1.0, on_number_change, NULL};
	XentNodeId                 numbox   = flux_app_create_number_box(&num_info);
	xent_set_size(d->ctx, numbox, 480, 32);
}

static void
menu_add_item(FluxMenuFlyout *menu, FluxMenuItemType type, char const *label, char const *accel, bool enabled) {
	FluxMenuItemDef item  = {0};
	item.type             = type;
	item.label            = label;
	item.accelerator_text = accel;
	item.enabled          = enabled;
	flux_menu_flyout_add_item(menu, &item);
}

static FluxMenuFlyout *demo_create_theme_menu(FluxWindow *win, FluxThemeManager *tmgr, FluxTextRenderer *tr) {
	FluxMenuFlyout *menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(menu, tmgr);
	flux_menu_flyout_set_text_renderer(menu, tr);
	for (int i = 0; i < 3; i++) {
		FluxMenuItemDef item = {0};
		item.type            = FLUX_MENU_ITEM_RADIO;
		item.label           = i == 0 ? "System" : (i == 1 ? "Light" : "Dark");
		item.radio_group     = "theme";
		item.checked         = i == 0;
		item.enabled         = true;
		flux_menu_flyout_add_item(menu, &item);
	}
	return menu;
}

static FluxMenuFlyout *
demo_create_view_menu(FluxWindow *win, FluxThemeManager *tmgr, FluxTextRenderer *tr, FluxMenuFlyout *theme) {
	FluxMenuFlyout *menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(menu, tmgr);
	flux_menu_flyout_set_text_renderer(menu, tr);

	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_TOGGLE;
	item.label           = "Word Wrap";
	item.checked         = true;
	item.enabled         = true;
	flux_menu_flyout_add_item(menu, &item);
	item.checked = false;
	item.label   = "Line Numbers";
	flux_menu_flyout_add_item(menu, &item);
	flux_menu_flyout_add_separator(menu);
	item         = (FluxMenuItemDef) {0};
	item.type    = FLUX_MENU_ITEM_SUBMENU;
	item.label   = "Theme";
	item.enabled = true;
	item.submenu = theme;
	flux_menu_flyout_add_item(menu, &item);
	return menu;
}

static void demo_fill_context_menu(FluxMenuFlyout *menu, FluxMenuFlyout *view) {
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Cut", "Ctrl+X", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Copy", "Ctrl+C", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Paste", "Ctrl+V", true);
	flux_menu_flyout_add_separator(menu);
	FluxMenuItemDef item = {0};
	item.type            = FLUX_MENU_ITEM_SUBMENU;
	item.label           = "View";
	item.enabled         = true;
	item.submenu         = view;
	flux_menu_flyout_add_item(menu, &item);
	flux_menu_flyout_add_separator(menu);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Select All", "Ctrl+A", true);
	menu_add_item(menu, FLUX_MENU_ITEM_NORMAL, "Delete", NULL, false);
}

static void demo_add_flyout_menu(Demo *d) {
	add_divider(d);
	make_section(d->ctx, d->store, d->root, "Flyout & MenuFlyout");
	FluxWindow       *win  = flux_app_get_window(d->app);
	FluxThemeManager *tmgr = flux_app_get_theme(d->app);
	FluxTextRenderer *tr   = flux_app_get_text_renderer(d->app);

	d->flyout              = flux_flyout_create(win);
	flux_flyout_set_theme_manager(d->flyout, tmgr);
	flux_flyout_set_text_renderer(d->flyout, tr);
	flux_flyout_set_content_size(d->flyout, 200, 40);

	d->menu = flux_menu_flyout_create(win);
	flux_menu_flyout_set_theme_manager(d->menu, tmgr);
	flux_menu_flyout_set_text_renderer(d->menu, tr);
	FluxMenuFlyout *theme = demo_create_theme_menu(win, tmgr, tr);
	FluxMenuFlyout *view  = demo_create_view_menu(win, tmgr, tr, theme);
	demo_fill_context_menu(d->menu, view);
}

static void demo_add_popup_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId fly = demo_button(d, row, "Click me (Flyout)", NULL, NULL);
	xent_set_size(d->ctx, fly, 200, 32);
	flux_button_set_style(d->store, fly, FLUX_BUTTON_ACCENT);
	FluxFlyoutBindingInfo flyout_info
	  = {d->store, fly, d->flyout, FLUX_PLACEMENT_BOTTOM, d->ctx, flux_app_get_window(d->app)};
	flux_node_set_flyout_ex(&flyout_info);
	flux_node_set_tooltip(d->store, fly, "Click to show a Flyout popup below this button");

	XentNodeId ctx_btn = demo_button(d, row, "Right-click (Menu)", NULL, NULL);
	xent_set_size(d->ctx, ctx_btn, 200, 32);
	FluxContextFlyoutBindingInfo context_info = {d->store, ctx_btn, d->menu, d->ctx, flux_app_get_window(d->app)};
	flux_node_set_context_flyout_ex(&context_info);
	flux_node_set_tooltip(d->store, ctx_btn, "Right-click to show a MenuFlyout context menu");
}

static void demo_add_dynamic_content(Demo *d) {
	demo_add_text_inputs(d);
	demo_add_flyout_menu(d);
	demo_add_popup_buttons(d);
	demo_restore_footer(d);
}

static void demo_destroy(Demo *d) {
	if (d->menu) flux_menu_flyout_destroy(d->menu);
	if (d->flyout) flux_flyout_destroy(d->flyout);
	if (d->app) flux_app_destroy(d->app);
	if (d->store) flux_node_store_destroy(d->store);
	if (d->ctx) xent_destroy_context(d->ctx);
	free(d->counter);
	free(d->repeat);
	free(d->toggle);
	free(d->radio);
	free(d->members);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	Demo demo = {0};
	if (!demo_init(&demo)) {
		demo_destroy(&demo);
		return 1;
	}

	demo_build_static_content(&demo);
	if (!demo_create_app(&demo)) {
		demo_destroy(&demo);
		return 1;
	}

	demo_add_dynamic_content(&demo);
	int result = flux_app_run(demo.app);
	demo_destroy(&demo);
	return result;
}
