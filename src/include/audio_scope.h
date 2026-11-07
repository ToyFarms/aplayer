#ifndef __AUDIO_SCOPE_H
#define __AUDIO_SCOPE_H

#include <stdint.h>

/*
 * The raw waveform of what is playing, for an oscilloscope-style display.
 *
 * The audio callback pushes the mono mix of every block into a circular
 * buffer; the UI reads a window of the most recent samples whenever it draws.
 * One writer (the audio thread), one reader (the UI thread), no locks: the
 * writer stores the samples first and only then moves `head`, and a reader
 * that races it at worst shows a few slightly wrong samples for one frame
 * (the same bargain app_spectrum makes).
 *
 * Samples are numbered from the start of playback: sample `head - 1` is the
 * newest, and the buffer still holds the AUDIO_SCOPE_SIZE before it. Reading a
 * number that is gone (overwritten) or not written yet gives silence.
 *
 * Pure arithmetic, no audio library involved.
 */

#define AUDIO_SCOPE_SIZE (1 << 15) /* samples kept (~0.68 s at 48 kHz) */

typedef struct audio_scope
{
    float buf[AUDIO_SCOPE_SIZE];
    volatile uint64_t head;     /* samples written so far */
    volatile int sample_rate;   /* of the last block, 0 until there is one */
} audio_scope;

/* Forgets everything. Only while the audio thread is not pushing. */
void audio_scope_reset(audio_scope *s);

/* Appends `nb_frames` interleaved frames as their channel average. A block
 * longer than the buffer keeps only its end. */
void audio_scope_push(audio_scope *s, const float *frames, int nb_frames,
                      int nb_channels, int sample_rate);

/* Copies samples [start, start + n) to `out`; numbers that are not available
 * read as 0. */
void audio_scope_read(const audio_scope *s, uint64_t start, float *out, int n);

/* Smallest and largest sample of [start, start + n), without copying them
 * (n < 1 counts as 1). Unavailable samples count as 0. */
void audio_scope_minmax(const audio_scope *s, uint64_t start, int n,
                        float *lo, float *hi);

#endif /* __AUDIO_SCOPE_H */
