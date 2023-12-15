#include <portaudio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <stdbool.h>
#include <windows.h>

#include "libaudio.h"
#include "libhelper.h"
#include "state.h"

#define ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

static PlayerState *pst;

void play_audio(char *filename)
{
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
        {
            Sleep(100);
        }

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
                    int64_t target_timestamp = pst->timestamp + pst->seek_incr;
                    av_log(NULL,
                           AV_LOG_DEBUG,
                           "Seeking from %.2fs to %.2fs.\n",
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

                audio_set_volume(frame, sst->audiodec->avctx->ch_layout.nb_channels, pst->volume);

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

#if 1
                printf("timestamp: %lldms (%.2fs) / %.2fs - %.3f %, volume=%f        \r",
                       pst->timestamp,
                       (double)pst->timestamp / 1000.0,
                       (double)us2ms(sst->ic->duration) / 1000.0,
                       ((double)pst->timestamp / (double)us2ms(sst->ic->duration)) * 100.0,
                       pst->volume);
#endif

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
    state_stream_free(sst);
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
}

DWORD WINAPI monitor_keypress(LPVOID lpParam)
{
    Sleep(500);
    while (true)
    {
        if (av_gettime() - pst->last_keypress < pst->keypress_cooldown)
        {
            Sleep(10);
            continue;
        }

        if (GetAsyncKeyState(VK_END) & 0x8001)
        {
            pst->paused = !pst->paused;
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(100);
        }
        else if (GetAsyncKeyState(VK_RIGHT) & 0x8001)
        {
            pst->req_seek = true;
            pst->seek_incr = 5000;
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(10);
        }
        else if (GetAsyncKeyState(VK_LEFT) & 0x8001)
        {
            pst->req_seek = true;
            pst->seek_incr = -5000;
            pst->keypress = true;
            pst->keypress_cooldown = ms2us(10);
        }
        else if (GetAsyncKeyState(VK_UP) & 0x8001)
        {
            pst->target_volume = FFMIN(pst->target_volume + pst->volume_incr, 1.0f);
            pst->keypress = true;
            pst->keypress_cooldown = 0;
        }
        else if (GetAsyncKeyState(VK_DOWN) & 0x8001)
        {
            pst->target_volume = FFMAX(pst->target_volume - pst->volume_incr, 0.0f);
            pst->keypress = true;
            pst->keypress_cooldown = 0;
        }

        if (pst->keypress)
        {
            pst->keypress = false;
            pst->last_keypress = av_gettime();
        }

        Sleep(100);
    }

    return 0;
}

DWORD WINAPI handle_event()
{
    Sleep(500);
    while (true)
    {
        pst->volume = lerpf(pst->volume, pst->target_volume, pst->volume_lerp);

        Sleep(pst->paused ? 100 : 10);
    }
}

#include <shellapi.h>
/* Will be leaked on exit */
static char **win32_argv_utf8 = NULL;
static int win32_argc = 0;

/**
 * Prepare command line arguments for executable.
 * For Windows - perform wide-char to UTF-8 conversion.
 * Input arguments should be main() function arguments.
 * @param argc_ptr Arguments number (including executable)
 * @param argv_ptr Arguments list.
 */
static void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    char *argstr_flat;
    wchar_t **argv_w;
    int i, buffsize = 0, offset = 0;

    if (win32_argv_utf8)
    {
        *argc_ptr = win32_argc;
        *argv_ptr = win32_argv_utf8;
        return;
    }

    win32_argc = 0;
    argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
    if (win32_argc <= 0 || !argv_w)
        return;

    /* determine the UTF-8 buffer size (including NULL-termination symbols) */
    for (i = 0; i < win32_argc; i++)
        buffsize += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                        NULL, 0, NULL, NULL);

    win32_argv_utf8 = av_mallocz(sizeof(char *) * (win32_argc + 1) + buffsize);
    argstr_flat = (char *)win32_argv_utf8 + sizeof(char *) * (win32_argc + 1);
    if (!win32_argv_utf8)
    {
        LocalFree(argv_w);
        return;
    }

    for (i = 0; i < win32_argc; i++)
    {
        win32_argv_utf8[i] = &argstr_flat[offset];
        offset += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                      &argstr_flat[offset],
                                      buffsize - offset, NULL, NULL);
    }
    win32_argv_utf8[i] = NULL;
    LocalFree(argv_w);

    *argc_ptr = win32_argc;
    *argv_ptr = win32_argv_utf8;
}

int main(int argc, char **argv)
{
    prepare_app_arguments(&argc, &argv);

    av_log_set_level(AV_LOG_DEBUG);
    pst = state_player_init();

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <audio_file>\n", argv[0]);
        return 1;
    }

    HANDLE keypress_thread = CreateThread(NULL, 0, monitor_keypress, NULL, 0, NULL);
    if (keypress_thread == NULL)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create keypress_thread: %d\n", GetLastError());
        return 1;
    }

    HANDLE event_thread = CreateThread(NULL, 0, handle_event, NULL, 0, NULL);
    if (event_thread == NULL)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not create event_thread: %d\n", GetLastError());
        return 1;
    }

    play_audio(argv[1]);

    CloseHandle(keypress_thread);
    CloseHandle(event_thread);
    state_player_free(pst);

    return 0;
}