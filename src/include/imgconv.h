#ifndef __IMGCONV_H
#define __IMGCONV_H

#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct imgconv_frame
{
    uint8_t *buffer;
    int size;
    int width, height;
} imgconv_frame;

typedef struct
{
    int width;
    int height;
    uint8_t *indices;
    uint8_t *palette;
} imgconv_palette_result;

imgconv_frame imgconv_decode(const uint8_t *data, int size,
                             enum AVCodecID codec_id,
                             enum AVPixelFormat dstfmt);
// imgconv_frame imgconv_resize(const uint8_t *src_buf, int sws_flags,
//                              int src_width, int src_height,
//                              enum AVPixelFormat srcfmt,
//                              enum AVPixelFormat dstfmt, int dst_width,
//                              int dst_height);
imgconv_frame imgconv_resize(const uint8_t *src_buf, int sws_flags,
                             int src_width, int src_height,
                             enum AVPixelFormat srcfmt,
                             enum AVPixelFormat dstfmt, int target_width,
                             int target_height, bool preserve_aspect_ratio);

imgconv_frame imgconv_filter_chain(const uint8_t *buf, int width, int height,
                                   enum AVPixelFormat dst_fmt,
                                   const char *filters);

enum dither_method
{
    DITHER_NONE,
    DITHER_FS,
    DITHER_ATKINSON
};

imgconv_palette_result imgconv_to_palette(const uint8_t *buf, int width,
                                          int height, int palette_size,
                                          enum dither_method method);

#endif /* __IMGCONV_H */
