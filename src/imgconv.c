#include "imgconv.h"
#include "_math.h"
#include "ds.h"
#include "logger.h"

#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

// TODO: add persistent decoder chain, and possibly for sws context also

imgconv_frame imgconv_decode(const uint8_t *data, int size,
                             enum AVCodecID codec_id, enum AVPixelFormat dstfmt)
{
    imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
    AVCodecContext *avctx = NULL;
    AVPacket *pkt = NULL;
    AVFrame *src_frame = NULL;
    AVFrame *dst_frame = NULL;
    struct SwsContext *sws = NULL;

    if (codec_id == AV_CODEC_ID_NONE)
    {
        AVProbeData pd = {.buf = (uint8_t *)data, .buf_size = size};
        const AVInputFormat *fmt = av_probe_input_format(&pd, 1);
        if (fmt == NULL || fmt->raw_codec_id == AV_CODEC_ID_NONE)
        {
            log_error("Failed to probe data\n");
            goto error;
        }

        codec_id = fmt->raw_codec_id;
    }

    if (codec_id == AV_CODEC_ID_NONE)
    {
        log_error("Failed to get codec id\n");
        goto error;
    }

    const AVCodec *dec = avcodec_find_decoder(codec_id);
    if (dec == NULL)
    {
        log_error("Failed to find decoder\n");
        goto error;
    }

    avctx = avcodec_alloc_context3(dec);
    if (avctx == NULL)
    {
        log_error("Could not found decoder for the id: %s\n",
                  avcodec_get_name(codec_id));
        goto error;
    }

    int ret = avcodec_open2(avctx, dec, NULL);
    if (ret < 0)
    {
        log_error("Failed to open decoder: %s\n", av_err2str(ret));
        goto error;
    }

    pkt = av_packet_alloc();
    src_frame = av_frame_alloc();

    pkt->data = (uint8_t *)data;
    pkt->size = size;
    ret = avcodec_send_packet(avctx, pkt);
    if (ret < 0)
    {
        log_error("Failed to send packet to decoder: %s\n", av_err2str(ret));
        goto error;
    }

    ret = avcodec_receive_frame(avctx, src_frame);
    if (ret < 0)
    {
        log_error("Failed to receive frame: %s\n", av_err2str(ret));
        goto error;
    }

    sws = sws_getContext(src_frame->width, src_frame->height, src_frame->format,
                         src_frame->width, src_frame->height, dstfmt, 0, NULL,
                         NULL, NULL);
    if (sws == NULL)
    {
        log_error("Failed to get sws context\n");
        goto error;
    }

    dst_frame = av_frame_alloc();
    dst_frame->format = dstfmt;
    dst_frame->width = src_frame->width;
    dst_frame->height = src_frame->height;
    ret = av_frame_get_buffer(dst_frame, 1);
    if (ret < 0)
    {
        log_error("Failed to allocate new buffer for dst_frame: %s\n",
                  av_err2str(ret));
        goto error;
    }

    ret = sws_scale_frame(sws, dst_frame, src_frame);
    if (ret < 0)
    {
        log_error("sws_scale failed: %s\n", av_err2str(ret));
        goto error;
    }

    int buf_size = av_image_get_buffer_size(dstfmt, src_frame->width,
                                            src_frame->height, 1);
    uint8_t *buf = av_malloc(buf_size);
    if (buf == NULL)
    {
        log_error("Allocation failure\n");
        goto error;
    }

    ret = av_image_copy_to_buffer(
        buf, buf_size, (const uint8_t *const *)dst_frame->data,
        dst_frame->linesize, dstfmt, dst_frame->width, dst_frame->height, 1);
    if (ret < 0)
    {
        log_error("av_image_copy_to_buffer failed: %s\n", av_err2str(ret));
        av_free(buf);
        goto error;
    }

    f.size = buf_size;
    f.width = dst_frame->width;
    f.height = dst_frame->height;
    f.buffer = buf;

error:
    avcodec_free_context(&avctx);
    av_packet_free(&pkt);
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    sws_freeContext(sws);

    return f;
}

