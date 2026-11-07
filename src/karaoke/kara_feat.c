
#include "kara_feat.h"

#include <fftw3.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define KARA_PI 3.14159265358979323846
#define N1 1024 /* onset front-end window */
#define N2 2048 /* pitch front-end window */
#define HALF 1024 /* margin kept before the oldest frame still to come */

/* mel */
#define N_MEL   48
#define MEL_LO  150.0
#define MEL_HI  7000.0
#define BAND_LO 200.0
#define BAND_HI 3500.0
#define MAX_BANDS N_MEL

/* pitch */
#define F0_LO   90.0
#define F0_HI   900.0
#define CENTS   25.0
#define N_CAND  160 /* int(log2(F0_HI / F0_LO) * 1200 / CENTS) + 1 */
#define N_HARM  10
#define HARM_DECAY 0.84
#define HARM_MAX_HZ 5000.0
#define SHS_BINS 500 /* spectrum bins the harmonics can reach, plus smoothing */
#define WHITEN   41
#define JUMP_COST 0.9
#define JUMP_CAP_SEMI 4.0

#define N_CEPS 12

struct kara_plans
{
    fftw_plan p1, p2;
    fftw_complex *in1, *out1, *in2, *out2; /* what the plans were made on */
};

kara_plans *kara_plans_new(void)
{
    kara_plans *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return NULL;
    p->in1 = fftw_malloc(sizeof(fftw_complex) * N1);
    p->out1 = fftw_malloc(sizeof(fftw_complex) * N1);
    p->in2 = fftw_malloc(sizeof(fftw_complex) * N2);
    p->out2 = fftw_malloc(sizeof(fftw_complex) * N2);
    if (!p->in1 || !p->out1 || !p->in2 || !p->out2)
    {
        kara_plans_free(p);
        return NULL;
    }
    p->p1 = fftw_plan_dft_1d(N1, p->in1, p->out1, FFTW_FORWARD, FFTW_ESTIMATE);
    p->p2 = fftw_plan_dft_1d(N2, p->in2, p->out2, FFTW_FORWARD, FFTW_ESTIMATE);
    if (!p->p1 || !p->p2)
    {
        kara_plans_free(p);
        return NULL;
    }
    return p;
}

static pthread_mutex_t shared_lock = PTHREAD_MUTEX_INITIALIZER;
static kara_plans *shared_plans;

const kara_plans *kara_plans_shared(void)
{
    pthread_mutex_lock(&shared_lock);
    if (shared_plans == NULL)
        shared_plans = kara_plans_new();
    pthread_mutex_unlock(&shared_lock);
    return shared_plans;
}

void kara_plans_free(kara_plans *p)
{
    if (p == NULL)
        return;
    if (p->p1)
        fftw_destroy_plan(p->p1);
    if (p->p2)
        fftw_destroy_plan(p->p2);
    fftw_free(p->in1);
    fftw_free(p->out1);
    fftw_free(p->in2);
    fftw_free(p->out2);
    free(p);
}

kara_feat_params kara_feat_stage2(void)
{
    return (kara_feat_params){.w_pitch = 0.5f,
                              .w_timbre = 0.25f,
                              .w_stable = 1.0f,
                              .peak_min = 0.25f};
}

void kara_evidence_free(kara_evidence *e)
{
    if (e == NULL)
        return;
    free(e->flux);
    free(e->peaks);
    free(e->off);
    free(e->flux0);
    free(e->note);
    free(e->timbre);
    free(e->stable);
    free(e);
}

/* ------------------------------------------------------------------------ */

typedef struct band
{
    int lo, n;
    float *w;
} band;

struct kara_feat
{
    const kara_plans *plans;
    kara_feat_params prm;

    /* samples not yet used up */
    float *xl, *xr;
    int64_t base, have;
    size_t cap;
    bool ended;
    int next_t;

    fftw_complex *in1, *out1, *in2, *out2;
    double w1[N1], w2[N2];

    int nb;
    band bands[MAX_BANDS];
    double *mag2; /* N1/2 + 1 */

    /* SHS tables */
    double cand_hz[N_CAND];
    int shs_i0[N_CAND][N_HARM];
    float shs_fr[N_CAND][N_HARM];
    float shs_c[N_CAND][N_HARM];
    float *spec; /* whitened pitch spectrum, SHS_BINS + 2 */
    double envmax;
    double vit[N_CAND]; /* Viterbi scores of the last frame */

