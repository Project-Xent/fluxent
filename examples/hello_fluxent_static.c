#include "hello_fluxent_demo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void radio_group_apply(RadioGroupMember const *m, int index) {
	RadioGroup   *g  = m->group;
	FluxNodeData *nd = flux_node_store_get(g->store, g->nodes [index]);
	if (!nd || !nd->component_data) return;

	FluxCheckboxData *cd = ( FluxCheckboxData * ) nd->component_data;
	if (!cd->enabled) return;

	FluxCheckState state = (index == m->index) ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
	cd->state            = state;
	if (index == m->index && cd->on_change) cd->on_change(cd->on_change_ctx, state);
}

static void radio_group_trampoline(void *ctx) {
	RadioGroupMember *m = ( RadioGroupMember * ) ctx;
	if (!m || !m->group) return;

	RadioGroup *g = m->group;
	for (int i = 0; i < g->count; i++) radio_group_apply(m, i);
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

static void on_hyperlink_click(void *ctx) { ( void ) ctx; }

XentNodeId  demo_button(Demo *d, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata) {
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
  Demo *d, XentNodeId parent, FluxControlType type, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
) {
	FluxToggleCreateInfo info = {d->ctx, d->store, parent, label, checked, on_change, userdata};
	if (type == FLUX_CONTROL_RADIO) return flux_create_radio(&info);
	if (type == FLUX_CONTROL_SWITCH) return flux_create_switch(&info);
	return flux_create_checkbox(&info);
}

XentNodeId demo_slider(
  Demo *d, XentNodeId parent, float min, float max, float value, void (*on_change)(void *, float), void *userdata
) {
	FluxSliderCreateInfo info = {d->ctx, d->store, parent, min, max, value, on_change, userdata};
	return flux_create_slider(&info);
}

XentNodeId demo_progress(Demo *d, XentNodeId parent, float value, float max_value) {
	FluxProgressCreateInfo info = {d->ctx, d->store, parent, value, max_value};
	return flux_create_progress(&info);
}

XentNodeId demo_progress_ring(Demo *d, XentNodeId parent, float value, float max_value) {
	FluxProgressCreateInfo info = {d->ctx, d->store, parent, value, max_value};
	return flux_create_progress_ring(&info);
}

XentNodeId demo_badge(Demo *d, XentNodeId parent, FluxInfoBadgeMode mode, int32_t value) {
	FluxInfoBadgeCreateInfo info = {d->ctx, d->store, parent, mode, value};
	return flux_create_info_badge(&info);
}

XentNodeId
demo_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size) {
	FluxTextCreateInfo info = {ctx, store, parent, content, font_size};
	return flux_create_text(&info);
}

XentNodeId demo_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	FluxContainerCreateInfo info = {ctx, store, parent};
	return flux_create_card(&info);
}

XentNodeId demo_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	FluxContainerCreateInfo info = {ctx, store, parent};
	return flux_create_divider(&info);
}

XentNodeId make_row(XentContext *ctx, XentNodeId parent, float gap, float h) {
	XentNodeId row = xent_create_node(ctx);
	xent_set_protocol(ctx, row, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, row, XENT_FLEX_ROW);
	xent_set_flex_align_items(ctx, row, XENT_FLEX_ALIGN_CENTER);
	xent_set_gap(ctx, row, gap);
	xent_set_size(ctx, row, (XentSize) {480, h});
	xent_append_child(ctx, parent, row);
	return row;
}

XentNodeId make_section(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *text) {
	XentNodeId t = demo_create_text(ctx, store, parent, text, 16.0f);
	xent_set_size(ctx, t, (XentSize) {480, 24});
	flux_text_set_weight(store, t, FLUX_FONT_SEMI_BOLD);
	return t;
}

void add_divider(Demo *d) {
	XentNodeId div = demo_create_divider(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, div, (XentSize) {480, 1});
}

void demo_make_scroll_root(Demo *d) {
	flux_set_control_type(d->ctx, d->scroll_root, FLUX_CONTROL_SCROLL);
	xent_set_protocol(d->ctx, d->scroll_root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->scroll_root, XENT_FLEX_COLUMN);

	d->scroll_nd = flux_node_store_get_or_create(d->store, d->scroll_root);
	xent_set_userdata(d->ctx, d->scroll_root, d->scroll_nd);
	if (!d->scroll_nd) return;
	d->scroll_nd->component_type = FLUX_CONTROL_SCROLL;

	FluxScrollData *sd           = ( FluxScrollData * ) calloc(1, sizeof(*sd));
	if (!sd) return;
	sd->v_vis                    = FLUX_SCROLL_AUTO;
	sd->h_vis                    = FLUX_SCROLL_AUTO;
	sd->content_h                = ( float ) HELLO_FLUXENT_DEMO_SCROLL_CONTENT_H;
	sd->content_w                = ( float ) HELLO_FLUXENT_DEMO_SCROLL_CONTENT_W;
	d->scroll_nd->component_data = sd;
}

