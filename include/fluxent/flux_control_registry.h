/**
 * @file flux_control_registry.h
 * @brief Control type → renderer table, decoupled from node storage.
 *
 * The registry maps each FluxControlType to its draw callbacks. It is built
 * once at app startup via flux_register_builtins() and never mutated per frame,
 * so its lifetime and change rate are completely different from the node store
 * (which churns with interaction). Keeping it separate lets the store stay pure
 * data and removes the "creating a control registers its renderer" side effect.
 */
#ifndef FLUX_CONTROL_REGISTRY_H
#define FLUX_CONTROL_REGISTRY_H

#include "flux_types.h"
#include "flux_control_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxRenderContext  FluxRenderContext;
typedef struct FluxRenderSnapshot FluxRenderSnapshot;
typedef struct FluxControlState   FluxControlState;

/** @brief Renderer callbacks for one control type. */
typedef struct FluxControlRenderer {
	void (*draw)(
	  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
	);
	void (*draw_overlay)(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);
} FluxControlRenderer;

typedef struct FluxControlRegistry FluxControlRegistry;

/** @brief Create an empty registry (all types unregistered). */
XENT_NODISCARD FluxControlRegistry *flux_control_registry_create(void);

/** @brief Destroy a registry (NULL is safe). */
void                               flux_control_registry_destroy(FluxControlRegistry *reg);

/** @brief Bind draw callbacks to a control type (overwrites any prior entry). */
void                               flux_control_registry_register(
  FluxControlRegistry *reg, FluxControlType type,
  void (*draw)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *),
  void (*draw_overlay)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *)
);

/** @brief Look up the renderer for a control type, or NULL if out of range. */
FluxControlRenderer const *flux_control_registry_get(FluxControlRegistry const *reg, FluxControlType type);

/** @brief Register every built-in control renderer in one declarative pass. */
void                       flux_register_builtins(FluxControlRegistry *reg);

#ifdef __cplusplus
}
#endif

#endif
