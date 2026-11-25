#include "a11y/flux_uia_app.h"
#include "runtime/flux_str.h"
#include "store/flux_node_data.h"

#include <stdlib.h>

static FluxUiaProps *uia_app_props(FluxStore *store, XentNodeId node, bool make) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd) return NULL;
	if (!nd->a11y && make) nd->a11y = ( FluxUiaProps * ) calloc(1, sizeof(FluxUiaProps));
	return nd->a11y;
}

void flux_uia_settext(FluxStore *store, XentNodeId node, int slot, char const *text) {
	FluxUiaProps *p = slot >= 0 && slot < FLUX_A11Y_TEXTS ? uia_app_props(store, node, true) : NULL;
	if (!p) return;
	free(p->text [slot]);
	p->text [slot] = text && text [0] ? flux_str_dup(text) : NULL;
}

void flux_uia_setnum(FluxStore *store, XentNodeId node, int slot, int32_t value) {
	FluxUiaProps *p = slot >= 0 && slot < FLUX_A11Y_NUMS ? uia_app_props(store, node, true) : NULL;
	if (!p) return;
	p->num [slot]  = value;
	p->set        |= 1u << slot;
}

char const *flux_uia_apptext(FluxStore *store, XentNodeId node, int slot) {
	FluxUiaProps *p = slot >= 0 && slot < FLUX_A11Y_TEXTS ? uia_app_props(store, node, false) : NULL;
	return p ? p->text [slot] : NULL;
}

bool flux_uia_appnum(FluxStore *store, XentNodeId node, int slot, int32_t *out) {
	FluxUiaProps *p = slot >= 0 && slot < FLUX_A11Y_NUMS ? uia_app_props(store, node, false) : NULL;
	if (!p || !(p->set & (1u << slot))) return false;
	*out = p->num [slot];
	return true;
}

void flux_uia_props_free(FluxUiaProps *props) {
	if (!props) return;
	for (int i = 0; i < FLUX_A11Y_TEXTS; i++) free(props->text [i]);
	free(props);
}
