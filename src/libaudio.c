#include "libaudio.h"

#define AUDIO_CHECK_INITIALIZED(fn_name, ret)                                                                  \
    do                                                                                                         \
    {                                                                                                          \
        if (!pst)                                                                                              \
        {                                                                                                      \
            av_log(NULL, AV_LOG_WARNING, "%s %s:%d Audio is not initialized.\n", fn_name, __FILE__, __LINE__); \
            ret;                                                                                               \
        };                                                                                                     \
    } while (0)

static PlayerState *pst;

static void _audio_set_volume(AVFrame *frame, float factor)
{
    int sample;
    __m128 scale_factor = _mm_set1_ps(factor);

    for (int ch = 0; ch < frame->ch_layout.nb_channels; ch++)
    {
        for (sample = 0; sample < frame->nb_samples - 3; sample += 4)
        {
            __m128 data = _mm_loadu_ps(&((float *)frame->data[ch])[sample]);
            data = _mm_mul_ps(data, scale_factor);
            _mm_storeu_ps(&((float *)frame->data[ch])[sample], data);
        }

        for (; sample < frame->nb_samples; sample++)
        {
            ((float *)frame->data[ch])[sample] *= factor;
        }
    }
}

static float volume_before_muted = 100.0f;

void audio_set_volume(float volume)
{
    AUDIO_CHECK_INITIALIZED("audio_set_volume", return);

    if (pst->muted)
        volume_before_muted = volume;
    else
        pst->volume = volume;
}

float audio_get_volume()
{
    AUDIO_CHECK_INITIALIZED("audio_get_volume", return 0.0f);

    return pst->volume;
}

void audio_mute()
{
    AUDIO_CHECK_INITIALIZED("audio_mute", return);

    volume_before_muted = pst->volume;
    pst->volume = 0.0f;
    pst->muted = true;
}

void audio_unmute()
{
    AUDIO_CHECK_INITIALIZED("audio_unmute", return);

    pst->volume = volume_before_muted;
    pst->muted = false;
}

bool audio_is_muted()
{
    AUDIO_CHECK_INITIALIZED("audio_is_muted", return false);

    return pst->muted;
}

void audio_seek(int64_t us)
{
    AUDIO_CHECK_INITIALIZED("audio_seek", return);

    pst->req_seek = true;
    pst->seek_absolute = false;
    pst->seek_incr = us;
}

void audio_seek_to(int64_t us)
{
    AUDIO_CHECK_INITIALIZED("audio_seek_to", return);

    pst->req_seek = true;
    pst->seek_absolute = true;
    pst->seek_incr = us;
}

int64_t audio_get_timestamp()
{
    AUDIO_CHECK_INITIALIZED("audio_get_timestamp", return 0);

    return pst->timestamp;
}

int64_t audio_get_duration()
{
    AUDIO_CHECK_INITIALIZED("audio_get_duration", return 0);

    return pst->duration;
}

void audio_play()
{
    AUDIO_CHECK_INITIALIZED("audio_play", return);

    av_log(NULL, AV_LOG_DEBUG, "Paused audio: 0.\n");
    pst->paused = false;
}

void audio_pause()
{
    AUDIO_CHECK_INITIALIZED("audio_pause", return);

    av_log(NULL, AV_LOG_DEBUG, "Paused audio: 1.\n");
    pst->paused = true;
}

void audio_toggle_play()
{
    AUDIO_CHECK_INITIALIZED("audio_toggle_play", return);

    av_log(NULL,
           AV_LOG_DEBUG,
           "Paused audio: %d -> %d.\n",
           pst->paused,
           !pst->paused);
    pst->paused = !pst->paused;
}

void audio_exit()
{
    AUDIO_CHECK_INITIALIZED("audio_exit", return);

    av_log(NULL, AV_LOG_DEBUG, "Requesting exit to audio.\n");
    pst->req_exit = true;
}

