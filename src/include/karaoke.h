#ifndef __KARAOKE_H
#define __KARAOKE_H

/*
 * Word-by-word timing for line-synced lyrics ("karaoke").
 *
 * The player knows when each lyric line starts (from the file's tags) but not
 * when each word is sung. This module finds out, on a background thread, from
 * the audio itself: no models, no network, a few MB of optional word lists.
 * See docs/KARAOKE.md for the method and its limits.
 *
 * How it stays out of the way of playback:
 *   - it runs on its own thread at low priority, with its own decoder (the
 *     player's decoder and buffers are never touched), and starts a moment
 *     after the track does, so skipping through tracks costs nothing;
 *   - the audio is streamed through in chunks and only per-frame features
 *     (a few hundred bytes per 10 ms) are kept, never the samples;
 *   - lines are aligned one at a time, nearest the playhead first, and each is
 *     published the moment it is done, so the line being sung is usually
 *     ready long before the rest of the song is;
 *   - everything stops within milliseconds when the track changes.
 *
 * Thread model: a karaoke is created, queried and destroyed by one thread (the
 * UI thread); the worker publishes results behind a mutex. Lines handed out by
 * kara_line_get() are immutable and live until kara_destroy().
 */

#include <stdbool.h>
#include <stdint.h>

/* One sung unit (a syllable; a mora in Japanese) and the part of the lyric
 * line's text it covers. */
typedef struct kara_syl
{
    int start_ms; /* ms from the start of the track */
    int end_ms;   /* when the next unit starts; for the last one, estimated */
    int off, len; /* bytes of the line's text */
    /* A glyph sung over several units (a kanji with a two-mora reading) is
     * shared: this unit is part `sub` of `nsub` of it. nsub is 1 otherwise. */
    uint8_t sub, nsub;
    bool word_start;
    float conf; /* 0..1: how much the audio, rather than the prior, decided it */
} kara_syl;

typedef struct kara_line
{
    int n;
    kara_syl *syl; /* start_ms ascending */
    float conf;    /* mean of the units' */
    bool uniform;  /* no usable evidence: the units are spread evenly */
} kara_line;

typedef struct karaoke karaoke;

typedef enum kara_status
{
    KARA_WAITING,   /* waiting to start, or just started */
    KARA_ANALYZING, /* listening to the track */
    KARA_ALIGNING,  /* placing words; lines appear as they are done */
    KARA_DONE,
    KARA_SKIPPED, /* not attempted (too long, no lyrics) */
    KARA_FAILED,
} kara_status;

typedef struct kara_source_line
{
    const char *text;
    int start_ms;
} kara_source_line;

/* Starts the job for the file at `path`. `lines` are the synced lyrics in
 * time order (copied). `lang_tag`: the file's language tag ("id", "ja-JP"...)
 * or NULL. Call it from the thread that creates the player's other FFT plans
 * (FFTW's planner is not thread-safe). NULL on failure. */
karaoke *kara_create(const char *path, const kara_source_line *lines, int n,
                     const char *lang_tag);

/* Stops the worker (it notices within a few milliseconds) and frees all. */
void kara_destroy(karaoke *k);

/* Which line is being listened to: those are aligned first, then the ones
 * after it, then the ones before. Cheap; call it every frame if you like. */
void kara_focus(karaoke *k, int line);

/* The timing of lyric line `line`, or NULL when it has none yet (still being
 * worked on) or ever (instrumental, too short, no sung units). */
const kara_line *kara_line_get(karaoke *k, int line);

/* How far along it is, 0..1 (listening is the first half, aligning the
 * second). */
kara_status kara_get_status(karaoke *k, float *progress);

/* Lines finished so far. */
int kara_lines_done(karaoke *k);

#endif /* __KARAOKE_H */
