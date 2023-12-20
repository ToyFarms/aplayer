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
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_set_volume: Audio is not initialized.\n");
        return;
    }
    pst->volume = volume;
}

float audio_get_volume()
{
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_get_volume: Audio is not initialized.\n");
        return 0.0f;
    }
    return pst->volume;
}

void audio_seek(float ms)
{
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_seek: Audio is not initialized.\n");
        return;
    }
    pst->req_seek = true;
    pst->seek_absolute = false;
    pst->seek_incr = ms;
}

void audio_seek_to(float ms)
{
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_seek_to: Audio is not initialized.\n");
        return;
    }
    pst->req_seek = true;
    pst->seek_absolute = true;
    pst->seek_incr = ms;
}

int64_t audio_get_timestamp()
{
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_get_timestamp: Audio is not initialized.\n");
        return 0;
    }
    return pst->timestamp;
}

void audio_play()
{
    av_log(NULL, AV_LOG_DEBUG, "Paused audio: 0.\n");
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "Audio is not initialized.\n");
        return;
    }
    pst->paused = false;
}

void audio_pause()
{
    av_log(NULL, AV_LOG_DEBUG, "Paused audio: 1.\n");
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "Audio is not initialized.\n");
        return;
    }
    pst->paused = true;
}

void audio_toggle_play()
{
    av_log(NULL,
           AV_LOG_DEBUG,
           "Paused audio: %d -> %d.\n",
           pst->paused,
           !pst->paused);
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "Audio is not initialized.\n");
        return;
    }
    pst->paused = !pst->paused;
}

void audio_exit()
{
    av_log(NULL, AV_LOG_DEBUG, "Requesting exit to audio.\n");
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "Audio is not initialized.\n");
        return;
    }
    pst->req_exit = true;
}

void audio_wait_until_initialized()
{
    av_log(NULL, AV_LOG_DEBUG, "Waiting until audio initialized.\n");

    while (!pst)
        av_usleep(ms2us(100));

    while (!pst->initialized && !pst->finished)
        av_usleep(ms2us(100));

    av_log(NULL, AV_LOG_DEBUG, "Audio is initialized.\n");
}

void audio_wait_until_finished()
{
    av_log(NULL, AV_LOG_DEBUG, "Waiting until audio finished.\n");
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "Audio is not initialized.\n");
        return;
    }

    int64_t start = av_gettime();

    while (!pst->finished)
        av_usleep(ms2us(100));

    av_log(NULL,
           AV_LOG_DEBUG,
           "Audio finished in %lldms.\n",
           us2ms(av_gettime() - start));
}

bool audio_is_finished()
{
    if (!pst)
    {
        av_log(NULL, AV_LOG_WARNING, "audio_is_finished: Audio is not initialized.\n");
        return true;
    }
    return pst->finished;
}

void audio_start(char *filename)
{
    av_log(NULL,
           AV_LOG_INFO,
           "Starting audio stream from file %s.\n",
           filename);

    if (!pst)
        pst = player_state_init();
    else if (pst && !pst->finished)
    {
        av_log(NULL, AV_LOG_FATAL, "Trying to start audio stream while another stream is still active.\n");
        pthread_exit(NULL);
        return;
    }

    if (pst)
        pst->finished = false;

    StreamState *sst = stream_state_init(filename);
    if (!sst)
        goto cleanup;

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

    if (pst)
        pst->initialized = true;

    int err;
    int err_nf; // error non-fatal
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

        while (pst && pst->paused)
            av_usleep(ms2us(100));

        if (pst && pst->req_exit)
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
                if (pst && (pst->req_seek && pst->timestamp > 0))
                {
                    int64_t target_timestamp =
                        pst->seek_absolute
                            ? pst->seek_incr
                            : pst->timestamp + pst->seek_incr;
                    target_timestamp =
                        FFMAX(FFMIN(target_timestamp, sst->ic->duration), 0);

                    av_log(NULL,
                           AV_LOG_DEBUG,
                           "\nSeeking from %.2fs to %.2fs.\n",
                           (double)pst->timestamp / 1000.0,
                           (double)target_timestamp / 1000.0);

                    err_nf = av_seek_frame(sst->ic,
                                           sst->audio_stream_index,
                                           target_timestamp,
                                           AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
                    if (err_nf < 0)
                    {
                        av_log(NULL,
                               AV_LOG_DEBUG,
                               "Could not seek to %.2fms. %s.\n",
                               (double)target_timestamp / 1000.0,
                               av_err2str(err_nf));
                    }
                    pst->req_seek = false;
                    pst->seek_absolute = false;
                    pst->seek_incr = 0;
                }

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

                if (pst)
                    pst->timestamp = sst->frame->best_effort_timestamp;

                _audio_set_volume(sst->frame,
                                  sst->audiodec->avctx->ch_layout.nb_channels,
                                  pst->volume);

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

                if (pa_err != paNoError)
                {
                    av_log(NULL,
                           AV_LOG_FATAL,
                           "Could not write to stream. %s.\n",
                           Pa_GetErrorText(pa_err));
                    goto cleanup;
                }

#if 0
                if (pst && (av_gettime() - pst->last_print_info > pst->print_cooldown))
                {
                    printf("timestamp: %lldms (%.2fs) / %.2fs - %.3f %, volume=%f        \r",
                           pst->timestamp,
                           (double)pst->timestamp / 1000.0,
                           (double)us2ms(sst->ic->duration) / 1000.0,
                           ((double)pst->timestamp / (double)us2ms(sst->ic->duration)) * 100.0,
                           pst->volume);

                    pst->last_print_info = av_gettime();
                }
#endif

                av_frame_unref(sst->swr_frame);
                av_frame_unref(sst->frame);
                av_packet_unref(sst->audiodec->pkt);
            }
        }
    }

cleanup:
    stream_state_free(&sst);

    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Stop PaStream.\n");
    Pa_StopStream(stream);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Close PaStream.\n");
    Pa_CloseStream(stream);
    av_log(NULL, AV_LOG_DEBUG, "Cleanup: Terminate PortAudio.\n");
    Pa_Terminate();

    if (pst)
    {
        pst->finished = true;
        pst->initialized = false;
    }
    pthread_exit(NULL);
}

static void *_audio_start_bridge(void *arg)
{
    audio_start((char *)arg);
}

/* Run audio_start in another thread, returns -1 on fail, otherwise the thread id */
pthread_t audio_start_async(char *filename)
{
    av_log(NULL, AV_LOG_DEBUG, "Starting audio thread.\n");

    pthread_t audio_thread_id;
    if (pthread_create(&audio_thread_id, NULL, _audio_start_bridge, (void *)filename) != 0)
        return -1;

    return audio_thread_id;
}

void audio_free()
{
    if (pst)
    {
        player_state_free(&pst);
        pst = NULL;
    }
}