void demo_make_root(Demo *d) {
	d->root = xent_create_node(d->ctx);
	xent_set_protocol(d->ctx, d->root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, d->root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, d->root, XENT_FLEX_ALIGN_CENTER);
	xent_set_padding(d->ctx, d->root, (XentInsets) {32, 32, 32, 32});
	xent_set_gap(d->ctx, d->root, 14);
	xent_set_size(d->ctx, d->root, (XentSize) {( float ) NAN, ( float ) HELLO_FLUXENT_DEMO_SCROLL_CONTENT_H});
	xent_append_child(d->ctx, d->scroll_root, d->root);
}

void demo_add_title(Demo *d) {
	XentNodeId title = demo_create_text(d->ctx, d->store, d->root, "Hello, Fluxent!", 28.0f);
	xent_set_size(d->ctx, title, (XentSize) {480, 40});
	flux_text_set_weight(d->store, title, FLUX_FONT_BOLD);
	flux_text_set_alignment(d->store, title, FLUX_TEXT_CENTER);

	XentNodeId subtitle
	  = demo_create_text(d->ctx, d->store, d->root, "A pure-C Fluent Design UI - All Controls Showcase", 13.0f);
	xent_set_size(d->ctx, subtitle, (XentSize) {480, 20});
	flux_text_set_alignment(d->store, subtitle, FLUX_TEXT_CENTER);
	add_divider(d);
}

static void demo_add_button_variants(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	XentNodeId b1  = demo_button(d, row, "Standard", NULL, NULL);
	xent_set_size(d->ctx, b1, (XentSize) {110, 32});
	flux_node_set_tooltip(d->store, b1, "This is a standard button");

	XentNodeId b2 = demo_button(d, row, "Accent", NULL, NULL);
	xent_set_size(d->ctx, b2, (XentSize) {110, 32});
	flux_button_set_style(d->store, b2, FLUX_BUTTON_ACCENT);
	flux_node_set_tooltip(d->store, b2, "This is an accent-styled button for primary actions");

	XentNodeId b3 = demo_button(d, row, "Subtle", NULL, NULL);
	xent_set_size(d->ctx, b3, (XentSize) {110, 32});
	flux_button_set_style(d->store, b3, FLUX_BUTTON_SUBTLE);
	flux_node_set_tooltip(d->store, b3, "Subtle buttons blend into the background");
}

static void demo_add_disabled_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	for (int i = 0; i < 3; i++) {
		XentNodeId btn = demo_button(d, row, "Disabled", NULL, NULL);
		xent_set_size(d->ctx, btn, (XentSize) {110, 32});
		if (i == 1) flux_button_set_style(d->store, btn, FLUX_BUTTON_ACCENT);
		if (i == 2) flux_button_set_style(d->store, btn, FLUX_BUTTON_SUBTLE);
		flux_button_set_enabled(d->store, btn, false);
	}
}

static void demo_add_icon_buttons(Demo *d) {
	XentNodeId row = make_row(d->ctx, d->root, 8, 32);
	XentNodeId ib1 = demo_button(d, row, "Settings", NULL, NULL);
	xent_set_size(d->ctx, ib1, (XentSize) {120, 32});
	flux_button_set_icon(d->store, ib1, "Settings");

	XentNodeId ib2 = demo_button(d, row, "Search", NULL, NULL);
	xent_set_size(d->ctx, ib2, (XentSize) {110, 32});
	flux_button_set_icon(d->store, ib2, "Search");
	flux_button_set_style(d->store, ib2, FLUX_BUTTON_ACCENT);
}

static void demo_add_counter_button(Demo *d) {
	XentNodeId row  = make_row(d->ctx, d->root, 12, 32);
	XentNodeId cbtn = demo_button(d, row, "Count", on_counter_click, d->counter);
	xent_set_size(d->ctx, cbtn, (XentSize) {100, 32});
	flux_button_set_style(d->store, cbtn, FLUX_BUTTON_ACCENT);
	flux_button_set_icon(d->store, cbtn, "Add");
	flux_node_set_tooltip(d->store, cbtn, "Click to increment the counter");

	XentNodeId clbl = demo_create_text(d->ctx, d->store, row, d->counter->buf, 14.0f);
	xent_set_size(d->ctx, clbl, (XentSize) {140, 32});
	d->counter->label_node = clbl;
}

void demo_add_buttons(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Button");
	demo_add_button_variants(d);
	demo_add_disabled_buttons(d);
	demo_add_icon_buttons(d);
	demo_add_counter_button(d);
	add_divider(d);
}

