#include "fx_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FX_FIRST_CHUNK (256u * 1024u)

FxArena *fx_ui_arena(FxUi *ui) { return &ui->arenas [ui->current]; }

FxUi    *fx_ui_create(void) {
	FxUi *ui = ( FxUi * ) calloc(1, sizeof(*ui));
	if (!ui) return NULL;
	if (!fx_arena_init(&ui->arenas [0], FX_FIRST_CHUNK) || !fx_arena_init(&ui->arenas [1], FX_FIRST_CHUNK)) {
		fx_arena_dispose(&ui->arenas [0]);
		fx_arena_dispose(&ui->arenas [1]);
		free(ui);
		return NULL;
	}
	return ui;
}

void fx_ui_destroy(FxUi *ui) {
	if (!ui) return;
	fx_arena_dispose(&ui->arenas [0]);
	fx_arena_dispose(&ui->arenas [1]);
	free(ui);
}

/* Flip arenas: the previous frame's tree must outlive this frame's build so
 * the reconciler can diff against it. */
void fx_ui_begin(FxUi *ui) {
	ui->current ^= 1;
	fx_arena_reset(fx_ui_arena(ui));
}

char *fx_strdup(FxUi *ui, char const *s) {
	if (!s) return NULL;
	size_t len  = strlen(s) + 1;
	char  *copy = ( char * ) fx_arena_alloc(fx_ui_arena(ui), len);
	if (copy) memcpy(copy, s, len);
	return copy;
}

char *fx_fmt(FxUi *ui, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (len < 0) return NULL;

	char *s = ( char * ) fx_arena_alloc(fx_ui_arena(ui), ( size_t ) len + 1);
	if (!s) return NULL;

	va_start(args, fmt);
	vsnprintf(s, ( size_t ) len + 1, fmt, args);
	va_end(args);
	return s;
}

FluxEl *fx_el_new(FxUi *ui, FluxControlType type, char const *text) {
	FluxEl *el = ( FluxEl * ) fx_arena_alloc(fx_ui_arena(ui), sizeof(*el));
	if (!el) return NULL;
	el->type   = type;
	el->width  = NAN;
	el->height = NAN;
	el->text   = fx_strdup(ui, text);
	return el;
}

static FluxEl fx_end_sentinel;
FluxEl *const FX_END = &fx_end_sentinel;

/* Children arrive as an FX_END-terminated compound literal; NULL entries are
 * skipped (conditional rendering) and the survivors are copied into a dense
 * arena array. */
void fx_el_adopt(FxUi *ui, FluxEl *el, FluxEl *children []) {
	if (!el || !children) return;

	int count = 0;
	for (FluxEl **c = children; *c != FX_END; c++)
		if (*c) count++;

	el->children = ( FluxEl ** ) fx_arena_alloc(fx_ui_arena(ui), sizeof(FluxEl *) * ( size_t ) (count ? count : 1));
	if (!el->children) return;

	int i = 0;
	for (FluxEl **c = children; *c != FX_END; c++)
		if (*c) el->children [i++] = *c;
	el->child_count = count;
}

FluxEl *fx_text(FxUi *ui, char const *content, FxTextDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_TEXT, content);
	if (el) el->text_props = desc;
	return el;
}

FluxEl *fx_button(FxUi *ui, char const *label, FxButtonDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_BUTTON, label);
	if (!el) return NULL;
	desc.icon  = fx_strdup(ui, desc.icon);
	el->button = desc;
	return el;
}

static FluxEl *fx_toggle(FxUi *ui, FluxControlType type, char const *label, FxToggleDesc desc) {
	FluxEl *el = fx_el_new(ui, type, label);
	if (el) el->toggle = desc;
	return el;
}

FluxEl *fx_checkbox(FxUi *ui, char const *label, FxToggleDesc desc) {
	return fx_toggle(ui, FLUX_CONTROL_CHECKBOX, label, desc);
}

FluxEl *fx_radio(FxUi *ui, char const *label, FxToggleDesc desc) {
	return fx_toggle(ui, FLUX_CONTROL_RADIO, label, desc);
}

FluxEl *fx_switch(FxUi *ui, FxToggleDesc desc) { return fx_toggle(ui, FLUX_CONTROL_SWITCH, NULL, desc); }

