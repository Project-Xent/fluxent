#include "controls/behavior/flux_color_picker.h"
#include "controls/factory/flux_control_build.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_value.h"
#include "controls/factory/flux_init_textbox.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/data/flux_slider_data.h"
#include "controls/data/flux_textbox_data.h"
#include "render/flux_color_picker_geom.h"
#include "host/flux_prop_apply.h"
#include "bridge/flux_internal.h"
#include <fluxent/flux_input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum
{
	CP_RED,
	CP_GREEN,
	CP_BLUE,
	CP_HUE,
	CP_SATURATION,
	CP_VALUE,
	CP_ALPHA,
	CP_HEX,
	CP_FIELDS,
};

#define CP_KEEP_RGB   (1u << CP_RED | 1u << CP_GREEN | 1u << CP_BLUE)
#define CP_KEEP_HSV   (1u << CP_HUE | 1u << CP_SATURATION | 1u << CP_VALUE)
#define CP_KEEP_THIRD (1u << CP_FIELDS)
#define CP_KEEP_ALPHA (1u << (CP_FIELDS + 1))

typedef struct ColorPickerData ColorPickerData;

typedef struct ColorField {
	ColorPickerData *owner;
	int              index;
	void             (*on_focus)(void *ctx);
	void            *on_focus_ctx;
	void             (*on_blur)(void *ctx);
	void            *on_blur_ctx;
} ColorField;

struct ColorPickerData {
	FluxColorPickerSnapshot visual;
	XentCtx                *ctx;
	FluxStore              *store;
	XentNodeId              node, third, alpha_slider, combo, more, field [CP_FIELDS];
	ColorField              binding [CP_FIELDS];
	bool                    (*alpha_key)(void *ctx, FluxKey key, bool down);
	void                   *alpha_key_ctx;
	char                    previous [16];
	bool                    valid, updating, dragging, dirty, layout_enabled;
};

static char const *const kLabels [CP_FIELDS] = {"Red", "Green", "Blue", "Hue", "Saturation", "Value", "Opacity", NULL};
static uint32_t const    kMaxLength [CP_FIELDS] = {3, 3, 3, 3, 3, 3, 4, 7};

static ColorPickerData  *cp_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_COLOR_PICKER) return NULL;
	return nd->component_data;
}

void flux_colorpicker_snapshot(FluxColorPickerSnapshot *out, void const *data) {
	ColorPickerData const *d    = data;
	FluxNodeData const    *more = flux_store_find(d->store, d->more);
	*out                        = d->visual;
	out->more_hover             = more && more->state.hovered;
	out->more_pressed           = more && more->state.pressed;
	out->more_focus             = more && more->state.focused && !more->state.suppress_focus;
}

char const *flux_colorpicker_label(int row, bool hsv) {
	if (row < 3) return kLabels [hsv ? CP_HUE + row : CP_RED + row];
	return kLabels [CP_ALPHA];
}

static FluxSliderData *cp_slider(ColorPickerData *d, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(d->store, node);
	return nd ? nd->component_data : NULL;
}

static FluxTextBoxData *cp_textbox(ColorPickerData *d, int index) {
	FluxNodeData *nd = flux_store_find(d->store, d->field [index]);
	return nd ? nd->component_data : NULL;
}

static char const *cp_text(ColorPickerData *d, int index) {
	FluxTextBoxData *td = cp_textbox(d, index);
	return td && td->content ? td->content : "";
}

static float cp_alpha(ColorPickerData const *d) { return flux_color_af(d->visual.color); }

static void  cp_write(ColorPickerData *d, int index, char const *text) {
	d->updating = true;
	flux_prop_textbox_content(d->store, d->field [index], text);
	d->updating = false;
}

