#include "controls/flux_module.h"
#include "store/flux_node_data.h"
#include "host/flux_limit.h"

#include <string.h>

static FluxControlModule const *g_modules [XTK_CTRL_TYPE_COUNT] = {
  [FLUX_CONTROL_CONTAINER] = &flux_ctrl_container,
};

void flux_controls_use(FluxControlModule const *module) {
	if (!module || ( uint32_t ) module->type >= XTK_CTRL_TYPE_COUNT || g_modules [module->type] == module) return;
	g_modules [module->type] = module;
	for (FluxControlModule const *const *dep = module->deps; dep && *dep; dep++) flux_controls_use(*dep);
}

FluxControlModule const *flux_module_get(FluxControlType type) {
	return ( uint32_t ) type < XTK_CTRL_TYPE_COUNT ? g_modules [type] : NULL;
}

FluxControlModule const *flux_module_of(FluxStore *store, XentNodeId node) {
	FluxNodeData const *nd = flux_store_find(store, node);
	return nd ? flux_module_get(nd->component_type) : NULL;
}

FluxTreeHooks const *flux_module_tree(FluxStore *store, XentNodeId node) {
	FluxControlModule const *m = flux_module_of(store, node);
	return m ? m->tree : NULL;
}

FluxInputHooks const *flux_module_input(FluxStore *store, XentNodeId node) {
	FluxControlModule const *m = flux_module_of(store, node);
	return m ? m->input : NULL;
}

FluxUiaHooks const *flux_module_uia(FluxStore *store, XentNodeId node) {
	FluxControlModule const *m = flux_module_of(store, node);
	return m ? m->uia : NULL;
}

FluxPropHandler const *flux_module_prop(FluxControlType type, XtkPropertyId id, XtkValueKind kind) {
	FluxControlModule const *m = flux_module_get(type);
	for (FluxPropHandler const *p = m ? m->props : NULL; p && p->fn; p++)
		if (p->id == id && p->kind == kind) return p;
	return NULL;
}

bool flux_module_consumes(FluxControlType type, XtkPropertyId id) {
	FluxControlModule const *m = flux_module_get(type);
	for (XtkPropertyId const *p = m ? m->consumed : NULL; p && *p != XTK_PROP_ID_INVALID; p++)
		if (*p == id) return true;
	return false;
}

void flux_touch_note(FluxTouch *touch, XentNodeId node) {
	if (node == XENT_NODE_INVALID) return;
	uint32_t at = 0u;
	while (at < touch->count && touch->nodes [at] != node) at++;
	if (at < touch->count) {
		memmove(&touch->nodes [at], &touch->nodes [at + 1u], sizeof(node) * (touch->count - at - 1u));
		touch->nodes [touch->count - 1u] = node;
		return;
	}
	if (touch->count == sizeof(touch->nodes) / sizeof(touch->nodes [0])) {
		flux_limit_exceeded(FLUX_LIMIT_TXN_OWNERS);
		return;
	}
	touch->nodes [touch->count++] = node;
}
