#include "kara_align.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

kara_params kara_params_default(void)
{
    return (kara_params){.alpha = 1.0,
                         .gamma = 1.0,
                         .delta = 1.0,
                         .sigma_short = 0.45,
                         .sigma_long = 0.80,
                         .lam_dur = 1.0,
                         .start_scale = 0.10,
                         .start_bias = 0.05,
                         .pre_roll = 0.30,
                         .max_first_delay = 1.5,
                         .guard = 6,
                         .n_tau = 11,
                         .tau_min = 0.10,
                         .tau_max = 2.20,
                         .tempo_sigma = 0.60,
                         .tail_assumed = 0.6,
                         .dmin = 0.06,
                         .dmax = 6.0,
                         .pen_floor = 0.5,
                         .tail_ok = 1.0,
                         .kappa = 10.0,
                         .rho = 0.0};
}

void kara_align_uniform(double start, double next_start, const float *w, int n,
                        kara_align_out *out)
{
    double sum = 0;
    for (int i = 0; i < n; i++)
        sum += w[i];
    double span = next_start - start - 0.3;
    if (span < 0.2)
        span = 0.2;
    double at = start;
    for (int i = 0; i < n; i++)
    {
        out->onset[i] = at;
        out->conf[i] = 0.0f;
        at += span * w[i] / sum;
    }
    out->end = at;
    out->tau = span / sum;
    out->uniform = true;
}

/* sum_k a[k] * b[k], four independent accumulators so it vectorises */
static inline double dot(const double *a, const double *b, int n)
{
    double s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    int k = 0;
    for (; k + 4 <= n; k += 4)
    {
        s0 += a[k] * b[k];
        s1 += a[k + 1] * b[k + 1];
        s2 += a[k + 2] * b[k + 2];
        s3 += a[k + 3] * b[k + 3];
    }
    for (; k < n; k++)
        s0 += a[k] * b[k];
    return (s0 + s1) + (s2 + s3);
}

/* Everything one line's lattice needs. */
typedef struct lat
{
    int T, g;
    int dmin, dmax; /* frames */
    double *eA;     /* exp(alpha * flux) */
    double *ef0;    /* exp(first-onset prior) */
    double *eF, *eH;
    double *Gt;
} lat;

/* duration kernel exp(logLogNormal) over d = dlo..dhi (frames); the support
 * is trimmed to where it exceeds 1e-12 */
static void kernel(const lat *L, const kara_params *p, double mu, double *K,
                   int *dlo, int *dhi)
{
    int lo = L->dmin, hi = L->dmax;
    for (int d = L->dmin; d <= L->dmax; d++)
    {
        double z = log((d / 100.0) / mu);
        double s = z < 0 ? p->sigma_short : p->sigma_long;
        double q = z / s;
        K[d] = exp(-p->lam_dur * q * q / 2.0);
    }
    while (lo < hi && K[lo] < 1e-12)
        lo++;
    while (hi > lo && K[hi] < 1e-12)
        hi--;
    *dlo = lo;
    *dhi = hi;
}

/* out[s] = sum over d of K[d] * w(s, s+d) * v[s+d]: the backward step, where
 * w is the penalty for peaks swallowed between s and s+d (none when the
 * syllable is no longer than two guards). */
static void back_step(const lat *L, const double *K, int dlo, int dhi,
                      const double *v, double *v2, double *out)
{
    int T = L->T, g2 = 2 * L->g;
    for (int t = 0; t < T; t++)
        v2[t] = L->eF[t] * v[t];
    int farlo = dlo > g2 + 1 ? dlo : g2 + 1;
    for (int s = 0; s < T; s++)
    {
        int room = T - 1 - s;
        double acc = 0;
        int nb = (g2 < dhi ? g2 : dhi);
        if (nb > room)
            nb = room;
        if (nb >= dlo)
            acc += dot(K + (dlo - dlo), v + s + dlo, nb - dlo + 1);
        int fb = dhi < room ? dhi : room;
        if (fb >= farlo)
            acc += L->eH[s] * dot(K + (farlo - dlo), v2 + s + farlo,
                                  fb - farlo + 1);
        out[s] = acc;
    }
}

