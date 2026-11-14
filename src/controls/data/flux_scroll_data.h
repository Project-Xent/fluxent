#ifndef FLUX_SCROLL_DATA_H
#define FLUX_SCROLL_DATA_H

#include <fluxent/flux_types.h>

#include <stdint.h>

typedef enum FluxScrollBarVis
{
	FLUX_SCROLL_AUTO,
	FLUX_SCROLL_ALWAYS,
	FLUX_SCROLL_NEVER,
} FluxScrollBarVis;

typedef struct FluxScrollData {
	float            scroll_x;
	float            scroll_y;
	float            content_w;
	float            content_h;
	FluxScrollBarVis h_vis;
	FluxScrollBarVis v_vis;

	uint8_t          content_manual;

	uint8_t          virtualized;
	float            origin_x;
	float            origin_y;

	float            snap_y;

	uint8_t          reports;
	uint8_t          pin_end;
	uint8_t          user_moved;
	float            reported_x;
	float            reported_y;
	float            max_x;
	float            max_y;

	uint8_t          mouse_over;
	float            mouse_local_x;
	float            mouse_local_y;
	uint8_t          drag_axis;
	uint8_t          v_up_pressed;
	uint8_t          v_dn_pressed;
	uint8_t          h_lf_pressed;
	uint8_t          h_rg_pressed;
	double           last_activity_time;

	void            *dmanip_viewport;
	void             (*on_offset_changed)(void *ctx);
	void            *on_offset_changed_ctx;
} FluxScrollData;

static float inline flux_scroll_off_x(FluxScrollData const *sd) { return sd->scroll_x - sd->origin_x; }

static float inline flux_scroll_off_y(FluxScrollData const *sd) { return sd->scroll_y - sd->origin_y; }

static void inline flux_scroll_offset_changed(FluxScrollData *sd) {
	if (sd && sd->on_offset_changed) sd->on_offset_changed(sd->on_offset_changed_ctx);
}

#endif