void audio_wait_until_initialized()
{
    av_log(NULL, AV_LOG_DEBUG, "Waiting until audio initialized.\n");

    while (!pst)
        av_usleep(MSTOUS(100));

    while (!pst->initialized)
        av_usleep(MSTOUS(100));

    av_log(NULL, AV_LOG_DEBUG, "Audio is initialized.\n");
}

void audio_wait_until_finished()
{
    AUDIO_CHECK_INITIALIZED("audio_wait_until_finished", return);

    av_log(NULL, AV_LOG_DEBUG, "Waiting until audio finished.\n");

    int64_t start = av_gettime();

    while (!pst->finished)
        av_usleep(MSTOUS(100));

    av_log(NULL,
           AV_LOG_DEBUG,
           "Audio finished in %lldms.\n",
           USTOMS(av_gettime() - start));
}

bool audio_is_finished()
{
    AUDIO_CHECK_INITIALIZED("audio_is_finished", return true);

    return pst->finished;
}

bool audio_is_initialized()
{
    AUDIO_CHECK_INITIALIZED("audio_is_initialized", return true);

    return pst->initialized;
}

bool audio_is_paused()
{
    AUDIO_CHECK_INITIALIZED("audio_is_paused", return true);

    return pst->paused;
}

static inline float dB_to_factor(float dB)
{
    return powf(10.0f, (dB / 20.0f));
}

static inline float factor_to_dB(float factor)
{
    return 20.0f * log10f(factor);
}

// TODO: Use some SIMD library, apparently SIMD is not portable (this one is for x86 cpu arch)
static float calculate_lufs(AVFrame *frame)
{
    float power_sum = 0.0f;
    int sample;

    for (int ch = 0; ch < frame->ch_layout.nb_channels; ch++)
    {
        __m128 sum_squared = _mm_setzero_ps();
        float *channel = (float *)frame->data[ch];

        for (sample = 0; sample < frame->nb_samples - 3; sample += 4)
        {
            __m128 sample_value = _mm_loadu_ps(&channel[sample]);
            sample_value = _mm_mul_ps(sample_value, sample_value);
            sum_squared = _mm_add_ps(sum_squared, sample_value);
        }

        float results[4];
        _mm_storeu_ps(results, sum_squared);
        float result = results[0] + results[1] + results[2] + results[3];

        for (; sample < frame->nb_samples; sample++)
            result += channel[sample] * channel[sample];

        power_sum += result / (float)frame->nb_samples;
    }

    if (power_sum == 0.0f)
        return -70.0f; // silence

    return -0.691f + 10.0f * log10f(power_sum);
}

static float calculate_lufs_ch(AVFrame *frame, int ch)
{
    float power_sum = 0.0f;
    int sample;

    __m128 sum_squared = _mm_setzero_ps();
    float *channel = (float *)frame->data[ch];

    for (sample = 0; sample < frame->nb_samples - 3; sample += 4)
    {
        __m128 sample_value = _mm_loadu_ps(&channel[sample]);
        sample_value = _mm_mul_ps(sample_value, sample_value);
        sum_squared = _mm_add_ps(sum_squared, sample_value);
    }

    float results[4];
    _mm_storeu_ps(results, sum_squared);
    float result = results[0] + results[1] + results[2] + results[3];

    for (; sample < frame->nb_samples; sample++)
        result += channel[sample] * channel[sample];

    power_sum += result / (float)frame->nb_samples;

    if (power_sum == 0.0f)
        return -70.0f; // silence

    return -0.691f + 10.0f * log10f(power_sum);
}

static void audio_apply_gain(AVFrame *frame, float target_dB, float dB)
{
    float gain = dB_to_factor(target_dB - dB);

    _audio_set_volume(frame, gain);
}