static void cp_field_text(ColorPickerData const *d, int field, int const *values, char *text, size_t size) {
	uint32_t rgba = d->visual.color.rgba;
	if (field != CP_HEX) snprintf(text, size, field == CP_ALPHA ? "%d%%" : "%d", values [field]);
	else if (d->visual.alpha) snprintf(text, size, "#%02X%06X", rgba & 255u, rgba >> 8);
	else snprintf(text, size, "#%06X", rgba >> 8);
}

static void cp_write_fields(ColorPickerData *d, uint32_t keep) {
	uint32_t rgba                  = d->visual.color.rgba;
	int      values [CP_ALPHA + 1] = {
	  ( int ) (rgba >> 24), ( int ) (rgba >> 16 & 255u), ( int ) (rgba >> 8 & 255u), ( int ) lroundf(d->visual.hue),
	  ( int ) lroundf(d->visual.saturation * 100.0f), ( int ) lroundf(d->visual.value * 100.0f),
	  ( int ) lroundf(cp_alpha(d) * 100.0f)};
	char text [16];
	for (int i = 0; i < CP_FIELDS; ++i) {
		if (keep & (1u << i)) continue;
		cp_field_text(d, i, values, text, sizeof(text));
		cp_write(d, i, text);
	}
}

static void cp_sync_sliders(ColorPickerData *d, uint32_t keep) {
	FluxSliderData *third = cp_slider(d, d->third);
	FluxSliderData *alpha = cp_slider(d, d->alpha_slider);
	if (third && !(keep & CP_KEEP_THIRD)) {
		bool hue             = d->visual.spectrum == XTK_SPECTRUM_SATURATION_VALUE;
		third->max_value     = hue ? FLUX_CP_MAX_HUE : 100.0f;
		float v              = hue                                          ? d->visual.hue
		                     : d->visual.spectrum == XTK_SPECTRUM_HUE_VALUE ? d->visual.saturation * 100.0f
		                                                                    : d->visual.value * 100.0f;
		third->current_value = third->intermediate_value = v;
	}
	if (alpha && !(keep & CP_KEEP_ALPHA)) alpha->current_value = alpha->intermediate_value = cp_alpha(d) * 100.0f;
}

static void cp_apply(ColorPickerData *d, FluxColor before, uint32_t keep, bool user) {
	cp_sync_sliders(d, keep);
	cp_write_fields(d, keep);
	if (!user || before.rgba == d->visual.color.rgba) return;
	flux_input_route_event(
	  flux_store_host(d->store), d->node, XTK_EVENT_VALUE_CHANGED,
	  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = ( int32_t ) d->visual.color.rgba}, XTK_EVENT_CAUSE_USER
	);
}

static void cp_set_hsv(ColorPickerData *d, float h, float s, float v, uint32_t keep) {
	FluxColor before     = d->visual.color;
	d->visual.hue        = h;
	d->visual.saturation = s;
	d->visual.value      = v;
	d->visual.color      = flux_color_fromhsv(h, s, v, cp_alpha(d));
	cp_apply(d, before, keep, true);
}

static void cp_set_rgb(ColorPickerData *d, FluxColor rgb, uint32_t keep) {
	float h, s, v;
	flux_color_tohsv(rgb, &h, &s, &v);
	h = fminf(h, FLUX_CP_MAX_HUE);
	cp_set_hsv(d, h, s, v, keep);
}

static void cp_set_alpha(ColorPickerData *d, float alpha, uint32_t keep) {
	FluxColor before     = d->visual.color;
	d->visual.color.rgba = (before.rgba & 0xffffff00u) | ( uint32_t ) lroundf(alpha * 255.0f);
	cp_apply(d, before, keep, true);
}

static bool cp_parse(char const *text, int base, unsigned long *out) {
	if (!text [0]) return false;
	char *end = NULL;
	*out      = strtoul(text, &end, base);
	return *end == '\0';
}

static bool cp_field_valid(int index, char const *text) {
	unsigned long v;
	if (!cp_parse(text, 10, &v)) return false;
	unsigned long hi = index < CP_HUE ? 255u : index == CP_HUE ? ( unsigned long ) FLUX_CP_MAX_HUE : 100u;
	return v <= hi;
}

