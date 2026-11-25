#include "a11y/flux_uia_doc.h"
#include "store/flux_node_data.h"

#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#define DOC_ATTR_FONT_NAME     40005
#define DOC_ATTR_FONT_SIZE     40006
#define DOC_ATTR_FONT_WEIGHT   40007
#define DOC_ATTR_FOREGROUND    40008
#define DOC_ATTR_ALIGNMENT     40009
#define DOC_ATTR_HIDDEN        40013
#define DOC_ATTR_ITALIC        40014
#define DOC_ATTR_READ_ONLY     40015
#define DOC_ATTR_STRIKETHROUGH 40026
#define DOC_ATTR_UNDERLINE     40030
#define DOC_DEFAULT_FONT       L"Segoe UI Variable"

static bool doc_wide(UiaDoc *doc) {
	doc->has_wide = flux_wide_buffer_from_utf8(&doc->wide, doc->view.text);
	doc->len      = doc->has_wide && doc->wide.len > 0 ? ( uint32_t ) doc->wide.len : 0u;
	return doc->has_wide;
}

static void doc_clamp_lines(UiaDoc *doc) {
	for (uint32_t i = 0; i < doc->nline; i++) {
		FluxTextLine *l = &doc->lines [i];
		if (l->start > doc->len) l->start = doc->len;
		if (l->length > doc->len - l->start) l->length = doc->len - l->start;
	}
}

static bool doc_lines(UiaDoc *doc) {
	FluxTextLayoutQuery q = flux_uia_doc_query(doc);
	uint32_t            n = flux_text_lines(&q, NULL, 0u);
	doc->lines            = ( FluxTextLine * ) calloc(n ? n : 1u, sizeof(*doc->lines));
	if (!doc->lines) return false;
	if (n) flux_text_lines(&q, doc->lines, n);
	doc->nline = n ? n : 1u;
	doc_clamp_lines(doc);
	return true;
}

static FluxUiaText const doc_sub_hooks = {0};

static bool              doc_view(FluxUiaHooks const *h, UiaDoc *doc) {
	FluxStore *store = doc->info->store;
	if (doc->sub >= 0) {
		doc->hooks = &doc_sub_hooks;
		return h && h->sub && h->sub->text && h->sub->text(store, doc->node, doc->sub, &doc->view);
	}
	doc->hooks = h ? h->doc : NULL;
	return doc->hooks && doc->hooks->view && doc->hooks->view(store, doc->node, &doc->view);
}

bool flux_uia_doc_open(FluxUiaContext const *info, XentNodeId node, int sub, UiaDoc *doc) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	memset(doc, 0, sizeof(*doc));
	doc->info = info;
	doc->node = node;
	doc->sub  = sub;
	if (!doc_view(h, doc)) return false;
	if (!doc->view.renderer) doc->view.renderer = info->text;
	if (!doc->view.text) doc->view.text = "";
	flux_uia_place(info, node, sub, &doc->rect, &doc->seen);
	doc->mirror = xent_resolved_dir(info->ctx, node) == XENT_DIRECTION_RTL;
	return doc->view.renderer && doc_wide(doc) && doc_lines(doc);
}

void flux_uia_doc_close(UiaDoc *doc) {
	if (doc->has_wide) flux_wide_buffer_free(&doc->wide);
	free(doc->lines);
	doc->has_wide = false;
	doc->lines    = NULL;
}

FluxTextLayoutQuery flux_uia_doc_query(UiaDoc const *doc) {
	return (FluxTextLayoutQuery) {doc->view.renderer, doc->view.text, &doc->view.style, doc->view.layout_w};
}

uint32_t flux_uia_doc_to16(UiaDoc const *doc, uint32_t utf8) {
	uint32_t n = ( uint32_t ) strlen(doc->view.text);
	return flux_utf8_to_utf16_offset(doc->view.text, n, utf8 < n ? utf8 : n);
}

uint32_t flux_uia_doc_to8(UiaDoc const *doc, uint32_t u16) {
	return flux_utf16_to_utf8_offset(doc->view.text, ( uint32_t ) strlen(doc->view.text), u16);
}

static uint32_t doc_cluster(UiaDoc const *doc, uint32_t pos, bool forward) {
	FluxTextClusterQuery q = {flux_uia_doc_query(doc), pos};
	uint32_t             r = forward ? flux_text_next(&q) : flux_text_prev(&q);
	return r > doc->len ? doc->len : r;
}

