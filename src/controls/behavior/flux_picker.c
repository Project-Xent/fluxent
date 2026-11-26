#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/behavior/flux_uia_part.h"
#include "controls/flux_module.h"
#include "controls/behavior/flux_picker_internal.h"
#include "controls/behavior/flux_flyout_binding.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_picker.h"
#include "runtime/flux_str.h"
#include "bridge/flux_internal.h"
#include "fluxent/flux_input.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FluxPickerData *picker_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || !nd->component_data) return NULL;
	if (nd->component_type != FLUX_CONTROL_DATE_PICKER && nd->component_type != FLUX_CONTROL_TIME_PICKER) return NULL;
	return ( FluxPickerData * ) nd->component_data;
}

static float picker_header_h(FluxPickerData const *pd) { return pd->header_h; }

static void  picker_flyout_accept(void *ctx) {
	FluxPickerData *pd = ( FluxPickerData * ) ctx;
	pd->open           = false;
	int previous       = pd->value;
	int value          = pd->ops->accept(pd);
	flux_picker_refresh(pd);
	if (previous == value) return;
	if (pd->on_change) pd->on_change(pd->on_change_ctx, value);
	flux_input_route_event(
	  flux_store_host(pd->store), pd->root, XTK_EVENT_VALUE_CHANGED,
	  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = value}, XTK_EVENT_CAUSE_USER
	);
}

static void picker_flyout_dismiss(void *ctx) { (( FluxPickerData * ) ctx)->open = false; }

static void picker_flyout_select(void *ctx, int column, int index) {
	FluxPickerData *pd = ( FluxPickerData * ) ctx;
	pd->ops->select(pd, column, index);
}

static bool picker_build_flyout(FluxPickerData *pd) {
	pd->flyout = flux_pflyout_create(&(FluxPickerFlyoutDesc) {
	  .ctx        = pd->ctx,
	  .store      = pd->store,
	  .input      = pd->input,
	  .window     = pd->window,
	  .overlay    = pd->overlay,
	  .owner      = pd->root,
	  .width      = pd->ops->width,
	  .time       = pd->ops->width == FLUX_TIME_PICKER_W,
	  .on_accept  = picker_flyout_accept,
	  .on_dismiss = picker_flyout_dismiss,
	  .on_select  = picker_flyout_select,
	  .userdata   = pd});
	if (!pd->flyout) return false;
	pd->ops->fill(pd, pd->flyout);
	return true;
}

static void picker_open(FluxPickerData *pd) {
	if (pd->open || !pd->window) return;
	if (!pd->flyout && !picker_build_flyout(pd)) return;

	pd->ops->load(pd);
	FluxRect box    = flux_binding_window_rect(pd->ctx, pd->store, pd->root);
	FluxRect target = {box.x, box.y + picker_header_h(pd), box.w, pd->button_h};
	pd->open        = true;
	flux_pflyout_setrtl(pd->flyout, xent_resolved_dir(pd->ctx, pd->root) == XENT_DIRECTION_RTL);
	flux_pflyout_show(pd->flyout, &target);
}

void flux_picker_close(FluxStore *store, XentNodeId id) {
	FluxPickerData *pd = picker_data(store, id);
	if (pd && pd->flyout) flux_pflyout_hide(pd->flyout);
	if (pd) pd->open = false;
}

static void picker_on_press(void *ctx, float x, float y, int clicks) {
	( void ) x;
	( void ) clicks;
	FluxPickerData *pd  = ( FluxPickerData * ) ctx;
	pd->press_valid     = true;
	pd->press_on_button = y >= picker_header_h(pd);
}

static void picker_on_click(void *ctx) {
	FluxPickerData *pd     = ( FluxPickerData * ) ctx;
	bool            landed = !pd->press_valid || pd->press_on_button;
	pd->press_valid        = false;
	if (pd->enabled && landed) flux_uia_note(pd->store, pd->button, -1, FLUX_UIA_EVT_INVOKED);
	if (pd->enabled && landed) picker_open(pd);
}

