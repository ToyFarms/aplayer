#ifndef __UI_OPTIONS_H
#define __UI_OPTIONS_H

/*
 * Behaviour and layout knobs of the interface (colours live in ui_theme).
 * Plain data with defaults from ui_options_default(), so a config loader can
 * fill it without touching any widget.
 */

#include "image_renderer.h"

#include <stdbool.h>

/* What the widget under the controls draws. */
enum ui_spectrum_mode
{
    UI_SPECTRUM_MODE_LINE_FILL, /* the spectrum as one line, the area under it filled */
    UI_SPECTRUM_MODE_LINE,      /* the spectrum as one line, the bottom left clear */
    UI_SPECTRUM_MODE_SCOPE,     /* its waveform, scrolling like an oscilloscope */
    UI_SPECTRUM_MODE_COUNT,
};

enum ui_vu_style
{
    UI_VU_STYLE_LINE,
    UI_VU_STYLE_HALF,
    UI_VU_STYLE_BAR,
};

/* Bounds of lyrics.big_rows when it is changed with the keys (- + =). */
#define UI_LYRICS_BIG_ROWS_MIN 2
#define UI_LYRICS_BIG_ROWS_MAX 16

typedef struct ui_options
{
    struct
    {
        int scrolloff; /* rows kept visible around the cursor */
    } playlist;

    struct
    {
        int slice_us; /* time budget per frame for scanning huge libraries */
    } search;

    struct
    {
        int timeout_ms;   /* incomplete / ambiguous key sequence timeout */
        bool langmap;     /* map non-Latin layouts / IME punctuation to keys */
    } keys;

    struct
    {
        int left_pad;
        int right_pad;
        int tick_gap;
        int bar_gap;
        int bar_width;
        enum ui_vu_style style;
        bool render_ticks;
        bool render_marks;
        bool render_numeric;
        bool render_separator;
        bool render_peak;
        bool discrete;
        float peak_decay;
    } vu_meter;

    struct
    {
        int gap;
        int padding;
    } tabs;

    struct
    {
        enum image_render_method method; /* initial rendering method */
        int max_covers; /* pictures kept per track (the rest are dropped) */
        int anim_ms;    /* one card moving to the back / front */
        float bump;     /* front card growth on a peak, in sub-pixels */
    } cover_art;

    /* How the VISUAL page follows the music (see ui_ambience.h). */
    struct
    {
        int levels;       /* energy steps; 0 turns the pulse off */
        float pulse_hz;   /* page repaints per second, at most */
        float release_ms; /* how long a peak takes to fall away */
        float fade_ms;    /* palette cross-fade between covers */
        float depth;      /* 0..1, how strongly the energy shows */
        int vu_min_width; /* narrower screens drop the level meters */
    } ambience;

    /* The spectrum line under the controls, and its oscilloscope mode. */
    struct
    {
        enum ui_spectrum_mode mode; /* shown at start; `w` cycles the modes */
        float attack_ms;  /* how fast the line climbs to a louder band */
        float release_ms; /* ... and falls back */
    } spectrum;

    struct
    {
        float window_ms;  /* time across the whole width */
        float delay_ms;   /* how far the picture trails the newest sample, so
                             it never runs dry between audio blocks */
        float gain;       /* vertical scale on top of auto_gain: 1.0 puts a
                             full-scale sample on the edge */
        bool auto_gain;   /* scale so the loudest recent peak nearly fills
                             the height, however quiet the track is */
        bool graticule;   /* a faint dotted centre line */
    } scope;

    struct
    {
        float speed;   /* plain lyrics: rows scrolled per second */
        float ease_ms; /* synced lyrics: time to glide to the next line */
        int resume_s;  /* seconds after the last scroll before following
                          again; 0: only when asked */

        /* The line being sung is drawn big: as pixels of a font, fitted to
         * cells (see ui_font.h). It grows when it starts and shrinks back
         * when the next one takes over. */
        int big_rows;      /* terminal rows of one line of big text; 0 turns
                              big text off. Shrinks when a line does not fit,
                              down to 3 (4 with blocks), below which the
                              letters are not readable; then the line stays
                              small. Lines of CJK / Hangul are drawn at 5
                              (6) rows at least, for the same reason. At
                              most UI_LYRICS_BIG_ROWS_MAX; the - + = keys
                              change it while playing */
        int grow_ms;       /* time for a line to grow, and to shrink back */
        bool big_braille;  /* braille dots (finer: clearer letters, and 3
                              rows suffice) instead of block glyphs (solid) */
        int weight;        /* weight of a variable font's weight axis (400
                              regular, 700 bold); 0: the font's own. Static
                              fonts are not affected */
        const char *font;  /* font file for the big text; NULL: the environment
                              variable APLAYER_LYRICS_FONT, then what the
                              system has */

        /* Word timing (karaoke): the current line of synced lyrics fills in
         * as it is sung. The timing is worked out from the audio by a
         * background thread (see docs/KARAOKE.md); a line shows as a whole
         * until its timing is ready. */
        bool karaoke;           /* false: no analysis, no fill */
        int karaoke_offset_ms;  /* shifts the fill: positive paints it later,
                                   negative sooner (a lead can feel better
                                   than the onset itself) */
    } lyrics;
} ui_options;

ui_options ui_options_default(void);

#endif /* __UI_OPTIONS_H */
