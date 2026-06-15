/**
 * @file flux_uia.h
 * @brief UI Automation provider projecting the xent semantic tree.
 *
 * Accessibility is a *projection* of the semantic tree xent-core already
 * maintains, not a parallel structure. Each provider wraps one node and answers
 * UIA property queries (Name, ControlType, AutomationId, IsEnabled, focus) plus
 * fragment navigation and the control patterns the node supports (Invoke, Toggle,
 * RangeValue, Value, SelectionItem). Pattern actions are routed back into the
 * control layer through the node store. The window's `WM_GETOBJECT` returns the
 * root provider.
 */
#ifndef FLUX_COMPOSE_UIA_H
#define FLUX_COMPOSE_UIA_H

#include "fluxent/flux_node_store.h"

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Inputs shared by every provider node: the trees it projects and acts on. */
typedef struct FluxUiaContext {
	XentContext   *ctx;   /**< Xent layout/semantic context (borrowed). */
	XentNodeId     root;  /**< Root node of the projected tree. */
	HWND           hwnd;  /**< Host window (bounds + host provider). */
	FluxNodeStore *store; /**< Node store for pattern actions (borrowed; may be NULL). */
} FluxUiaContext;

/**
 * @brief Create the root UIA fragment provider for a window's element tree.
 *
 * @param info Trees and host the provider projects/acts on (copied).
 * @return An `IRawElementProviderSimple*` (as void*); caller `Release()`s it.
 *         NULL on failure.
 */
void *flux_uia_provider_create(FluxUiaContext const *info);

/**
 * @brief Raise the UIA focus-changed event for @p node, if a client is listening.
 *
 * Builds a transient provider for the node, raises, and releases it. Safe to call
 * on the UI thread when keyboard focus moves.
 */
void  flux_uia_raise_focus_changed(FluxUiaContext const *info, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_UIA_H */
