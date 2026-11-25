#ifndef FLUX_TB_METRICS_H
#define FLUX_TB_METRICS_H

#include <stdbool.h>

#define FLUX_TEXTBOX_ACTION_BUTTON_W 30.0f
#define FLUX_PASSWORD_REVEAL_BTN_W   FLUX_TEXTBOX_ACTION_BUTTON_W

#define FLUX_NUMBER_BOX_SPIN_BTN_W   32.0f
#define FLUX_NUMBER_BOX_UP_MARGIN    4.0f
#define FLUX_NUMBER_BOX_DN_MARGIN_L  0.0f
#define FLUX_NUMBER_BOX_DN_MARGIN_T  4.0f
#define FLUX_NUMBER_BOX_DN_MARGIN_R  4.0f
#define FLUX_NUMBER_BOX_DN_MARGIN_B  4.0f
#define FLUX_NUMBER_BOX_COL_UP_W     (FLUX_NUMBER_BOX_SPIN_BTN_W + FLUX_NUMBER_BOX_UP_MARGIN * 2.0f)
#define FLUX_NUMBER_BOX_COL_DN_W                                                             \
	(FLUX_NUMBER_BOX_SPIN_BTN_W + FLUX_NUMBER_BOX_DN_MARGIN_L + FLUX_NUMBER_BOX_DN_MARGIN_R)
#define FLUX_NUMBER_BOX_SPIN_W       (FLUX_NUMBER_BOX_COL_UP_W + FLUX_NUMBER_BOX_COL_DN_W)
#define FLUX_NUMBER_BOX_DELETE_BTN_W 40.0f

static float inline tb_clear_button_width(bool suggest_editor) {
	return suggest_editor ? 32.0f : FLUX_TEXTBOX_ACTION_BUTTON_W;
}

static bool inline tb_clear_button_hit(float local_x, float textbox_width, float button_width, float right_reserve) {
	float end = textbox_width - right_reserve;
	return local_x >= end - button_width && local_x < end;
}

static int inline nb_inline_spin_at(float x, float y, float w, float h) {
	if (y < FLUX_NUMBER_BOX_UP_MARGIN || y >= h - FLUX_NUMBER_BOX_UP_MARGIN) return 0;
	float up   = w - FLUX_NUMBER_BOX_SPIN_W + FLUX_NUMBER_BOX_UP_MARGIN;
	float down = w - FLUX_NUMBER_BOX_COL_DN_W + FLUX_NUMBER_BOX_DN_MARGIN_L;
	if (x >= up && x < up + FLUX_NUMBER_BOX_SPIN_BTN_W) return 1;
	return x >= down && x < down + FLUX_NUMBER_BOX_SPIN_BTN_W ? -1 : 0;
}

#endif