    /* per frame, grown as we go */
    int cap_t;
    float *logmel; /* nb per frame */
    float *S;      /* N_CAND per frame */
    float *S_mean;
    uint8_t *bp;   /* N_CAND per frame */
};

kara_feat *kara_feat_new(const kara_plans *plans, kara_feat_params prm)
{
    kara_feat *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return NULL;
    f->plans = plans;
    f->prm = prm;

    f->in1 = fftw_malloc(sizeof(fftw_complex) * N1);
    f->out1 = fftw_malloc(sizeof(fftw_complex) * N1);
    f->in2 = fftw_malloc(sizeof(fftw_complex) * N2);
    f->out2 = fftw_malloc(sizeof(fftw_complex) * N2);
    f->mag2 = malloc(sizeof(double) * (N1 / 2 + 1));
    f->spec = malloc(sizeof(float) * (SHS_BINS + 2));
    if (!f->in1 || !f->out1 || !f->in2 || !f->out2 || !f->mag2 || !f->spec)
    {
        kara_feat_free(f);
        return NULL;
    }

    for (int i = 0; i < N1; i++) /* np.hanning: symmetric */
        f->w1[i] = 0.5 - 0.5 * cos(2.0 * KARA_PI * i / (N1 - 1));
    for (int i = 0; i < N2; i++)
        f->w2[i] = 0.5 - 0.5 * cos(2.0 * KARA_PI * i / (N2 - 1));

    /* triangular mel filters, normalised to unit sum, keeping the bands the
     * onset evidence uses */
    double pts[N_MEL + 2];
    double m0 = 2595.0 * log10(1.0 + MEL_LO / 700.0);
    double m1 = 2595.0 * log10(1.0 + MEL_HI / 700.0);
    for (int i = 0; i < N_MEL + 2; i++)
    {
        double m = m0 + (m1 - m0) * i / (N_MEL + 1);
        pts[i] = 700.0 * (pow(10.0, m / 2595.0) - 1.0);
    }
    double binhz = (double)KARA_SR / N1;
    f->nb = 0;
    for (int b = 0; b < N_MEL; b++)
    {
        double c = pts[b + 1];
        if (c < BAND_LO || c > BAND_HI)
            continue;
        double lo = pts[b], hi = pts[b + 2];
        int k0 = (int)floor(lo / binhz), k1 = (int)ceil(hi / binhz);
        if (k0 < 0)
            k0 = 0;
        if (k1 > N1 / 2)
            k1 = N1 / 2;
        band *bd = &f->bands[f->nb++];
        bd->lo = k0;
        bd->n = k1 - k0 + 1;
        bd->w = calloc((size_t)bd->n, sizeof(float));
        if (bd->w == NULL)
        {
            kara_feat_free(f);
            return NULL;
        }
        double sum = 0;
        double *tmp = calloc((size_t)bd->n, sizeof(double));
        if (tmp == NULL)
        {
            kara_feat_free(f);
            return NULL;
        }
        for (int k = k0; k <= k1; k++)
        {
            double hz = k * binhz;
            double up = (hz - lo) / (c - lo), dn = (hi - hz) / (hi - c);
            double v = up < dn ? up : dn;
            if (v < 0)
                v = 0;
            tmp[k - k0] = v;
            sum += v;
        }
        for (int k = 0; k < bd->n; k++)
            bd->w[k] = (float)(tmp[k] / (sum + 1e-9));
        free(tmp);
    }

    double df = (double)KARA_SR / N2;
    for (int c = 0; c < N_CAND; c++)
    {
        double hz = F0_LO * pow(2.0, c * CENTS / 1200.0);
        f->cand_hz[c] = hz;
        for (int h = 1; h <= N_HARM; h++)
        {
            double pos = hz * h / df;
            bool ok = hz * h < HARM_MAX_HZ;
            int i0 = (int)floor(pos);
            double fr = pos - i0;
            if (!ok || i0 > SHS_BINS)
            {
                f->shs_i0[c][h - 1] = 0;
                f->shs_fr[c][h - 1] = 0;
                f->shs_c[c][h - 1] = 0;
                continue;
            }
            f->shs_i0[c][h - 1] = i0;
            f->shs_fr[c][h - 1] = (float)fr;
            f->shs_c[c][h - 1] = (float)pow(HARM_DECAY, h - 1);
        }
    }
    return f;
}

