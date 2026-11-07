#include "base_test.h"

INCLUDE_BEGIN
#include "ui_ambience.h"
#include <math.h>

static ui_palette red(void)
{
    return ui_palette_from_accent(COLOR(230, 60, 60), COLOR(230, 150, 60));
}

static ui_palette blue(void)
{
    return ui_palette_from_accent(COLOR(60, 90, 230), COLOR(60, 200, 220));
}

static ui_ambience_params params(void)
{
    return (ui_ambience_params){.levels = 10, .pulse_hz = 0.0f,
                                .release_ms = 200.0f, .fade_ms = 1000.0f,
                                .depth = 1.0f};
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_ambience.c
 src/ui/ui_palette.c
 src/term/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(energy_attack_is_instant_and_release_is_gradual)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &p);

    ui_ambience_step(&a, 0.016, 1.0f, false, &pr);
    ASSERT_FLOAT_WITHIN(a.energy, 1.0f, 0.001f);
    ASSERT_INT_EQ(a.level, 10);

    ui_ambience_step(&a, 0.1, 0.0f, false, &pr); /* half a release time */
    ASSERT_TRUE(a.energy > 0.5f && a.energy < 0.7f);

    for (int i = 0; i < 100; i++)
        ui_ambience_step(&a, 0.05, 0.0f, false, &pr);
    ASSERT_INT_EQ(a.level, 0);
}
TEST_END()

TEST_BEGIN(silence_overrides_a_frozen_analyzer)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &p);

    /* paused: the analyzer keeps reporting its last value */
    for (int i = 0; i < 200; i++)
        ui_ambience_step(&a, 0.05, 0.9f, true, &pr);
    ASSERT_INT_EQ(a.level, 0);
}
TEST_END()

TEST_BEGIN(level_changes_are_rate_limited_but_silence_is_not)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    pr.pulse_hz = 10.0f; /* one change per 100 ms */
    ui_ambience_init(&a, &p);

    ui_ambience_step(&a, 0.2, 1.0f, false, &pr);
    ASSERT_INT_EQ(a.level, 10);
    ui_ambience_step(&a, 0.016, 0.4f, false, &pr); /* too soon */
    uint32_t rev = a.rev;
    ui_ambience_step(&a, 0.016, 1.0f, false, &pr);
    ASSERT_INT_EQ((int)a.rev, (int)rev); /* nothing visible changed */

    pr.release_ms = 0.0f;
    ui_ambience_step(&a, 0.001, 0.0f, false, &pr); /* silence: immediate */
    ASSERT_INT_EQ(a.level, 0);
}
TEST_END()

TEST_BEGIN(rev_is_stable_while_nothing_changes)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &p);

    ui_ambience_step(&a, 0.016, 0.0f, false, &pr);
    uint32_t rev = a.rev;
    for (int i = 0; i < 50; i++)
        ui_ambience_step(&a, 0.016, 0.0f, false, &pr);
    ASSERT_INT_EQ((int)a.rev, (int)rev);
}
TEST_END()

TEST_BEGIN(pulse_off_never_leaves_level_zero)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    pr.levels = 0;
    ui_ambience_init(&a, &p);

    ui_ambience_step(&a, 0.016, 1.0f, false, &pr);
    ASSERT_INT_EQ(a.level, 0);
    ASSERT_FLOAT_WITHIN(ui_ambience_pulse(&a, &pr), 0.0f, 0.0001f);
}
TEST_END()

TEST_BEGIN(bad_activity_values_are_ignored)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &p);

    ui_ambience_step(&a, 0.016, NAN, false, &pr);
    ASSERT_FLOAT_WITHIN(a.energy, 0.0f, 0.0001f);
    ui_ambience_step(&a, 0.016, 7.0f, false, &pr);
    ASSERT_FLOAT_WITHIN(a.energy, 1.0f, 0.0001f);
}
TEST_END()

TEST_BEGIN(palette_fades_between_covers)
{
    ui_palette r = red(), b = blue();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &r);
    ui_ambience_target(&a, &b);

    ui_ambience_step(&a, 0.5, 0.0f, false, &pr); /* halfway */
    ASSERT_TRUE(a.now.accent.r < r.accent.r && a.now.accent.r > b.accent.r);
    ASSERT_TRUE(a.fade > 0.4f && a.fade < 0.6f);

    ui_ambience_step(&a, 1.0f, 0.0f, false, &pr);
    ASSERT_MEM_EQ(&a.now, &b, sizeof(ui_palette));

    /* a retarget mid-way starts from what is on screen, not from `to` */
    ui_ambience_target(&a, &r);
    ASSERT_MEM_EQ(&a.from, &b, sizeof(ui_palette));
}
TEST_END()

TEST_BEGIN(instant_fade_when_disabled)
{
    ui_palette r = red(), b = blue();
    ui_ambience a;
    ui_ambience_params pr = params();
    pr.fade_ms = 0.0f;
    ui_ambience_init(&a, &r);
    ui_ambience_target(&a, &b);
    ui_ambience_step(&a, 0.016, 0.0f, false, &pr);
    ASSERT_MEM_EQ(&a.now, &b, sizeof(ui_palette));
}
TEST_END()

TEST_BEGIN(the_page_does_not_move_with_the_music)
{
    ui_palette p = red();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &p);

    uint32_t palette_rev = a.palette_rev;
    ui_ambience_step(&a, 0.016, 1.0f, false, &pr); /* a full-scale peak */
    ASSERT_INT_EQ(a.level, 10);
    ASSERT_TRUE(a.rev != 0); /* what shows the energy repaints ... */
    ASSERT_INT_EQ((int)a.palette_rev, (int)palette_rev); /* ... the page not */
    ASSERT_MEM_EQ(&a.now, &p, sizeof(ui_palette));

    for (int i = 0; i < 100; i++)
        ui_ambience_step(&a, 0.05, 0.0f, false, &pr);
    ASSERT_INT_EQ((int)a.palette_rev, (int)palette_rev);
    ASSERT_MEM_EQ(&a.now, &p, sizeof(ui_palette));
}
TEST_END()

TEST_BEGIN(palette_rev_moves_only_with_the_fade)
{
    ui_palette r = red(), b = blue();
    ui_ambience a;
    ui_ambience_params pr = params();
    ui_ambience_init(&a, &r);
    ui_ambience_target(&a, &b);

    uint32_t before = a.palette_rev;
    ui_ambience_step(&a, 0.5, 0.0f, false, &pr);
    ASSERT_TRUE(a.palette_rev != before);

    ui_ambience_step(&a, 1.0, 0.0f, false, &pr); /* the fade ends */
    uint32_t done = a.palette_rev;
    ui_ambience_step(&a, 0.016, 1.0f, false, &pr); /* loud, but no fade */
    ASSERT_INT_EQ((int)a.palette_rev, (int)done);
}
TEST_END()

TEST_BEGIN(fade_repaints_in_a_few_steps_not_every_frame)
{
    ui_palette r = red(), b = blue();
    ui_ambience a;
    ui_ambience_params pr = params();
    pr.levels = 0; /* only the fade may change anything */
    ui_ambience_init(&a, &r);
    ui_ambience_target(&a, &b);

    uint32_t before = a.rev;
    for (int i = 0; i < 61; i++) /* a second at 60 fps, plus rounding */
        ui_ambience_step(&a, 1.0 / 60.0, 0.0f, false, &pr);
    uint32_t changes = a.rev - before;
    ASSERT_TRUE(changes >= 8 && changes <= 20);
    ASSERT_MEM_EQ(&a.now, &b, sizeof(ui_palette));
}
TEST_END()
