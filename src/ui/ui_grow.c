#include "ui_grow.h"

#include "_math.h"

#include <string.h>

static float ease(float t)
{
    t = MATH_CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static int find(const ui_grow *g, int line)
{
    for (int i = 0; i < g->n; i++)
        if (g->s[i].line == line)
            return i;
    return -1;
}

static void drop(ui_grow *g, int i)
{
    memmove(&g->s[i], &g->s[i + 1],
            (size_t)(g->n - i - 1) * sizeof(ui_grow_slot));
    g->n--;
}

void ui_grow_reset(ui_grow *g)
{
    g->n = 0;
}

void ui_grow_step(ui_grow *g, int focus, float dt, float dur)
{
    if (focus >= 0 && find(g, focus) < 0)
    {
        if (g->n == UI_GROW_MAX)
        {
            /* a seek left more lines in flight than there is room for: the
             * one closest to small is the least missed */
            int least = 0;
            for (int i = 1; i < g->n; i++)
                if (g->s[i].t < g->s[least].t)
                    least = i;
            drop(g, least);
        }
        g->s[g->n++] = (ui_grow_slot){focus, 0.0f};
    }

    float step = dur > 0.0f ? MATH_MAX(dt, 0.0f) / dur : 1.0f;
    for (int i = 0; i < g->n;)
    {
        ui_grow_slot *s = &g->s[i];
        if (s->line == focus)
            s->t = MATH_MIN(s->t + step, 1.0f);
        else
            s->t = MATH_MAX(s->t - step, 0.0f);

        if (s->t <= 0.0f && s->line != focus)
            drop(g, i);
        else
            i++;
    }
}

float ui_grow_get(const ui_grow *g, int line)
{
    int i = find(g, line);
    return i < 0 ? 0.0f : ease(g->s[i].t);
}

bool ui_grow_busy(const ui_grow *g, int focus)
{
    for (int i = 0; i < g->n; i++)
        if (g->s[i].line != focus || g->s[i].t < 1.0f)
            return true;
    return false;
}
