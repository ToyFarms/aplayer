# Word timing ("karaoke")

Synced lyrics (LRC, TTML, ID3 `SYLT`...) say when each **line** starts. With
word timing on, the line being sung also fills in, syllable by syllable, as it
is sung. The timing is worked out from the audio itself: no model files, no
network, nothing to install (two word lists, English and Japanese, ship in `lexicon/`; see `lexicon/README.md`).

It is on by default; `lyrics.karaoke = false` turns it off (no analysis is done
then), `lyrics.karaoke_offset_ms` shifts the fill later (positive) or sooner
(negative). A line shows as a whole until its timing is ready, and lines the
analysis could not time (instrumental markers, windows longer than 30 s) always
do.

## What it costs

Measured on a 4 minute stereo track (one core, release build):

| step | cost |
|---|---|
| decode + features, the whole track | about 1.0 s of CPU |
| align all 20 lines | about 0.5 s (26 ms a line) |
| analysis state while it runs | about 0.9 KB per 10 ms of audio: 20 MB for this track, 54 MB at the 10 minute limit; freed when done (pitch scores and Viterbi back-pointers dominate) |

(Measured on a 4 minute stereo mp4, one core, release build: 1.1 s user time
and 57 MB peak for the whole process, `kara_cli` included.)

The reference implementation in Python needs 37 s for the same file.

How the player stays responsive:

* **Own thread, low priority** (nice 10 / `THREAD_PRIORITY_BELOW_NORMAL`), with
  **its own decoder**. The playback decoder, ring buffers and effects are never
  touched.
* **Lazy and delayed.** Nothing runs until the lyrics widget asks for the
  current track's timing, and then only after 300 ms, so skipping through a
  playlist costs nothing.
* **Streaming.** The track is decoded and resampled to 22.05 kHz in chunks that
  go straight into the feature extractor. Audio is never held; only per-frame
  features are (a few hundred bytes per 10 ms). Results do not depend on the
  chunk size (tested).
* **Nearest line first.** Lines are aligned one at a time, the line being
  sung first, then the ones after it, then those before; each is published the
  moment it is done. Moving the playhead re-prioritises.
* **Never blocks the audio thread.** A track that ends on its own is freed on
  the audio thread. `kara_destroy()` only raises a flag; the worker notices
  within milliseconds and frees its own memory on the way out.
* Tracks over 10 minutes are not analysed.

## How it works

Port of the Stage 2 prototype (`Karaoke_Timing_Stage_2_Technical_Report.md`),
checked against it: on the same file the C code places all 230 syllables of the
Nadin Amizah test track within 10 ms of the Python result 96.5 % of the time
(median difference 0, max 60 ms; the residue is the Python reading a 16 bit WAV).

1. **Text -> units** (`kara_text.c`). Each line is split into syllables (Indonesian
   by rules, English by a pronunciation list or vowel groups, other Latin
   scripts by vowel groups), morae (Japanese), or characters (Korean, Chinese),
   each with a relative duration weight (stressed syllables longer, small tsu
   shorter...). Every unit knows which bytes of the line it covers.
2. **Audio -> evidence** (`kara_feat.c`), per 10 ms: the centre channel (vocals
   are usually panned centre) -> onset strength (log-mel flux, 200-3500 Hz,
   robust to vibrato), a pitch track (subharmonic summation + Viterbi) giving a
   note-change cue, a timbre-change cue, and damping of the flux where the voice
   holds a steady note.
3. **Evidence -> onsets** (`kara_align.c`). For each line, a lattice over the 10 ms
   grid chooses the N onsets, combining the evidence, a duration prior, a prior
   that the first onset is near the annotated line start, and a tempo marginalised
   over 11 values. Each onset is the median of its posterior. The recursion is the
   Python one evaluated as a correlation of probabilities (the penalty for
   onset peaks a syllable swallows factorises as `F(t) - H(s)`), O(T*D) per step
   with no logarithms.
4. **Publishing** (`kara_job.c`): per-line results are immutable and handed to
   the widget, which copies the current line's timing and paints it.

## What it does not do, honestly

The prototype was tuned on **synthetic** singing with exact ground truth
(median error about 200 ms, 34 % of onsets within 100 ms on its test split). On
real songs **there is no ground truth**: it was only shown to be self-consistent
(median 140 ms between repeats). Treat the fill as "about right, with the
syllable boundaries jittery", not as studio-grade karaoke. Known weaknesses:

* Held notes, melisma and backing vocals are poorly handled; repeated notes on
  one pitch give no pitch cue.
* Voicing is a per-file percentile threshold, wrong for tracks with long
  instrumental sections.
* Many boundaries are decided mostly by the duration prior (mean confidence
  0.34 on real lines): the line shows as evenly paced when the audio is silent
  about it. Lines with no usable evidence are spread evenly (`kara_line.uniform`).
* `lexicon/ja_readings.txt` (shipped, from pykakasi, GPL) gives kanji their readings;
  readings are picked without okurigana context, so a few are the wrong variant.
  Without the file a kanji is assumed to be two morae, which is wrong often
  enough to shift the fill within a line. Kana is always exact.
* Without `lexicon/en_syllables.txt` English syllables are counted by vowel groups.
* Latin words in a line tagged Japanese are counted as English (the prototype
  gave them no syllables at all).
* Only the main lines are timed: no word-level tags are read from files that
  have them (an easy addition: use them instead of the analysis).

## Files

| | |
|---|---|
| `src/include/karaoke.h` | the API the player uses (create, focus, line, status, destroy) |
| `src/karaoke/kara_text.*` | text -> units |
| `src/karaoke/kara_lex.*` | optional word lists |
| `src/karaoke/kara_feat.*` | streaming feature extractor |
| `src/karaoke/kara_align.*` | per-line aligner |
| `src/karaoke/kara_job.c` | worker thread, decoder, priorities, publishing |
| `src/ui/widgets/ui_lyrics.c` | the fill (big text: pixel-clipped; small text: by character) |
| `tools/kara_cli.c` | `-DAPLAYER_KARA_TOOLS=ON`: run the job on a file from the command line |
| `tests/test_kara_*.c` | unit tests |

Threading rules worth knowing before changing it: FFTW's planner is not
thread-safe, so the plans are made once on first use (on the thread that asks:
the UI thread) and never destroyed; executing them from the worker is safe.