// imgconv_frame imgconv_resize(const uint8_t *src_buf, int sws_flags,
//                              int src_width, int src_height,
//                              enum AVPixelFormat srcfmt,
//                              enum AVPixelFormat dstfmt, int target_width,
//                              int target_height, enum imgconv_scale_mode mode)
// {
//     imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
//     struct SwsContext *sws = NULL;
//     AVFrame *src_frame = NULL;
//     AVFrame *tmp_frame = NULL;
//     AVFrame *dst_frame = NULL;
//     int ret = 0;
//
//     float src_aspect = (float)src_width / src_height;
//     int scaled_w = target_width;
//     int scaled_h = target_height;
//     if (mode == IMGCONV_SCALE_BY_WIDTH)
//         scaled_h = lrintf(target_width / src_aspect);
//     else
//         scaled_w = lrintf(target_height * src_aspect);
//
//     // Allocate source frame
//     src_frame = av_frame_alloc();
//     if (!src_frame)
//         goto cleanup;
//     ret = av_image_fill_arrays(src_frame->data, src_frame->linesize, src_buf,
//                                srcfmt, src_width, src_height, 1);
//     if (ret < 0)
//         goto cleanup;
//
//     tmp_frame = av_frame_alloc();
//     if (!tmp_frame)
//         goto cleanup;
//     tmp_frame->format = dstfmt;
//     tmp_frame->width = scaled_w;
//     tmp_frame->height = scaled_h;
//     ret = av_frame_get_buffer(tmp_frame, 1);
//     if (ret < 0)
//         goto cleanup;
//
//     sws = sws_getContext(src_width, src_height, srcfmt, scaled_w, scaled_h,
//                          dstfmt, sws_flags, NULL, NULL, NULL);
//     if (!sws)
//         goto cleanup;
//     ret = sws_scale(sws, (const uint8_t *const *)src_frame->data,
//                     src_frame->linesize, 0, src_height, tmp_frame->data,
//                     tmp_frame->linesize);
//     if (ret < 0)
//         goto cleanup;
//     sws_freeContext(sws);
//     sws = NULL;
//
//     dst_frame = av_frame_alloc();
//     if (!dst_frame)
//         goto cleanup;
//     dst_frame->format = dstfmt;
//     dst_frame->width = target_width;
//     dst_frame->height = target_height;
//     ret = av_frame_get_buffer(dst_frame, 1);
//     if (ret < 0)
//         goto cleanup;
//
//     ptrdiff_t linesizes[AV_NUM_DATA_POINTERS] = {0};
//     for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
//     {
//         linesizes[i] = (ptrdiff_t)dst_frame->linesize[i];
//     }
//     ret = av_image_fill_black(dst_frame->data, linesizes, dst_frame->format,
//                               AVCOL_RANGE_UNSPECIFIED, dst_frame->width,
//                               dst_frame->height);
//     if (ret < 0)
//         goto cleanup;
//
//     int x_off = (target_width - scaled_w) / 2;
//     int y_off = (target_height - scaled_h) / 2;
//     const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(dstfmt);
//
//     // for (int p = 0; p < AV_NUM_DATA_POINTERS && dst_frame->data[p]; p++)
//     // {
//     //     int pw = (p == 0) ? scaled_w : (scaled_w >> desc->log2_chroma_w);
//     //     int ph = (p == 0) ? scaled_h : (scaled_h >> desc->log2_chroma_h);
//     //     int step = desc->comp[p].step;
//     //     int dst_linesz = dst_frame->linesize[p];
//     //     int src_linesz = dst_frame->linesize[p];
//     //     int x_plane_off =
//     //         (p == 0 ? x_off : (x_off >> desc->log2_chroma_w)) * step;
//     //     int y_plane_off = (p == 0 ? y_off : (y_off >>
//     desc->log2_chroma_h));
//     //
//     //     for (int y = 0; y < ph; y++)
//     //     {
//     //         uint8_t *dst_row = dst_frame->data[p] +
//     //                            (y + y_plane_off) * dst_linesz +
//     x_plane_off;
//     //         uint8_t *src_row = dst_frame->data[p] + y * src_linesz;
//     //         memcpy(dst_row, src_row, pw * step);
//     //     }
//     // }
//
//     int buf_size =
//         av_image_get_buffer_size(dstfmt, target_width, target_height, 1);
//     uint8_t *buf = av_malloc(buf_size);
//     if (!buf)
//         goto cleanup;
//     ret = av_image_copy_to_buffer(
//         buf, buf_size, (const uint8_t *const *)dst_frame->data,
//         dst_frame->linesize, dstfmt, target_width, target_height, 1);
//     if (ret < 0)
//     {
//         av_free(buf);
//         goto cleanup;
//     }
//
//     f.buffer = buf;
//     f.size = buf_size;
//     f.width = target_width;
//     f.height = target_height;
//
// cleanup:
//     av_frame_free(&src_frame);
//     av_frame_free(&tmp_frame);
//     av_frame_free(&dst_frame);
//     if (sws)
//         sws_freeContext(sws);
//     return f;
// }

static int imgconv_resize_no_preserve(const uint8_t *src_buf, int sws_flags,
                                      int src_width, int src_height,
                                      enum AVPixelFormat srcfmt,
                                      enum AVPixelFormat dstfmt, int dst_width,
                                      int dst_height, AVFrame *dst_frame)
{
    int ret = 0;
    AVFrame *src_frame = NULL;
    struct SwsContext *sws = NULL;

    sws = sws_getContext(src_width, src_height, srcfmt, dst_width, dst_height,
                         dstfmt, sws_flags, NULL, NULL, NULL);
    if (sws == NULL)
    {
        ret = -1;
        goto cleanup;
    }

    src_frame = av_frame_alloc();
    if (src_frame == NULL)
    {
        ret = -1;
        goto cleanup;
    }
    ret = av_image_fill_arrays(src_frame->data, src_frame->linesize, src_buf,
                               srcfmt, src_width, src_height, 1);
    if (ret < 0)
        goto cleanup;

    dst_frame->format = dstfmt;
    dst_frame->width = dst_width;
    dst_frame->height = dst_height;
    ret = av_frame_get_buffer(dst_frame, 1);
    if (ret < 0)
        goto cleanup;

    ret = sws_scale(sws, (const uint8_t *const *)src_frame->data,
                    src_frame->linesize, 0, src_height, dst_frame->data,
                    dst_frame->linesize);
    if (ret < 0)
        goto cleanup;

cleanup:
    av_frame_free(&src_frame);
    sws_freeContext(sws);

    return ret;
}