static void cp_rgb_changed(ColorPickerData *d) {
	FluxColor rgb = flux_color_rgba(
	  ( uint8_t ) atoi(cp_text(d, CP_RED)), ( uint8_t ) atoi(cp_text(d, CP_GREEN)),
	  ( uint8_t ) atoi(cp_text(d, CP_BLUE)), 255
	);
	cp_set_rgb(d, rgb, CP_KEEP_RGB);
}

static void cp_hsv_changed(ColorPickerData *d) {
	cp_set_hsv(
	  d, ( float ) atoi(cp_text(d, CP_HUE)), atoi(cp_text(d, CP_SATURATION)) / 100.0f,
	  atoi(cp_text(d, CP_VALUE)) / 100.0f, CP_KEEP_HSV
	);
}

static void cp_alpha_text(ColorPickerData *d) {
	FluxTextBoxData *td = cp_textbox(d, CP_ALPHA);
	char             text [16];
	snprintf(text, sizeof(text), "%s", cp_text(d, CP_ALPHA));
	size_t n = strlen(text);
	if (!n || text [n - 1] != '%') {
		uint32_t caret = td ? td->cursor_position : 0;
		snprintf(text + n, sizeof(text) - n, "%%");
		cp_write(d, CP_ALPHA, text);
		if (td) td->cursor_position = td->selection_start = td->selection_end = caret;
		n = strlen(text);
	}
	text [n - 1] = '\0';
	unsigned long v;
	d->valid = cp_parse(text, 10, &v) && v <= 100u;
	if (d->valid) cp_set_alpha(d, v / 100.0f, 1u << CP_ALPHA);
}

static void cp_hex_text(ColorPickerData *d) {
	FluxTextBoxData *td = cp_textbox(d, CP_HEX);
	char             text [16];
	snprintf(text, sizeof(text), "%s", cp_text(d, CP_HEX));
	if (text [0] != '#') {
		char fixed [17];
		snprintf(fixed, sizeof(fixed), "#%s", text);
		cp_write(d, CP_HEX, fixed);
		snprintf(text, sizeof(text), "%s", fixed);
		if (td) td->cursor_position = td->selection_start = td->selection_end = ( uint32_t ) strlen(text);
	}
	unsigned long v;
	d->valid = cp_parse(text + 1, 16, &v);
	if (!d->valid) return;
	uint32_t hex = ( uint32_t ) v;
	cp_set_rgb(d, flux_color_rgba(hex >> 16 & 255u, hex >> 8 & 255u, hex & 255u, 255), 1u << CP_HEX);
	cp_set_alpha(d, d->visual.alpha ? (hex >> 24) / 255.0f : 1.0f, 1u << CP_HEX);
}

static void cp_field_changed(void *ctx, char const *text) {
	ColorField      *field = ctx;
	ColorPickerData *d     = field->owner;
	if (d->updating) return;
	if (field->index == CP_ALPHA) {
		cp_alpha_text(d);
		return;
	}
	if (field->index == CP_HEX) {
		cp_hex_text(d);
		return;
	}
	d->valid = cp_field_valid(field->index, text);
	if (!d->valid) return;
	if (field->index < CP_HUE) cp_rgb_changed(d);
	else cp_hsv_changed(d);
}

static void cp_field_focus(void *ctx) {
	ColorField      *field = ctx;
	ColorPickerData *d     = field->owner;
	if (field->on_focus) field->on_focus(field->on_focus_ctx);
	d->valid = true;
	snprintf(d->previous, sizeof(d->previous), "%s", cp_text(d, field->index));
}

static void cp_field_blur(void *ctx) {
	ColorField      *field = ctx;
	ColorPickerData *d     = field->owner;
	if (field->on_blur) field->on_blur(field->on_blur_ctx);
	if (!d->valid) cp_write(d, field->index, d->previous);
	d->valid = true;
	cp_sync_sliders(d, 0);
	cp_write_fields(d, 0);
}

