#include "audio_analyzer.h"

/*
 * Hands every block that passes through the audio callback, untouched, to the
 * callback, which keeps the part it wants (see audio_scope.h). Nothing is
 * stored here, so there is nothing to free.
 */

static void scope_free(audio_analyzer *analyzer)
{
    (void)analyzer;
}

static void scope_process(audio_analyzer *analyzer, audio_callback_param p)
{
    if (p.nb_channels <= 0 || p.sample_rate <= 0 || p.size < p.nb_channels)
        return;

    analyzer->callback(&(analyzer_scope_ctx){.samples = p.out,
                                             .nb_frames = p.size / p.nb_channels,
                                             .nb_channels = p.nb_channels,
                                             .sample_rate = p.sample_rate},
                       analyzer->userdata);
}

audio_analyzer audio_analyzer_scope(analyzer_callback callback, void *userdata)
{
    audio_analyzer analyzer = {0};

    analyzer.type = AUDIO_ANALYZER_SCOPE;
    analyzer.callback = callback;
    analyzer.userdata = userdata;
    analyzer.free = scope_free;
    analyzer.process = scope_process;

    return analyzer;
}
