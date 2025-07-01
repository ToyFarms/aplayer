#ifndef __IMGCONV_H
#define __IMGCONV_H

#include "libavcodec/codec_id.h"
#include "libavutil/pixfmt.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct imgconv_frame
{
    uint8_t *buffer;
    int size;
    int width, height;
} imgconv_frame;

imgconv_frame imgconv_decode(const uint8_t *data, int size,
                             enum AVCodecID codec_id,
                             enum AVPixelFormat dstfmt);
imgconv_frame imgconv_resize(const uint8_t *src_buf, int src_width,
                             int src_height, enum AVPixelFormat srcfmt,
                             enum AVPixelFormat dstfmt, int dst_width,
                             int dst_height);

#endif /* __IMGCONV_H */