void kara_feat_free(kara_feat *f)
{
    if (f == NULL)
        return;
    for (int b = 0; b < f->nb; b++)
        free(f->bands[b].w);
    fftw_free(f->in1);
    fftw_free(f->out1);
    fftw_free(f->in2);
    fftw_free(f->out2);
    free(f->mag2);
    free(f->spec);
    free(f->xl);
    free(f->xr);
    free(f->logmel);
    free(f->S);
    free(f->S_mean);
    free(f->bp);
    free(f);
}

int kara_feat_frames(const kara_feat *f)
{
    return f->next_t;
}

/* ------------------------------------------------------------------------ */
/* one frame                                                                */
/* ------------------------------------------------------------------------ */

static inline float sample(const kara_feat *f, const float *buf, int64_t q)
{
    if (q < 0)
        q = -q; /* numpy "reflect": the edge sample is not repeated */
    if (f->ended && q >= f->have)
        q = 2 * (f->have - 1) - q;
    return buf[q - f->base];
}

/* Windowed transform of both channels at once (L in the real part, R in the
 * imaginary), then the two spectra apart: L[k], R[k] for k = 0..n/2. */
static void stereo_fft(const kara_feat *f, fftw_plan plan, fftw_complex *in,
                       fftw_complex *out, const double *win, int n, int64_t q0)
{
    for (int i = 0; i < n; i++)
    {
        in[i][0] = (double)sample(f, f->xl, q0 + i) * win[i];
        in[i][1] = (double)sample(f, f->xr, q0 + i) * win[i];
    }
    fftw_execute_dft(plan, in, out);
}

/* mid magnitude times the centre-panning mask at bin k */
static inline double masked_mag(const fftw_complex *Z, int n, int k)
{
    int j = k == 0 ? 0 : n - k;
    double ar = Z[k][0], ai = Z[k][1];
    double br = Z[j][0], bi = -Z[j][1];
    double Lr = 0.5 * (ar + br), Li = 0.5 * (ai + bi);
    double dr = ar - br, di = ai - bi;
    double Rr = 0.5 * di, Ri = -0.5 * dr;
    double Mr = 0.5 * (Lr + Rr), Mi = 0.5 * (Li + Ri);
    double num = Lr * Rr + Li * Ri;
    double den = 0.5 * (Lr * Lr + Li * Li + Rr * Rr + Ri * Ri) + 1e-9;
    double m = num / den;
    m = m < 0 ? 0 : m > 1 ? 1 : m;
    return sqrt(Mr * Mr + Mi * Mi) * m * m;
}

static bool grow(kara_feat *f, int need)
{
    if (need <= f->cap_t)
        return true;
    int cap = f->cap_t ? f->cap_t : 4096;
    while (cap < need)
        cap *= 2;
    float *a = realloc(f->logmel, sizeof(float) * (size_t)cap * (size_t)f->nb);
    if (a)
        f->logmel = a;
    float *b = realloc(f->S, sizeof(float) * (size_t)cap * N_CAND);
    if (b)
        f->S = b;
    float *c = realloc(f->S_mean, sizeof(float) * (size_t)cap);
    if (c)
        f->S_mean = c;
    uint8_t *d = realloc(f->bp, (size_t)cap * N_CAND);
    if (d)
        f->bp = d;
    if (!a || !b || !c || !d)
        return false;
    f->cap_t = cap;
    return true;
}

