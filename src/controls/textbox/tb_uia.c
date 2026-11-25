#include "tb_internal.h"
#include "tb_metrics.h"
#include "bridge/flux_internal.h"
#include "controls/flux_module.h"
#include "render/flux_fluent.h"
#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_control_uia.h"
#include "store/flux_node_data.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool tb_is_field(FluxControlType type) {
	return type == FLUX_CONTROL_TEXT_INPUT || type == FLUX_CONTROL_PASSWORD_BOX || type == FLUX_CONTROL_NUMBER_BOX;
}

static FluxTextBoxInputData *tb_uia_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	bool ok = nd && (nd->component_type == FLUX_CONTROL_TEXT_INPUT || nd->component_type == FLUX_CONTROL_NUMBER_BOX);
	return ok ? ( FluxTextBoxInputData * ) nd->component_data : NULL;
}

void flux_textbox_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	if (!out || size == 0) return;
	out [0] = '\0';
	flux_suggest_uia_boxname(store, node, out, size);
	if (out [0]) return;
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || !tb_is_field(nd->component_type) || !nd->component_data) return;
	char const *placeholder = (( FluxTextBoxInputData * ) nd->component_data)->base.placeholder;
	if (placeholder && placeholder [0]) snprintf(out, size, "%s", placeholder);
}

bool flux_textbox_uia_view(FluxStore *store, XentNodeId node, FluxUiaTextView *out) {
	FluxTextBoxInputData *tb   = tb_uia_data(store, node);
	XentRect              rect = {0};
	if (!tb) return false;
	xent_layout_rect(tb->ctx, node, &rect);
	float visible = tb_text_max_w(tb, flux_node_type(store, node));
	*out          = (FluxUiaTextView) {
	  .renderer   = tb->app ? flux_app_text(tb->app) : NULL,
	  .text       = tb_sync_content(tb),
	  .style      = tb_make_style(tb),
	  .layout_w   = tb_layout_width(tb, visible),
	  .origin_x   = FLUX_TEXTBOX_PAD_L - tb->base.scroll_offset_x,
	  .origin_y   = FLUX_TEXTBOX_PAD_T - tb->base.scroll_offset_y,
	  .clip       = {FLUX_TEXTBOX_PAD_L, FLUX_TEXTBOX_PAD_T, visible, tb_visible_text_height(tb, &rect)},
	  .sel_start  = tb->base.selection_start,
	  .sel_end    = tb->base.selection_end,
	  .selectable = true,
	  .readonly   = tb->base.readonly,
	  .paragraphs = true,
	};
	out->style.vert_align = FLUX_TEXT_TOP;
	out->style.color      = tb->base.text_color;
	return true;
}

void flux_textbox_uia_select(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end) {
	FluxTextBoxInputData *tb = tb_uia_data(store, node);
	if (!tb) return;
	tb->caret_column_held    = false;
	tb->base.selection_start = start;
	tb->base.selection_end   = end;
	tb->base.cursor_position = end;
	tb_update_scroll(tb);
}

void flux_textbox_uia_reveal(FluxStore *store, XentNodeId node, uint32_t start, uint32_t end) {
	FluxTextBoxInputData *tb = tb_uia_data(store, node);
	if (!tb) return;
	uint32_t cursor          = tb->base.cursor_position;
	tb->base.cursor_position = end;
	tb_update_scroll(tb);
	tb->base.cursor_position = start;
	tb_update_scroll(tb);
	tb->base.cursor_position = cursor;
}

static FluxTextBoxInputData *nb_uia_data(FluxStore *store, XentNodeId node) {
	FluxTextBoxInputData *tb = tb_uia_data(store, node);
	return tb && tb->nb ? tb : NULL;
}

static bool nb_uia_spin(FluxStore *store, XentNodeId node) { return xent_sem_expanded(flux_store_ctx(store), node); }

int         flux_number_uia_heads(FluxStore *store, XentNodeId node) {
	if (!nb_uia_data(store, node)) return 0;
	return nb_uia_spin(store, node) ? 3 : 1;
}

int flux_number_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns) {
	( void ) store;
	( void ) node;
	bool edit = sub == FLUX_UIA_SUB_HEAD;
	*patterns = edit ? FLUX_UIA_P_VALUE | FLUX_UIA_P_TEXT | FLUX_UIA_P_TEXT2 : FLUX_UIA_P_INVOKE;
	return edit ? FLUX_UIA_EDIT : FLUX_UIA_BUTTON;
}

