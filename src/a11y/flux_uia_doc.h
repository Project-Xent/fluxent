#ifndef FLUX_UIA_DOC_H
#define FLUX_UIA_DOC_H

#include "a11y/flux_uia_object.h"
#include "controls/flux_module.h"
#include "text/flux_utf.h"

typedef struct UiaDoc {
	FluxUiaContext const *info;
	XentNodeId            node;
	FluxUiaText const    *hooks;
	FluxUiaTextView       view;
	FluxWideBuffer        wide;
	bool                  has_wide;
	uint32_t              len;
	FluxTextLine         *lines;
	uint32_t              nline;
	XentRect              rect;
	bool                  mirror;
} UiaDoc;

bool                flux_uia_doc_open(FluxUiaContext const *info, XentNodeId node, UiaDoc *doc);
void                flux_uia_doc_close(UiaDoc *doc);
FluxTextLayoutQuery flux_uia_doc_query(UiaDoc const *doc);
uint32_t            flux_uia_doc_next(UiaDoc const *doc, int unit, uint32_t pos);
uint32_t            flux_uia_doc_prev(UiaDoc const *doc, int unit, uint32_t pos);
void                flux_uia_doc_expand(UiaDoc const *doc, int unit, uint32_t pos, uint32_t *start, uint32_t *end);
uint32_t            flux_uia_doc_to16(UiaDoc const *doc, uint32_t utf8);
uint32_t            flux_uia_doc_to8(UiaDoc const *doc, uint32_t u16);
SAFEARRAY          *flux_uia_doc_rects(UiaDoc const *doc, uint32_t start, uint32_t end);
uint32_t            flux_uia_doc_at(UiaDoc const *doc, double x, double y);
bool                flux_uia_doc_line_shown(UiaDoc const *doc, uint32_t line);
void                flux_uia_doc_attr(UiaDoc const *doc, TEXTATTRIBUTEID id, VARIANT *out);
bool flux_uia_doc_state(FluxUiaContext const *info, XentNodeId node, uint32_t *text, uint32_t *selection);

ITextRangeProvider *flux_uia_range_new(FluxUiaContext const *info, XentNodeId node, uint32_t start, uint32_t end);
SAFEARRAY          *flux_uia_range_array(ITextRangeProvider **ranges, uint32_t count);

#endif