static void process_frame(kara_feat *f, int t)
{
    int64_t centre = (int64_t)t * KARA_HOP;

    /* ---- onset front-end: mel of the masked mid signal ---- */
    stereo_fft(f, f->plans->p1, f->in1, f->out1, f->w1, N1, centre - N1 / 2);
    for (int k = 0; k <= N1 / 2; k++)
    {
        double m = masked_mag(f->out1, N1, k);
        f->mag2[k] = m * m;
    }
    float *lm = f->logmel + (size_t)t * (size_t)f->nb;
    for (int b = 0; b < f->nb; b++)
    {
        const band *bd = &f->bands[b];
        double e = 0;
        for (int k = 0; k < bd->n; k++)
            e += (double)bd->w[k] * f->mag2[bd->lo + k];
        lm[b] = (float)(10.0 * log10(e + 1e-7));
    }

    /* ---- pitch front-end ---- */
    stereo_fft(f, f->plans->p2, f->in2, f->out2, f->w2, N2, centre - N2 / 2);
    float mag[SHS_BINS + 2 + WHITEN];
    int nbin = SHS_BINS + 2 + WHITEN / 2;
    if (nbin > (int)(sizeof(mag) / sizeof(*mag)))
        nbin = (int)(sizeof(mag) / sizeof(*mag));
    for (int k = 0; k < nbin; k++)
        mag[k] = powf((float)masked_mag(f->out2, N2, k), 0.6f);

    /* divide by the local mean over 41 bins: formants and bass tilt go,
     * the harmonic ripple stays */
    double run = 0;
    const int hw = WHITEN / 2;
    for (int j = -hw; j <= hw; j++)
        run += mag[j < 0 ? 0 : j];
    double env_max = f->envmax;
    double env_at[SHS_BINS + 2];
    for (int k = 0; k < SHS_BINS + 2; k++)
    {
        double env = run / WHITEN;
        env_at[k] = env;
        if (env > env_max)
            env_max = env;
        run += mag[k + hw + 1 < nbin ? k + hw + 1 : nbin - 1] -
               mag[k - hw < 0 ? 0 : k - hw];
    }
    f->envmax = env_max;
    for (int k = 0; k < SHS_BINS + 2; k++)
        f->spec[k] = (float)(mag[k] / (env_at[k] + 1e-6 * env_max + 1e-9));

    float *S = f->S + (size_t)t * N_CAND;
    double sum = 0;
    float smax = 0;
    for (int c = 0; c < N_CAND; c++)
    {
        float s = 0;
        for (int h = 0; h < N_HARM; h++)
        {
            int i0 = f->shs_i0[c][h];
            float fr = f->shs_fr[c][h];
            s += f->shs_c[c][h] *
                 (f->spec[i0] * (1.0f - fr) + f->spec[i0 + 1] * fr);
        }
        S[c] = s;
        sum += s;
        if (s > smax)
            smax = s;
    }
    f->S_mean[t] = (float)(sum / N_CAND);

    /* ---- Viterbi step: score = best predecessor minus a jump penalty
     * (0.9 per semitone, capped at 4), plus log of the frame-normalised
     * salience. The penalty is a capped L1 distance, so the best predecessor
     * comes from two linear sweeps and a global maximum. ---- */
    double lp[N_CAND];
    for (int c = 0; c < N_CAND; c++)
        lp[c] = log((double)(S[c] / (smax + 1e-9)) + 1e-3);
    uint8_t *bp = f->bp + (size_t)t * N_CAND;
    if (t == 0)
    {
        for (int c = 0; c < N_CAND; c++)
        {
            f->vit[c] = lp[c];
            bp[c] = 0;
        }
        return;
    }
    const double step = JUMP_COST * (CENTS / 100.0);
    const double cap = JUMP_COST * JUMP_CAP_SEMI;
    const double *sc = f->vit;
    double Lv[N_CAND], Rv[N_CAND];
    int La[N_CAND], Ra[N_CAND];
    for (int i = 0; i < N_CAND; i++)
    {
        if (i > 0 && Lv[i - 1] - step >= sc[i])
        {
            Lv[i] = Lv[i - 1] - step;
            La[i] = La[i - 1];
        }
        else
        {
            Lv[i] = sc[i];
            La[i] = i;
        }
    }
    for (int i = N_CAND - 1; i >= 0; i--)
    {
        if (i < N_CAND - 1 && Rv[i + 1] - step > sc[i])
        {
            Rv[i] = Rv[i + 1] - step;
            Ra[i] = Ra[i + 1];
        }
        else
        {
            Rv[i] = sc[i];
            Ra[i] = i;
        }
    }
    double gmax = sc[0];
    int garg = 0;
    for (int i = 1; i < N_CAND; i++)
        if (sc[i] > gmax)
        {
            gmax = sc[i];
            garg = i;
        }
    double next[N_CAND];
    for (int i = 0; i < N_CAND; i++)
    {
        double best = Lv[i];
        int arg = La[i];
        if (Rv[i] > best)
        {
            best = Rv[i];
            arg = Ra[i];
        }
        if (gmax - cap > best || (gmax - cap == best && garg < arg))
        {
            best = gmax - cap;
            arg = garg;
        }
        next[i] = best + lp[i];
        bp[i] = (uint8_t)arg;
    }
    memcpy(f->vit, next, sizeof(next));
}

