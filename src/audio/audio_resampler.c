#include "audio_resampler.h"
#include "logger.h"

#include <libavutil/channel_layout.h>
#include <stdbool.h>
#include <string.h>

int resample_ctx_init(resample_ctx_t *rs)
{
    memset(rs, 0, sizeof(*rs));

    rs->scratch_frame = av_frame_alloc();
    if (rs->scratch_frame == NULL)
    {
        log_error("Failed to allocate resampler scratch AVFrame\n");
        return -1;
    }

    return 0;
}

void resample_ctx_free(resample_ctx_t *rs)
{
    if (rs == NULL)
        return;

    swr_free(&rs->swr);
    av_frame_free(&rs->scratch_frame);
}

int resample_convert(resample_ctx_t *rs, const uint8_t **src_data,
                     int src_nb_samples, enum AVSampleFormat src_fmt,
                     int src_ch, int src_sample_rate,
                     enum AVSampleFormat tgt_fmt, int tgt_ch,
                     int tgt_sample_rate, resample_sink_fn sink, void *sink_ctx)
{
    AVChannelLayout tgt_layout;
    av_channel_layout_default(&tgt_layout, tgt_ch);

    if (rs->swr == NULL || tgt_ch != rs->tgt_nb_channels ||
        tgt_sample_rate != rs->tgt_sample_rate ||
        tgt_fmt != rs->tgt_sample_fmt || src_ch != rs->src_nb_channels ||
        src_sample_rate != rs->src_sample_rate || src_fmt != rs->src_sample_fmt)
    {
        log_debug("Reinitialization of swr context:\n    from: ch=%d sr=%d "
                  "fmt=%s\n    to: ch=%d sr=%d fmt=%s\n",
                  src_ch, src_sample_rate, av_get_sample_fmt_name(src_fmt),
                  tgt_ch, tgt_sample_rate, av_get_sample_fmt_name(tgt_fmt));

        AVChannelLayout src_layout;
        av_channel_layout_default(&src_layout, src_ch);

        swr_free(&rs->swr);
        int ret = swr_alloc_set_opts2(&rs->swr, &tgt_layout, tgt_fmt,
                                      tgt_sample_rate, &src_layout, src_fmt,
                                      src_sample_rate, AV_LOG_DEBUG, NULL);

        if (ret < 0 || rs->swr == NULL || swr_init(rs->swr) < 0)
        {
            log_error("Failed to initialize SwrContext: %s\n", av_err2str(ret));
            return -1;
        }

        rs->src_nb_channels = src_ch;
        rs->src_sample_rate = src_sample_rate;
        rs->src_sample_fmt = src_fmt;
        rs->tgt_nb_channels = tgt_ch;
        rs->tgt_sample_rate = tgt_sample_rate;
        rs->tgt_sample_fmt = tgt_fmt;
    }

    int nb_samples, max_nb_samples;
    nb_samples = max_nb_samples =
        av_rescale_rnd(swr_get_delay(rs->swr, src_sample_rate) + src_nb_samples,
                       tgt_sample_rate, src_sample_rate, AV_ROUND_UP);
    if (nb_samples <= 0)
    {
        log_error("av_rescale_rnd() error\n");
        return -1;
    }

    int n = max_nb_samples;
    bool first_iter = true;
    int total_written = 0;
    do
    {
        rs->scratch_frame->ch_layout = tgt_layout;
        rs->scratch_frame->sample_rate = tgt_sample_rate;
        rs->scratch_frame->format = tgt_fmt;
        rs->scratch_frame->nb_samples = n;

        int ret = av_frame_get_buffer(rs->scratch_frame, 0);
        if (ret < 0)
        {
            log_error("Failed to allocate sample buffer: %s\n",
                      av_err2str(ret));
            goto fail_inner;
        }

        nb_samples = ret = swr_convert(rs->swr, rs->scratch_frame->data,
                                       nb_samples, first_iter ? src_data : NULL,
                                       first_iter ? src_nb_samples : 0);
        if (ret < 0)
        {
            log_error("Failed to resample buffer: %s\n", av_err2str(ret));
            goto fail_inner;
        }
        if (nb_samples == 0)
            goto fail_inner;

        rs->scratch_frame->nb_samples = nb_samples;

        if (sink(sink_ctx, rs->scratch_frame) < 0)
        {
            av_frame_unref(rs->scratch_frame);
            return -1;
        }

        total_written += nb_samples;

        av_frame_unref(rs->scratch_frame);

        n -= nb_samples;
        first_iter = false;

        continue;
    fail_inner:
        av_frame_unref(rs->scratch_frame);
        break;
    } while (n && nb_samples);

    return total_written;
}