static FluxPickerData *picker_of_part(FluxStore *store, XentNodeId part) {
	XentNodeId owner = flux_uia_part_owner(store, part, NULL);
	return owner != XENT_NODE_INVALID ? picker_data(store, owner) : NULL;
}

static bool picker_uia_invoke(FluxStore *store, XentNodeId part) {
	FluxPickerData *pd = picker_of_part(store, part);
	if (!pd || !pd->enabled) return false;
	picker_open(pd);
	return true;
}

static char const *picker_parent_name(FluxPickerData const *pd) {
	char const *app = flux_uia_apptext(pd->store, pd->root, FLUX_A11Y_NAME);
	if (app) return app;
	return pd->header ? pd->header : "";
}

static void picker_uia_name(FluxStore *store, XentNodeId part, char *out, size_t size) {
	FluxPickerData *pd          = picker_of_part(store, part);
	char            value [128] = {0};
	if (!pd) return;
	if (pd->has_value && pd->ops->spoken) pd->ops->spoken(pd, value, sizeof(value));
	snprintf(out, size, "%s %s %s", picker_parent_name(pd), value, pd->ops->kind);
}

static FluxUiaHooks const picker_uia_button = {
  .role     = FLUX_UIA_BUTTON,
  .patterns = FLUX_UIA_P_INVOKE,
  .invoked  = true,
  .klass    = L"Button",
  .invoke   = picker_uia_invoke,
  .name     = picker_uia_name};

XentNodeId flux_picker_uia_delegate(FluxStore *store, XentNodeId node) {
	FluxPickerData *pd = picker_data(store, node);
	return pd ? pd->button : XENT_NODE_INVALID;
}

void flux_picker_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	FluxPickerData *pd = picker_data(store, node);
	if (pd) snprintf(out, size, "%s", pd->ops->kind);
}

static void picker_on_cancel(void *ctx) { (( FluxPickerData * ) ctx)->press_valid = false; }

void        flux_picker_sync(XentNodeId node, FluxNodeData *nd) {
	FluxPickerData *pd     = nd->component_data;
	XentRect        bounds = {0};
	xent_layout_rect(pd->ctx, node, &bounds);
	if (pd->root == node && bounds.w != pd->measured_width) flux_picker_refresh(pd);
	if (pd->open) flux_pflyout_refresh(pd->flyout);
}

static void picker_set_label(FluxPickerData *pd) {
	xent_sem_setlabel(pd->ctx, pd->root, pd->header ? pd->header : "");
	flux_uia_part_place(
	  pd->ctx, pd->button, (XentRect) {0.0f, pd->header_h, pd->measured_width, pd->button_h}, (XentPoint) {0}
	);
	flux_uia_part_label(pd->ctx, pd->button, NULL, pd->enabled);
}

static float picker_text_height(FluxPickerData const *pd, char const *text, float width, bool wrap) {
	XentTextMeasureReq request = {
	  .text             = text,
	  .font_size        = 14.0f,
	  .font_weight      = 400,
	  .width_constraint = width,
	  .max_lines        = wrap ? 0u : 1u,
	  .width_mode       = wrap ? XENT_MEASURE_AT_MOST : XENT_MEASURE_UNDEFINED};
	XentTextMetrics metrics = {0};
	if (!xent_text_measure(pd->ctx, &request, &metrics)) return 20.0f;
	return ceilf(metrics.height);
}

void flux_picker_refresh(FluxPickerData *pd) {
	XentRect bounds = {0};
	xent_layout_rect(pd->ctx, pd->root, &bounds);
	float width        = bounds.w > 0.0f ? bounds.w : pd->ops->width;
	pd->measured_width = width;
	pd->header_h       = 0.0f;
	if (pd->header && pd->header [0]) pd->header_h = picker_text_height(pd, pd->header, width, true) + 4.0f;
	pd->button_h = picker_text_height(pd, "Ag", 0.0f, false) + 11.0f;
	xent_seth(pd->ctx, pd->root, pd->button_h + pd->header_h);
	picker_set_label(pd);
}

