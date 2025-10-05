#ifndef __IMAGE_H
#define __IMAGE_H

#include "imgconv.h"
#include <stddef.h>
#include <stdint.h>

typedef struct image_t
{
    uint8_t *data;
    size_t size;
    int width, height;
    char *title;
    char *comment;
    struct image_t *repr;
} image_t;

void image_free(image_t *img);
int image_resize(image_t *img, int flags, int width, int height);
image_t image_from_frame(imgconv_frame *frame);

#endif /* __IMAGE_H */
