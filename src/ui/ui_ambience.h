#ifndef __UI_AMBIENCE_H
#define __UI_AMBIENCE_H

/*
 * The mood of the VISUAL page: which palette it wears right now and how loud
 * the music is. Two things evolve over time and both live here so every widget
 * on the page reads the same values:
 *
 *  - the palette cross-fades to the one of the new cover art;
 *  - the energy follows the activity analyzer (the "ACT" bar of the VU meter):
 *    instant attack, adjustable release, then quantised to a few levels and
 *    rate limited, so what follows it repaints when the level steps, never
 *    every frame.
 *
 * The page itself stays still (flat colour, only the palette fade moves it);
 * the energy is read by the things that show it: the front card, the title and
 * the current lyric line.
 *
 * Pure state + arithmetic; the application feeds it once per frame.
 */

#include "color.h"
#include "ui_palette.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ui_ambience_params
{
    int levels;       /* energy steps; 0 turns the pulse off */
    float pulse_hz;   /* at most this many level changes per second */
    float release_ms; /* how long a peak takes to fall away */
    float fade_ms;    /* palette cross-fade */
    float depth;      /* 0..1, how strongly energy shows */
} ui_ambience_params;

typedef struct ui_ambience
{
    ui_palette from, to, now; /* `now` is what is on screen */
    float fade;       /* 0..1 progress from `from` to `to` */
    float fade_shown; /* ... as far as `now` has caught up (in steps) */

    float energy; /* followed activity, 0..1 */
    int level;    /* energy quantised to 0..levels */
    float level_age; /* seconds since the level last changed */

    /* Bumped whenever something a painter shows has visibly changed: the
     * palette moved or the level stepped. */
    uint32_t rev;
    /* Bumped only when `now` moved, for what is derived from the palette (the
     * theme) and must not be rebuilt on every beat. */
    uint32_t palette_rev;
} ui_ambience;

void ui_ambience_init(ui_ambience *a, const ui_palette *p);

/* Start fading to `p` (from whatever is shown now). */
void ui_ambience_target(ui_ambience *a, const ui_palette *p);

/* Advances by `dt` seconds. `activity` is the loudest channel of the analyzer
 * (0..1); `silent` is true while nothing plays, so the energy falls away
 * instead of freezing on the analyzer's last value. */
void ui_ambience_step(ui_ambience *a, double dt, float activity, bool silent,
                      const ui_ambience_params *p);

/* Energy as a fraction of the quantiser, 0..1. */
float ui_ambience_pulse(const ui_ambience *a, const ui_ambience_params *p);

#endif /* __UI_AMBIENCE_H */