static void cp_third_changed(void *ctx, float value) {
	ColorPickerData *d = ctx;
	if (d->updating) return;
	float h = d->visual.hue, s = d->visual.saturation, v = d->visual.value;
	if (d->visual.spectrum == XTK_SPECTRUM_HUE_VALUE) s = value / 100.0f;
	else if (d->visual.spectrum == XTK_SPECTRUM_SATURATION_VALUE) h = value;
	else v = value / 100.0f;
	cp_set_hsv(d, h, s, v, CP_KEEP_THIRD);
}

static void cp_alpha_changed(void *ctx, float value) {
	ColorPickerData *d = ctx;
	if (!d->updating) cp_set_alpha(d, value / 100.0f, CP_KEEP_ALPHA);
}

static bool cp_alpha_key(void *ctx, FluxKey key, bool down) {
	ColorPickerData *d     = ctx;
	bool             lower = key == FLUX_KEY_LEFT || key == FLUX_KEY_DOWN;
	bool             arrow = lower || key == FLUX_KEY_RIGHT || key == FLUX_KEY_UP;
	if (!down || !arrow || !flux_key_held(FLUX_KEY_CONTROL))
		return d->alpha_key && d->alpha_key(d->alpha_key_ctx, key, down);
	float a = cp_alpha(d) * 100.0f;
	a       = lower ? ceilf((a - 10.0f) / 10.0f) * 10.0f : floorf((a + 10.0f) / 10.0f) * 10.0f;
	cp_set_alpha(d, fminf(100.0f, fmaxf(0.0f, a)) / 100.0f, 0);
	return true;
}

static void cp_point(ColorPickerData *d, float x, float y) {
	float span = FLUX_CP_SPECTRUM - 1.0f;
	float cx   = fminf(span, fmaxf(0.0f, roundf(x))) / span;
	float cy   = fminf(span, fmaxf(0.0f, roundf(y - FLUX_CP_PAD))) / span;
	float h = d->visual.hue, s = d->visual.saturation, v = d->visual.value;
	switch (d->visual.spectrum) {
	case XTK_SPECTRUM_HUE_VALUE        : h = cx * FLUX_CP_MAX_HUE, v = 1.0f - cy; break;
	case XTK_SPECTRUM_SATURATION_VALUE : s = cx, v = 1.0f - cy; break;
	default                            : h = cx * FLUX_CP_MAX_HUE, s = 1.0f - cy; break;
	}
	cp_set_hsv(d, h, s, v, 0);
}

static void cp_pointer(void *ctx, float x, float y) {
	ColorPickerData *d = ctx;
	if (d->dragging) cp_point(d, x, y);
}

static void cp_press(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	ColorPickerData *d  = ctx;
	FluxNodeData    *nd = flux_store_find(d->store, d->node);
	d->dragging         = x >= 0.0f && x < FLUX_CP_SPECTRUM && y >= FLUX_CP_PAD && y < FLUX_CP_PAD + FLUX_CP_SPECTRUM;
	d->visual.large     = d->dragging && nd && nd->state.pointer_type != FLUX_POINTER_MOUSE;
	if (d->dragging) cp_point(d, x, y);
}

static void cp_release(void *ctx) {
	ColorPickerData *d = ctx;
	d->dragging        = false;
	d->visual.large    = false;
}

static float cp_step(float now, float step, bool lower, float hi) {
	float next = now + (lower ? -step : step);
	if (next < 0.0f) next = now == 0.0f ? hi : 0.0f;
	if (next > hi) next = now == hi ? 0.0f : hi;
	return next;
}

