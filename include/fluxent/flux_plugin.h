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
	/**
	 * @brief Render callback for the registered control type.
	 * @return true when the plugin handled rendering and the default renderer should be skipped.
	 */
	bool (*render)(
	  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds,
	  FluxControlState const *state, void *userdata
	);

	/** @brief Optional frame update callback. */
	void  (*update)(float dt, void *userdata);

	/** @brief Optional cleanup callback. */
	void  (*destroy)(void *userdata);

	/** @brief Opaque pointer passed to all callbacks. */
	void *userdata;
} FluxPlugin;

/** @brief Registry mapping string type keys to renderer plugins. */
typedef struct FluxPluginRegistry FluxPluginRegistry;

/** @brief Create a plugin registry. */
FluxPluginRegistry               *flux_plugin_registry_create(void);
/** @brief Destroy a plugin registry and its registered plugins. */
void                              flux_plugin_registry_destroy(FluxPluginRegistry *reg);

/** @brief Register a plugin under a type key, replacing any existing plugin for that key. */
void                              flux_plugin_register(FluxPluginRegistry *reg, char const *type, FluxPlugin plugin);

/** @brief Remove a plugin by key. */
void                              flux_plugin_unregister(FluxPluginRegistry *reg, char const *type);

/** @brief Look up a plugin by key. */
FluxPlugin const                 *flux_plugin_get(FluxPluginRegistry const *reg, char const *type);

/** @brief Call update callbacks on all registered plugins that define one. */
void                              flux_plugin_update_all(FluxPluginRegistry *reg, float dt);

#ifdef __cplusplus
}
#endif

#endif
