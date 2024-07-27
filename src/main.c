#include <stdio.h>
#include <unistd.h>

#include "audio.h"
#include "libavutil/log.h"
#include "portaudio.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file_with_audio>\n", argv[0]);
        return 1;
    }

    av_log_set_level(AV_LOG_DEBUG);
    audio_src sources[] = {
        audio_from_file(argv[1])
    };
    audio_set_metadata(&sources[0], 2, 48000, AUDIO_FLT);

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
        audio_src source = sources[0];
        int ret = source.update(&source);

        int one_frame = 500;
        ret = source.get_frame(&source, one_frame, buffer);
        if (ret == -ENODATA)
        {
            fprintf(stderr, "Audio cannot keep up!\n");
            continue;
        }
        else if (ret == EOF)
            break;
        else if (ret < 0)
        {
            fprintf(stderr, "Could not get audio frame\n");
            break;
        }

        for (int i = 0; i < one_frame; i++)
        {
            buffer[i] *= 0.1;
        }

        ret = Pa_WriteStream(stream, buffer, one_frame / 2);
        if (ret != paNoError)
        {
            fprintf(stderr, "Pa_WriteStream error: %s\n", Pa_GetErrorText(ret));
        }
    }

    sources[0].free(&sources[0]);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
