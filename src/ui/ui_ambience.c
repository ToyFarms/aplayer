#include "ui_ambience.h"

#include "_math.h"

#include <math.h>
#include <stddef.h>

#define FADE_STEPS   16 /* `now` moves in this many steps: a repaint each */

static float smooth(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void ui_ambience_init(ui_ambience *a, const ui_palette *p)
{
    *a = (ui_ambience){.from = *p, .to = *p, .now = *p, .fade = 1.0f,
                       .fade_shown = 1.0f};
}

void ui_ambience_target(ui_ambience *a, const ui_palette *p)
{
    a->from = a->now;
    a->to = *p;
    a->fade = 0.0f;
    a->fade_shown = 0.0f;
}

void ui_ambience_step(ui_ambience *a, double dt, float activity, bool silent,
                      const ui_ambience_params *p)
{
    float dts = (float)MATH_MAX(dt, 0.0);

    if (a->fade < 1.0f)
    {
        a->fade = p->fade_ms <= 0.0f
                      ? 1.0f
                      : MATH_MIN(a->fade + dts * 1000.0f / p->fade_ms, 1.0f);
        float shown = a->fade >= 1.0f
                          ? 1.0f
                          : floorf(a->fade * FADE_STEPS) / FADE_STEPS;
        if (shown != a->fade_shown)
        {
            a->fade_shown = shown;
            a->now = ui_palette_mix(&a->from, &a->to, smooth(shown));
            a->rev++;
            a->palette_rev++;
        }
    }

    float in = silent || isnan(activity) ? 0.0f
                                         : MATH_CLAMP(activity, 0.0f, 1.0f);
    if (in >= a->energy)
        a->energy = in; /* attack is instant, like the analyzer's own */
    else if (p->release_ms <= 0.0f)
        a->energy = in;
    else
        a->energy =
            MATH_MAX(in, a->energy * expf(-dts * 1000.0f / p->release_ms));

    a->level_age += dts;
    int want = p->levels > 0
                   ? (int)lroundf(a->energy * (float)p->levels)
                   : 0;
    bool due = p->pulse_hz <= 0.0f || a->level_age >= 1.0f / p->pulse_hz;
    /* going quiet is never held back: a card stuck big looks broken */
    if (want != a->level && (due || want == 0))
    {
        a->level = want;
        a->level_age = 0.0f;
        a->rev++;
    }
}

float ui_ambience_pulse(const ui_ambience *a, const ui_ambience_params *p)
{
    return p->levels > 0 ? (float)a->level / (float)p->levels : 0.0f;
}
