/**
 * @file test_fx_rows.c
 * @brief Repro for inconsistent row layout after keyed add/remove sequences.
 *
 * Mirrors hello_fx's dynamic list: add 9 keyed rows (one frame each), remove
 * ids 0/1/2/5 (one frame each), lay out, and assert every surviving row has
 * identical geometry (same text width, same button offset, same height).
 */
#include "fx_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum
{
	MSG_NONE = 0,
	MSG_ADD,
	MSG_REMOVE,
};

typedef struct Model {
	int row_ids [64];
	int row_count;
	int next_id;
} Model;

static void update(void *model, FxMsg msg) {
	Model *m = ( Model * ) model;
	if (msg.tag == MSG_ADD && m->row_count < 64) m->row_ids [m->row_count++] = m->next_id++;
	if (msg.tag == MSG_REMOVE) {
		for (int i = 0; i < m->row_count; i++) {
			if (m->row_ids [i] != msg.i) continue;
			memmove(&m->row_ids [i], &m->row_ids [i + 1], sizeof(int) * ( size_t ) (m->row_count - i - 1));
			m->row_count--;
			break;
		}
	}
}

static FluxEl *view(FxUi *ui, void *model) {
	Model   *m    = ( Model * ) model;
	FluxEl **rows = fx_children_alloc(ui, m->row_count);
	for (int i = 0; i < m->row_count; i++) {
		int id   = m->row_ids [i];
		rows [i] = fx_keyed(
		  ui, fx_fmt(ui, "row-%d", id),
		  fx_row(
			ui, (FxStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
			(FluxEl *[]) {
			  fx_text(ui, fx_fmt(ui, "Row #%d", id), (FxTextDesc) {0}),
			  fx_button(ui, "Remove", (FxButtonDesc) {.on_click = fx_msg_i(MSG_REMOVE, id)}), FX_END}
		  )
		);
	}
	return fx_column(
	  ui,
	  (FxStackDesc) {
		.gap = 8, .padding = {32, 32, 32, 32}
    },
	  rows
	);
}

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

int main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	xent_set_protocol(ctx, host, XENT_PROTOCOL_FLEX);

	Model      m  = {0};
	FxRuntime *rt = fx_runtime_create(ctx, store, host, NULL, &m, update, view);
	EXPECT(rt, "runtime");

	/* Interleave adds and removals so later rows mount on recycled node ids:
	 * a recycled id must not inherit its previous occupant's style. */
	for (int i = 0; i < 6; i++) {
		fx_runtime_post(rt, fx_msg(MSG_ADD));
		fx_runtime_frame(rt);
		xent_layout(ctx, host, 640.0f, 720.0f);
	}
	int removals [] = {0, 1, 2};
	for (int i = 0; i < 3; i++) {
		fx_runtime_post(rt, fx_msg_i(MSG_REMOVE, removals [i]));
		fx_runtime_frame(rt);
		xent_layout(ctx, host, 640.0f, 720.0f);
	}
	for (int i = 0; i < 3; i++) {
		fx_runtime_post(rt, fx_msg(MSG_ADD));
		fx_runtime_frame(rt);
		xent_layout(ctx, host, 640.0f, 720.0f);
	}
	fx_runtime_post(rt, fx_msg_i(MSG_REMOVE, 5));
	fx_runtime_frame(rt);
	xent_layout(ctx, host, 640.0f, 720.0f);

	EXPECT(rt->root->child_count == 5, "five rows survive");

	float    text_w = NAN, btn_x_off = NAN, row_h = NAN;
	XentRect first_row = {0};
	bool     ok        = true;
	for (int i = 0; i < rt->root->child_count; i++) {
		FxNode  *row = rt->root->children [i];
		XentRect rr = {0}, tr = {0}, br = {0};
		xent_get_layout_rect(ctx, row->node, &rr);
		xent_get_layout_rect(ctx, row->children [0]->node, &tr);
		xent_get_layout_rect(ctx, row->children [1]->node, &br);
		if (i == 0) first_row = rr;

		printf(
		  "row[%d] node=%u rect={%.1f,%.1f %.1fx%.1f} text_w=%.1f btn_off=%.1f btn={%.1fx%.1f}\n", i, row->node, rr.x,
		  rr.y, rr.w, rr.h, tr.w, br.x - rr.x, br.w, br.h
		);

		if (i == 0) {
			text_w    = tr.w;
			btn_x_off = br.x - rr.x;
			row_h     = rr.h;
		}
		else {
			if (tr.w != text_w || br.x - rr.x != btn_x_off || rr.h != row_h) ok = false;
		}
	}
	EXPECT(ok, "all rows share identical geometry");
	( void ) first_row;

	fx_runtime_destroy(rt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: fx rows geometry\n");
	return 0;
}
