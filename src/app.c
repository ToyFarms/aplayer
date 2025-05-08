#include "app.h"
#include "exception.h"
#include "libavutil/log.h"
#include "term.h"

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

static int audio_callback(const void *input, void *output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData);
static void av_log_callback(void *avcl, int level, const char *fmt,
                            va_list args);

static app_instance *g_app = NULL;

int app_init()
{
    if (g_app != NULL)
    {
        log_warning("App already initialized\n");
        return 0;
    }

    setlocale(LC_ALL, "");
    logger_set_level(LOG_DEBUG);
    logger_add_output(LOG_FATAL, stdout, LOG_USE_COLOR);
    logger_add_output(-1, fopen("out.log", "a"), LOG_DEFER_CLOSE);

    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(av_log_callback);

    log_debug("################################ NEW RUN "
              "################################\n");

    log_debug("Initializing exception context\n");
    exception_init();
    exception_panic(term_mainbuf);
    atexit(term_mainbuf);

    log_debug("Allocating app\n");
    app_instance *app = calloc(1, sizeof(*app));
    if (app == NULL)
        return -ENOMEM;

    log_debug("Initializing audio\n");
    app->audio = audio_create(audio_callback, -1, 2, 48000, AUDIO_FLT);
    // TODO: errno is not 0 (even though its fine), Socket operation on non-socket
    if (errno != 0)
    {
        log_error("Failed to initialize audio: %s\n", strerror(errno));
        // return errnb;
    }

    log_debug("Switching to alt buffer\n");
    term_altbuf();

    g_app = app;
    return 0;
}

app_instance *app_get()
{
    if (g_app == NULL)
    {
        log_error("App is not initialized\n");
        return NULL;
    }

    return g_app;
}

void app_cleanup()
{
    term_mainbuf();
    if (g_app == NULL)
        return;

    audio_free(g_app->audio);
    g_app->audio = NULL;

    free(g_app);
    g_app = NULL;
}

static int audio_callback(const void *input, void *output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData)
{
    audio_mixer *mixer = userData;

    float buffer[mixer->sample_rate];
    memset(buffer, 0, sizeof(buffer));

    int ret = mixer_get_frame(mixer, frameCount * mixer->nb_channels, buffer);
    if (ret == EOF)
    {
        log_debug("Audio finished\n");
        return paComplete;
    }
    else if (ret < 0)
    {
        log_error("Failed to get frame from mixer: code=%d\n", ret);
        return paAbort;
    }

    memcpy(output, buffer, frameCount * mixer->nb_channels * sizeof(float));

    return paContinue;
}

static void av_log_callback(void *avcl, int level, const char *fmt,
                            va_list args)
{
    if (level > av_log_get_level())
        return;
    logger_logv(level, "", "ffmpeg", 0, fmt, args);
}
