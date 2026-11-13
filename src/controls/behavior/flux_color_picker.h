#ifndef FLUX_COLOR_PICKER_H
#define FLUX_COLOR_PICKER_H
#include <fluxent/flux_node_store.h>
#include <fluxent/flux_types.h>
#include <xtk/xtk.h>
#include <xtk/xtk_batch.h>
#include <xent/xent_display.h>

typedef struct FluxControlEnv   FluxControlEnv;
typedef struct FluxNodeData     FluxNodeData;
typedef struct FluxRenderCtx    FluxRenderCtx;
typedef struct FluxRenderSnap   FluxRenderSnap;
typedef struct FluxControlState FluxControlState;

typedef struct FluxColorPickerSnapshot {
	FluxColor color;
	float     hue, saturation, value;
	uint8_t   spectrum;
	bool      alpha, hsv, large;
	bool      more_button, opened, more_hover, more_pressed, more_focus;
} FluxColorPickerSnapshot;

FluxColor   flux_color_fromhsv(float hue, float saturation, float value, float alpha);
void        flux_color_tohsv(FluxColor color, float *hue, float *saturation, float *value);
XentNodeId  flux_colorpicker_init(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env);
void        flux_colorpicker_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
bool        flux_colorpicker_set(FluxStore *store, XentNodeId node, XtkPropertyId property, XtkValue const *value);
void        flux_colorpicker_sync(XentNodeId node, FluxNodeData *nd);
void        flux_colorpicker_snapshot(FluxColorPickerSnapshot *out, void const *data);
char const *flux_colorpicker_label(int row, bool hsv);
XentDl     *flux_colorpicker_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
bool flux_colorpicker_slider_record(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
#endif
