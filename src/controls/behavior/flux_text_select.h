#ifndef FLUX_TEXT_SELECT_H
#define FLUX_TEXT_SELECT_H

#include "controls/data/flux_text_data.h"
#include "store/flux_node_data.h"

#include <stdbool.h>

void flux_text_select_wire(FluxNodeData *nd, FluxTextData *td, bool selectable);

#endif
