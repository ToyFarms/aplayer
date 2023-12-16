#include "libaudio.h"

#ifdef _USE_SIMD
#include <immintrin.h>
#endif // _USE_SIMD

static PlayerState *pst;

static void _audio_set_volume(AVFrame *frame, int nb_channels, float factor)
{
#ifdef _USE_SIMD
    int sample;
    __m128 scale_factor = _mm_set1_ps(factor);

    for (int ch = 0; ch < nb_channels; ch++)
    {
        for (sample = 0; sample < frame->nb_samples - 3; sample += 4)
        {
            __m128 data = _mm_load_ps(&((float *)frame->data[ch])[sample]);
            data = _mm_mul_ps(data, scale_factor);
            _mm_store_ps(&((float *)frame->data[ch])[sample], data);
        }

        for (; sample < frame->nb_samples; sample++)
        {
            ((float *)frame->data[ch])[sample] *= factor;
        }
    }
#else
    for (int ch = 0; ch < nb_channels; ch++)
    {
#pragma omp parallel for
        for (int sample = 0; sample < frame->nb_samples; sample++)
        {
            ((float *)frame->data[ch])[sample] *= factor;
        }
    }
#endif // _USE_SIMD
}

void audio_set_volume(float volume)
{
    pst->target_volume = volume;
}

void audio_seek(float ms)
{
    pst->req_seek = true;
    pst->seek_absolute = false;
    pst->seek_incr = ms;
}

void audio_seek_to(float ms)
{
    pst->req_seek = true;
    pst->seek_absolute = true;
    pst->seek_incr = ms;
}

PlayerState *audio_get_state()
{
    if (!pst)
        pst = state_player_init();
    return pst;
}

void audio_play()
{
    pst->paused = false;
}

void audio_pause()
{
    pst->paused = true;
}

void audio_toggle_play()
{
    pst->paused = !pst->paused;
}

void audio_stop()
{
    pst->req_stop = true;
}

