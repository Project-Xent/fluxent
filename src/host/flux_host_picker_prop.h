#ifndef FLUX_HOST_PICKER_PROP_H
#define FLUX_HOST_PICKER_PROP_H

#include "flux_host_internal.h"

#define FLUX_PICKER_PROP(name)                                                                       \
	bool name(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v)

FLUX_PICKER_PROP(flux_host_apply_header);
FLUX_PICKER_PROP(flux_host_apply_date);
FLUX_PICKER_PROP(flux_host_apply_time);
FLUX_PICKER_PROP(flux_host_apply_min_year);
FLUX_PICKER_PROP(flux_host_apply_max_year);
FLUX_PICKER_PROP(flux_host_apply_min_date);
FLUX_PICKER_PROP(flux_host_apply_max_date);
FLUX_PICKER_PROP(flux_host_apply_first_day);
FLUX_PICKER_PROP(flux_host_apply_clock);
FLUX_PICKER_PROP(flux_host_apply_minute_increment);
FLUX_PICKER_PROP(flux_host_apply_color);
FLUX_PICKER_PROP(flux_host_apply_alpha_enabled);
FLUX_PICKER_PROP(flux_host_apply_more_button);
FLUX_PICKER_PROP(flux_host_apply_spectrum);

#endif
