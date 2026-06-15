#include "flux_str.h"

#include <stdlib.h>
#include <string.h>

char *flux_str_dup(char const *s) {
	if (!s) return NULL;
	size_t len  = strlen(s) + 1;
	char  *copy = ( char * ) malloc(len);
	if (copy) memcpy(copy, s, len);
	return copy;
}

void flux_str_replace(char const **slot, char const *s) {
	if (!slot) return;
	char *copy = flux_str_dup(s);
	free(( void * ) *slot);
	*slot = copy;
}

void flux_str_free(char const *s) { free(( void * ) s); }
