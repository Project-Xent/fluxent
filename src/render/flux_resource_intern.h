#ifndef FLUX_RESOURCE_INTERN_H
#define FLUX_RESOURCE_INTERN_H

#include <xent/xent_display.h>

XentResourceId flux_resource_intern_path(char const *path);
char const    *flux_resource_path(XentResourceId id);

#endif