static bool run_frames(kara_feat *f, int last_t /* exclusive */)
{
    if (!grow(f, last_t))
        return false;
    while (f->next_t < last_t)
    {
        process_frame(f, f->next_t);
        f->next_t++;
    }
    /* forget samples no later frame needs */
    int64_t keep = (int64_t)f->next_t * KARA_HOP - N2 - 128;
    if (keep > f->base + 65536)
    {
        int64_t drop = keep - f->base;
        int64_t left = f->have - keep;
        memmove(f->xl, f->xl + drop, (size_t)left * sizeof(float));
        memmove(f->xr, f->xr + drop, (size_t)left * sizeof(float));
        f->base = keep;
    }
    return true;
}

bool kara_feat_push(kara_feat *f, const float *lr, int frames)
{
    if (frames <= 0)
        return true;
    size_t used = (size_t)(f->have - f->base);
    size_t need = used + (size_t)frames;
    if (need > f->cap)
    {
        size_t cap = f->cap ? f->cap : 1 << 17;
        while (cap < need)
            cap *= 2;
        float *a = realloc(f->xl, cap * sizeof(float));
        if (a)
            f->xl = a;
        float *b = realloc(f->xr, cap * sizeof(float));
        if (b)
            f->xr = b;
        if (!a || !b)
            return false;
        f->cap = cap;
    }
    for (int i = 0; i < frames; i++)
    {
        f->xl[used + (size_t)i] = lr[2 * i];
        f->xr[used + (size_t)i] = lr[2 * i + 1];
    }
    f->have += frames;

    /* a frame needs the samples up to half the longer window past its centre */
    int last = 0;
    if (f->have >= N2 / 2)
        last = (int)((f->have - N2 / 2) / KARA_HOP) + 1;
    return run_frames(f, last);
}

/* ------------------------------------------------------------------------ */
/* the end: global statistics, the pitch path, the cues                     */
/* ------------------------------------------------------------------------ */

