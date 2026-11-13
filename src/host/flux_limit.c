#include "flux_limit.h"

#include <assert.h>

static unsigned limit_hits [FLUX_LIMIT_COUNT];

void            flux_limit_exceeded(FluxLimitKind kind) {
	limit_hits [kind]++;
	assert(!"fixed-capacity buffer overflow");
}
