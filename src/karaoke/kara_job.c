#ifndef _GNU_SOURCE
#  define _GNU_SOURCE /* gettid via syscall, setpriority */
#endif

#include "karaoke.h"

#include "kara_align.h"
#include "kara_feat.h"
#include "kara_text.h"
#include "logger.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
static void sleep_ms(int ms)
{
    Sleep((DWORD)ms);
}
static void lower_priority(void)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}
#else
#  include <unistd.h>
static void sleep_ms(int ms)
{
    usleep((useconds_t)ms * 1000);
}
#  if defined(__linux__)
#    include <sys/resource.h>
#    include <sys/syscall.h>
static void lower_priority(void)
{
    /* nice applies to the calling thread alone on Linux */
    setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);
}
#  else
static void lower_priority(void)
{
}
#  endif
#endif

#define START_DELAY_MS 300 /* a track skipped past in less costs nothing */
#define MAX_SECONDS    600 /* longer than this is not a song with lyrics */
#define MAX_LINE_SECONDS 30.0 /* longer is not one sung line; bounds the CPU a line can take */

enum line_state
{
    LINE_PENDING,
    LINE_BUSY,
    LINE_DONE,
};

struct karaoke
{
    char *path;
    int n;
    char **text;
    int *start_ms;
    kara_lang default_lang;

    const kara_plans *plans; /* shared, never freed */

    pthread_mutex_t mu;
    bool finished; /* the worker has left */
    bool orphaned; /* the owner has let go: the last one out frees */
    int cancel; /* set once by kara_destroy, read by the worker: atomic */
    kara_status status;
    float progress;
    int focus;
    int done;
    uint8_t *state;      /* enum line_state per line */
    kara_line **result;  /* published lines; NULL: none */
    double seconds;      /* length of the audio, once known */
};

/* ------------------------------------------------------------------------ */

#if defined(_MSC_VER) && !defined(__clang__)
#  include <intrin.h>
static int flag_load(int *f)
{
    return _InterlockedCompareExchange((volatile long *)f, 0, 0);
}
static void flag_store(int *f, int v)
{
    _InterlockedExchange((volatile long *)f, v);
}
#else
static int flag_load(int *f)
{
    return __atomic_load_n(f, __ATOMIC_ACQUIRE);
}
static void flag_store(int *f, int v)
{
    __atomic_store_n(f, v, __ATOMIC_RELEASE);
}
#endif

static bool cancelled(karaoke *k)
{
    return flag_load(&k->cancel) != 0;
}

static bool cancelled_cb(void *k)
{
    return cancelled(k);
}

static void set_status(karaoke *k, kara_status s, float progress)
{
    pthread_mutex_lock(&k->mu);
    k->status = s;
    if (progress >= 0)
        k->progress = progress;
    pthread_mutex_unlock(&k->mu);
}