static bool doc_space(wchar_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x0b || c == 0x0c; }

static bool doc_wordch(wchar_t c) { return iswalnum(c) || c == L'_' || c >= 0x80; }

static int  doc_class(UiaDoc const *doc, uint32_t i) {
	wchar_t c = doc->wide.text [i];
	return doc_space(c) ? 0 : (doc_wordch(c) ? 1 : 2);
}

static uint32_t doc_word_next(UiaDoc const *doc, uint32_t pos) {
	if (pos >= doc->len) return doc->len;
	int      cls = doc_class(doc, pos);
	uint32_t p   = pos;
	while (p < doc->len && doc_class(doc, p) == cls) p++;
	while (cls != 0 && p < doc->len && doc_class(doc, p) == 0) p++;
	return p;
}

static uint32_t doc_word_prev(UiaDoc const *doc, uint32_t pos) {
	uint32_t p = pos > doc->len ? doc->len : pos;
	while (p > 0 && doc_class(doc, p - 1) == 0) p--;
	if (p == 0) return 0;
	int cls = doc_class(doc, p - 1);
	while (p > 0 && doc_class(doc, p - 1) == cls) p--;
	return p;
}

static uint32_t doc_line_next(UiaDoc const *doc, uint32_t pos) {
	for (uint32_t i = 0; i < doc->nline; i++)
		if (doc->lines [i].start > pos) return doc->lines [i].start;
	return doc->len;
}

static uint32_t doc_line_prev(UiaDoc const *doc, uint32_t pos) {
	uint32_t best = 0;
	for (uint32_t i = 0; i < doc->nline && doc->lines [i].start < pos; i++) best = doc->lines [i].start;
	return best;
}

static bool doc_para_start(UiaDoc const *doc, uint32_t i) {
	if (i == 0) return true;
	wchar_t before = doc->wide.text [i - 1];
	return before == L'\n' || (before == L'\r' && (i >= doc->len || doc->wide.text [i] != L'\n'));
}

static uint32_t doc_para_next(UiaDoc const *doc, uint32_t pos) {
	for (uint32_t i = pos + 1; i < doc->len; i++)
		if (doc_para_start(doc, i)) return i;
	return doc->len;
}

static uint32_t doc_para_prev(UiaDoc const *doc, uint32_t pos) {
	for (uint32_t i = pos; i-- > 0;)
		if (doc_para_start(doc, i)) return i;
	return 0;
}

static int doc_unit(UiaDoc const *doc, int unit) {
	if (unit == TextUnit_Paragraph && !doc->view.paragraphs) return TextUnit_Document;
	return unit == TextUnit_Format || unit == TextUnit_Page ? TextUnit_Document : unit;
}

uint32_t flux_uia_doc_next(UiaDoc const *doc, int unit, uint32_t pos) {
	if (pos >= doc->len) return doc->len;
	switch (doc_unit(doc, unit)) {
	case TextUnit_Character : return doc_cluster(doc, pos, true);
	case TextUnit_Word      : return doc_word_next(doc, pos);
	case TextUnit_Line      : return doc_line_next(doc, pos);
	case TextUnit_Paragraph : return doc_para_next(doc, pos);
	default                 : return doc->len;
	}
}

uint32_t flux_uia_doc_prev(UiaDoc const *doc, int unit, uint32_t pos) {
	if (pos == 0) return 0;
	switch (doc_unit(doc, unit)) {
	case TextUnit_Character : return doc_cluster(doc, pos > doc->len ? doc->len : pos, false);
	case TextUnit_Word      : return doc_word_prev(doc, pos);
	case TextUnit_Line      : return doc_line_prev(doc, pos);
	case TextUnit_Paragraph : return doc_para_prev(doc, pos);
	default                 : return 0;
	}
}

void flux_uia_doc_expand(UiaDoc const *doc, int unit, uint32_t pos, uint32_t *start, uint32_t *end) {
	uint32_t at = pos > doc->len ? doc->len : pos;
	if (at == doc->len && (unit == TextUnit_Character || doc->len == 0)) {
		*start = *end = at;
		return;
	}
	if (at == doc->len) at = flux_uia_doc_prev(doc, unit, at);
	*end   = flux_uia_doc_next(doc, unit, at);
	*start = flux_uia_doc_prev(doc, unit, *end);
	if (*start > at) *start = at;
}