static AVFrame *imgconv_fit(AVFrame *frame, int target_width, int target_height)
{
    int ret;
    AVFrame *dst = av_frame_alloc();
    if (!dst)
        return NULL;

    dst->width = target_width;
    dst->height = target_height;
    dst->format = frame->format;
    dst->color_range = frame->color_range;

    if ((ret = av_frame_get_buffer(dst, 1)) < 0)
    {
        av_frame_free(&dst);
        return NULL;
    }

    if ((ret = av_image_fill_black(dst->data, (const ptrdiff_t *)dst->linesize,
                                   dst->format, dst->color_range, dst->width,
                                   dst->height)) < 0)
    {
        av_frame_free(&dst);
        return NULL;
    }

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    // Compute offsets
    int x_off = (target_width - frame->width) / 2;
    int y_off = (target_height - frame->height) / 2;
    // For cropping, determine source offset if frame larger
    int src_x_off = x_off < 0 ? -x_off : 0;
    int src_y_off = y_off < 0 ? -y_off : 0;
    // Destination offset inside dst
    int dst_x_off = x_off > 0 ? x_off : 0;
    int dst_y_off = y_off > 0 ? y_off : 0;

    for (int p = 0; p < AV_NUM_DATA_POINTERS; p++)
    {
        if (!frame->data[p])
            continue;
        int shift_x = (p == 0 ? 0 : desc->log2_chroma_w);
        int shift_y = (p == 0 ? 0 : desc->log2_chroma_h);

        // calculate plane dimensions
        int in_w = (frame->width + ((1 << shift_x) - 1)) >> shift_x;
        int in_h = (frame->height + ((1 << shift_y) - 1)) >> shift_y;
        int out_w = (target_width + ((1 << shift_x) - 1)) >> shift_x;
        int out_h = (target_height + ((1 << shift_y) - 1)) >> shift_y;

        // effective copy region
        int copy_w = FFMIN(in_w - (src_x_off >> shift_x), out_w);
        int copy_h = FFMIN(in_h - (src_y_off >> shift_y), out_h);
        if (copy_w <= 0 || copy_h <= 0)
            continue;

        int bpp = desc->comp[p].step;
        int copy_bytes = copy_w * bpp;

        uint8_t *src_ptr = frame->data[p] +
                           (src_y_off >> shift_y) * frame->linesize[p] +
                           (src_x_off >> shift_x) * bpp;
        uint8_t *dst_ptr = dst->data[p] +
                           (dst_y_off >> shift_y) * dst->linesize[p] +
                           (dst_x_off >> shift_x) * bpp;

        av_image_copy_plane(dst_ptr, dst->linesize[p], src_ptr,
                            frame->linesize[p], copy_bytes, copy_h);
    }

    return dst;
}

imgconv_frame imgconv_resize(const uint8_t *src_buf, int sws_flags,
                             int src_width, int src_height,
                             enum AVPixelFormat srcfmt,
                             enum AVPixelFormat dstfmt, int dst_width,
                             int dst_height, bool preserve_aspect_ratio)
{
    imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
    int ret = 0;
    bool need_padding = false;

    int target_width = dst_width;
    int target_height = dst_height;
    float aspect_ratio = (float)src_width / (float)src_height;

    // TODO: don't hardcode packed rgb24, since we need the alpha channel if we
    // ever need to not render the padded border
    if (preserve_aspect_ratio)
    {
        if (dst_width > dst_height)
        {
            target_width = lrintf(dst_height * aspect_ratio);
            target_height = dst_height;
        }
        else
        {
            target_width = dst_width;
            target_height = lrintf(dst_width / aspect_ratio);
        }
    }

    AVFrame *resized = av_frame_alloc();
    ret = imgconv_resize_no_preserve(src_buf, sws_flags, src_width, src_height,
                                     srcfmt, dstfmt, target_width,
                                     target_height, resized);
    if (ret < 0)
    {
        log_error("Failed to resize the image: %s\n", av_err2str(ret));
        goto cleanup;
    }

    AVFrame *final_frame = resized;

    need_padding = preserve_aspect_ratio &&
                   (target_width != dst_width || target_height != dst_height);

    if (need_padding)
    {
        // TODO: allocate frame from outside the function
        AVFrame *padded = imgconv_fit(resized, dst_width, dst_height);
        if (padded == NULL)
        {
            log_error("Failed to pad the image\n");
            goto cleanup;
        }
        final_frame = padded;
    }

    int buf_size = av_image_get_buffer_size(
        final_frame->format, final_frame->width, final_frame->height, 1);
    if (buf_size < 0)
    {
        log_error("Failed to get buffer size: %s\n", av_err2str(buf_size));
        goto cleanup;
    }

    uint8_t *buf = av_malloc(buf_size);
    if (buf == NULL)
    {
        log_error("Allocation failure\n");
        goto cleanup;
    }

    ret = av_image_copy_to_buffer(buf, buf_size,
                                  (const uint8_t *const *)final_frame->data,
                                  final_frame->linesize, final_frame->format,
                                  final_frame->width, final_frame->height, 1);
    if (ret < 0)
    {
        log_error("av_image_copy_to_buffer failed: %s\n", av_err2str(ret));
        av_free(buf);
        goto cleanup;
    }
    f.buffer = buf;
    f.size = buf_size;
    f.width = final_frame->width;
    f.height = final_frame->height;

cleanup:
    av_frame_free(&resized);
    if (need_padding)
        av_frame_free(&final_frame);
    return f;
}