bool flux_number_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect) {
	XentRect r = *rect;
	float    x = r.x + r.w - FLUX_NUMBER_BOX_SPIN_W;
	if (sub == FLUX_UIA_SUB_HEAD + 1) {
		float margin = FLUX_NUMBER_BOX_UP_MARGIN;
		*rect = (XentRect) {x + margin, r.y + margin, FLUX_NUMBER_BOX_SPIN_BTN_W, fmaxf(0.0f, r.h - 2.0f * margin)};
	}
	if (sub == FLUX_UIA_SUB_HEAD + 2)
		*rect = (XentRect) {
		  x + FLUX_NUMBER_BOX_COL_UP_W + FLUX_NUMBER_BOX_DN_MARGIN_L, r.y + FLUX_NUMBER_BOX_DN_MARGIN_T,
		  FLUX_NUMBER_BOX_SPIN_BTN_W, fmaxf(0.0f, r.h - FLUX_NUMBER_BOX_DN_MARGIN_T - FLUX_NUMBER_BOX_DN_MARGIN_B)};
	( void ) store;
	( void ) node;
	return true;
}

static int nb_uia_shortest(char *buf, size_t size, double v, bool fixed) {
	for (int p = 0; p <= 340; p++) {
		int n = fixed ? snprintf(buf, size, "%.*f", p, v) : snprintf(buf, size, "%.*e", p > 16 ? 16 : p, v);
		if (strtod(buf, NULL) == v || (!fixed && p >= 16)) return n;
	}
	return ( int ) strlen(buf);
}

static void nb_uia_number(char *out, size_t size, double v) {
	char fixed [400], sci [32];
	int  nf = nb_uia_shortest(fixed, sizeof(fixed), v, true);
	int  ns = nb_uia_shortest(sci, sizeof(sci), v, false);
	snprintf(out, size, "%s", nf <= ns ? fixed : sci);
}

static void nb_uia_bound(char *out, size_t size, char const *what, double v, double limit) {
	size_t n = strlen(out);
	char   num [400];
	nb_uia_number(num, sizeof(num), v);
	if (v != limit && n < size) snprintf(out + n, size - n, " %s%s", what, num);
}

static char const *nb_uia_label(FluxStore *store, XentNodeId node) {
	char const *name = flux_uia_apptext(store, node, FLUX_A11Y_NAME);
	return name && name [0] ? name : xent_sem_label(flux_store_ctx(store), node);
}

static void nb_uia_editor_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	FluxTextBoxInputData *tb = nb_uia_data(store, node);
	if (!tb) return;
	char const *label = nb_uia_label(store, node);
	if (!label || !label [0]) {
		snprintf(out, size, "%s", tb->base.placeholder ? tb->base.placeholder : "");
		return;
	}
	snprintf(out, size, "%s", label);
	nb_uia_bound(out, size, "Minimum", tb->nb->minimum, -DBL_MAX);
	nb_uia_bound(out, size, "Maximum", tb->nb->maximum, DBL_MAX);
}

void flux_number_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size) {
	if (sub == FLUX_UIA_SUB_HEAD + 1) snprintf(out, size, "Increase");
	if (sub == FLUX_UIA_SUB_HEAD + 2) snprintf(out, size, "Decrease");
	if (sub == FLUX_UIA_SUB_HEAD) nb_uia_editor_name(store, node, out, size);
}

bool flux_number_uia_focused(FluxStore *store, XentNodeId node, int sub) {
	( void ) store;
	( void ) node;
	return sub == FLUX_UIA_SUB_HEAD;
}

bool flux_number_uia_enabled(FluxStore *store, XentNodeId node, int sub) {
	FluxTextBoxInputData *tb = nb_uia_data(store, node);
	if (!tb || !xent_sem_enabled(flux_store_ctx(store), node)) return false;
	if (sub == FLUX_UIA_SUB_HEAD) return true;
	return nb_spin_enabled(tb->nb, sub == FLUX_UIA_SUB_HEAD + 1);
}

bool flux_number_uia_invoke(FluxStore *store, XentNodeId node, int sub) {
	FluxTextBoxInputData *tb = nb_uia_data(store, node);
	if (!tb || sub == FLUX_UIA_SUB_HEAD || !flux_number_uia_enabled(store, node, sub)) return false;
	flux_uia_note(store, node, sub, FLUX_UIA_EVT_INVOKED);
	nb_step_value(tb, sub == FLUX_UIA_SUB_HEAD + 1 ? tb->nb->small_change : -tb->nb->small_change);
	return true;
}

void flux_number_uia_settext(FluxStore *store, XentNodeId node, char const *text) {
	FluxTextBoxInputData *tb = nb_uia_data(store, node);
	if (tb) tb_replace_text(tb, text);
}

void flux_number_uia_steps(FluxStore *store, XentNodeId node, double *small_step, double *large_step) {
	FluxTextBoxInputData *tb = nb_uia_data(store, node);
	if (!tb) return;
	*small_step = tb->nb->small_change;
	*large_step = tb->nb->large_change;
}
