#ifndef FLUX_COLOR_NAME_H
#define FLUX_COLOR_NAME_H

#include <stdbool.h>
#include <stdint.h>

char const *flux_color_name(uint8_t r, uint8_t g, uint8_t b);
void        flux_color_next_named(double *hsv, int channel, bool lower, bool wrap, double max_hue);

#endif
