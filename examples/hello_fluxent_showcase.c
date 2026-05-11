#include "hello_fluxent_demo.h"

static void on_slider_change(void *ctx, float v) {
	( void ) ctx;
	( void ) v;
}

static void demo_add_slider(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Slider");
	XentNodeId sld = demo_slider(d, d->root, 0.0f, 100.0f, 50.0f, on_slider_change, NULL);
	xent_set_size(d->ctx, sld, (XentSize) {480, 32});
	add_divider(d);
}

static void demo_add_progress(Demo *d) {
	make_section(d->ctx, d->store, d->root, "ProgressBar");
	XentNodeId prog1 = demo_progress(d, d->root, 65.0f, 100.0f);
	xent_set_size(d->ctx, prog1, (XentSize) {480, 20});
	XentNodeId prog2 = demo_progress(d, d->root, 0.0f, 100.0f);
	xent_set_size(d->ctx, prog2, (XentSize) {480, 20});
	flux_progress_set_indeterminate(d->store, prog2, true);

	make_section(d->ctx, d->store, d->root, "ProgressRing");
	XentNodeId row   = make_row(d->ctx, d->root, 20, 48);
	XentNodeId ring1 = demo_progress_ring(d, row, 75.0f, 100.0f);
	XentNodeId ring2 = demo_progress_ring(d, row, 0.0f, 0.0f);
	XentNodeId ring3 = demo_progress_ring(d, row, 40.0f, 100.0f);
	xent_set_size(d->ctx, ring1, (XentSize) {32, 32});
	xent_set_size(d->ctx, ring2, (XentSize) {32, 32});
	xent_set_size(d->ctx, ring3, (XentSize) {48, 48});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "75%", 12.0f), (XentSize) {35, 20});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "Loading...", 12.0f), (XentSize) {72, 20});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "40%", 12.0f), (XentSize) {35, 20});
	add_divider(d);
}

static void demo_add_badge(Demo *d) {
	make_section(d->ctx, d->store, d->root, "InfoBadge");
	XentNodeId row = make_row(d->ctx, d->root, 14, 24);
	XentNodeId bd  = demo_badge(d, row, FLUX_BADGE_DOT, 0);
	xent_set_size(d->ctx, bd, (XentSize) {8, 8});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "Dot", 12.0f), (XentSize) {28, 20});
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 3), (XentSize) {16, 16});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "= 3", 12.0f), (XentSize) {28, 20});
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 42), (XentSize) {24, 16});
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "= 42", 12.0f), (XentSize) {36, 20});
	xent_set_size(d->ctx, demo_badge(d, row, FLUX_BADGE_NUMBER, 999), (XentSize) {28, 16});
	XentNodeId bi = demo_badge(d, row, FLUX_BADGE_ICON, 0);
	xent_set_size(d->ctx, bi, (XentSize) {16, 16});
	flux_info_badge_set_icon(d->store, bi, "Info");
	xent_set_size(d->ctx, demo_create_text(d->ctx, d->store, row, "Icon", 12.0f), (XentSize) {36, 20});
	add_divider(d);
}

static void demo_add_card(Demo *d) {
	make_section(d->ctx, d->store, d->root, "Card");
	XentNodeId card = demo_create_card(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, card, (XentSize) {480, 148});
	xent_set_protocol(d->ctx, card, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(d->ctx, card, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(d->ctx, card, XENT_FLEX_ALIGN_START);
	xent_set_padding(d->ctx, card, (XentInsets) {16, 16, 16, 16});
	xent_set_gap(d->ctx, card, 8);

	XentNodeId ct = demo_create_text(d->ctx, d->store, card, "This is a Card", 16.0f);
	xent_set_size(d->ctx, ct, (XentSize) {440, 24});
	flux_text_set_weight(d->store, ct, FLUX_FONT_SEMI_BOLD);
	XentNodeId body = demo_create_text(
	  d->ctx, d->store, card, "Cards group related content with a subtle background and border.", 13.0f
	);
	xent_set_size(d->ctx, body, (XentSize) {440, 44});

	XentNodeId row = make_row(d->ctx, card, 8, 32);
	xent_set_size(d->ctx, row, (XentSize) {440, 32});
	XentNodeId action = demo_button(d, row, "Action", NULL, NULL);
	xent_set_size(d->ctx, action, (XentSize) {100, 32});
	flux_button_set_style(d->store, action, FLUX_BUTTON_ACCENT);
	xent_set_size(d->ctx, demo_button(d, row, "Cancel", NULL, NULL), (XentSize) {100, 32});
	add_divider(d);
}

static void demo_add_footer_placeholder(Demo *d) {
	make_section(d->ctx, d->store, d->root, "TextBox / PasswordBox / NumberBox");
	d->footer_div = demo_create_divider(d->ctx, d->store, d->root);
	xent_set_size(d->ctx, d->footer_div, (XentSize) {480, 1});
	d->footer
	  = demo_create_text(d->ctx, d->store, d->root, "xent-core + fluxent - Mica backdrop - All controls demo", 11.0f);
	xent_set_size(d->ctx, d->footer, (XentSize) {480, 20});
	flux_text_set_alignment(d->store, d->footer, FLUX_TEXT_CENTER);
	d->spacer = xent_create_node(d->ctx);
	xent_set_size(d->ctx, d->spacer, (XentSize) {1, 32});
	xent_append_child(d->ctx, d->root, d->spacer);
}

void demo_build_static_content(Demo *d) {
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
