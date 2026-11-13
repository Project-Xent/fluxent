#ifndef FLUX_INPUT_H
#define FLUX_INPUT_H

#include <stddef.h>
#include "flux_node_store.h"
#include "flux_types.h"

typedef struct FluxHitResult {
	XentNodeId    node;
	FluxNodeData *data;
	FluxRect      bounds;
	FluxPoint     local;
} FluxHitResult;

typedef struct FluxInput  FluxInput;

XENT_NODISCARD FluxInput *flux_input_create(XentCtx *ctx, FluxStore *store);
void                      flux_input_destroy(FluxInput *input);

FluxHitResult             flux_input_hittest(FluxInput *input, XentNodeId root, float px, float py);

void                      flux_input_dispatch(FluxInput *input, XentNodeId root, FluxPointerEvent const *ev);

bool                      flux_input_keydown(FluxInput *input, FluxKey key);
void                      flux_input_keyup(FluxInput *input, FluxKey key);
void                      flux_input_char(FluxInput *input, wchar_t ch);

void                      flux_input_ime(FluxInput *input, wchar_t const *text, uint32_t length, uint32_t cursor);
void                      flux_input_imeend(FluxInput *input);

int                       flux_input_clicks(FluxInput const *input);

XentNodeId                flux_input_focus(FluxInput const *input);
XentNodeId                flux_input_hover(FluxInput const *input);
XentNodeId                flux_input_pressed(FluxInput const *input);

bool                      flux_input_pointerinside(FluxInput const *input, XentNodeId node);

XentNodeId                flux_input_pantarget(FluxInput const *input);

void                      flux_input_clearpan(FluxInput *input);

bool                      flux_input_takepan(FluxInput *input);

FluxPointerType           flux_input_pointertype(FluxInput const *input);

uint32_t                  flux_input_pointerid(FluxInput const *input);

void                      flux_input_tab(FluxInput *input, XentNodeId root, bool shift);

void                      flux_input_arrow(FluxInput *input, XentNodeId root, FluxKey direction);

void                      flux_input_activate(FluxInput *input);
bool                      flux_input_buttonkey(FluxInput *input, FluxKey key, bool down);

void                      flux_input_escape(FluxInput *input);

void                      flux_input_setfocus(FluxInput *input, XentNodeId node, bool programmatic);

void                      flux_input_dropnode(FluxInput *input, XentNodeId node);

void flux_input_setmodal(FluxInput *input, XentNodeId root, void (*on_escape)(void *ctx), void *ctx);

#endif
