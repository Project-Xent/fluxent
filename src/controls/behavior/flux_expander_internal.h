#ifndef FLUX_EXPANDER_INTERNAL_H
#define FLUX_EXPANDER_INTERNAL_H

#include <stdbool.h>
#include <windows.h>
#include <xent/xent.h>
#include "store/flux_node_data.h"

typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

typedef struct FluxExpanderData {
	XentNodeId  root;
	XentNodeId  header;
	XentNodeId  header_text;
	XentNodeId  content;
	XentCtx    *ctx;
	FluxStore  *store;
	FluxWindow *window;

	float       width;
	float       content_height;

	bool        expanded;
	bool        anim_active;
	bool        anim_expanding;
	DWORD       anim_start;

	void        (*on_toggle)(void *ctx, bool expanded);
	void       *on_toggle_ctx;
} FluxExpanderData;

#endif
