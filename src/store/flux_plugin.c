#include "fluxent/flux_plugin.h"
#include <stdlib.h>
#include <string.h>

#define FLUX_MAX_PLUGINS 16
#define FLUX_KEY_MAX     32

typedef struct FluxPluginSlot {
	char       key [FLUX_KEY_MAX];
	FluxPlugin plugin;
	bool       occupied;
} FluxPluginSlot;

struct FluxPluginRegistry {
	FluxPluginSlot slots [FLUX_MAX_PLUGINS];
};

FluxPluginRegistry *flux_plugin_registry_create(void) {
	FluxPluginRegistry *reg = calloc(1, sizeof(FluxPluginRegistry));
	return reg;
}

void flux_plugin_registry_destroy(FluxPluginRegistry *reg) {
	if (!reg) return;

	for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
		FluxPluginSlot *slot = &reg->slots [i];
		if (slot->occupied && slot->plugin.destroy) slot->plugin.destroy(slot->plugin.userdata);
	}
	free(reg);
}

static bool plugin_key_matches(FluxPluginSlot const *slot, char const *type) {
	return slot->occupied && strncmp(slot->key, type, FLUX_KEY_MAX - 1) == 0;
}

static void plugin_destroy_slot(FluxPluginSlot *slot) {
	if (slot->occupied && slot->plugin.destroy) slot->plugin.destroy(slot->plugin.userdata);
}

static int plugin_find_index(FluxPluginRegistry const *reg, char const *type, int *empty) {
	if (empty) *empty = -1;
	for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
		FluxPluginSlot const *slot = &reg->slots [i];
		if (plugin_key_matches(slot, type)) return i;
		if (empty && !slot->occupied && *empty < 0) *empty = i;
	}
	return -1;
}

void flux_plugin_register(FluxPluginRegistry *reg, char const *type, FluxPlugin plugin) {
	if (!reg || !type) return;

	int empty = -1;
	int found = plugin_find_index(reg, type, &empty);
	if (found >= 0) {
		FluxPluginSlot *slot = &reg->slots [found];
		plugin_destroy_slot(slot);
		slot->plugin = plugin;
		return;
	}

	if (empty < 0) return;

	FluxPluginSlot *slot = &reg->slots [empty];
	strncpy_s(slot->key, FLUX_KEY_MAX, type, _TRUNCATE);
	slot->plugin   = plugin;
	slot->occupied = true;
}

void flux_plugin_unregister(FluxPluginRegistry *reg, char const *type) {
	if (!reg || !type) return;

	int found = plugin_find_index(reg, type, NULL);
	if (found < 0) return;

	FluxPluginSlot *slot = &reg->slots [found];
	plugin_destroy_slot(slot);
	memset(slot, 0, sizeof(FluxPluginSlot));
}

FluxPlugin const *flux_plugin_get(FluxPluginRegistry const *reg, char const *type) {
	if (!reg || !type) return NULL;

	int found = plugin_find_index(reg, type, NULL);
	return found >= 0 ? &reg->slots [found].plugin : NULL;
}

void flux_plugin_update_all(FluxPluginRegistry *reg, float dt) {
	if (!reg) return;

	for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
		FluxPluginSlot *slot = &reg->slots [i];
		if (slot->occupied && slot->plugin.update) slot->plugin.update(dt, slot->plugin.userdata);
	}
}
