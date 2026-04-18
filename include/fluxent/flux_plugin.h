/**
 * @file flux_plugin.h
 * @brief Renderer plugin interface for extending control rendering.
 *
 * Allows external libraries (e.g. lottiedc) to register custom renderers
 * that override or extend the default drawing for specific control types.
 */

#ifndef FLUX_PLUGIN_H
#define FLUX_PLUGIN_H

#include "flux_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxRenderContext  FluxRenderContext;
typedef struct FluxRenderSnapshot FluxRenderSnapshot;
typedef struct FluxControlState   FluxControlState;

/** @brief Renderer plugin function table. */
typedef struct FluxPlugin {
	/* Called during the main render phase for the registered control type.
	 * Return true if the plugin handled rendering (skip default renderer). */
	bool (*render)(
	  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds,
	  FluxControlState const *state, void *userdata
	);

	/* Called once per frame with delta time (optional, may be NULL). */
	void  (*update)(float dt, void *userdata);

	/* Cleanup when the plugin is unregistered or the registry is destroyed. */
	void  (*destroy)(void *userdata);

	/* Opaque pointer passed to all callbacks. */
	void *userdata;
} FluxPlugin;

/*
 * Plugin registry — maps string type keys to FluxPlugin entries.
 * Typically one per application, owned by FluxApp or FluxEngine.
 */
typedef struct FluxPluginRegistry FluxPluginRegistry;

FluxPluginRegistry               *flux_plugin_registry_create(void);
void                              flux_plugin_registry_destroy(FluxPluginRegistry *reg);

/* Register a plugin under a type key (e.g. "checkbox", "lottie").
 * Takes ownership — destroy callback will be called on removal/cleanup.
 * If a plugin with the same key exists, the old one is destroyed first. */
void                              flux_plugin_register(FluxPluginRegistry *reg, char const *type, FluxPlugin plugin);

/* Remove a plugin by key. Calls destroy if set. */
void                              flux_plugin_unregister(FluxPluginRegistry *reg, char const *type);

/* Look up a plugin by key. Returns NULL if not found. */
FluxPlugin const                 *flux_plugin_get(FluxPluginRegistry const *reg, char const *type);

/* Call update(dt) on all registered plugins that have an update callback. */
void                              flux_plugin_update_all(FluxPluginRegistry *reg, float dt);

#ifdef __cplusplus
}
#endif

#endif
