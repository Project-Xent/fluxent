#include "render/flux_resource_intern.h"
#include <fluxent/flux_host.h>

#include <stdlib.h>
#include <string.h>

#define FLUX_RESOURCE_CAP 128u

typedef struct FluxResourceSlot {
	XentResourceId id;
	char          *path;
} FluxResourceSlot;

static FluxResourceSlot g_slots [FLUX_RESOURCE_CAP];
static uint32_t         g_count;
static uint64_t         g_next = 1u;

static char            *res_dup(char const *s) {
	if (!s) return NULL;
	size_t n = strlen(s) + 1u;
	char  *p = ( char * ) malloc(n);
	if (p) memcpy(p, s, n);
	return p;
}

XentResourceId flux_resource_intern_path(char const *path) {
	if (!path || !path [0]) return XENT_RESOURCE_INVALID;
	for (uint32_t i = 0; i < g_count; i++)
		if (g_slots [i].path && strcmp(g_slots [i].path, path) == 0) return g_slots [i].id;
	if (g_count >= FLUX_RESOURCE_CAP) return XENT_RESOURCE_INVALID;
	char *dup = res_dup(path);
	if (!dup) return XENT_RESOURCE_INVALID;
	XentResourceId id   = ( XentResourceId ) g_next++;
	g_slots [g_count++] = (FluxResourceSlot) {.id = id, .path = dup};
	return id;
}

char const *flux_resource_path(XentResourceId id) {
	if (id == XENT_RESOURCE_INVALID) return NULL;
	for (uint32_t i = 0; i < g_count; i++)
		if (g_slots [i].id == id) return g_slots [i].path;
	return NULL;
}

XtkResource flux_image_respath(char const *path) { return ( XtkResource ) flux_resource_intern_path(path); }