void demo_add_toggle(Demo *d) {
	make_section(d->ctx, d->store, d->root, "ToggleButton");
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId tb1 = demo_button(d, row, "Toggle Me", on_toggle_click, d->toggle);
	xent_set_size(d->ctx, tb1, (XentSize) {130, 32});
	flux_set_control_type(d->ctx, tb1, FLUX_CONTROL_TOGGLE_BUTTON);
	d->toggle->node = tb1;

	XentNodeId tb2  = demo_button(d, row, "Toggled On", NULL, NULL);
	xent_set_size(d->ctx, tb2, (XentSize) {130, 32});
	flux_set_control_type(d->ctx, tb2, FLUX_CONTROL_TOGGLE_BUTTON);
	FluxNodeData *nd = flux_node_store_get(d->store, tb2);
	if (nd && nd->component_data) (( FluxButtonData * ) nd->component_data)->is_checked = true;
	add_divider(d);
}

void demo_add_repeat(Demo *d) {
	make_section(d->ctx, d->store, d->root, "RepeatButton (hold to repeat)");
	XentNodeId row = make_row(d->ctx, d->root, 12, 32);
	XentNodeId rb  = demo_repeat_button(d, row, "Hold Me", on_repeat_click, d->repeat);
	xent_set_size(d->ctx, rb, (XentSize) {120, 32});

	XentNodeId rlbl = demo_create_text(d->ctx, d->store, row, d->repeat->buf, 14.0f);
	xent_set_size(d->ctx, rlbl, (XentSize) {160, 32});
	d->repeat->label_node = rlbl;
	add_divider(d);
}

void demo_add_hyperlinks(Demo *d) {
	make_section(d->ctx, d->store, d->root, "HyperlinkButton");
	XentNodeId row = make_row(d->ctx, d->root, 16, 32);
	XentNodeId h1  = demo_hyperlink(d, row, "Visit GitHub", "https://github.com", on_hyperlink_click, NULL);
	xent_set_size(d->ctx, h1, (XentSize) {130, 32});
	XentNodeId h2 = demo_hyperlink(d, row, "xmake.io", "https://xmake.io", on_hyperlink_click, NULL);
	xent_set_size(d->ctx, h2, (XentSize) {100, 32});
	XentNodeId h3 = demo_hyperlink(d, row, "Disabled Link", NULL, NULL, NULL);
	xent_set_size(d->ctx, h3, (XentSize) {120, 32});
	flux_hyperlink_set_enabled(d->store, h3, false);
	add_divider(d);
}

void demo_add_check_switch(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Checkbox & Switch");
	XentNodeId row  = make_row(d->ctx, d->root, 20, 32);
	XentNodeId chk1 = demo_toggle(d, row, FLUX_CONTROL_CHECKBOX, "Enable feature", false, on_check_change, NULL);
	xent_set_size(d->ctx, chk1, (XentSize) {160, 32});
	XentNodeId chk2 = demo_toggle(d, row, FLUX_CONTROL_CHECKBOX, "Checked", true, on_check_change, NULL);
	xent_set_size(d->ctx, chk2, (XentSize) {110, 32});
	XentNodeId sw1 = demo_toggle(d, row, FLUX_CONTROL_SWITCH, "Wi-Fi", true, on_switch_change, NULL);
	xent_set_size(d->ctx, sw1, (XentSize) {110, 32});

	row              = make_row(d->ctx, d->root, 20, 32);
	XentNodeId chk_d = demo_toggle(d, row, FLUX_CONTROL_CHECKBOX, "Disabled", false, on_check_change, NULL);
	xent_set_size(d->ctx, chk_d, (XentSize) {120, 32});
	flux_checkbox_set_enabled(d->store, chk_d, false);
	XentNodeId sw_d = demo_toggle(d, row, FLUX_CONTROL_SWITCH, "Disabled", false, on_switch_change, NULL);
	xent_set_size(d->ctx, sw_d, (XentSize) {120, 32});
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

void demo_add_radio(Demo *d) {
	make_section(d->ctx, d->store, d->root, "RadioButton (mutually exclusive)");
	XentNodeId row      = make_row(d->ctx, d->root, 16, 32);
	d->radio->nodes [0] = demo_toggle(d, row, FLUX_CONTROL_RADIO, "Option A", true, on_radio_change, NULL);
	d->radio->nodes [1] = demo_toggle(d, row, FLUX_CONTROL_RADIO, "Option B", false, on_radio_change, NULL);
	d->radio->nodes [2] = demo_toggle(d, row, FLUX_CONTROL_RADIO, "Option C", false, on_radio_change, NULL);
	d->radio->count     = 3;
	for (int i = 0; i < d->radio->count; i++) xent_set_size(d->ctx, d->radio->nodes [i], (XentSize) {110, 32});
	demo_wire_radio_group(d);
	add_divider(d);
}
