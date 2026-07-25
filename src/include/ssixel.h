#ifndef __SSIXEL_H
#define __SSIXEL_H

#include "ds.h"
#include <stdint.h>

void sixel_encode_rgb(str_t *out, const uint8_t *rgb, int width, int height,
                      int max_colors);

#endif /* __SSIXEL_H */