static bool cp_key(void *ctx, FluxKey key, bool down) {
	ColorPickerData *d = ctx;
	if (!down) return false;
	bool horizontal = key == FLUX_KEY_LEFT || key == FLUX_KEY_RIGHT;
	if (!horizontal && key != FLUX_KEY_UP && key != FLUX_KEY_DOWN) return false;
	int  spectrum = d->visual.spectrum;
	bool hue      = horizontal ? spectrum != XTK_SPECTRUM_SATURATION_VALUE : false;
	bool lower    = hue ? key == FLUX_KEY_LEFT : key == FLUX_KEY_RIGHT || key == FLUX_KEY_DOWN;
	if (horizontal && spectrum == XTK_SPECTRUM_SATURATION_VALUE) lower = !lower;
	int   c       = hue ? 0 : horizontal || spectrum == XTK_SPECTRUM_HUE_SATURATION ? 1 : 2;
	float hsv [3] = {d->visual.hue, d->visual.saturation, d->visual.value};
	float hi = c == 0 ? FLUX_CP_MAX_HUE : 100.0f, scale = c == 0 ? 1.0f : 100.0f;
	hsv [c] = cp_step(hsv [c] * scale, 1.0f, lower, hi) / scale;
	cp_set_hsv(d, hsv [0], hsv [1], hsv [2], 0);
	return true;
}

static void cp_representation(void *ctx, int index) {
	ColorPickerData *d = ctx;
	d->visual.hsv      = index == 1;
	d->dirty           = true;
}

static XentNodeId cp_slider_init(ColorPickerData *d, void (*on_change)(void *, float)) {
	XentNodeId      node = flux_slider_init(&(FluxSliderDesc) {
	  .ctx       = d->ctx,
	  .store     = d->store,
	  .parent    = d->node,
	  .max       = 100,
	  .value     = 100,
	  .on_change = on_change,
	  .userdata  = d});
	FluxSliderData *sd   = node == XENT_NODE_INVALID ? NULL : cp_slider(d, node);
	if (sd) sd->color_picker = true;
	return node;
}

static bool cp_field_init(ColorPickerData *d, FluxControlEnv const *env, int i) {
	d->binding [i] = (ColorField) {.owner = d, .index = i};
	d->field [i]
	  = env->app
	    ? flux_app_create_textbox(&(FluxAppTextBoxDesc) {env->app, d->node, NULL, cp_field_changed, &d->binding [i]})
	    : flux_textbox_init(&(FluxTextBoxDesc) {d->ctx, d->store, d->node, NULL, cp_field_changed, &d->binding [i]});
	FluxNodeData *nd = d->field [i] == XENT_NODE_INVALID ? NULL : flux_store_find(d->store, d->field [i]);
	if (!nd) return false;
	(( FluxTextBoxData * ) nd->component_data)->max_length = kMaxLength [i];
	d->binding [i].on_focus                                = nd->behavior.on_focus;
	d->binding [i].on_focus_ctx                            = nd->behavior.on_focus_ctx;
	d->binding [i].on_blur                                 = nd->behavior.on_blur;
	d->binding [i].on_blur_ctx                             = nd->behavior.on_blur_ctx;
	nd->behavior.on_focus                                  = cp_field_focus;
	nd->behavior.on_focus_ctx                              = &d->binding [i];
	nd->behavior.on_blur                                   = cp_field_blur;
	nd->behavior.on_blur_ctx                               = &d->binding [i];
	return true;
}

static void cp_more_toggle(void *ctx) {
	ColorPickerData *d = ctx;
	d->visual.opened   = !d->visual.opened;
	d->dirty           = true;
	xent_sem_setlabel(d->ctx, d->more, d->visual.opened ? "Less" : "More");
}

static bool cp_more_key(void *ctx, FluxKey key, bool down) {
	if (!down || (key != FLUX_KEY_SPACE && key != FLUX_KEY_ENTER)) return false;
	cp_more_toggle(ctx);
	return true;
}