static double audio_get_lufs(char *filename, int sampling_cap)
{
    av_log(NULL, AV_LOG_DEBUG, "Getting LUFS from %s, cap=%d.\n", filename, sampling_cap);

    StreamState *sst = stream_state_init(filename);
    if (!sst)
        return 0.0f;

    double lufs_sum = 0.0;
    int lufs_sampled = 0;

    int err;
    while (true)
    {
    read_frame:
        err = av_read_frame(sst->ic, sst->audiodec->pkt);
        if (err == AVERROR_EOF)
            goto cleanup;
        else if (err < 0)
        {
            av_log(NULL, AV_LOG_FATAL, "Error while reading frame. %s.\n", av_err2str(err));
            goto cleanup;
        }

        while (pst->paused)
            av_usleep(MSTOUS(100));

        if (pst->req_exit)
        {
            pst->req_exit = false;
            goto cleanup;
        }

        if (sst->audiodec->pkt->stream_index == sst->audio_stream_index)
        {
            err = avcodec_send_packet(sst->audiodec->avctx, sst->audiodec->pkt);

            if (err < 0)
            {
                av_log(NULL,
                       AV_LOG_WARNING,
                       "Error sending a packet for decoding. %s.\n",
                       av_err2str(err));
                goto cleanup;
            }

            while (true)
            {
                err = avcodec_receive_frame(sst->audiodec->avctx, sst->frame);

                if (err == AVERROR(EAGAIN))
                    goto read_frame;
                else if (err == AVERROR_EOF)
                    goto cleanup;
                else if (err < 0)
                {
                    av_log(NULL, AV_LOG_FATAL, "Error during decoding. %s.\n", av_err2str(err));
                    goto cleanup;
                }

                int dst_nb_samples = av_rescale_rnd(swr_get_delay(sst->swr_ctx,
                                                                  sst->audiodec->avctx->sample_rate) +
                                                        sst->frame->nb_samples,
                                                    sst->audiodec->avctx->sample_rate,
                                                    sst->audiodec->avctx->sample_rate, AV_ROUND_UP);

                sst->swr_frame->nb_samples = dst_nb_samples;
                sst->swr_frame->format = AV_SAMPLE_FMT_FLT;
                sst->swr_frame->ch_layout = sst->audiodec->avctx->ch_layout;

                err = av_frame_get_buffer(sst->swr_frame, 0);

                if (err < 0)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not allocate new buffer for AVFrame swr_frame. %s.\n",
                           av_err2str(err));
                    goto cleanup;
                }

                err = swr_convert(sst->swr_ctx,
                                  sst->swr_frame->data,
                                  dst_nb_samples,
                                  (const uint8_t **)sst->frame->data,
                                  sst->frame->nb_samples);

                if (err < 0)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not convert audio data. %s.\n",
                           av_err2str(err));
                    goto cleanup;
                }

                lufs_sum += (double)calculate_lufs(sst->frame);
                lufs_sampled++;

                if (lufs_sampled > sampling_cap)
                    goto cleanup;

                av_frame_unref(sst->swr_frame);
                av_frame_unref(sst->frame);
                av_packet_unref(sst->audiodec->pkt);
            }
        }
    }

cleanup:
    stream_state_free(&sst);
    return lufs_sum / (double)lufs_sampled;
}

static void _audio_seek(StreamState *sst, PlayerState *pst)
{
    int64_t target_timestamp =
        pst->seek_absolute
            ? pst->seek_incr
            : pst->timestamp + pst->seek_incr;

    target_timestamp =
        FFMAX(FFMIN(target_timestamp, sst->ic->duration), 0);

    av_log(NULL,
           AV_LOG_DEBUG,
           "Seeking from %.2fs to %.2fs.\n",
           (double)pst->timestamp / (double)AV_TIME_BASE,
           (double)target_timestamp / (double)AV_TIME_BASE);

    int err = avformat_seek_file(sst->ic,
                                 -1,
                                 INT64_MIN,
                                 target_timestamp,
                                 INT64_MAX,
                                 AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);

    if (err < 0)
    {
        av_log(NULL,
               AV_LOG_DEBUG,
               "Could not seek to %.2fs. %s.\n",
               (double)target_timestamp / (double)AV_TIME_BASE,
               av_err2str(err));
    }

    pst->req_seek = false;
    pst->seek_absolute = false;
    pst->seek_incr = 0;
}

