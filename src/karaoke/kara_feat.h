#ifndef __KARA_FEAT_H
#define __KARA_FEAT_H

/*
 * Acoustic evidence for the aligner, computed while the audio streams past
 * (port of kara/dsp.py + kara/pitch.py, the Stage 2 "singing" profile).
 *
 * Per 10 ms frame, from the stereo signal at 22.05 kHz:
 *   - the mid signal with a centre-panning mask (vocals are usually centred),
 *   - onset strength: vibrato-robust log-mel flux over 200..3500 Hz,
 *   - offset strength: the same on falling energy,
 *   - pitch: subharmonic summation + Viterbi smoothing -> note-change cue,
 *   - timbre: cepstral change -> vowel-change cue,
 *   - "stable" (voiced, pitch- and timbre-steady) to damp flux inside held
 *     notes,
 * combined into one onset evidence curve and its peaks.
 *
 * Streaming: samples go in a block at a time (kara_feat_push), frames are
 * computed as soon as the samples they need have arrived, and only the
 * per-frame features are kept (a few hundred bytes per 10 ms), never the
 * audio. What cannot be done before the end (percentile normalisation, the
 * pitch path) happens in kara_feat_finish().
 */

#include <stdbool.h>
#include <stdint.h>

#define KARA_SR  22050
#define KARA_HOP 220 /* 10 ms */
#define KARA_FPS 100

/* FFTW plans, shared by every extractor. FFTW's planner is not thread-safe,
 * so plans are made once and never destroyed: kara_plans_shared() makes them
 * on first use (call it from the thread that makes the player's other FFT
 * plans, before any worker thread runs); executing them from any number of
 * threads is fine. kara_plans_new/free are for tests. */
typedef struct kara_plans kara_plans;
const kara_plans *kara_plans_shared(void);
kara_plans *kara_plans_new(void);
void kara_plans_free(kara_plans *p);

typedef struct kara_feat_params
{
    float w_pitch;  /* weight of the note-change cue */
    float w_timbre; /* weight of the timbre-change cue */
    float w_stable; /* flux suppression where voiced and steady */
    float peak_min; /* smallest onset peak that counts */
} kara_feat_params;

/* the Stage 2 defaults, tuned on the synthetic singing benchmark */
kara_feat_params kara_feat_stage2(void);

typedef struct kara_evidence
{
    int T;          /* frames */
    double seconds; /* length of the audio */
    float *flux;    /* onset evidence, 0..3 (cues combined) */
    float *peaks;   /* flux at its local maxima, else 0 */
    float *off;     /* offset evidence, 0..3 */
    /* the parts it was made of, kept for tests and tuning */
    float *flux0;  /* energy flux alone, p95 = 1 */
    float *note;   /* pitch-step cue, 0..1.5 */
    float *timbre; /* timbre-change cue, 0..3 */
    float *stable; /* 0 or 1 */
} kara_evidence;

void kara_evidence_free(kara_evidence *e);

typedef struct kara_feat kara_feat;

/* NULL on allocation failure. `plans` must outlive the extractor. */
kara_feat *kara_feat_new(const kara_plans *plans, kara_feat_params p);

/* Interleaved stereo float samples (-1..1) at KARA_SR. Returns false when
 * out of memory. */
bool kara_feat_push(kara_feat *f, const float *lr, int frames);

/* Frames computed so far, for progress reporting. */
int kara_feat_frames(const kara_feat *f);

/* Ends the signal and returns the evidence (NULL if there was too little
 * audio or on allocation failure). The extractor is spent afterwards: free it. */
kara_evidence *kara_feat_finish(kara_feat *f);

void kara_feat_free(kara_feat *f);

/* np.percentile(a, q) with linear interpolation; q in 0..100. Reorders `a`. */
float kara_percentile(float *a, int n, float q);

#endif /* __KARA_FEAT_H */