static bool cp_more_init(ColorPickerData *d) {
	d->more          = flux_factory_create_node(d->ctx, d->store, d->node, FLUX_CONTROL_CONTAINER);
	FluxNodeData *nd = d->more == XENT_NODE_INVALID ? NULL : flux_store_find(d->store, d->more);
	if (!nd) return false;
	nd->behavior.on_click     = cp_more_toggle;
	nd->behavior.on_click_ctx = d;
	nd->behavior.on_key       = cp_more_key;
	nd->behavior.on_key_ctx   = d;
	xent_sem_setrole(d->ctx, d->more, XENT_SEM_ROLE_BUTTON);
	xent_sem_setlabel(d->ctx, d->more, "More");
	return true;
}

static bool cp_children(ColorPickerData *d, FluxControlEnv const *env) {
	static char const *const choices [2] = {"RGB", "HSV"};
	d->third                             = cp_slider_init(d, cp_third_changed);
	d->alpha_slider                      = cp_slider_init(d, cp_alpha_changed);
	d->combo                             = flux_combo_init(&(FluxComboBoxDesc) {
	  .ctx            = d->ctx,
	  .store          = d->store,
	  .parent         = d->node,
	  .window         = env->window,
	  .text           = env->text,
	  .theme          = env->theme,
	  .app            = env->app,
	  .input          = env->input,
	  .items          = choices,
	  .item_count     = 2,
	  .selected_index = 0,
	  .on_select      = cp_representation,
	  .userdata       = d});
	if (d->third == XENT_NODE_INVALID
	    || d->alpha_slider == XENT_NODE_INVALID
	    || d->combo == XENT_NODE_INVALID
	    || !cp_more_init(d))
		return false;
	FluxNodeData *alpha        = flux_store_find(d->store, d->alpha_slider);
	d->alpha_key               = alpha->behavior.on_key;
	d->alpha_key_ctx           = alpha->behavior.on_key_ctx;
	alpha->behavior.on_key     = cp_alpha_key;
	alpha->behavior.on_key_ctx = d;
	for (int i = 0; i < CP_FIELDS; ++i)
		if (!cp_field_init(d, env, i)) return false;
	return true;
}

static void cp_destroy(void *component_data) { free(component_data); }

XentNodeId  flux_colorpicker_init(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	XentNodeId node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_COLOR_PICKER);
	if (node == XENT_NODE_INVALID) return node;
	FluxNodeData    *nd = flux_store_find(store, node);
	ColorPickerData *d  = calloc(1, sizeof(*d));
	if (!d || !nd) {
		free(d);
		return flux_factory_fail_node(ctx, store, node);
	}
	*d = (ColorPickerData) {
	  .ctx    = ctx,
	  .store  = store,
	  .node   = node,
	  .dirty  = true,
	  .valid  = true,
	  .visual = {.color = {0xffffffffu}, .value = 1.0f}
    };
	nd->component_data               = d;
	nd->destroy_component_data       = cp_destroy;
	nd->behavior.on_pointer_down     = cp_press;
	nd->behavior.on_pointer_down_ctx = d;
	nd->behavior.on_pointer_move     = cp_pointer;
	nd->behavior.on_pointer_move_ctx = d;
	nd->behavior.on_click            = cp_release;
	nd->behavior.on_click_ctx        = d;
	nd->behavior.on_cancel           = cp_release;
	nd->behavior.on_cancel_ctx       = d;
	nd->behavior.on_key              = cp_key;
	nd->behavior.on_key_ctx          = d;
	xent_setproto(ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_setsize(ctx, node, (XentSize) {FLUX_CP_CONTENT_W, flux_colorpicker_geom(false, false, false).height});
	xent_setminsize(ctx, node, (XentSize) {FLUX_CP_CONTENT_W, 0});
	xent_setmaxsize(ctx, node, (XentSize) {392, NAN});
	xent_set_focusable(ctx, node, true);
	xent_sem_setlabel(ctx, node, "color spectrum");
	if (!cp_children(d, env)) return flux_factory_fail_node(ctx, store, node);
	cp_apply(d, d->visual.color, 0, false);
	return node;
}