bool flux_picker_setup(
  FluxPickerData *pd, XentCtx *ctx, FluxStore *store, XentNodeId root, FluxControlType type, float width
) {
	FluxNodeData *nd = flux_store_find(store, root);
	if (!nd) return false;

	pd->ctx            = ctx;
	pd->store          = store;
	pd->root           = root;
	pd->overlay        = XENT_NODE_INVALID;
	pd->enabled        = true;

	nd->component_type = type;
	flux_store_track_frame(store, root, type);
	nd->component_data               = pd;
	nd->destroy_component_data       = flux_picker_destroy;
	nd->behavior.on_pointer_down     = picker_on_press;
	nd->behavior.on_pointer_down_ctx = pd;
	nd->behavior.on_click            = picker_on_click;
	nd->behavior.on_click_ctx        = pd;
	nd->behavior.on_cancel           = picker_on_cancel;
	nd->behavior.on_cancel_ctx       = pd;

	xent_set_focusable(ctx, root, true);
	xent_sem_setrole(ctx, root, XENT_SEM_ROLE_BUTTON);
	pd->button = flux_uia_part(ctx, store, root, root, 0);
	flux_uia_part_hooks(store, pd->button, &picker_uia_button);
	xent_setsize(ctx, root, (XentSize) {NAN, FLUX_PICKER_BUTTON_H});
	xent_setminsize(ctx, root, (XentSize) {width, FLUX_PICKER_BUTTON_H});
	xent_setmaxsize(ctx, root, (XentSize) {456.0f, NAN});
	return true;
}

void flux_picker_destroy(void *component_data) {
	FluxPickerData *pd = ( FluxPickerData * ) component_data;
	if (!pd) return;
	flux_pflyout_destroy(pd->flyout);
	flux_str_free(pd->header);
	free(pd);
}

void flux_picker_snapshot(FluxPickerSnapshot *out, FluxPickerData const *pd) {
	out->header            = pd->header;
	out->header_h          = pd->header_h;
	out->button_h          = pd->button_h;
	out->columns           = pd->columns;
	out->has_value         = pd->has_value;
	out->open              = pd->open;
	out->press_on_button   = !pd->press_valid || pd->press_on_button;
	FluxNodeData const *nd = flux_store_find(pd->store, pd->root);
	out->pointer_on_button = nd && nd->hover_local_y >= picker_header_h(pd);
	for (int i = 0; i < pd->columns; i++) {
		out->text [i]   = pd->text [i];
		out->weight [i] = pd->weight [i];
		out->left [i]   = pd->left [i];
	}
}

void flux_picker_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id) {
	FluxPickerData *pd = picker_data(store, id);
	if (!pd) return;
	pd->ctx   = ctx;
	pd->store = store;
	pd->root  = id;
}

void flux_picker_setheader(FluxStore *store, XentNodeId id, char const *header) {
	FluxPickerData *pd = picker_data(store, id);
	if (!pd) return;
	flux_str_replace(&pd->header, header);
	flux_picker_refresh(pd);
}

void flux_picker_setenabled(FluxStore *store, XentNodeId id, bool enabled) {
	FluxPickerData *pd = picker_data(store, id);
	if (!pd) return;
	pd->enabled = enabled;
	xent_sem_setenabled(pd->ctx, pd->root, enabled);
	flux_uia_part_label(pd->ctx, pd->button, NULL, enabled);
	if (!enabled) flux_picker_close(store, id);
}

char const *flux_picker_valuetext(FluxStore *store, XentNodeId id) {
	FluxPickerData *pd = picker_data(store, id);
	if (!pd) return NULL;
	static char text [256];
	int         n = 0;
	for (int i = 0; i < pd->columns && n < ( int ) sizeof(text) - 1; i++)
		n += snprintf(text + n, sizeof(text) - ( size_t ) n, i ? " %s" : "%s", pd->text [i]);
	return text;
}