void audio_start(char *filename)
{
    if (!pst)
        pst = state_player_init();

    StreamState *sst = state_stream_init(filename);
    if (!sst)
        return;

    av_log(NULL, AV_LOG_DEBUG, "Allocating AVFrame.\n");
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate AVFrame.\n");
        goto cleanup;
    }

    av_log(NULL, AV_LOG_DEBUG, "Initializing PortAudio.\n");
    PaError pa_err;
    pa_err = Pa_Initialize();
    if (pa_err != paNoError)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize PortAudio. %s\n", Pa_GetErrorText(pa_err));
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
        av_log(NULL, AV_LOG_FATAL, "Could not start PortAudio stream. %s.\n", Pa_GetErrorText(pa_err));
        goto cleanup;
    }

    if (pst->hide_cursor)
        printf("\x1b[?25l");

    int err;
    AVFrame *swr_frame = av_frame_alloc();
    if (!swr_frame)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate swr_frame.\n");
        goto cleanup;
    }

    while (av_read_frame(sst->ic, sst->audiodec->pkt) >= 0)
    {
        while (pst->paused)
            av_usleep(ms2us(100));

        if (pst->req_stop)
            break;

        if (sst->audiodec->pkt->stream_index == sst->audio_stream_index)
        {
            err = avcodec_send_packet(sst->audiodec->avctx, sst->audiodec->pkt);
            if (err < 0)
            {
                av_log(NULL, AV_LOG_WARNING, "Error sending a packet for decoding. %s.\n", av_err2str(err));
                break;
            }

            while (err >= 0)
            {
                if (pst->req_seek && pst->timestamp > 0)
                {
                    int64_t target_timestamp = pst->seek_absolute ? pst->seek_incr : pst->timestamp + pst->seek_incr;
                    target_timestamp = FFMAX(FFMIN(target_timestamp, sst->ic->duration), 0);

                    av_log(NULL,
                           AV_LOG_DEBUG,
                           "\nSeeking from %.2fs to %.2fs.\n",
                           (double)pst->timestamp / 1000.0,
                           (double)target_timestamp / 1000.0);

                    int ret = av_seek_frame(sst->ic,
                                            sst->audio_stream_index,
                                            target_timestamp,
                                            AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
                    if (ret < 0)
                    {
                        av_log(NULL,
                               AV_LOG_DEBUG,
                               "Could not seek to %.2fms. %s.\n",
                               (double)target_timestamp / 1000.0,
                               av_err2str(ret));
                    }
                    pst->req_seek = false;
                    pst->seek_absolute = false;
                    pst->seek_incr = 0;
                }

                err = avcodec_receive_frame(sst->audiodec->avctx, frame);

                if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
                    break;
                else if (err < 0)
                {
                    av_log(NULL, AV_LOG_FATAL, "Error during decoding. %s.\n", av_err2str(err));
                    goto cleanup;
                }
                pst->timestamp = frame->best_effort_timestamp;

                _audio_set_volume(frame, sst->audiodec->avctx->ch_layout.nb_channels, pst->volume);

                int dst_nb_samples = av_rescale_rnd(swr_get_delay(sst->swr_ctx,
                                                                  sst->audiodec->avctx->sample_rate) +
                                                        frame->nb_samples,
                                                    sst->audiodec->avctx->sample_rate,
                                                    sst->audiodec->avctx->sample_rate, AV_ROUND_UP);

                swr_frame->nb_samples = dst_nb_samples;
                swr_frame->format = AV_SAMPLE_FMT_FLT;
                swr_frame->ch_layout = sst->audiodec->avctx->ch_layout;

                err = av_frame_get_buffer(swr_frame, 0);
                err = swr_convert(sst->swr_ctx, swr_frame->data, dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);

                pa_err = Pa_WriteStream(stream, swr_frame->data[0], swr_frame->nb_samples);
                if (pa_err != paNoError)
                {
                    av_log(NULL, AV_LOG_FATAL, "Could not write to stream. %s.\n", Pa_GetErrorText(pa_err));
                    goto cleanup;
                }

                if (av_gettime() - pst->last_print_info > pst->print_cooldown)
                {
                    printf("timestamp: %lldms (%.2fs) / %.2fs - %.3f %, volume=%f        \r",
                           pst->timestamp,
                           (double)pst->timestamp / 1000.0,
                           (double)us2ms(sst->ic->duration) / 1000.0,
                           ((double)pst->timestamp / (double)us2ms(sst->ic->duration)) * 100.0,
                           pst->volume);

                    pst->last_print_info = av_gettime();
                }

                av_frame_unref(swr_frame);
                av_frame_unref(frame);
            }
            av_packet_unref(sst->audiodec->pkt);
        }
    }
    printf("\n");

cleanup:
    if (pst->hide_cursor)
        printf("\x1b[?25h");

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free StreamState.\n");
    state_stream_free(&sst);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free AVFrame.\n");
    av_frame_free(&frame);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free Swr AVFrame.\n");
    av_frame_free(&swr_frame);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Stop PaStream.\n");
    Pa_StopStream(stream);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close PaStream.\n");
    Pa_CloseStream(stream);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Terminate PortAudio.\n");
    Pa_Terminate();

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Free PlayerState.\n");
    state_player_free(&pst);
}

static void *_audio_start_bridge(void *arg)
{
    audio_start((char *)arg);
}

/* Run audio_start in another thread, returns -1 on fail, otherwise the thread id */
pthread_t audio_start_async(char *filename)
{
    pthread_t audio_thread_id;
    if (pthread_create(&audio_thread_id, NULL, _audio_start_bridge, filename) != 0)
        return -1;

    return audio_thread_id;
}

void audio_wait_until_initialized()
{
    while (!pst) av_usleep(ms2us(50));
}

void audio_wait_until_finished()
{
    while (pst) av_usleep(ms2us(1000));
}

bool audio_is_finished()
{
    return (bool)!pst;
}