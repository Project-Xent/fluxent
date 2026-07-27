/**
 * @file fluxent.h
 * @brief Umbrella header for the Fluxent UI framework (pure aggregation).
 *
 * Controls are built via XtkEl / flux_run / XtkHost batches — not a second
 * flux_create_* family.
 *
 * @code
 * #include <fluxent/fluxent.h>
 * return flux_run(&cfg, &model, update, view);
 * @endcode
 */

#ifndef FLUXENT_H
#define FLUXENT_H

#include "flux_control_type.h"
#include "flux_theme.h"
#include "flux_popup.h"
#include "flux_tooltip.h"
#include "flux_flyout.h"
#include "flux_menu.h"
#include "flux_app.h"
#include "flux_host.h"

#endif