/* Decodes the file on this thread and streams it through the extractor. */
static kara_evidence *listen(karaoke *k)
{
    AVFormatContext *ic = NULL;
    AVCodecContext *avctx = NULL;
    SwrContext *swr = NULL;
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    kara_feat *feat = NULL;
    kara_evidence *ev = NULL;
    float *out = NULL;
    int out_cap = 0;
    bool ok = false;

    if (avformat_open_input(&ic, k->path, NULL, NULL) < 0)
        goto done;
    if (avformat_find_stream_info(ic, NULL) < 0)
        goto done;
    const AVCodec *codec = NULL;
    int stream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream < 0 || codec == NULL)
        goto done;
    for (unsigned i = 0; i < ic->nb_streams; i++)
        if ((int)i != stream)
            ic->streams[i]->discard = AVDISCARD_ALL; /* no video demuxing */

    avctx = avcodec_alloc_context3(codec);
    if (avctx == NULL ||
        avcodec_parameters_to_context(avctx, ic->streams[stream]->codecpar) < 0 ||
        avcodec_open2(avctx, codec, NULL) < 0)
        goto done;

    double duration = ic->duration > 0 ? (double)ic->duration / AV_TIME_BASE : 0;
    if (duration > MAX_SECONDS)
    {
        log_info("karaoke: %s is %.0f s long, not analysed\n", k->path, duration);
        set_status(k, KARA_SKIPPED, 1.0f);
        goto done;
    }

    AVChannelLayout in_layout, out_layout;
    if (avctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&in_layout, avctx->ch_layout.nb_channels);
    else
        av_channel_layout_copy(&in_layout, &avctx->ch_layout);
    av_channel_layout_default(&out_layout, 2);
    if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_FLT, KARA_SR,
                            &in_layout, avctx->sample_fmt, avctx->sample_rate,
                            0, NULL) < 0 ||
        swr_init(swr) < 0)
    {
        av_channel_layout_uninit(&in_layout);
        goto done;
    }
    av_channel_layout_uninit(&in_layout);

    feat = kara_feat_new(k->plans, kara_feat_stage2());
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (feat == NULL || frame == NULL || pkt == NULL)
        goto done;

    double heard = 0;
    bool flushing = false;
    for (;;)
    {
        if (cancelled(k))
            goto done;

        int r;
        if (!flushing)
        {
            r = av_read_frame(ic, pkt);
            if (r == AVERROR(EAGAIN))
                continue;
            if (r < 0)
            {
                flushing = true;
                avcodec_send_packet(avctx, NULL);
            }
            else
            {
                if (pkt->stream_index == stream)
                    avcodec_send_packet(avctx, pkt);
                av_packet_unref(pkt);
            }
        }

        bool got = false;
        while (avcodec_receive_frame(avctx, frame) == 0)
        {
            got = true;
            int cap = swr_get_out_samples(swr, frame->nb_samples);
            if (cap > out_cap)
            {
                free(out);
                out = malloc(sizeof(float) * 2 * (size_t)cap);
                out_cap = out ? cap : 0;
                if (out == NULL)
                    goto done;
            }
            uint8_t *dst[1] = {(uint8_t *)out};
            int n = swr_convert(swr, dst, cap, (const uint8_t **)frame->extended_data,
                                frame->nb_samples);
            av_frame_unref(frame);
            if (n > 0)
            {
                if (!kara_feat_push(feat, out, n))
                    goto done;
                heard += (double)n / KARA_SR;
            }
        }
        if (got && duration > 0 && heard > 0)
        {
            float p = (float)(heard / duration);
            set_status(k, KARA_ANALYZING, 0.5f * (p > 1 ? 1 : p));
        }
        if (flushing && !got)
            break;
    }

    /* what the resampler still holds */
    {
        int cap = swr_get_out_samples(swr, 0);
        if (cap > 0)
        {
            float *tail = malloc(sizeof(float) * 2 * (size_t)cap);
            if (tail != NULL)
            {
                uint8_t *dst[1] = {(uint8_t *)tail};
                int n = swr_convert(swr, dst, cap, NULL, 0);
                if (n > 0)
                    kara_feat_push(feat, tail, n);
                free(tail);
            }
        }
    }

    ev = kara_feat_finish(feat);
    ok = ev != NULL;

done:
    free(out);
    kara_feat_free(feat);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&avctx);
    if (ic != NULL)
        avformat_close_input(&ic);
    if (!ok && !cancelled(k))
        log_error("karaoke: could not read the audio of %s\n", k->path);
    return ev;
}

