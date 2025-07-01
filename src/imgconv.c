#include "imgconv.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "logger.h"

imgconv_frame imgconv_decode(const uint8_t *data, int size,
                             enum AVCodecID codec_id, enum AVPixelFormat dstfmt)
{
    imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
    AVCodecContext *avctx = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
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
        avcodec_free_context(&avctx);
        goto error;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    pkt->data = (uint8_t *)data;
    pkt->size = size;
    ret = avcodec_send_packet(avctx, pkt);
    if (ret < 0)
    {
        log_error("Failed to send packet to decoder: %s\n", av_err2str(ret));
        goto error;
    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret < 0)
    {
        log_error("Failed to receive frame: %s\n", av_err2str(ret));
        goto error;
    }

    f.width = frame->width;
    f.height = frame->height;

    sws =
        sws_getContext(frame->width, frame->height, frame->format, frame->width,
                       frame->height, dstfmt, 0, NULL, NULL, NULL);
    if (sws == NULL)
    {
        log_error("Failed to get sws context\n");
        goto error;
    }

    dst_frame = av_frame_alloc();
    dst_frame->format = dstfmt;
    dst_frame->width = frame->width;
    dst_frame->height = frame->height;
    ret = av_image_alloc(dst_frame->data, dst_frame->linesize, dst_frame->width,
                         dst_frame->height, dst_frame->format, 1);

    ret = sws_scale_frame(sws, dst_frame, frame);
    if (ret < 0)
    {
        log_error("sws_scale failed: %s\n", av_err2str(ret));
        goto error;
    }

    int buf_size =
        av_image_get_buffer_size(dstfmt, frame->width, frame->height, 1);
    uint8_t *buf = av_malloc(buf_size);
    if (buf == NULL)
    {
        log_error("Allocation failure\n");
        goto error;
    }

    ret = av_image_copy_to_buffer(
        buf, buf_size, (const uint8_t *const *)dst_frame->data,
        dst_frame->linesize, dstfmt, frame->width, frame->height, 1);
    if (ret < 0)
    {
        log_error("av_image_copy_to_buffer failed: %s\n", av_err2str(ret));
        av_free(buf);
        goto error;
    }

    f.size = buf_size;
    f.buffer = buf;

    avcodec_free_context(&avctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&dst_frame);
    sws_freeContext(sws);
    return f;

error:
    f.width = -1;
    f.height = -1;
    avcodec_free_context(&avctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&dst_frame);
    sws_freeContext(sws);

    return f;
}

imgconv_frame imgconv_resize(const uint8_t *src_buf, int src_width,
                             int src_height, enum AVPixelFormat srcfmt,
                             enum AVPixelFormat dstfmt, int dst_width,
                             int dst_height)
{
    imgconv_frame f = {.width = -1, .height = -1, .buffer = NULL};
    struct SwsContext *sws = NULL;
    AVFrame *src_frame = NULL;
    AVFrame *dst_frame = NULL;
    int ret = 0;

    sws = sws_getContext(src_width, src_height, srcfmt, dst_width, dst_height,
                         dstfmt, SWS_POINT, NULL, NULL, NULL);
    if (sws == NULL)
    {
        log_error("Failed to create SwsContext\n");
        goto cleanup;
    }

    src_frame = av_frame_alloc();
    if (src_frame == NULL)
    {
        log_error("Failed to allocate src frame\n");
        goto cleanup;
    }
    ret = av_image_fill_arrays(src_frame->data, src_frame->linesize, src_buf,
                               srcfmt, src_width, src_height, 1);
    if (ret < 0)
    {
        log_error("Failed to fill array to src_frame: %s\n", av_err2str(ret));
        goto cleanup;
    }

    dst_frame = av_frame_alloc();
    if (src_frame == NULL)
    {
        log_error("Failed to allocate dst frame\n");
        goto cleanup;
    }
    ret = av_image_alloc(dst_frame->data, dst_frame->linesize, dst_width,
                         dst_height, dstfmt, 1);
    if (ret < 0)
    {
        log_error("Failed to allocate image buffer: %s\n", av_err2str(ret));
        goto cleanup;
    }

    ret = sws_scale(sws, (const uint8_t *const *)src_frame->data,
                    src_frame->linesize, 0, src_height, dst_frame->data,
                    dst_frame->linesize);
    if (ret < 0)
    {
        log_error("sws_scale failed: %s\n", av_err2str(ret));
        goto cleanup;
    }

    int buf_size = av_image_get_buffer_size(dstfmt, dst_width, dst_height, 1);
    uint8_t *buf = av_malloc(buf_size);
    if (buf == NULL)
    {
        log_error("Allocation failure\n");
        goto cleanup;
    }

    ret = av_image_copy_to_buffer(
        buf, buf_size, (const uint8_t *const *)dst_frame->data,
        dst_frame->linesize, dstfmt, dst_width, dst_height, 1);
    if (ret < 0)
    {
        log_error("av_image_copy_to_buffer failed: %s\n", av_err2str(ret));
        av_free(buf);
        goto cleanup;
    }
    f.buffer = buf;
    f.size = buf_size;
    f.width = dst_width;
    f.height = dst_height;

cleanup:
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    sws_freeContext(sws);
    return f;
}