static XentRect doc_local(UiaDoc const *doc, FluxRect const *r) {
	FluxUiaTextView const *v = &doc->view;
	float                  x = v->style.rtl ? v->layout_w - r->x - r->w : r->x;
	return (XentRect) {v->origin_x + x, v->origin_y + r->y, r->w, r->h};
}

static bool doc_clip(UiaDoc const *doc, XentRect *r) {
	XentRect c  = doc->view.clip;
	float    x0 = r->x > c.x ? r->x : c.x;
	float    y0 = r->y > c.y ? r->y : c.y;
	float    x1 = r->x + r->w < c.x + c.w ? r->x + r->w : c.x + c.w;
	float    y1 = r->y + r->h < c.y + c.h ? r->y + r->h : c.y + c.h;
	if (x1 < x0 || y1 <= y0) return false;
	*r = (XentRect) {x0, y0, x1 - x0, y1 - y0};
	return true;
}

static bool doc_abs(UiaDoc const *doc, XentRect *r) {
	XentRect c  = doc->seen;
	float    x  = doc->mirror ? doc->rect.x + doc->rect.w - r->x - r->w : doc->rect.x + r->x;
	float    y  = doc->rect.y + r->y;
	float    x0 = x > c.x ? x : c.x, y0 = y > c.y ? y : c.y;
	float    x1 = x + r->w < c.x + c.w ? x + r->w : c.x + c.w;
	float    y1 = y + r->h < c.y + c.h ? y + r->h : c.y + c.h;
	if (x1 < x0 || y1 <= y0) return false;
	*r = (XentRect) {x0, y0, x1 - x0, y1 - y0};
	return true;
}

static void doc_screen(UiaDoc const *doc, XentRect r, double *out) {
	POINT origin = {0, 0};
	float scale  = 1.0f;
	flux_uia_origin(doc->info, doc->node, &origin, &scale);
	out [0] = origin.x + r.x * scale;
	out [1] = origin.y + r.y * scale;
	out [2] = r.w * scale;
	out [3] = r.h * scale;
}

static uint32_t doc_raw_rects(UiaDoc const *doc, uint32_t start, uint32_t end, FluxRect *rects, uint32_t cap) {
	FluxTextLayoutQuery q = flux_uia_doc_query(doc);
	if (start < end) return flux_text_selection(&(FluxTextSelectionQuery) {q, ( int ) start, ( int ) end, rects, cap});
	rects [0] = flux_text_caret(&(FluxTextCaretQuery) {q, ( int ) start});
	return 1u;
}

SAFEARRAY *flux_uia_doc_rects(UiaDoc const *doc, uint32_t start, uint32_t end) {
	uint32_t  cap   = doc->nline * 2u + 2u;
	FluxRect *rects = ( FluxRect * ) calloc(cap, sizeof(*rects));
	XentRect *kept  = ( XentRect * ) calloc(cap, sizeof(*kept));
	uint32_t  n = rects && kept ? doc_raw_rects(doc, start, end, rects, cap) : 0u, count = 0u;
	for (uint32_t i = 0; i < n; i++) {
		kept [count]  = doc_local(doc, &rects [i]);
		count        += doc_clip(doc, &kept [count]) && doc_abs(doc, &kept [count]);
	}
	SAFEARRAY *sa   = SafeArrayCreateVector(VT_R8, 0, count * 4u);
	double    *data = NULL;
	if (sa && SUCCEEDED(SafeArrayAccessData(sa, ( void ** ) &data))) {
		for (uint32_t i = 0; i < count; i++) doc_screen(doc, kept [i], data + i * 4u);
		SafeArrayUnaccessData(sa);
	}
	free(rects);
	free(kept);
	return sa;
}