/* One line's timing, or NULL when it has none. */
static kara_line *align_one(karaoke *k, int i, const kara_evidence *ev,
                            const kara_lex *lex, const kara_params *P)
{
    const char *text = k->text[i];
    double start = k->start_ms[i] / 1000.0;
    double next = i + 1 < k->n ? k->start_ms[i + 1] / 1000.0
                               : (start + 6.0 < ev->seconds ? start + 6.0
                                                            : ev->seconds);
    if (kara_is_instrumental(text) || next - start < 0.3 ||
        next - start > MAX_LINE_SECONDS)
        return NULL;

    kara_lang lang = kara_detect_lang(text, k->default_lang, true);
    kara_units *u = kara_units_build(text, lang, lex);
    if (u == NULL || u->n == 0)
    {
        kara_units_free(u);
        return NULL;
    }

    int n = u->n;
    float *w = malloc(sizeof(float) * (size_t)n);
    double *on = malloc(sizeof(double) * (size_t)n);
    float *cf = malloc(sizeof(float) * (size_t)n);
    kara_line *line = calloc(1, sizeof(*line));
    kara_syl *syl = calloc((size_t)n, sizeof(*syl));
    bool ok = w && on && cf && line && syl;
    if (ok)
    {
        for (int j = 0; j < n; j++)
            w[j] = u->u[j].weight;
        kara_align_out o = {.onset = on, .conf = cf};
        ok = kara_align_line(ev, start, next, w, n, P, &o, cancelled_cb, k);
        if (ok)
        {
            /* the end of the last unit is a guess: the decoded end, at least
             * 150 ms after its start, before the next line */
            double last_end = o.end;
            if (last_end < on[n - 1] + 0.15)
                last_end = on[n - 1] + 0.15;
            if (last_end > next)
                last_end = next;
            double csum = 0;
            int prev = -1;
            for (int j = 0; j < n; j++)
            {
                int s = (int)(on[j] * 1000.0 + 0.5);
                if (s <= prev)
                    s = prev + 1;
                prev = s;
                syl[j].start_ms = s;
                syl[j].off = u->u[j].off;
                syl[j].len = u->u[j].len;
                syl[j].sub = u->u[j].sub;
                syl[j].nsub = u->u[j].nsub;
                syl[j].word_start = u->u[j].word_start;
                syl[j].conf = cf[j];
                csum += cf[j];
            }
            for (int j = 0; j < n; j++)
            {
                int e = j + 1 < n ? syl[j + 1].start_ms
                                  : (int)(last_end * 1000.0 + 0.5);
                syl[j].end_ms = e > syl[j].start_ms ? e : syl[j].start_ms + 1;
            }
            line->n = n;
            line->syl = syl;
            line->conf = (float)(csum / n);
            line->uniform = o.uniform;
        }
    }
    free(w);
    free(on);
    free(cf);
    kara_units_free(u);
    if (!ok)
    {
        free(syl);
        free(line);
        return NULL;
    }
    return line;
}

/* The pending line nearest the playhead: forwards first, then backwards.
 * Called with the lock held. */
static int next_pending(karaoke *k)
{
    int f = k->focus < 0 ? 0 : (k->focus >= k->n ? k->n - 1 : k->focus);
    for (int i = f; i < k->n; i++)
        if (k->state[i] == LINE_PENDING)
            return i;
    for (int i = f - 1; i >= 0; i--)
        if (k->state[i] == LINE_PENDING)
            return i;
    return -1;
}

static void run(karaoke *k)
{
    lower_priority();

    for (int waited = 0; waited < START_DELAY_MS; waited += 20)
    {
        if (cancelled(k))
            return;
        sleep_ms(20);
    }

    kara_evidence *ev = listen(k);
    if (ev == NULL)
    {
        if (!cancelled(k))
        {
            pthread_mutex_lock(&k->mu);
            if (k->status != KARA_SKIPPED)
                k->status = KARA_FAILED;
            pthread_mutex_unlock(&k->mu);
        }
        return;
    }
    pthread_mutex_lock(&k->mu);
    k->seconds = ev->seconds;
    k->status = KARA_ALIGNING;
    k->progress = 0.5f;
    pthread_mutex_unlock(&k->mu);

    kara_lex *lex = kara_lex_open(NULL);
    kara_params P = kara_params_default();
    int total = 0;
    for (int i = 0; i < k->n; i++)
        total += !kara_is_instrumental(k->text[i]);

    for (;;)
    {
        pthread_mutex_lock(&k->mu);
        int i = cancelled(k) ? -1 : next_pending(k);
        if (i >= 0)
            k->state[i] = LINE_BUSY;
        pthread_mutex_unlock(&k->mu);
        if (i < 0)
            break;

        kara_line *line = align_one(k, i, ev, lex, &P);

        pthread_mutex_lock(&k->mu);
        if (cancelled(k))
        {
            pthread_mutex_unlock(&k->mu);
            if (line != NULL)
            {
                free(line->syl);
                free(line);
            }
            break;
        }
        k->result[i] = line;
        k->state[i] = LINE_DONE;
        k->done++;
        if (total > 0)
        {
            float p = 0.5f + 0.5f * (float)k->done / (float)(k->n > 0 ? k->n : 1);
            k->progress = p > 1 ? 1 : p;
        }
        pthread_mutex_unlock(&k->mu);
    }

    if (!cancelled(k))
        set_status(k, KARA_DONE, 1.0f);
    kara_lex_close(lex);
    kara_evidence_free(ev);
}

