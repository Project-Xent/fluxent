#ifndef FLUX_TEXT_DATA_H
#define FLUX_TEXT_DATA_H

#include <fluxent/flux_types.h>
#include <fluxent/flux_node_store.h>
#include <fluxent/flux_text.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxTextData {
	char const    *content;
	char const    *font_family;
	FluxColor      text_color;
	float          font_size;
	FluxFontWeight font_weight;
	FluxTextAlign  alignment;
	FluxTextVAlign vertical_alignment;
	uint8_t        max_lines;
	XentTextTrim   trim;
	bool           wrap;
	bool           selectable;

	uint32_t       selection_start;
	uint32_t       selection_end;
	uint32_t       drag_anchor;
	bool           dragging;

	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     node;
	FluxText      *text;
} FluxTextData;

#endif