FluxEl *fx_slider(FxUi *ui, FxSliderDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_SLIDER, NULL);
	if (!el) return NULL;
	if (desc.max == 0.0f && desc.min == 0.0f) desc.max = 100.0f;
	if (desc.step == 0.0f) desc.step = 1.0f;
	if (desc.small_change == 0.0f) desc.small_change = 1.0f;
	if (desc.large_change == 0.0f) desc.large_change = 10.0f;
	el->slider = desc;
	return el;
}

static FluxEl *fx_text_input(FxUi *ui, FluxControlType type, char const *placeholder, FxTextBoxDesc desc) {
	FluxEl *el = fx_el_new(ui, type, placeholder);
	if (!el) return NULL;
	desc.content = fx_strdup(ui, desc.content);
	el->textbox  = desc;
	return el;
}

FluxEl *fx_textbox(FxUi *ui, char const *placeholder, FxTextBoxDesc desc) {
	return fx_text_input(ui, FLUX_CONTROL_TEXT_INPUT, placeholder, desc);
}

FluxEl *fx_password(FxUi *ui, char const *placeholder, FxTextBoxDesc desc) {
	return fx_text_input(ui, FLUX_CONTROL_PASSWORD_BOX, placeholder, desc);
}

static FluxEl *fx_progress_el(FxUi *ui, FluxControlType type, FxProgressDesc desc) {
	FluxEl *el = fx_el_new(ui, type, NULL);
	if (!el) return NULL;
	if (desc.max == 0.0f) desc.max = 100.0f;
	el->progress = desc;
	return el;
}

FluxEl *fx_progress(FxUi *ui, FxProgressDesc desc) { return fx_progress_el(ui, FLUX_CONTROL_PROGRESS, desc); }

FluxEl *fx_progress_ring(FxUi *ui, FxProgressDesc desc) { return fx_progress_el(ui, FLUX_CONTROL_PROGRESS_RING, desc); }

FluxEl *fx_badge(FxUi *ui, FxBadgeDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_INFO_BADGE, NULL);
	if (!el) return NULL;
	desc.icon = fx_strdup(ui, desc.icon);
	el->badge = desc;
	return el;
}

FluxEl *fx_image(FxUi *ui, char const *source, FxImageDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_IMAGE, source);
	if (el) el->image = desc;
	return el;
}

FluxEl *fx_hyperlink(FxUi *ui, char const *label, FxHyperlinkDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_HYPERLINK, label);
	if (!el) return NULL;
	desc.url      = fx_strdup(ui, desc.url);
	el->hyperlink = desc;
	return el;
}

FluxEl *fx_divider(FxUi *ui) { return fx_el_new(ui, FLUX_CONTROL_DIVIDER, NULL); }

FluxEl *fx_repeat_button(FxUi *ui, char const *label, FxButtonDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_REPEAT_BUTTON, label);
	if (!el) return NULL;
	desc.icon  = fx_strdup(ui, desc.icon);
	el->button = desc;
	return el;
}

FluxEl *fx_info_bar(FxUi *ui, char const *title, char const *message, FxInfoBarDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_INFO_BAR, title);
	if (!el) return NULL;
	desc.message = fx_strdup(ui, message);
	el->info_bar = desc;
	return el;
}

FluxEl *fx_tooltip(FxUi *ui, FluxEl *el, char const *text) {
	if (el) el->tooltip = fx_strdup(ui, text);
	return el;
}

FluxEl *fx_spacer(FxUi *ui) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_CONTAINER, NULL);
	if (el) el->grow = 1.0f;
	return el;
}

FluxEl **fx_children_alloc(FxUi *ui, int capacity) {
	if (capacity < 0) capacity = 0;
	FluxEl **buf = ( FluxEl ** ) fx_arena_alloc(fx_ui_arena(ui), sizeof(FluxEl *) * (( size_t ) capacity + 1));
	if (buf) buf [capacity] = FX_END;
	return buf;
}

FluxEl *fx_keyed(FxUi *ui, char const *key, FluxEl *el) {
	if (el) el->key = fx_strdup(ui, key);
	return el;
}

FluxEl *fx_sized(FluxEl *el, float width, float height) {
	if (!el) return NULL;
	el->width  = width;
	el->height = height;
	return el;
}

FluxEl *fx_grow(FluxEl *el, float grow) {
	if (el) el->grow = grow;
	return el;
}

FluxEl *fx_align(FluxEl *el, XentFlexAlign align_self) {
	if (el) el->align_self = align_self;
	return el;
}

FluxEl *fx_margin(FluxEl *el, XentInsets margin) {
	if (el) el->margin = margin;
	return el;
}
