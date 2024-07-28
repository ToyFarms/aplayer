#include "audio.h"
#include "audio_effect.h"
#include "audio_mixer.h"
#include "libavutil/log.h"
#include "portaudio.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file_with_audio>\n", argv[0]);
        return 1;
    }
    av_log_set_level(AV_LOG_DEBUG);

    audio_mixer mixer = mixer_create();

    {
        audio_src src = audio_from_file(argv[1], 2, 48000, AUDIO_FLT);

        audio_effect gain = audio_eff_gain(-4);
        array_append(&src.effects, &gain, 1);

        audio_effect lowpass =
            audio_eff_filter(AUDIO_FILT_LOWPASS, 3000, 2, 48000, NULL);
        array_append(&src.effects, &lowpass, 1);

        audio_effect highpass =
            audio_eff_filter(AUDIO_FILT_HIGHPASS, 500, 2, 48000, NULL);
        array_append(&src.effects, &highpass, 1);

        audio_effect bell =
            audio_eff_filter(AUDIO_FILT_BELL, 1000, 2, 48000,
                             &(filter_param){
                                 .bell = (filter_bell){.gain = 10, .Q = 1.0}
        });
        array_append(&src.effects, &bell, 1);

        array_append(&mixer.sources, &src, 1);
    }

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

        int ret = Pa_WriteStream(stream, buffer, frame_size / 2);
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
