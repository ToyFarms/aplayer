#include "audio.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "audio_mixer.h"
#include "libavutil/log.h"
#include "portaudio.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

static float lerp(float v0, float v1, float t)
{
    return v0 + t * (v1 - v0);
}

// NOTE: Temporary visualization
static void display_rms(void *ctx, void *userdata)
{
    analyzer_rms_ctx *rms = ctx;
    char buf[4096] = {0};
    char fmtbuf[2048] = {0};
    int len;
    static float prev_scale[8] = {0};
    static const wchar_t *blocks_horizontal = L" ▎▎▍▌▋▊▉█";
    static int activity_color[8] = {0};
    static float threshold = 1.0f;
    static float prev_diff[8] = {0};
    // TODO: ?adjust threshold dynamically

    for (int ch = 0; ch < rms->nb_channels; ch++)
    {
        float dbfs =
            rms->rms[ch] == 0 ? -70.0f : -0.691f + 10.0f * log10f(rms->rms[ch]);
        float lin_scale = powf(10.0, dbfs / 20.0) * 100.0f;
        lin_scale = lerp(prev_scale[ch], lin_scale, 0.5f);

        float diff = fabs(prev_scale[ch] - lin_scale);
        bool activity = diff > threshold;

        if (activity)
            activity_color[ch] = FFMIN(activity_color[ch] + (prev_diff[ch] * 15), 255);
        else
            activity_color[ch] =
                FFMAX(activity_color[ch] - (prev_diff[ch] * 17), 0);

        int index = (lin_scale - (int)lin_scale) * (9.0f - 1.0f);
        wchar_t block = blocks_horizontal[index];

        snprintf(fmtbuf, 2048,
                 "%10.1f dBFS  \x1b[%d;2;%d;%d;%dm  \x1b[0m  "
                 "\x1b[500X\x1b[48;2;255;255;255m\x1b[%dX%lc\x1b[0m\n",
                 dbfs, activity_color[ch] <= 10 ? 38 : 48, activity_color[ch],
                 activity_color[ch], activity_color[ch], (int)lin_scale,
                 block);
        strcat(buf, fmtbuf);

        prev_scale[ch] = lin_scale;
        prev_diff[ch] = diff;
    }

    strcat(buf, "\x1b[2A");
    write(STDOUT_FILENO, buf, strlen(buf));
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file_with_audio>\n", argv[0]);
        return 1;
    }
    av_log_set_level(AV_LOG_DEBUG);

    audio_mixer mixer = mixer_create(2, 48000, AUDIO_FLT);

    {
        audio_src src = audio_from_file(argv[1], mixer.nb_channels,
                                        mixer.sample_rate, mixer.sample_fmt);

        // audio_effect lowpass =
        //     audio_eff_filter(AUDIO_FILT_LOWPASS, 1000, mixer.sample_rate,
        //     NULL);
        // array_append(&src.effects, &lowpass, 1);
        //
        // audio_effect highpass =
        //     audio_eff_filter(AUDIO_FILT_HIGHPASS, 500, mixer.sample_rate,
        //     NULL);
        // array_append(&src.effects, &highpass, 1);
        //
        // audio_effect bell =
        //     audio_eff_filter(AUDIO_FILT_BELL, 2000, mixer.sample_rate,
        //                      &(filter_param){
        //                          .bell = (filter_bell){.gain = 10, .Q = 1}
        // });
        // array_append(&src.effects, &bell, 1);

        array_append(&mixer.sources, &src, 1);
    }

    audio_effect gain = audio_eff_gain(-4);
    array_append(&mixer.master_effect, &gain, 1);

    audio_analyzer rms = audio_analyzer_rms(display_rms, NULL);
    array_append(&mixer.master_analyzer, &rms, 1);

    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        fprintf(stderr, "Failed to initialize portaudio: %s\n",
                Pa_GetErrorText(err));
        return 1;
    }

    PaStreamParameters param;
    param.device = Pa_GetDefaultOutputDevice();
    param.channelCount = 2;
    param.sampleFormat = paFloat32;
    param.suggestedLatency =
        Pa_GetDeviceInfo(param.device)->defaultLowOutputLatency;
    param.hostApiSpecificStreamInfo = NULL;

    PaStream *stream;
    err = Pa_OpenStream(&stream, NULL, &param, 48000,
                        paFramesPerBufferUnspecified, paNoFlag, NULL, NULL);
    if (err != paNoError)
    {
        fprintf(stderr, "Failed to open PortAudio stream: %s\n",
                Pa_GetErrorText(err));
        return 1;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError)
    {
        fprintf(stderr, "Failed to start PortAudio stream: %s\n",
                Pa_GetErrorText(err));
        return 1;
    }

    float buffer[48000] = {0};
    while (true)
    {
        int frame_size = mixer_get_frame(&mixer, buffer);
        if (frame_size < 0)
            break;

        int ret =
            Pa_WriteStream(stream, buffer, frame_size / mixer.nb_channels);
        if (ret != paNoError)
        {
            fprintf(stderr, "Pa_WriteStream error: %s\n", Pa_GetErrorText(ret));
        }
        memset(buffer, 0, sizeof(buffer));
    }

    mixer_free(&mixer);

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