static StreamState *_sst = NULL;

void audio_start(char *filename, void (*finished_callback)(void))
{
    AUDIO_CHECK_INITIALIZED("audio_start", return);

    av_log(NULL,
           AV_LOG_INFO,
           "Starting audio stream from file %s.\n",
           filename);

    pst->finished = false;
    pst->req_exit = false;

    StreamState *sst = stream_state_init(filename);
    if (!sst)
        goto cleanup;

    _sst = sst;

    float lufs = (float)audio_get_lufs(filename, pst->LUFS_sampling_cap);
    pst->LUFS_avg = lufs;
    float gain = pst->LUFS_target - lufs;
    float gain_factor = dB_to_factor(gain);
    av_log(NULL,
           AV_LOG_INFO,
           "\"%s\":\n    loudness: %f LUFS\n    gain: %f LUFS\n    factor: %f\n",
           filename,
           lufs,
           gain,
           gain_factor);

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio.\n");
    PaError pa_err;
    pa_err = Pa_Initialize();
    if (pa_err != paNoError)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not initialize PortAudio. %s\n",
               Pa_GetErrorText(pa_err));
        goto cleanup;
    }

    av_log(NULL, AV_LOG_DEBUG, "Opening PortAudio default stream.\n");
    PaStreamParameters param;
    param.device = Pa_GetDefaultOutputDevice();
    param.channelCount = sst->audiodec->avctx->ch_layout.nb_channels;
    param.sampleFormat = paFloat32;
    param.suggestedLatency = Pa_GetDeviceInfo(param.device)->defaultLowOutputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    PaStream *stream;
    pa_err = Pa_OpenStream(&stream,
                           NULL,
                           &param,
                           sst->audiodec->avctx->sample_rate,
                           paFramesPerBufferUnspecified,
                           paNoFlag,
                           NULL,
                           NULL);

    if (pa_err != paNoError)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not open PortAudio stream. %s.\n",
               Pa_GetErrorText(pa_err));
        goto cleanup;
    }

    if (!stream)
    {
        av_log(NULL, AV_LOG_FATAL, "Null pointer: PaStream.\n");
        goto cleanup;
    }

    av_log(NULL, AV_LOG_DEBUG, "Starting PortAudio stream.\n");
    pa_err = Pa_StartStream(stream);
    if (pa_err != paNoError)
    {
        av_log(NULL,
               AV_LOG_FATAL,
               "Could not start PortAudio stream. %s.\n",
               Pa_GetErrorText(pa_err));
        goto cleanup;
    }

    pst->initialized = true;
    pst->duration = sst->ic->duration;

    int err;
    while (true)
    {
    read_frame:
        err = av_read_frame(sst->ic, sst->audiodec->pkt);
        if (err == AVERROR_EOF)
            goto cleanup;
        else if (err < 0)
        {
            av_log(NULL, AV_LOG_FATAL, "Error while reading frame. %s.\n", av_err2str(err));
            goto cleanup;
        }

        while (pst->paused)
            av_usleep(MSTOUS(100));

        if (pst->req_exit)
            goto cleanup;

        if (sst->audiodec->pkt->stream_index == sst->audio_stream_index)
        {
            err = avcodec_send_packet(sst->audiodec->avctx, sst->audiodec->pkt);

            if (err < 0)
            {
                av_log(NULL,
                       AV_LOG_WARNING,
                       "Error sending a packet for decoding. %s.\n",
                       av_err2str(err));
                goto cleanup;
            }

            while (true)
            {
                if (pst->req_seek && pst->timestamp > 0)
                    _audio_seek(sst, pst);

                err = avcodec_receive_frame(sst->audiodec->avctx, sst->frame);

                if (err == AVERROR(EAGAIN))
                    goto read_frame;
                else if (err == AVERROR_EOF)
                    goto cleanup;
                else if (err < 0)
                {
                    av_log(NULL, AV_LOG_FATAL, "Error during decoding. %s.\n", av_err2str(err));
                    goto cleanup;
                }

                pst->timestamp = (sst->audiodec->pkt->pts * sst->audio_stream->time_base.num * AV_TIME_BASE) / sst->audio_stream->time_base.den;

                _audio_set_volume(sst->frame, gain_factor * pst->volume);

                if (sst->frame->ch_layout.nb_channels >= 2)
                {
                    pst->LUFS_current_l = calculate_lufs_ch(sst->frame, 0);
                    pst->LUFS_current_r = calculate_lufs_ch(sst->frame, 1);
                }
                else
                {
                    pst->LUFS_current_l = calculate_lufs(sst->frame);
                    pst->LUFS_current_r = pst->LUFS_current_l;
                }

                pst->frame = sst->frame;

                int dst_nb_samples = av_rescale_rnd(swr_get_delay(sst->swr_ctx,
                                                                  sst->audiodec->avctx->sample_rate) +
                                                        sst->frame->nb_samples,
                                                    sst->audiodec->avctx->sample_rate,
                                                    sst->audiodec->avctx->sample_rate, AV_ROUND_UP);

                sst->swr_frame->nb_samples = dst_nb_samples;
                sst->swr_frame->format = AV_SAMPLE_FMT_FLT;
                sst->swr_frame->ch_layout = sst->audiodec->avctx->ch_layout;

                err = av_frame_get_buffer(sst->swr_frame, 0);

                if (err < 0)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not allocate new buffer for AVFrame swr_frame. %s.\n",
                           av_err2str(err));
                    goto cleanup;
                }

                err = swr_convert(sst->swr_ctx,
                                  sst->swr_frame->data,
                                  dst_nb_samples,
                                  (const uint8_t **)sst->frame->data,
                                  sst->frame->nb_samples);

                if (err < 0)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not convert audio data. %s.\n",
                           av_err2str(err));
                    goto cleanup;
                }

                pa_err = Pa_WriteStream(stream,
                                        sst->swr_frame->data[0],
                                        sst->swr_frame->nb_samples);

                if (pa_err == paOutputUnderflowed)
                    continue;
                else if (pa_err != paNoError)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not write to stream. %s.\n",
                           Pa_GetErrorText(pa_err));
                    goto cleanup;
                }

                av_frame_unref(sst->swr_frame);
                av_frame_unref(sst->frame);
                av_packet_unref(sst->audiodec->pkt);
            }
        }
    }