static int cmp_float(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

float kara_percentile(float *a, int n, float q)
{
    if (n <= 0)
        return 0;
    qsort(a, (size_t)n, sizeof(float), cmp_float);
    double pos = (double)(n - 1) * q / 100.0;
    int i = (int)floor(pos);
    double fr = pos - i;
    if (i + 1 >= n)
        return a[n - 1];
    return (float)(a[i] + (a[i + 1] - a[i]) * fr);
}

/* scipy.ndimage.median_filter on a 1-D signal, mode="nearest": the window of
 * `size` runs from i - size/2 to i + (size-1)/2 and the result is the
 * element of rank size/2 (the upper median for an even size). */
static void median_filter(const float *x, float *out, int n, int size)
{
    float w[64];
    for (int i = 0; i < n; i++)
    {
        for (int k = 0; k < size; k++)
        {
            int j = i - size / 2 + k;
            w[k] = x[j < 0 ? 0 : j >= n ? n - 1 : j];
        }
        for (int a = 1; a < size; a++) /* insertion sort */
        {
            float v = w[a];
            int b = a - 1;
            while (b >= 0 && w[b] > v)
            {
                w[b + 1] = w[b];
                b--;
            }
            w[b + 1] = v;
        }
        out[i] = w[size / 2];
    }
}

static void local_peaks(const float *x, float *pk, int n, float peak_min)
{
    for (int t = 0; t < n; t++)
    {
        float m = x[t];
        for (int j = t - 4; j <= t + 4; j++)
        {
            float v = x[j < 0 ? 0 : j >= n ? n - 1 : j];
            if (v > m)
                m = v;
        }
        pk[t] = (x[t] >= m && x[t] >= peak_min) ? x[t] : 0.0f;
    }
}

kara_evidence *kara_feat_finish(kara_feat *f)
{
    f->ended = true;
    int total = (int)f->have;
    int T = 1 + total / KARA_HOP;
    if (total < N2 + 64 || T < 64)
        return NULL;
    if (!run_frames(f, T))
        return NULL;
    const int nb = f->nb;

    kara_evidence *ev = calloc(1, sizeof(*ev));
    if (ev == NULL)
        return NULL;
    ev->T = T;
    ev->seconds = (double)total / KARA_SR;
    float *on = malloc(sizeof(float) * (size_t)T);
    float *off = malloc(sizeof(float) * (size_t)T);
    float *tmp = malloc(sizeof(float) * (size_t)T);
    float *st = malloc(sizeof(float) * (size_t)T);
    float *sal = malloc(sizeof(float) * (size_t)T);
    uint8_t *voiced = malloc((size_t)T);
    int *path = malloc(sizeof(int) * (size_t)T);
    ev->flux = malloc(sizeof(float) * (size_t)T);
    ev->peaks = malloc(sizeof(float) * (size_t)T);
    ev->off = malloc(sizeof(float) * (size_t)T);
    ev->flux0 = malloc(sizeof(float) * (size_t)T);
    ev->note = malloc(sizeof(float) * (size_t)T);
    ev->timbre = malloc(sizeof(float) * (size_t)T);
    ev->stable = malloc(sizeof(float) * (size_t)T);
    if (!on || !off || !tmp || !st || !sal || !voiced || !path || !ev->flux ||
        !ev->peaks || !ev->off || !ev->flux0 || !ev->note || !ev->timbre ||
        !ev->stable)
        goto fail;

    /* ---- onset / offset strength: log-mel flux, lag 3, 3-band max filter on
     * the reference (vibrato robust) ---- */
    for (int t = 0; t < T; t++)
    {
        if (t < 3)
        {
            on[t] = off[t] = 0;
            continue;
        }
        const float *cur = f->logmel + (size_t)t * (size_t)nb;
        const float *old = f->logmel + (size_t)(t - 3) * (size_t)nb;
        double so = 0, sf = 0;
        for (int b = 0; b < nb; b++)
        {
            float mx = old[b], mn = old[b];
            for (int j = b - 1; j <= b + 1; j++)
            {
                float v = old[j < 0 ? 0 : j >= nb ? nb - 1 : j];
                if (v > mx)
                    mx = v;
                if (v < mn)
                    mn = v;
            }
            float d = cur[b] - mx;
            if (d > 0)
                so += d;
            float e = mn - cur[b];
            if (e > 0)
                sf += e;
        }
        on[t] = (float)(so / nb);
        off[t] = (float)(sf / nb);
    }
    memcpy(tmp, on, sizeof(float) * (size_t)T);
    float p = kara_percentile(tmp, T, 95) + 1e-9f;
    for (int t = 0; t < T; t++)
        ev->flux0[t] = fminf(fmaxf(on[t] / p, 0.0f), 3.0f);
    memcpy(tmp, off, sizeof(float) * (size_t)T);
    p = kara_percentile(tmp, T, 95) + 1e-9f;
    for (int t = 0; t < T; t++)
        ev->off[t] = fminf(fmaxf(off[t] / p, 0.0f), 3.0f);

    /* ---- pitch path, f0 in semitones, voicing ---- */
    {
        const double *sc = f->vit;
        int best = 0;
        for (int i = 1; i < N_CAND; i++)
            if (sc[i] > sc[best])
                best = i;
        path[T - 1] = best;
        for (int t = T - 1; t > 0; t--)
            path[t - 1] = f->bp[(size_t)t * N_CAND + (size_t)path[t]];
        for (int t = 0; t < T; t++)
        {
            st[t] = (float)(12.0 * log2(f->cand_hz[path[t]] / 440.0));
            sal[t] = f->S[(size_t)t * N_CAND + (size_t)path[t]] /
                     (f->S_mean[t] + 1e-9f);
        }
        memcpy(tmp, sal, sizeof(float) * (size_t)T);
        float thr = kara_percentile(tmp, T, 30);
        for (int t = 0; t < T; t++)
            voiced[t] = sal[t] > thr;
    }

    /* ---- note change: |median after - median before| over 10 frames, in
     * semitones, with the last voiced pitch carried over unvoiced gaps ---- */
    {
        float *s = tmp, *m = on; /* `on` is free again */
        int last = 0;
        for (int t = 0; t < T; t++)
        {
            if (voiced[t])
                last = t;
            s[t] = st[last];
        }
        /* before the first voiced frame numpy used index 0 */
        median_filter(s, m, T, 10);
        for (int t = 0; t < T; t++)
        {
            float d = 0;
            if (t >= 10 && t < T - 10)
                d = fabsf(m[t + 5] - m[t - 5]);
            ev->note[t] = fminf(d / 0.8f, 1.5f);
        }
    }

    /* ---- timbre change: cepstral delta of the log-mel, c1..c12, no energy */
    {
        float *ceps = malloc(sizeof(float) * (size_t)T * N_CEPS);
        float *sm = malloc(sizeof(float) * (size_t)T * N_CEPS);
        float *d = tmp;
        if (!ceps || !sm)
        {
            free(ceps);
            free(sm);
            goto fail;
        }
        double dct[N_CEPS][MAX_BANDS];
        for (int c = 0; c < N_CEPS; c++)
            for (int k = 0; k < nb; k++)
                dct[c][k] = cos(KARA_PI / nb * (k + 0.5) * (c + 1));
        for (int t = 0; t < T; t++)
        {
            const float *lm = f->logmel + (size_t)t * (size_t)nb;
            for (int c = 0; c < N_CEPS; c++)
            {
                double a = 0;
                for (int k = 0; k < nb; k++)
                    a += dct[c][k] * lm[k];
                ceps[(size_t)t * N_CEPS + c] = (float)(a / sqrt((double)(c + 1)));
            }
        }
        for (int t = 0; t < T; t++)
            for (int c = 0; c < N_CEPS; c++)
            {
                int a = t > 0 ? t - 1 : 0, b = t < T - 1 ? t + 1 : T - 1;
                sm[(size_t)t * N_CEPS + c] =
                    (ceps[(size_t)a * N_CEPS + c] + ceps[(size_t)t * N_CEPS + c] +
                     ceps[(size_t)b * N_CEPS + c]) /
                    3.0f;
            }
        for (int t = 0; t < T; t++)
        {
            d[t] = 0;
            if (t >= 3 && t < T - 3)
            {
                double a = 0;
                for (int c = 0; c < N_CEPS; c++)
                {
                    double x = sm[(size_t)(t + 3) * N_CEPS + c] -
                               sm[(size_t)(t - 3) * N_CEPS + c];
                    a += x * x;
                }
                d[t] = (float)sqrt(a);
            }
        }
        free(ceps);
        free(sm);
        float *base = on;
        median_filter(d, base, T, 31);
        for (int t = 0; t < T; t++)
            ev->timbre[t] = fmaxf(d[t] - base[t], 0.0f);
        memcpy(tmp, ev->timbre, sizeof(float) * (size_t)T);
        float q = kara_percentile(tmp, T, 95) + 1e-9f;
        for (int t = 0; t < T; t++)
            ev->timbre[t] = fminf(ev->timbre[t] / q, 3.0f);
    }

    /* ---- put the cues together ---- */
    const kara_feat_params *P = &f->prm;
    for (int t = 0; t < T; t++)
    {
        ev->stable[t] = voiced[t] ? (1.0f - fminf(ev->note[t], 1.0f)) *
                                        (1.0f - fminf(ev->timbre[t], 1.0f))
                                  : 0.0f;
        float v = ev->flux0[t] * (1.0f - P->w_stable * ev->stable[t]) +
                  P->w_pitch * ev->note[t] + P->w_timbre * ev->timbre[t];
        ev->flux[t] = fminf(fmaxf(v, 0.0f), 3.0f);
    }
    local_peaks(ev->flux, ev->peaks, T, P->peak_min);

    free(on);
    free(off);
    free(tmp);
    free(st);
    free(sal);
    free(voiced);
    free(path);
    return ev;

fail:
    free(on);
    free(off);
    free(tmp);
    free(st);
    free(sal);
    free(voiced);
    free(path);
    kara_evidence_free(ev);
    return NULL;
}
