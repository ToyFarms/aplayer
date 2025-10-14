#include "app.h"
#include "audio_analyzer.h"
#include "audio_effect.h"
#include "exception.h"
#include "libavutil/log.h"
#include "session.h"
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

static void log_to_widget(const char *log);

static app_instance *g_app = NULL;
static void rms_callback(void *actx, void *userdata);

int app_init()
{
    if (g_app != NULL)
    {
        log_warning("App already initialized\n");
        return 0;
    }

    setlocale(LC_ALL, "");
    logger_set_level(LOG_DEBUG);
    logger_add_output(-1, fopen("out.log", "a"), LOG_DEFER_CLOSE);
    logger_add_callback(-1, log_to_widget, LOG_USE_COLOR);

    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(av_log_callback);

    app_instance *app = calloc(1, sizeof(*app));
    if (app == NULL)
        return -ENOMEM;

    app->term.buf = str_alloc(1024);
    ui_init(&app->ui, &app->term, app);

    log_debug("################################ NEW RUN "
              "################################\n");

    app->term.capability = term_query_capability();
    log_debug("Term capability: is_tmux=%d, supports_sixel=%d, "
              "supports_tgp=%d, cell_width=%d, cell_height=%d, color_mode=%s",
              app->term.capability.is_tmux, app->term.capability.supports_sixel,
              app->term.capability.supports_tgp,
              app->term.capability.cell_width, app->term.capability.cell_height,
              term_color_mode_name(app->term.capability.color));

    log_debug("Switching to alt buffer\n");
    term_altbuf();

    log_debug("Initializing exception context\n");
    exception_init();
    exception_panic(term_mainbuf);
    atexit(term_mainbuf);

    log_debug("Initializing playlist\n");
    playlist_init(&app->playlist);

    log_debug("Initializing audio\n");
    app->audio = audio_create(audio_callback, -1, 2, 48000, AUDIO_FLT);
    // FIXME: errno is not 0 (even though its fine), Socket operation on
    // non-socket
    if (errno != 0)
    {
        log_error("Failed to initialize audio: %s\n", strerror(errno));
        // return errnb;
    }

    audio_analyzer rms = audio_analyzer_rms(rms_callback, NULL);
    array_append(&app->audio->mixer.analyzer, &rms, 1);

    audio_effect autogain = audio_eff_autogain();
    array_append(&app->audio->mixer.effects, &autogain, 1);

    g_app = app;
    return 0;
}

static void rms_callback(void *actx, void *userdata)
{
    app_instance *app = app_get();
    if (app == NULL)
        return;

    analyzer_rms_ctx *ctx = actx;

    if (app->ui.vu_meter_st.bars.capacity < ctx->nb_channels)
        array_resize(&app->ui.vu_meter_st.bars, ctx->nb_channels);

    app->ui.vu_meter_st.bars.length = ctx->nb_channels;

    for (int i = 0; i < ctx->nb_channels; i++)
        ARR_AS(app->ui.vu_meter_st.bars, float)[i] = ctx->rms[i];
}

app_instance *app_get()
{
    if (g_app == NULL)
        return NULL;

    return g_app;
}

void app_cleanup()
{
    logger_remove_callback(log_to_widget);

    term_mainbuf();
    if (g_app == NULL)
        return;

    char *s = session_serialize(g_app);
    FILE *f = fopen(".session.json", "w");
    fwrite(s, 1, strlen(s), f);
    if (errno != 0)
        log_error("Failed to write session: %s\n", strerror(errno));
    fclose(f);
    free(s);

    audio_free(g_app->audio);
    g_app->audio = NULL;

    str_free(&g_app->term.buf);
    ui_free(&g_app->ui);

    playlist_free(&g_app->playlist);

    free(g_app);
    g_app = NULL;
}

static int audio_callback(const void *input, void *output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData)
{
    audio_mixer *mixer = userData;

    int nb_samples = frameCount * mixer->nb_channels;
    float buffer[nb_samples];
    memset(buffer, 0, nb_samples * sizeof(float));

    int ret = mixer_get_frame(mixer, nb_samples, buffer);
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

    memcpy(output, buffer, nb_samples * sizeof(float));

    return paContinue;
}

static void av_log_callback(void *avcl, int level, const char *fmt,
                            va_list args)
{
    if (level > av_log_get_level())
        return;
    logger_logv(level, "", "ffmpeg", 0, fmt, args);
}

static void log_to_widget(const char *log)
{
    if (g_app == NULL)
        return;

    // TODO: widget is broken
    queue_push_copy(&g_app->ui.debug_st.logs, log, strlen(log) + 1);
}
