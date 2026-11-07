#ifndef __KARA_ALIGN_H
#define __KARA_ALIGN_H

/*
 * Places N syllables of known relative weight inside one lyric line's window
 * (port of kara/align.py, align_line_posterior): onsets are chosen on the
 * 10 ms grid by combining acoustic evidence with a duration prior and a
 * per-line tempo prior, and decoded as the posterior median of each onset
 * (forward-backward over the same lattice, tempo marginalised).
 *
 * The lattice is the Python one, evaluated differently. The penalty for onset
 * peaks a syllable swallows is a difference F(t) - H(s) of prefix sums, so each
 * step of the recursion is a correlation of the previous step with the
 * duration kernel, in plain probabilities with rescaling: O(T * D) multiply-
 * adds with no logarithms, which is what makes a whole song a fraction of a
 * second of CPU instead of half a minute.
 */

#include "kara_feat.h"

#include <stdbool.h>

/* Doubles, like the Python they come from: int(0.06 * 100) must be 6, and
 * 0.06f * 100.0 is 5.99999987. */
typedef struct kara_params
{
    double alpha;       /* onset evidence weight */
    double gamma;       /* penalty for peaks inside a syllable */
    double delta;       /* offset evidence weight at the end */
    double sigma_short; /* duration prior width, log domain, shorter than ... */
    double sigma_long;  /* ... and longer than expected */
    double lam_dur;
    double start_scale; /* Huber anchor of the first onset on the line start */
    double start_bias;
    double pre_roll;        /* s before the line start the window begins */
    double max_first_delay; /* s after the start the first onset may be */
    int guard;             /* frames around a boundary where its own peak is */
    int n_tau;
    double tau_min, tau_max;
    double tempo_sigma;  /* log-std of the prior on tempo; 0 turns it off */
    double tail_assumed; /* s of silence assumed at the end of a window */
    double dmin, dmax;   /* s: shortest and longest syllable */
    double pen_floor;    /* peaks below this are free inside a syllable */
    double tail_ok;      /* s between the sung end and the next line allowed */
    double kappa;        /* penalty per s beyond that */
    double rho;          /* penalty factor for peaks after the end */
} kara_params;

/* The Stage 2 defaults. */
kara_params kara_params_default(void);

/* Polled between lattice steps; true: give up. NULL: never. */
typedef bool (*kara_cancel_fn)(void *ctx);

/* A line with this many syllables or more is not aligned (uniform spread). */
#define KARA_MAX_SYLLABLES 256

typedef struct kara_align_out
{
    double *onset; /* seconds, n entries (caller's) */
    float *conf;   /* posterior mass within 80 ms of the onset, n entries */
    double end;    /* end of the last syllable, seconds */
    double tau;    /* seconds per unit weight, as decoded */
    bool uniform;  /* no solution: the syllables are spread evenly */
} kara_align_out;

/* `start`, `next_start`: the line's annotated start and the start of the next
 * line (seconds). `w`: weights of the n syllables. Returns false only when
 * cancelled or out of memory; in every other case
 * `out` is filled, `uniform` telling whether the evidence was used. */
bool kara_align_line(const kara_evidence *ev, double start, double next_start,
                     const float *w, int n, const kara_params *p,
                     kara_align_out *out, kara_cancel_fn cancel, void *ctx);

/* The even spread, the baseline the aligner has to beat. */
void kara_align_uniform(double start, double next_start, const float *w, int n,
                        kara_align_out *out);

#endif /* __KARA_ALIGN_H */
