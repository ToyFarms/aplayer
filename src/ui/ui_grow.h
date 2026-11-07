#ifndef __UI_GROW_H
#define __UI_GROW_H

/*
 * Which lyric lines are big, and how big they are right now.
 *
 * One line at a time wants to be big (the one being sung). When it changes,
 * the old line does not vanish from the set: it shrinks back at the same pace
 * the new one grows, so for the length of a transition two lines carry a
 * share of the size and the page moves smoothly from one to the other. Lines
 * that finished shrinking drop out.
 *
 * Every slot holds a linear progress t in 0..1; ui_grow_get() eases it. The
 * ease is symmetric (smoothstep), so a line that is leaving at t and one that
 * is arriving at 1 - t always add up to exactly 1: the weight of the reading
 * position is conserved while it moves between the two.
 *
 * Pure state + arithmetic; the widget feeds it once per frame.
 */

#include <stdbool.h>

#define UI_GROW_MAX 6 /* lines in flight at once (a fast seek leaves several) */

typedef struct ui_grow_slot
{
    int line;
    float t; /* 0..1, linear in time */
} ui_grow_slot;

typedef struct ui_grow
{
    ui_grow_slot s[UI_GROW_MAX];
    int n;
} ui_grow;

/* Forget everything: all lines at rest, small. */
void ui_grow_reset(ui_grow *g);

/* Advances by `dt` seconds. `focus` is the line that should be big (-1 for
 * none); `dur` is the time a line takes to go from small to big, or back.
 * dur <= 0 makes every change instant. */
void ui_grow_step(ui_grow *g, int focus, float dt, float dur);

/* 0 (small) .. 1 (full size) for `line`, eased. */
float ui_grow_get(const ui_grow *g, int line);

/* Is anything still moving, given the same `focus` as the last step? */
bool ui_grow_busy(const ui_grow *g, int focus);

#endif /* __UI_GROW_H */