uint32_t flux_uia_doc_at(UiaDoc const *doc, double x, double y) {
	POINT origin = {0, 0};
	float scale  = 1.0f;
	flux_uia_origin(doc->info, doc->node, &origin, &scale);
	float                  ax = ( float ) ((x - origin.x) / scale), ay = ( float ) ((y - origin.y) / scale);
	float                  lx = doc->mirror ? doc->rect.x + doc->rect.w - ax : ax - doc->rect.x;
	FluxUiaTextView const *v  = &doc->view;
	float                  tx = lx - v->origin_x;
	FluxTextHitTestQuery   q
	  = {flux_uia_doc_query(doc), v->style.rtl ? v->layout_w - tx : tx, ay - doc->rect.y - v->origin_y};
	int at = flux_text_hittest(&q);
	return at < 0 ? 0u : (( uint32_t ) at > doc->len ? doc->len : ( uint32_t ) at);
}

bool flux_uia_doc_line_shown(UiaDoc const *doc, uint32_t line) {
	FluxTextLine const *l   = &doc->lines [line];
	float               top = doc->view.origin_y + l->top;
	return top + l->height > doc->view.clip.y && top < doc->view.clip.y + doc->view.clip.h;
}

static void doc_unsupported(VARIANT *out) {
	out->vt = VT_UNKNOWN;
	UiaGetReservedNotSupportedValue(&out->punkVal);
}

static bool doc_attr_int(UiaDoc const *doc, TEXTATTRIBUTEID id, int *out) {
	switch (id) {
	case DOC_ATTR_FONT_WEIGHT   : *out = flux_font_weightnum(doc->view.style.font_weight); return true;
	case DOC_ATTR_ALIGNMENT     : *out = ( int ) doc->view.style.text_align; return true;
	case DOC_ATTR_STRIKETHROUGH :
	case DOC_ATTR_UNDERLINE     : *out = 0; return true;
	default                     : return false;
	}
}

static bool doc_attr_bool(UiaDoc const *doc, TEXTATTRIBUTEID id, bool *out) {
	*out = id == DOC_ATTR_READ_ONLY && doc->view.readonly;
	return id == DOC_ATTR_READ_ONLY || id == DOC_ATTR_HIDDEN || id == DOC_ATTR_ITALIC;
}

static bool doc_attr_color(UiaDoc const *doc, VARIANT *out) {
	uint32_t rgba = doc->view.style.color.rgba;
	if ((rgba & 0xffu) == 0u) return false;
	out->vt   = VT_I4;
	out->lVal = ( LONG ) (((rgba >> 24) & 0xffu) | (((rgba >> 16) & 0xffu) << 8) | (((rgba >> 8) & 0xffu) << 16));
	return true;
}

static void doc_attr_font(UiaDoc const *doc, VARIANT *out) {
	wchar_t name [128] = DOC_DEFAULT_FONT;
	if (doc->view.style.font_family && doc->view.style.font_family [0])
		flux_utf8_to_wchar(doc->view.style.font_family, name, ( int ) (sizeof(name) / sizeof(name [0])));
	out->bstrVal = SysAllocString(name);
	out->vt      = out->bstrVal ? VT_BSTR : VT_EMPTY;
}

void flux_uia_doc_attr(UiaDoc const *doc, TEXTATTRIBUTEID id, VARIANT *out) {
	int  i = 0;
	bool b = false;
	VariantInit(out);
	if (id == DOC_ATTR_FONT_NAME) {
		doc_attr_font(doc, out);
		return;
	}
	if (id == DOC_ATTR_FONT_SIZE) {
		*out = (VARIANT) {.vt = VT_R8, .dblVal = doc->view.style.font_size};
		return;
	}
	if (id == DOC_ATTR_FOREGROUND && doc_attr_color(doc, out)) return;
	if (doc_attr_int(doc, id, &i)) {
		*out = (VARIANT) {.vt = VT_I4, .lVal = i};
		return;
	}
	if (doc_attr_bool(doc, id, &b)) {
		*out = (VARIANT) {.vt = VT_BOOL, .boolVal = b ? VARIANT_TRUE : VARIANT_FALSE};
		return;
	}
	doc_unsupported(out);
}

bool flux_uia_doc_state(FluxUiaContext const *info, XentNodeId node, uint32_t *text, uint32_t *selection) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	FluxUiaTextView     v = {0};
	if (!h || !h->doc || !h->doc->view || !h->doc->view(info->store, node, &v)) return false;
	uint32_t hash = 2166136261u;
	for (char const *s = v.text ? v.text : ""; *s; s++) hash = (hash ^ ( uint8_t ) *s) * 16777619u;
	*text      = hash;
	*selection = v.sel_start * 2654435761u ^ v.sel_end;
	return true;
}