void flux_colorpicker_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	ColorPickerData *d = cp_data(store, node);
	if (!d) return;
	d->ctx   = ctx;
	d->store = store;
	d->node  = node;
	d->dirty = true;
}

static void cp_place(ColorPickerData *d, XentNodeId node, FluxRect rect, bool visible, bool enabled) {
	xent_setminsize(d->ctx, node, (XentSize) {0, 0});
	xent_setpos(d->ctx, node, (XentPoint) {rect.x, rect.y});
	xent_setsize(d->ctx, node, (XentSize) {visible ? rect.w : 0, visible ? rect.h : 0});
	xent_set_focusable(d->ctx, node, visible && enabled);
	xent_sem_setenabled(d->ctx, node, enabled);
}

void flux_colorpicker_sync(XentNodeId node, FluxNodeData *nd) {
	ColorPickerData *d = nd->component_data;
	if (!d) return;
	bool enabled = xent_sem_enabled(d->ctx, node);
	if (!d->dirty && d->layout_enabled == enabled) return;
	d->dirty                  = false;
	d->layout_enabled         = enabled;
	bool                alpha = d->visual.alpha;
	FluxColorPickerGeom g     = flux_colorpicker_geom_of(&d->visual);
	float               w     = FLUX_CP_CONTENT_W;
	bool                shown = g.entries;
	cp_place(d, d->third, (FluxRect) {0, g.third_y, w, FLUX_CP_SLIDER_H}, true, enabled);
	cp_place(d, d->alpha_slider, (FluxRect) {0, g.alpha_y, w, FLUX_CP_SLIDER_H}, alpha, enabled);
	cp_place(
	  d, d->more, (FluxRect) {w - FLUX_CP_MORE_W, g.more_y, FLUX_CP_MORE_W, FLUX_CP_ROW_H}, d->visual.more_button,
	  enabled
	);
	cp_place(d, d->combo, (FluxRect) {0, g.entries_y, FLUX_CP_FIELD_W, FLUX_CP_ROW_H}, shown, enabled);
	cp_place(
	  d, d->field [CP_HEX], (FluxRect) {w - FLUX_CP_HEX_W, g.entries_y, FLUX_CP_HEX_W, FLUX_CP_ROW_H}, shown, enabled
	);
	for (int i = 0; i < 3; ++i) {
		FluxRect row = {0, flux_colorpicker_row_y(&g, i), FLUX_CP_FIELD_W, FLUX_CP_ROW_H};
		cp_place(d, d->field [CP_RED + i], row, shown && !d->visual.hsv, enabled);
		cp_place(d, d->field [CP_HUE + i], row, shown && d->visual.hsv, enabled);
	}
	cp_place(
	  d, d->field [CP_ALPHA], (FluxRect) {0, flux_colorpicker_row_y(&g, 3), FLUX_CP_FIELD_W, FLUX_CP_ROW_H},
	  shown && alpha, enabled
	);
	FluxTextBoxData *hex = cp_textbox(d, CP_HEX);
	if (hex) hex->max_length = alpha ? 9u : 7u;
	xent_seth(d->ctx, node, g.height);
}

bool flux_colorpicker_set(FluxStore *store, XentNodeId node, XtkPropertyId property, XtkValue const *value) {
	ColorPickerData *d = cp_data(store, node);
	if (!d) return false;
	switch (property) {
	case XTK_PROP_COLOR :
		d->visual.color.rgba = value->u.color.rgba;
		flux_color_tohsv(d->visual.color, &d->visual.hue, &d->visual.saturation, &d->visual.value);
		break;
	case XTK_PROP_ALPHA_ENABLED       : d->visual.alpha = value->u.b; break;
	case XTK_PROP_MORE_BUTTON_VISIBLE : d->visual.more_button = value->u.b; break;
	case XTK_PROP_SPECTRUM            : d->visual.spectrum = ( uint8_t ) value->u.u32; break;
	default                           : return false;
	}
	d->dirty = true;
	cp_apply(d, d->visual.color, 0, false);
	return true;
}
