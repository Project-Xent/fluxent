#ifndef FLUX_PLUGIN_H
#define FLUX_PLUGIN_H

#include "flux_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxRenderContext FluxRenderContext;
typedef struct FluxRenderSnapshot FluxRenderSnapshot;
typedef struct FluxControlState FluxControlState;

/*
 * Renderer plugin — a set of function pointers that an external library
 * (e.g. lottiedc) fills in to override or extend control rendering.
 */
typedef struct FluxPlugin {
    /* Called during the main render phase for the registered control type.
     * Return true if the plugin handled rendering (skip default renderer). */
    bool (*render)(const FluxRenderContext *rc,
                   const FluxRenderSnapshot *snap,
                   const FluxRect *bounds,
                   const FluxControlState *state,
                   void *userdata);

    /* Called once per frame with delta time (optional, may be NULL). */
    void (*update)(float dt, void *userdata);

    /* Cleanup when the plugin is unregistered or the registry is destroyed. */
    void (*destroy)(void *userdata);

    /* Opaque pointer passed to all callbacks. */
    void *userdata;
} FluxPlugin;

/*
 * Plugin registry — maps string type keys to FluxPlugin entries.
 * Typically one per application, owned by FluxApp or FluxEngine.
 */
typedef struct FluxPluginRegistry FluxPluginRegistry;

FluxPluginRegistry *flux_plugin_registry_create(void);
void                flux_plugin_registry_destroy(FluxPluginRegistry *reg);

/* Register a plugin under a type key (e.g. "checkbox", "lottie").
 * Takes ownership — destroy callback will be called on removal/cleanup.
 * If a plugin with the same key exists, the old one is destroyed first. */
void flux_plugin_register(FluxPluginRegistry *reg, const char *type, FluxPlugin plugin);

/* Remove a plugin by key. Calls destroy if set. */
void flux_plugin_unregister(FluxPluginRegistry *reg, const char *type);

/* Look up a plugin by key. Returns NULL if not found. */
const FluxPlugin *flux_plugin_get(const FluxPluginRegistry *reg, const char *type);

/* Call update(dt) on all registered plugins that have an update callback. */
void flux_plugin_update_all(FluxPluginRegistry *reg, float dt);

#ifdef __cplusplus
}
#endif

#endif