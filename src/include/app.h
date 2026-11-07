#ifndef __APP_H
#define __APP_H

#include "audio.h"
#include "audio_scope.h"
#include "audio_source.h"
#include "playlist.h"
#include "term.h"

#include <stdint.h>

/* Most channels the meters track; matches the activity analyzer's limit. */
#define APP_METERS_MAX_CHANNELS 8

typedef struct app_level_meter
{
    int channels; /* valid entries in `level` */
    float level[APP_METERS_MAX_CHANNELS];
} app_level_meter;

/*
 * Latest per-channel levels, written by the audio analyzers and read by
 * whatever wants to display them. Plain data on purpose: no allocation, no
 * ownership, safe to read while the analyzers keep writing.
 */
typedef struct app_meters
{
    app_level_meter rms;      /* mean-square power per channel */
    app_level_meter activity; /* 0..1 activity per channel */
} app_meters;

/* The spectrum of what is playing: APP_SPECTRUM_BANDS levels on a log-frequency
 * axis (10 Hz at 0, Nyquist at the end), each 0..1 on a dB
 * scale (see audio_spectrum.h). Written by the audio thread, read by the UI: a
 * torn read only shows a slightly wrong band for one frame. */
#define APP_SPECTRUM_BANDS 256

typedef struct app_spectrum
{
    float band[APP_SPECTRUM_BANDS];
} app_spectrum;

typedef struct app_instance
{
    audio_ctx *audio;
    playlist_manager playlist;
    term_state term;
    app_meters meters;
    app_spectrum spectrum;
    audio_scope scope; /* recent samples, for the oscilloscope mode */

    /* Incremented every time a new track starts playing. Observers compare it
     * with the value they last saw instead of being notified. */
    uint32_t track_serial;

    int64_t want_to_seek_ms;
} app_instance;

/* Copies up to APP_METERS_MAX_CHANNELS levels into `meter`. */
void app_meters_set(app_level_meter *meter, const float *levels, int channels);

int app_init();
app_instance *app_get();
void app_cleanup();

/* The track currently feeding the mixer, or NULL when nothing is loaded. */
audio_source *app_current_source(app_instance *app);

#endif /* __APP_H */
