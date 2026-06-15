/**
 * @file test_fx_el.c
 * @brief Headless test for the FX description layer: tree shape, conditional
 * rendering, arena string ownership, zero-value defaults, and frame reuse.
 */
#include <fluxent/fx.h>
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

static FluxEl *build(FxUi *ui, bool show_reset, char const *transient) {
	return fx_scroll(
	  ui, (FluxEl *[]) {
			fx_column(
			  ui, (FxStackDesc) {.gap = 12, .padding = {32, 32, 32, 32}},
			  (FluxEl *[]) {
								 fx_text(ui, transient, (FxTextDesc) {.size = 24}),
								 fx_button(ui, "Increment", (FxButtonDesc) {.on_click = fx_msg_i(MSG_CLICK, 7)}),
								 show_reset ? fx_button(ui, "Reset", (FxButtonDesc) {.disabled = true}) : NULL,
								 fx_keyed(
				  ui, "row-1",
								 fx_row(
					ui, (FxStackDesc) {.gap = 8},
								 (FluxEl *[]) {
					  fx_checkbox(ui, "Check", (FxToggleDesc) {.checked = true, .on_change = fx_msg(MSG_TOGGLE)}),
					  fx_slider(ui, (FxSliderDesc) {0}), FX_END}
				  )
				), FX_END}
			),
			FX_END
    }
	);
}

int main(void) {
	FxUi *ui = fx_ui_create();
	EXPECT(ui, "ui creation");

	fx_ui_begin(ui);

	char transient [32];
	snprintf(transient, sizeof(transient), "Frame %d", 1);
	FluxEl *root = build(ui, false, transient);
	memset(transient, 'X', sizeof(transient) - 1);

	EXPECT(root && root->type == FLUX_CONTROL_SCROLL, "root is a scroll");
	EXPECT(root->child_count == 1, "scroll has one child");

	FluxEl *col = root->children [0];
	EXPECT(col->type == FLUX_CONTROL_CONTAINER && !col->stack_row, "column container");
	EXPECT(col->child_count == 3, "NULL conditional skipped");
	EXPECT(col->stack.gap == 12.0f && col->stack.padding.left == 32.0f, "stack desc carried");

	FluxEl *title = col->children [0];
	EXPECT(strcmp(title->text, "Frame 1") == 0, "string copied into arena, not borrowed");
	EXPECT(title->text != ( char const * ) transient, "arena copy is a distinct pointer");
	EXPECT(title->text_props.size == 24.0f, "text size carried");
	EXPECT(isnan(title->width) && isnan(title->height), "size defaults to auto");

	FluxEl *inc = col->children [1];
	EXPECT(inc->type == FLUX_CONTROL_BUTTON && !inc->button.disabled, "button enabled by default");
	EXPECT(inc->button.on_click.tag == MSG_CLICK && inc->button.on_click.i == 7, "message payload carried");

	FluxEl *row = col->children [2];
	EXPECT(row->key && strcmp(row->key, "row-1") == 0, "key copied");
	EXPECT(row->stack_row && row->child_count == 2, "row with two children");
	EXPECT(row->children [1]->slider.max == 100.0f, "slider {0} defaults to 0..100");

	FluxEl *again = build(ui, true, "second");
	EXPECT(again->children [0]->child_count == 4, "conditional included when true");

	FluxEl **list = fx_children_alloc(ui, 8);
	list [0]      = fx_text(ui, "a", (FxTextDesc) {0});
	list [5]      = fx_text(ui, "b", (FxTextDesc) {0});
	FluxEl *holes = fx_column(ui, (FxStackDesc) {0}, list);
	EXPECT(holes->child_count == 2, "children_alloc holes skipped");

	char *fmt = fx_fmt(ui, "%s-%04d", "id", 42);
	EXPECT(fmt && strcmp(fmt, "id-0042") == 0, "fx_fmt formats into arena");

	for (int frame = 0; frame < 5000; frame++) {
		fx_ui_begin(ui);
		FluxEl *r = build(ui, frame & 1, "loop");
		if (!r) return 1;
	}

	fx_ui_destroy(ui);
	printf("PASS: fx element layer\n");
	return 0;
}
