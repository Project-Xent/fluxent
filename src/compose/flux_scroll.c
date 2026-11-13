#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "compose/flux_scroll.h"

#include <cwinrt/cast.h>
#include <cwinrt/factory.h>

#include <stdlib.h>

struct FluxScrollTracker {
	WUC_Comp                        *comp;
	WUICOIN_InteractionTracker      *tracker;
	WUICOIN_VisualInteractionSource *source;
};
