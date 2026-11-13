#ifndef FLUX_LIMIT_H
#define FLUX_LIMIT_H

typedef enum FluxLimitKind
{
	FLUX_LIMIT_EVENT_CHAIN,
	FLUX_LIMIT_TXN_LISTS,
	FLUX_LIMIT_COUNT,
} FluxLimitKind;

void flux_limit_exceeded(FluxLimitKind kind);

#endif