cleanup:
    stream_state_free(&sst);
    _sst = NULL;

    if (stream)
    {
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Stop PaStream.\n");
        Pa_StopStream(stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close PaStream.\n");
        Pa_CloseStream(stream);
        av_log(NULL, AV_LOG_DEBUG, "Cleanup: Terminate PortAudio.\n");
        Pa_Terminate();
    }

    pst->finished = true;
    pst->initialized = false;

    if (finished_callback && !pst->req_exit)
        finished_callback();

    pst->req_exit = false;

    pthread_exit(NULL);
}

void (*_finished_callback)(void);

static void *_audio_start_bridge(void *arg)
{
    audio_start((char *)arg, _finished_callback);

    return NULL;
}

/* Run audio_start in another thread, returns -1 on fail, otherwise the thread id */
pthread_t audio_start_async(char *filename, void (*finished_callback)(void))
{
    av_log(NULL, AV_LOG_DEBUG, "Starting audio thread.\n");
    _finished_callback = finished_callback;

    pthread_t audio_thread_id;
    if (pthread_create(&audio_thread_id, NULL, _audio_start_bridge, (void *)filename) != 0)
        return -1;

    return audio_thread_id;
}

PlayerState *audio_init()
{
    if (!pst)
        pst = player_state_init();

    if (!pst)
        return NULL;

    return pst;
}

void audio_free()
{
    audio_play();

    if (audio_is_initialized())
    {
        audio_exit();
        audio_wait_until_finished();
    }

    player_state_free(&pst);
}

StreamState *_audio_get_stream()
{
    return _sst;
}
