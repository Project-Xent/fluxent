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

void flux_plugin_register(FluxPluginRegistry *reg, char const *type, FluxPlugin plugin) {
	if (!reg || !type) return;

	/* Look for an existing slot with the same key. */
	int empty = -1;
		for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
			FluxPluginSlot *slot = &reg->slots [i];
				if (slot->occupied && strncmp(slot->key, type, FLUX_KEY_MAX - 1) == 0) {
					/* Replace: destroy old plugin first. */
					if (slot->plugin.destroy) slot->plugin.destroy(slot->plugin.userdata);
					slot->plugin = plugin;
					return;
				}
			if (!slot->occupied && empty < 0) empty = i;
		}

	/* Insert into the first empty slot. */
	if (empty < 0) return; /* Registry full — silently drop. */

	FluxPluginSlot *slot = &reg->slots [empty];
	strncpy_s(slot->key, FLUX_KEY_MAX, type, _TRUNCATE);
	slot->plugin   = plugin;
	slot->occupied = true;
}

void flux_plugin_unregister(FluxPluginRegistry *reg, char const *type) {
	if (!reg || !type) return;

		for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
			FluxPluginSlot *slot = &reg->slots [i];
				if (slot->occupied && strncmp(slot->key, type, FLUX_KEY_MAX - 1) == 0) {
					if (slot->plugin.destroy) slot->plugin.destroy(slot->plugin.userdata);
					memset(slot, 0, sizeof(FluxPluginSlot));
					return;
				}
		}
}

FluxPlugin const *flux_plugin_get(FluxPluginRegistry const *reg, char const *type) {
	if (!reg || !type) return NULL;

		for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
			FluxPluginSlot const *slot = &reg->slots [i];
			if (slot->occupied && strncmp(slot->key, type, FLUX_KEY_MAX - 1) == 0) return &slot->plugin;
		}
	return NULL;
}

void flux_plugin_update_all(FluxPluginRegistry *reg, float dt) {
	if (!reg) return;

		for (int i = 0; i < FLUX_MAX_PLUGINS; ++i) {
			FluxPluginSlot *slot = &reg->slots [i];
			if (slot->occupied && slot->plugin.update) slot->plugin.update(dt, slot->plugin.userdata);
		}
}