static double rescale(double *a, int n)
{
    double m = 0;
    for (int i = 0; i < n; i++)
        if (a[i] > m)
            m = a[i];
    if (m > 0 && m != 1.0)
    {
        double inv = 1.0 / m;
        for (int i = 0; i < n; i++)
            a[i] *= inv;
    }
    return m;
}

bool kara_align_line(const kara_evidence *ev, double start, double next_start,
                     const float *w, int N, const kara_params *p,
                     kara_align_out *out, kara_cancel_fn cancel, void *ctx)
{
    out->uniform = false;
    if (N <= 0)
        return true;
    if (N >= KARA_MAX_SYLLABLES)
    {
        kara_align_uniform(start, next_start, w, N, out);
        return true;
    }

    double a = start - p->pre_roll;
    if (a < 0)
        a = 0;
    int i0 = (int)(a * KARA_FPS);
    int i1 = (int)(next_start * KARA_FPS);
    if (i1 > ev->T)
        i1 = ev->T;
    int T = i1 - i0;
    int g = p->guard;
    int dmin = (int)(p->dmin * 100.0);

    double wsum = 0, wmax = 0;
    for (int i = 0; i < N; i++)
    {
        wsum += w[i];
        if (w[i] > wmax)
            wmax = w[i];
    }
    if (T < N * dmin + 2 || wsum <= 0)
    {
        kara_align_uniform(start, next_start, w, N, out);
        return true;
    }

    /* tempo grid */
    double span = next_start - start;
    double tau0 = (span - p->tail_assumed > 0.3 ? span - p->tail_assumed : 0.3) /
                  wsum;
    int nt = p->n_tau;
    double taus[64];
    if (nt > 64)
        nt = 64;
    double taumax = 0;
    for (int k = 0; k < nt; k++)
    {
        double x = nt > 1 ? (double)k / (nt - 1) : 0.0;
        double tau = tau0 * 0.4 * pow(2.2 / 0.4, x);
        tau = tau < p->tau_min ? p->tau_min : tau > p->tau_max ? p->tau_max : tau;
        taus[k] = tau;
        if (tau > taumax)
            taumax = tau;
    }
    double dmax_s = 3.0 * wmax * taumax;
    if (dmax_s < 1.5)
        dmax_s = 1.5;
    if (dmax_s > p->dmax)
        dmax_s = p->dmax;
    int dmax = (int)(dmax_s * 100.0);
    if (dmax > T)
        dmax = T;
    if (dmax < dmin)
    {
        kara_align_uniform(start, next_start, w, N, out);
        return true;
    }

    /* ---- per-line arrays ---- */
    size_t Ts = (size_t)T;
    size_t nd = (size_t)dmax + 2;
    double *mem = malloc(sizeof(double) *
                         (Ts * 8 + nd * 3 + (size_t)N * Ts * (size_t)(nt + 1) + 64));
    if (mem == NULL)
        return false;
    double *cp = mem;                  /* T + 1 */
    double *eA = cp + Ts + 1;
    double *ef0 = eA + Ts;
    double *eF = ef0 + Ts;
    double *eH = eF + Ts;
    double *Gt = eH + Ts;
    double *v = Gt + Ts;
    double *v2 = v + Ts;
    double *K = v2 + Ts + 1;           /* nd, indexed by d - dlo */
    double *Kr = K + nd;               /* the same, reversed */
    double *Kfull = Kr + nd;           /* nd, indexed by d */
    double *post = Kfull + nd;         /* nt * N * T */
    double *alpha = post + (size_t)nt * (size_t)N * Ts; /* N * T */
    double *tail = malloc(sizeof(double) * (Ts * 3 + 8));
    if (tail == NULL)
    {
        free(mem);
        return false;
    }
    double *beta2 = tail;
    double *u = tail + Ts;
    double *Fv = tail + 2 * Ts;

    cp[0] = 0;
    for (int t = 0; t < T; t++)
    {
        double pk = ev->peaks[i0 + t] - p->pen_floor;
        cp[t + 1] = cp[t] + (pk > 0 ? pk : 0);
        eA[t] = exp((double)p->alpha * ev->flux[i0 + t]);

        double rel = (double)(t + i0) / KARA_FPS - start;
        double z0 = fabs(rel - p->start_bias) / p->start_scale;
        double first = -(z0 < 2.0 ? z0 * z0 / 2.0 : 2.0 * z0 - 2.0);
        if (rel > p->max_first_delay || rel < -p->pre_roll - 1e-6)
            ef0[t] = 0;
        else
            ef0[t] = exp(first);
    }
    double gm = p->gamma;
    if (gm * cp[T] > 500.0)
        gm = 500.0 / cp[T]; /* only a saturated, plateau-ridden window */
    for (int t = 0; t < T; t++)
    {
        int hi = t - g + 1;
        eF[t] = exp(-gm * cp[hi < 0 ? 0 : hi]);
        int lo = t + g + 1;
        eH[t] = exp(gm * cp[lo > T ? T : lo]);
        double e_gap = (double)(T - 1 - t) / KARA_FPS;
        double pen = p->kappa * (e_gap - p->tail_ok > 0 ? e_gap - p->tail_ok : 0);
        double after = (t + g + 1 < T) ? cp[T] - cp[t + g + 1] : 0.0;
        Gt[t] = exp((double)p->delta * ev->off[i0 + t] - pen -
                    (double)p->rho * gm * after);
    }

    lat L = {T, g, dmin, dmax, eA, ef0, eF, eH, Gt};
    int g2 = 2 * g;

    double logZ[64];
    int feasible = 0;
    bool aborted = false;

    for (int k = 0; k < nt && !aborted; k++)
    {
        double tau = taus[k];
        double *al = alpha;

        /* ---- forward ---- */
        double lscale = 0;
        for (int t = 0; t < T; t++)
            al[t] = ef0[t] * eA[t];
        double m = rescale(al, T);
        if (m <= 0)
        {
            logZ[k] = -INFINITY;
            continue;
        }
        lscale += log(m);
        bool dead = false;
        for (int i = 1; i < N && !dead; i++)
        {
            if (cancel != NULL && cancel(ctx))
            {
                aborted = true;
                break;
            }
            int dlo, dhi;
            kernel(&L, p, w[i - 1] * tau, Kfull, &dlo, &dhi);
            memcpy(K, Kfull + dlo, sizeof(double) * (size_t)(dhi - dlo + 1));
            for (int j = 0; j <= dhi - dlo; j++)
                Kr[j] = K[dhi - dlo - j];
            const double *prev = al + (size_t)(i - 1) * Ts;
            double *cur = al + (size_t)i * Ts;
            for (int s = 0; s < T; s++)
                u[s] = prev[s] * eH[s];
            int farlo = dlo > g2 + 1 ? dlo : g2 + 1;
            for (int t = 0; t < T; t++)
            {
                /* sum over d of K[d] * x[t - d]: with the kernel reversed
                 * both operands run forwards */
                double acc = 0;
                int nb = g2 < dhi ? g2 : dhi;
                if (nb > t)
                    nb = t;
                if (nb >= dlo)
                    acc = dot(Kr + (dhi - nb), prev + (t - nb), nb - dlo + 1);
                int fb = dhi < t ? dhi : t;
                double far = 0;
                if (fb >= farlo)
                    far = dot(Kr + (dhi - fb), u + (t - fb), fb - farlo + 1);
                cur[t] = eA[t] * (acc + eF[t] * far);
            }
            m = rescale(cur, T);
            if (m <= 0)
            {
                dead = true;
                break;
            }
            lscale += log(m);
        }
        if (aborted)
            break;
        if (dead)
        {
            logZ[k] = -INFINITY;
            continue;
        }

        /* ---- the end of the last syllable, then backward ---- */
        int dlo, dhi;
        kernel(&L, p, w[N - 1] * tau, Kfull, &dlo, &dhi);
        memcpy(K, Kfull + dlo, sizeof(double) * (size_t)(dhi - dlo + 1));
        back_step(&L, K, dlo, dhi, Gt, v2, Fv);
        double Z = 0;
        const double *last = al + (size_t)(N - 1) * Ts;
        for (int t = 0; t < T; t++)
            Z += last[t] * Fv[t];
        if (!(Z > 0))
        {
            logZ[k] = -INFINITY;
            continue;
        }
        logZ[k] = log(Z) + lscale;
        feasible++;

        double *pk = post + (size_t)k * (size_t)N * Ts;
        memcpy(beta2, Fv, sizeof(double) * Ts);
        rescale(beta2, T);
        for (int i = N - 1; i >= 0; i--)
        {
            const double *ai = al + (size_t)i * Ts;
            double *pi = pk + (size_t)i * Ts;
            double sum = 0;
            for (int t = 0; t < T; t++)
            {
                pi[t] = ai[t] * beta2[t];
                sum += pi[t];
            }
            if (sum > 0)
                for (int t = 0; t < T; t++)
                    pi[t] /= sum;
            if (i == 0)
                break;
            if (cancel != NULL && cancel(ctx))
            {
                aborted = true;
                break;
            }
            for (int t = 0; t < T; t++)
                v[t] = eA[t] * beta2[t];
            kernel(&L, p, w[i - 1] * tau, Kfull, &dlo, &dhi);
            memcpy(K, Kfull + dlo, sizeof(double) * (size_t)(dhi - dlo + 1));
            back_step(&L, K, dlo, dhi, v, v2, beta2);
            rescale(beta2, T);
        }
    }

    if (aborted)
    {
        free(mem);
        free(tail);
        return false;
    }
    if (feasible == 0)
    {
        free(mem);
        free(tail);
        kara_align_uniform(start, next_start, w, N, out);
        return true;
    }

    /* ---- marginalise the tempo ---- */
    double wt[64], zmax = -INFINITY;
    for (int k = 0; k < nt; k++)
    {
        double pr = 0;
        if (p->tempo_sigma > 0)
        {
            double q = log(taus[k] / tau0) / p->tempo_sigma;
            pr = -0.5 * q * q;
        }
        wt[k] = logZ[k] + pr;
        if (wt[k] > zmax)
            zmax = wt[k];
    }
    double wsum_t = 0, tau_mean = 0;
    for (int k = 0; k < nt; k++)
    {
        wt[k] = isfinite(wt[k]) ? exp(wt[k] - zmax) : 0.0;
        wsum_t += wt[k];
    }
    for (int k = 0; k < nt; k++)
    {
        wt[k] /= wsum_t;
        tau_mean += wt[k] * taus[k];
    }

    int prev_med = -1;
    int *med = malloc(sizeof(int) * (size_t)N);
    double *rows = malloc(sizeof(double) * (size_t)N * Ts);
    if (med == NULL || rows == NULL)
    {
        free(med);
        free(rows);
        free(mem);
        free(tail);
        return false;
    }
    for (int i = 0; i < N; i++)
    {
        double *row = rows + (size_t)i * Ts;
        memset(row, 0, sizeof(double) * Ts);
        for (int k = 0; k < nt; k++)
        {
            if (wt[k] == 0)
                continue;
            const double *pi = post + (size_t)k * (size_t)N * Ts + (size_t)i * Ts;
            for (int t = 0; t < T; t++)
                row[t] += wt[k] * pi[t];
        }
        double sum = 0;
        for (int t = 0; t < T; t++)
            sum += row[t];
        sum += 1e-300;
        double c = 0;
        int mi = T - 1;
        for (int t = 0; t < T; t++)
        {
            row[t] /= sum;
            c += row[t];
            if (c >= 0.5)
            {
                mi = t;
                break;
            }
        }
        /* finish normalising the rest of the row for the confidence window */
        for (int t = mi + 1; t < T; t++)
            row[t] /= sum;
        if (mi < prev_med + 1 && i > 0)
            mi = prev_med + 1;
        med[i] = mi;
        prev_med = mi;
    }
    for (int i = 0; i < N; i++)
    {
        const double *row = rows + (size_t)i * Ts;
        double c = 0;
        int lo = med[i] - 8 < 0 ? 0 : med[i] - 8;
        int hi = med[i] + 8 >= T ? T - 1 : med[i] + 8;
        for (int t = lo; t <= hi; t++)
            c += row[t];
        out->onset[i] = (double)(med[i] + i0) / KARA_FPS;
        out->conf[i] = (float)(c > 1.0 ? 1.0 : c);
    }
    out->tau = tau_mean;
    out->end = out->onset[N - 1] + w[N - 1] * tau_mean;
    out->uniform = false;

    free(med);
    free(rows);
    free(mem);
    free(tail);
    return true;
}
