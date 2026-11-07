#include "audio_scope.h"

#include <string.h>

#define MASK (AUDIO_SCOPE_SIZE - 1)

void audio_scope_reset(audio_scope *s)
{
    memset(s->buf, 0, sizeof(s->buf));
    s->head = 0;
    s->sample_rate = 0;
}

void audio_scope_push(audio_scope *s, const float *frames, int nb_frames,
                      int nb_channels, int sample_rate)
{
    if (frames == NULL || nb_frames <= 0 || nb_channels <= 0)
        return;

    /* only the end of an oversized block fits */
    int skip = nb_frames > AUDIO_SCOPE_SIZE ? nb_frames - AUDIO_SCOPE_SIZE : 0;
    uint64_t pos = s->head + (uint64_t)skip;
    float inv = 1.0f / (float)nb_channels;

    for (int i = skip; i < nb_frames; i++, pos++)
    {
        const float *f = frames + (size_t)i * (size_t)nb_channels;
        float m = 0.0f;
        for (int ch = 0; ch < nb_channels; ch++)
            m += f[ch];
        s->buf[pos & MASK] = m * inv;
    }

    s->sample_rate = sample_rate;
    s->head = pos; /* published last: the samples are in place */
}

/* is sample number `i` still in the buffer, given `head` */
static int available(uint64_t head, uint64_t i)
{
    return i < head && head - i <= AUDIO_SCOPE_SIZE;
}

void audio_scope_read(const audio_scope *s, uint64_t start, float *out, int n)
{
    uint64_t head = s->head;
    for (int i = 0; i < n; i++)
    {
        uint64_t k = start + (uint64_t)i;
        out[i] = available(head, k) ? s->buf[k & MASK] : 0.0f;
    }
}

void audio_scope_minmax(const audio_scope *s, uint64_t start, int n, float *lo,
                        float *hi)
{
    uint64_t head = s->head;
    if (n < 1)
        n = 1;
    float mn = 0.0f, mx = 0.0f;
    for (int i = 0; i < n; i++)
    {
        uint64_t k = start + (uint64_t)i;
        float v = available(head, k) ? s->buf[k & MASK] : 0.0f;
        if (i == 0)
            mn = mx = v;
        else if (v < mn)
            mn = v;
        else if (v > mx)
            mx = v;
    }
    *lo = mn;
    *hi = mx;
}