imgconv_frame imgconv_filter_chain(const uint8_t *buf, int width, int height,
                                   enum AVPixelFormat dst_fmt,
                                   const char *filters)
{
    imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
    AVFilterGraph *filter_graph = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    enum AVPixelFormat pix_fmts[] = {dst_fmt, AV_PIX_FMT_NONE};
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterInOut *inputs = NULL;
    AVFilterInOut *outputs = NULL;
    AVFrame *input = NULL;
    AVFrame *output = NULL;
    uint8_t *b = NULL;
    str_t args = {0};
    int ret = 0;

    filter_graph = avfilter_graph_alloc();
    if (filter_graph == NULL)
    {
        log_error("Failed to allocate graph context\n");
        goto cleanup;
    }

    buffersrc = avfilter_get_by_name("buffer");
    if (buffersrc == NULL)
    {
        log_error("Failed to get buffer filter\n");
        goto cleanup;
    }
    buffersink = avfilter_get_by_name("buffersink");
    if (buffersink == NULL)
    {
        log_error("Failed to get buffer sink filter\n");
        goto cleanup;
    }

    args = str_create();
    str_catf(&args,
             "video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=1/1",
             width, height, AV_PIX_FMT_RGB24);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args.buf, NULL, filter_graph);
    if (ret < 0)
    {
        log_error("Failed to create input node: %s\n", av_err2str(ret));
        goto cleanup;
    }
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL,
                                       NULL, filter_graph);
    if (ret < 0)
    {
        log_error("Failed to create output node: %s\n", av_err2str(ret));
        goto cleanup;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        log_error("Failed to set pix_fmts to output: %s\n", av_err2str(ret));
        goto cleanup;
    }

    ret = avfilter_graph_parse_ptr(filter_graph, filters, &inputs, &outputs,
                                   NULL);
    if (ret < 0)
    {
        log_error("Failed to parse filter expression: %s\n", av_err2str(ret));
        goto cleanup;
    }

    ret = avfilter_link(buffersrc_ctx, 0, inputs->filter_ctx, inputs->pad_idx);
    if (ret < 0)
    {
        log_error("Failed to link input to filter input: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }

    ret = avfilter_link(outputs->filter_ctx, 0, buffersink_ctx, 0);
    if (ret < 0)
    {
        log_error("Failed to link filter output to output: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }

    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0)
    {
        log_error("Failed to configure filter graph: %s\n", av_err2str(ret));
        goto cleanup;
    }

    input = av_frame_alloc();
    if (input == NULL)
    {
        log_error("Failed allocate input frame\n");
        goto cleanup;
    }
    input->format = AV_PIX_FMT_RGB24;
    input->width = width;
    input->height = height;
    ret = av_image_fill_arrays(input->data, input->linesize, buf,
                               AV_PIX_FMT_RGB24, width, height, 1);
    if (ret < 0)
    {
        log_error("Failed to fill input frame with data: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }

    ret = av_buffersrc_add_frame(buffersrc_ctx, input);
    if (ret < 0)
    {
        log_error("Failed to add input frame to filter input: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }
    ret = av_buffersrc_add_frame(buffersrc_ctx, NULL);
    if (ret < 0)
    {
        log_error("Failed to send EOF to filter input: %s\n", av_err2str(ret));
        goto cleanup;
    }

    output = av_frame_alloc();
    if (output == NULL)
    {
        log_error("Failed to allocate output frame\n");
        goto cleanup;
    }
    ret = av_buffersink_get_frame(buffersink_ctx, output);
    if (ret < 0)
    {
        log_error("Failed to receive frame from filter output: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }

    int buf_size = av_image_get_buffer_size(output->format, output->width,
                                            output->height, 1);
    if (buf_size < 0)
    {
        log_error("Could not calculate the required output buffer size: %s\n",
                  av_err2str(ret));
        goto cleanup;
    }
    b = av_malloc(buf_size);
    if (b == NULL)
    {
        log_error("Failed to allocate output buffer\n");
        goto cleanup;
    }
    ret = av_image_copy_to_buffer(
        b, buf_size, (const uint8_t *const *)output->data, output->linesize,
        output->format, output->width, output->height, 1);
    if (ret < 0)
    {
        log_error("Failed to copy from output frame to output buffer: %s\n",
                  av_err2str(ret));
        av_free(b);
        goto cleanup;
    }

    f.buffer = b;
    f.size = buf_size;
    f.width = output->width;
    f.height = output->height;

cleanup:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    avfilter_graph_free(&filter_graph);
    av_frame_free(&input);
    av_frame_free(&output);
    str_free(&args);

    return f;
}

static inline uint32_t color_dist2(uint8_t r1, uint8_t g1, uint8_t b1,
                                   uint8_t r2, uint8_t g2, uint8_t b2)
{
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return dr * dr + dg * dg + db * db;
}

imgconv_palette_result imgconv_to_palette(const uint8_t *buf, int width,
                                          int height, int palette_size,
                                          enum dither_method method)
{
    imgconv_palette_result res = {
        .width = -1, .height = -1, .indices = NULL, .palette = NULL};

    if (!buf || width <= 0 || height <= 0 || palette_size <= 0)
    {
        log_error("invalid parameters");
        return res;
    }
    int npix = width * height;
    uint8_t *palette = malloc(palette_size * 3);
    if (!palette)
    {
        log_error("alloc palette");
        return res;
    }
    // simple k-means init: pick first palette_size pixels
    for (int i = 0; i < palette_size; i++)
    {
        int idx = (i * npix) / palette_size;
        palette[3 * i + 0] = buf[3 * idx + 0];
        palette[3 * i + 1] = buf[3 * idx + 1];
        palette[3 * i + 2] = buf[3 * idx + 2];
    }
    int *assign = malloc(npix * sizeof(int));
    if (!assign)
    {
        log_error("alloc assign");
        free(palette);
        return res;
    }
    int max_iter = 10;
    for (int it = 0; it < max_iter; it++)
    {
        // assign
        for (int i = 0; i < npix; i++)
        {
            uint8_t r = buf[3 * i + 0], g = buf[3 * i + 1], b = buf[3 * i + 2];
            uint32_t best = UINT32_MAX;
            int bi = 0;
            for (int j = 0; j < palette_size; j++)
            {
                uint32_t d =
                    color_dist2(r, g, b, palette[3 * j + 0], palette[3 * j + 1],
                                palette[3 * j + 2]);
                if (d < best)
                {
                    best = d;
                    bi = j;
                }
            }
            assign[i] = bi;
        }
        // update
        uint64_t *sum = calloc(palette_size * 3, sizeof(uint64_t));
        uint32_t *count = calloc(palette_size, sizeof(uint32_t));
        if (!sum || !count)
        {
            log_error("alloc sum/count");
            free(sum);
            free(count);
            free(assign);
            free(palette);
            return res;
        }
        for (int i = 0; i < npix; i++)
        {
            int c = assign[i];
            sum[3 * c + 0] += buf[3 * i + 0];
            sum[3 * c + 1] += buf[3 * i + 1];
            sum[3 * c + 2] += buf[3 * i + 2];
            count[c]++;
        }
        for (int j = 0; j < palette_size; j++)
        {
            if (count[j])
            {
                palette[3 * j + 0] = sum[3 * j + 0] / count[j];
                palette[3 * j + 1] = sum[3 * j + 1] / count[j];
                palette[3 * j + 2] = sum[3 * j + 2] / count[j];
            }
        }
        free(sum);
        free(count);
    }
    uint8_t *idx_img = malloc(npix);
    if (!idx_img)
    {
        log_error("alloc idx_img");
        free(assign);
        free(palette);
        return res;
    }
    // dithering buffers
    float *err_buf = NULL;
    int stride = width + 4;
    if (method != DITHER_NONE)
    {
        err_buf = calloc((height + 4) * stride * 3, sizeof(float));
        if (!err_buf)
        {
            log_error("alloc err_buf");
            free(idx_img);
            free(assign);
            free(palette);
            return res;
        }
    }
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = y * width + x;
            float r = buf[3 * i + 0];
            float g = buf[3 * i + 1];
            float b = buf[3 * i + 2];
            if (err_buf)
            {
                int eidx = ((y + 2) * stride + (x + 2)) * 3;
                r += err_buf[eidx + 0];
                g += err_buf[eidx + 1];
                b += err_buf[eidx + 2];
            }
            uint8_t r0 = (uint8_t)fminf(fmaxf(r, 0), 255);
            uint8_t g0 = (uint8_t)fminf(fmaxf(g, 0), 255);
            uint8_t b0 = (uint8_t)fminf(fmaxf(b, 0), 255);
            int best_i = 0;
            uint32_t best_d = UINT32_MAX;
            for (int j = 0; j < palette_size; j++)
            {
                uint32_t d =
                    color_dist2(r0, g0, b0, palette[3 * j + 0],
                                palette[3 * j + 1], palette[3 * j + 2]);
                if (d < best_d)
                {
                    best_d = d;
                    best_i = j;
                }
            }
            idx_img[i] = best_i;

            if (err_buf && method == DITHER_FS)
            {
                int base = ((y + 2) * stride + (x + 2)) * 3;
                float dr = r - palette[3 * best_i + 0];
                float dg = g - palette[3 * best_i + 1];
                float db = b - palette[3 * best_i + 2];
                err_buf[base + 3 + 0] += dr * 7.0f / 16.0f;
                err_buf[base + 3 + 1] += dg * 7.0f / 16.0f;
                err_buf[base + 3 + 2] += db * 7.0f / 16.0f;
                err_buf[(base - 3 + stride * 3) + 0] += dr * 3.0f / 16.0f;
                err_buf[(base - 3 + stride * 3) + 1] += dg * 3.0f / 16.0f;
                err_buf[(base - 3 + stride * 3) + 2] += db * 3.0f / 16.0f;
                err_buf[(base + stride * 3) + 0] += dr * 5.0f / 16.0f;
                err_buf[(base + stride * 3) + 1] += dg * 5.0f / 16.0f;
                err_buf[(base + stride * 3) + 2] += db * 5.0f / 16.0f;
                err_buf[(base + 3 + stride * 3) + 0] += dr * 1.0f / 16.0f;
                err_buf[(base + 3 + stride * 3) + 1] += dg * 1.0f / 16.0f;
                err_buf[(base + 3 + stride * 3) + 2] += db * 1.0f / 16.0f;
            }
        }
    }
    free(assign);
    free(err_buf);
    res.width = width;
    res.height = height;
    res.indices = idx_img;
    res.palette = palette;
    return res;
}

// imgconv_palette_result imgconv_to_256palette(const uint8_t *buf, int width,
//                                              int height)
// {
//     imgconv_palette_result res = {.width = -1, .height = -1, .indices =
//     NULL}; AVFilterGraph *filter_graph = NULL; AVFilterContext *buffersrc_ctx
//     = NULL; AVFilterContext *buffersink_ctx = NULL; AVFilterContext
//     *palgen_ctx = NULL; enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_RGB24,
//     AV_PIX_FMT_NONE}; const AVFilter *buffersrc = NULL; const AVFilter
//     *buffersink = NULL; const AVFilter *palgen = NULL; AVFrame *input = NULL;
//     AVFrame *pal_frame = NULL;
//     uint8_t *b = NULL;
//     str_t args = {0};
//     int ret = 0;
//
//     filter_graph = avfilter_graph_alloc();
//     if (filter_graph == NULL)
//     {
//         log_error("Failed to allocate graph context\n");
//         goto cleanup;
//     }
//
//     buffersrc = avfilter_get_by_name("buffer");
//     if (buffersrc == NULL)
//     {
//         log_error("Failed to get buffer filter\n");
//         goto cleanup;
//     }
//     buffersink = avfilter_get_by_name("buffersink");
//     if (buffersink == NULL)
//     {
//         log_error("Failed to get buffer sink filter\n");
//         goto cleanup;
//     }
//
//     args = str_create();
//     str_catf(&args,
//              "video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=1/1",
//              width, height, AV_PIX_FMT_RGB24);
//
//     ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
//                                        args.buf, NULL, filter_graph);
//     if (ret < 0)
//     {
//         log_error("Failed to create input node: %s\n", av_err2str(ret));
//         goto cleanup;
//     }
//
//     ret = avfilter_graph_create_filter(&palgen_ctx, palgen, "palgen", NULL,
//                                        NULL, filter_graph);
//     if (ret < 0)
//     {
//         log_error("Failed to create input node: %s\n", av_err2str(ret));
//         goto cleanup;
//     }
//
//     ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
//     NULL,
//                                        NULL, filter_graph);
//     if (ret < 0)
//     {
//         log_error("Failed to create output node: %s\n", av_err2str(ret));
//         goto cleanup;
//     }
//
//     ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
//                               AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
//     if (ret < 0)
//     {
//         log_error("Failed to set pix_fmts to output: %s\n", av_err2str(ret));
//         goto cleanup;
//     }
//
//     if ((ret = avfilter_link(buffersink_ctx, 0, palgen_ctx, 0)) < 0 ||
//         (ret = avfilter_link(palgen_ctx, 0, buffersink_ctx, 0)) < 0 ||
//         (ret = avfilter_graph_config(filter_graph, NULL)) < 0)
//     {
//         log_error("Palette graph config failed: %s\n", av_err2str(ret));
//         goto cleanup;
//     }
//
//     input = av_frame_alloc();
//     if (input == NULL)
//     {
//         log_error("Failed allocate input frame\n");
//         goto cleanup;
//     }
//     input->format = AV_PIX_FMT_RGB24;
//     input->width = width;
//     input->height = height;
//     ret = av_image_fill_arrays(input->data, input->linesize, buf,
//                                AV_PIX_FMT_RGB24, width, height, 1);
//     if (ret < 0)
//     {
//         log_error("Failed to fill input frame with data: %s\n",
//                   av_err2str(ret));
//         goto cleanup;
//     }
//
//     ret = av_buffersrc_add_frame(buffersrc_ctx, input);
//     if (ret < 0)
//     {
//         log_error("Failed to add input frame to filter input: %s\n",
//                   av_err2str(ret));
//         goto cleanup;
//     }
//     ret = av_buffersrc_add_frame(buffersrc_ctx, NULL);
//     if (ret < 0)
//     {
//         log_error("Failed to send EOF to filter input: %s\n",
//         av_err2str(ret)); goto cleanup;
//     }
//
//     pal_frame = av_frame_alloc();
//     if (pal_frame == NULL)
//     {
//         log_error("Failed to allocate output frame\n");
//         goto cleanup;
//     }
//     ret = av_buffersink_get_frame(buffersink_ctx, pal_frame);
//     if (ret < 0)
//     {
//         log_error("Failed to receive frame from filter output: %s\n",
//                   av_err2str(ret));
//         goto cleanup;
//     }
//
//     int buf_size = av_image_get_buffer_size(pal_frame->format,
//     pal_frame->width,
//                                             pal_frame->height, 1);
//     if (buf_size < 0)
//     {
//         log_error("Could not calculate the required output buffer size:
//         %s\n",
//                   av_err2str(ret));
//         goto cleanup;
//     }
//     b = av_malloc(buf_size);
//     if (b == NULL)
//     {
//         log_error("Failed to allocate output buffer\n");
//         goto cleanup;
//     }
//     ret = av_image_copy_to_buffer(b, buf_size,
//                                   (const uint8_t *const *)pal_frame->data,
//                                   pal_frame->linesize, pal_frame->format,
//                                   pal_frame->width, pal_frame->height, 1);
//     if (ret < 0)
//     {
//         log_error("Failed to copy from output frame to output buffer: %s\n",
//                   av_err2str(ret));
//         av_free(b);
//         goto cleanup;
//     }
//
// cleanup:
//     avfilter_graph_free(&filter_graph);
//     av_frame_free(&input);
//     av_frame_free(&pal_frame);
//     str_free(&args);
//
//     //     if (!pal_graph) {
//     //         log_error("Failed to alloc palette graph\n");
//     //         return res;
//     //     }
//     //     const AVFilter *buffersrc = avfilter_get_by_name("buffer");
//     //     const AVFilter *palettegen = avfilter_get_by_name("palettegen");
//     //     const AVFilter *buffersink = avfilter_get_by_name("buffersink");
//     //     if (!buffersrc || !palettegen || !buffersink) {
//     //         log_error("Filters missing\n");
//     //         goto cleanup_palette;
//     //     }
//     //     snprintf(args, sizeof(args),
//     // "video_size=%dx%d:pix_fmt=%d:time_base=1/25:pixel_aspect=1/1",
//     //              width, height, AV_PIX_FMT_RGB24);
//     //     if ((ret = avfilter_graph_create_filter(&src_ctx, buffersrc,
//     "src",
//     //                                              args, NULL, pal_graph)) <
//     0
//     //                                              ||
//     //         (ret = avfilter_graph_create_filter(&palgen_ctx, palettegen,
//     //         "palgen",
//     //                                              NULL, NULL, pal_graph)) <
//     0
//     //                                              ||
//     //         (ret = avfilter_graph_create_filter(&sink_ctx, buffersink,
//     //         "sink",
//     //                                              NULL, NULL, pal_graph)) <
//     0)
//     //                                              {
//     //         log_error("Palette filters creation failed: %s\n",
//     //         av_err2str(ret)); goto cleanup_palette;
//     //     }
//     //
//     //     enum AVPixelFormat out_fmts[] = { AV_PIX_FMT_RGBA, AV_PIX_FMT_NONE
//     };
//     //     av_opt_set_int_list(sink_ctx, "pix_fmts", out_fmts,
//     AV_PIX_FMT_NONE,
//     //     AV_OPT_SEARCH_CHILDREN);
//     //
//     //     // link: src -> palgen -> sink
//     //     if ((ret = avfilter_link(src_ctx, 0, palgen_ctx, 0)) < 0 ||
//     //         (ret = avfilter_link(palgen_ctx, 0, sink_ctx, 0)) < 0 ||
//     //         (ret = avfilter_graph_config(pal_graph, NULL)) < 0) {
//     //         log_error("Palette graph config failed: %s\n",
//     av_err2str(ret));
//     //         goto cleanup_palette;
//     //     }
//     //     // prepare input frame
//     //     in_frame = av_frame_alloc();
//     //     if (!in_frame) goto cleanup_palette;
//     //     in_frame->format = AV_PIX_FMT_RGB24;
//     //     in_frame->width  = width;
//     //     in_frame->height = height;
//     //     if ((ret = av_image_fill_arrays(in_frame->data,
//     in_frame->linesize,
//     //                                     buf, AV_PIX_FMT_RGB24,
//     //                                     width, height, 1)) < 0) {
//     //         log_error("Fill input failed: %s\n", av_err2str(ret));
//     //         goto cleanup_palette;
//     //     }
//     //     ret = av_buffersrc_add_frame(src_ctx, in_frame);
//     //     ret = av_buffersrc_add_frame(src_ctx, NULL);
//     //     pal_frame = av_frame_alloc();
//     //     if (!pal_frame || (ret = av_buffersink_get_frame(sink_ctx,
//     //     pal_frame)) < 0) {
//     //         log_error("Getting palette frame failed: %s\n",
//     av_err2str(ret));
//     //         goto cleanup_palette;
//     //     }
//     //     // extract palette
//     //     int pal_w = pal_frame->width;
//     //     int pal_h = pal_frame->height;
//     //     uint8_t *pdat = pal_frame->data[0];
//     //     int lsz = pal_frame->linesize[0];
//     //     int cnt = FFMIN(256, pal_w * pal_h);
//     //     for (int i = 0; i < cnt; i++) {
//     //         int x = i % pal_w;
//     //         int y = i / pal_w;
//     //         uint8_t *px = pdat + y * lsz + x * 4;
//     //         res.palette[i][0] = px[0];
//     //         res.palette[i][1] = px[1];
//     //         res.palette[i][2] = px[2];
//     //     }
//     //     for (int i = cnt; i < 256; i++) {
//     //         res.palette[i][0] = res.palette[i][1] = res.palette[i][2] = 0;
//     //     }
//     //
//     // cleanup_palette:
//     //     avfilter_graph_free(&pal_graph);
//     //     av_frame_free(&in_frame);
//     //     if (!pal_frame)
//     //         return res;
//     //
//     //     // // 2) Map to indices with paletteuse
//     //     // AVFilterGraph *idx_graph = avfilter_graph_alloc();
//     //     // AVFilterContext *src_img = NULL, *src_pal = NULL, *idx_sink =
//     //     NULL;
//     //     // AVFrame *out_frame = NULL;
//     //     // if (!idx_graph) {
//     //     //     log_error("Idx graph alloc failed\n");
//     //     //     goto cleanup_all;
//     //     // }
//     //     // const AVFilter *paletteuse =
//     avfilter_get_by_name("paletteuse");
//     //     // if (!buffersrc || !buffersink || !paletteuse) {
//     //     //     log_error("Required idx filters missing\n");
//     //     //     goto cleanup_all;
//     //     // }
//     //     // // image input
//     //     // if ((ret = avfilter_graph_create_filter(&src_img, buffersrc,
//     //     "srcImg",
//     //     //                                          args, NULL,
//     idx_graph)) <
//     //     0 ||
//     //     //     (ret = avfilter_graph_create_filter(&src_pal, buffersrc,
//     //     "srcPal",
//     //     //                                          args, NULL,
//     idx_graph)) <
//     //     0 ||
//     //     //     (ret = avfilter_graph_create_filter(&idx_sink, paletteuse,
//     //     "idxSink",
//     //     //                                          NULL, NULL,
//     idx_graph)) <
//     //     0) {
//     //     //     log_error("Idx filters creation failed: %s\n",
//     //     av_err2str(ret));
//     //     //     goto cleanup_all;
//     //     // }
//     //     // if ((ret = avfilter_link(src_img, 0, idx_sink, 0)) < 0 ||
//     //     //     (ret = avfilter_link(src_pal, 0, idx_sink, 1)) < 0 ||
//     //     //     (ret = avfilter_graph_config(idx_graph, NULL)) < 0) {
//     //     //     log_error("Idx graph config failed: %s\n",
//     av_err2str(ret));
//     //     //     goto cleanup_all;
//     //     // }
//     //     // // feed frames
//     //     // in_frame = av_frame_alloc();
//     //     // in_frame->format = AV_PIX_FMT_RGB24;
//     //     // in_frame->width  = width;
//     //     // in_frame->height = height;
//     //     // av_image_fill_arrays(in_frame->data, in_frame->linesize,
//     //     //                      buf, AV_PIX_FMT_RGB24, width, height, 1);
//     //     // ret = av_buffersrc_add_frame(src_img, in_frame);
//     //     // ret = av_buffersrc_add_frame(src_pal, pal_frame);
//     //     // ret = av_buffersrc_add_frame(src_img, NULL);
//     //     // ret = av_buffersrc_add_frame(src_pal, NULL);
//     //     // // get indexed frame
//     //     // out_frame = av_frame_alloc();
//     //     // if (!out_frame || (ret = av_buffersink_get_frame(idx_sink,
//     //     out_frame)) < 0) {
//     //     //     log_error("Paletteuse failed: %s\n", av_err2str(ret));
//     //     //     goto cleanup_all;
//     //     // }
//     //     // int np = width * height;
//     //     // res.indices = malloc(np);
//     //     // if (res.indices)
//     //     //     memcpy(res.indices, out_frame->data[0], np);
//     //
//     // // cleanup_all:
//     //     // avfilter_graph_free(&idx_graph);
//     //     av_frame_free(&pal_frame);
//     //     av_frame_free(&in_frame);
//     //     // av_frame_free(&out_frame);
//     //     return res;
// }