static void free_all(karaoke *k)
{
    pthread_mutex_destroy(&k->mu);
    for (int i = 0; i < k->n; i++)
    {
        free(k->text[i]);
        if (k->result[i] != NULL)
        {
            free(k->result[i]->syl);
            free(k->result[i]);
        }
    }
    free(k->path);
    free(k->text);
    free(k->start_ms);
    free(k->state);
    free(k->result);
    free(k);
}

static void *worker(void *arg)
{
    karaoke *k = arg;
    run(k);

    pthread_mutex_lock(&k->mu);
    k->finished = true;
    bool orphan = k->orphaned;
    pthread_mutex_unlock(&k->mu);
    if (orphan)
        free_all(k); /* nobody is left to ask for the results */
    return NULL;
}

/* ------------------------------------------------------------------------ */

karaoke *kara_create(const char *path, const kara_source_line *lines, int n,
                     const char *lang_tag)
{
    if (path == NULL || lines == NULL || n <= 0)
        return NULL;
    karaoke *k = calloc(1, sizeof(*k));
    if (k == NULL)
        return NULL;
    k->n = n;
    k->path = strdup(path);
    k->text = calloc((size_t)n, sizeof(char *));
    k->start_ms = calloc((size_t)n, sizeof(int));
    k->state = calloc((size_t)n, 1);
    k->result = calloc((size_t)n, sizeof(kara_line *));
    if (!k->path || !k->text || !k->start_ms || !k->state || !k->result)
        goto fail;

    size_t all_len = 1;
    for (int i = 0; i < n; i++)
    {
        k->text[i] = strdup(lines[i].text ? lines[i].text : "");
        if (k->text[i] == NULL)
            goto fail;
        k->start_ms[i] = lines[i].start_ms;
        all_len += strlen(k->text[i]) + 1;
    }
    char *all = malloc(all_len);
    if (all == NULL)
        goto fail;
    all[0] = '\0';
    for (int i = 0; i < n; i++)
    {
        strcat(all, k->text[i]);
        strcat(all, "\n");
    }
    kara_lang hint;
    if (kara_lang_from_tag(lang_tag, &hint))
        k->default_lang = hint;
    else if (kara_text_has_kana(all))
        k->default_lang = KARA_LANG_JA;
    else
        k->default_lang = kara_guess_latin_lang(all);
    free(all);

    k->plans = kara_plans_shared();
    if (k->plans == NULL || pthread_mutex_init(&k->mu, NULL) != 0)
        goto fail;
    k->status = KARA_WAITING;
    pthread_t th;
    if (pthread_create(&th, NULL, worker, k) != 0)
    {
        pthread_mutex_destroy(&k->mu);
        goto fail;
    }
    pthread_detach(th);
    return k;

fail:
    if (k->text != NULL)
        for (int i = 0; i < n; i++)
            free(k->text[i]);
    free(k->path);
    free(k->text);
    free(k->start_ms);
    free(k->state);
    free(k->result);
    free(k);
    return NULL;
}

void kara_destroy(karaoke *k)
{
    if (k == NULL)
        return;
    /* Never waits: this can be called on the audio thread. The worker sees
     * the flag within a few milliseconds and, if it is still running, frees
     * everything on its way out. */
    flag_store(&k->cancel, 1);
    pthread_mutex_lock(&k->mu);
    bool done = k->finished;
    k->orphaned = true;
    pthread_mutex_unlock(&k->mu);
    if (done)
        free_all(k);
}

void kara_focus(karaoke *k, int line)
{
    if (k == NULL)
        return;
    pthread_mutex_lock(&k->mu);
    k->focus = line;
    pthread_mutex_unlock(&k->mu);
}

const kara_line *kara_line_get(karaoke *k, int line)
{
    if (k == NULL || line < 0 || line >= k->n)
        return NULL;
    pthread_mutex_lock(&k->mu);
    const kara_line *r = k->result[line];
    pthread_mutex_unlock(&k->mu);
    return r;
}

kara_status kara_get_status(karaoke *k, float *progress)
{
    pthread_mutex_lock(&k->mu);
    kara_status s = k->status;
    if (progress != NULL)
        *progress = k->progress;
    pthread_mutex_unlock(&k->mu);
    return s;
}

int kara_lines_done(karaoke *k)
{
    pthread_mutex_lock(&k->mu);
    int d = k->done;
    pthread_mutex_unlock(&k->mu);
    return d;
}
