/**
 * @file test_fx_el.c
 * @brief Headless test for the xtk element layer: tree shape, conditional
 * rendering, arena string ownership, zero-value defaults, and frame reuse.
 */
#include <fluxent/flux_app.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum
{
	MSG_NONE = 0,
	MSG_CLICK,
	MSG_TOGGLE,
};

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static XtkEl *build(XtkUi *ui, bool show_reset, char const *transient) {
	return xtk_scroll(
	  ui, (XtkEl *[]) {
			xtk_column(
			  ui, (XtkStackDesc) {.gap = 12, .padding = {32, 32, 32, 32}},
			  (XtkEl *[]) {
								 xtk_text(ui, transient, (XtkTextDesc) {.size = 24}),
								 xtk_button(ui, "Increment", (XtkButtonDesc) {.on_click = xtk_msg_i(MSG_CLICK, 7)}),
								 show_reset ? xtk_button(ui, "Reset", (XtkButtonDesc) {.disabled = true}) : NULL,
								 xtk_keyed(
				  ui, "row-1",
								 xtk_row(
					ui, (XtkStackDesc) {.gap = 8},
								 (XtkEl *[]) {
					  xtk_checkbox(ui, "Check", (XtkToggleDesc) {.checked = true, .on_change = xtk_msg(MSG_TOGGLE)}),
					  xtk_slider(ui, (XtkSliderDesc) {0}), XTK_END}
				  )
				), XTK_END}
			),
			XTK_END
    }
	);
}

int main(void) {
	XtkUi *ui = xtk_ui_create();
	EXPECT(ui, "ui creation");

	xtk_ui_begin(ui);

	char transient [32];
	snprintf(transient, sizeof(transient), "Frame %d", 1);
	XtkEl *root = build(ui, false, transient);
	memset(transient, 'X', sizeof(transient) - 1);

	EXPECT(root && root->type == XTK_CONTROL_SCROLL, "root is a scroll");
	EXPECT(root->child_count == 1, "scroll has one child");

	XtkEl *col = root->children [0];
	EXPECT(col->type == XTK_CONTROL_CONTAINER && !col->stack_row, "column container");
	EXPECT(col->child_count == 3, "NULL conditional skipped");
	EXPECT(col->stack.gap == 12.0f && col->stack.padding.left == 32.0f, "stack desc carried");

	XtkEl *title = col->children [0];
	EXPECT(strcmp(title->text, "Frame 1") == 0, "string copied into arena, not borrowed");
	EXPECT(title->text != ( char const * ) transient, "arena copy is a distinct pointer");
	EXPECT(title->text_props.size == 24.0f, "text size carried");
	EXPECT(isnan(title->width) && isnan(title->height), "size defaults to auto");

	XtkEl *inc = col->children [1];
	EXPECT(inc->type == XTK_CONTROL_BUTTON && !inc->button.disabled, "button enabled by default");
	EXPECT(inc->button.on_click.tag == MSG_CLICK && inc->button.on_click.i == 7, "message payload carried");

	XtkEl *row = col->children [2];
	EXPECT(row->key && strcmp(row->key, "row-1") == 0, "key copied");
	EXPECT(row->stack_row && row->child_count == 2, "row with two children");
	EXPECT(row->children [1]->slider.max == 100.0f, "slider {0} defaults to 0..100");

	XtkEl *again = build(ui, true, "second");
	EXPECT(again->children [0]->child_count == 4, "conditional included when true");

	XtkEl **list = xtk_children_alloc(ui, 8);
	list [0]      = xtk_text(ui, "a", (XtkTextDesc) {0});
	list [5]      = xtk_text(ui, "b", (XtkTextDesc) {0});
	XtkEl *holes = xtk_column(ui, (XtkStackDesc) {0}, list);
	EXPECT(holes->child_count == 2, "children_alloc holes skipped");

	char *fmt = xtk_fmt(ui, "%s-%04d", "id", 42);
	EXPECT(fmt && strcmp(fmt, "id-0042") == 0, "xtk_fmt formats into arena");

	for (int frame = 0; frame < 5000; frame++) {
		xtk_ui_begin(ui);
		XtkEl *r = build(ui, frame & 1, "loop");
		if (!r) return 1;
	}

	xtk_ui_destroy(ui);
	printf("PASS: xtk element layer\n");
	return 0;
}
