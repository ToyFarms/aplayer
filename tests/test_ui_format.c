#include "base_test.h"

INCLUDE_BEGIN
#include "ui_format.h"
#include <string.h>

#define SEC(n)  ((uint64_t)(n) * 1000000ULL)
#define MIN(n)  (SEC(n) * 60ULL)
#define HOUR(n) (MIN(n) * 60ULL)
#define DAY(n)  (HOUR(n) * 24ULL)

static void check(uint64_t us, const char *want)
{
    char buf[64];
    ui_format_time(buf, sizeof(buf), us);
    ASSERT_STR_EQ(buf, want, strlen(want) + 1);
}
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 -Isrc/ui
 src/ui/ui_format.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(zero)
{
    check(0, "0:00");
}
TEST_END()

TEST_BEGIN(seconds_and_minutes)
{
    check(SEC(7), "0:07");
    check(SEC(59), "0:59");
    check(MIN(1), "1:00");
    check(MIN(12) + SEC(5), "12:05");
    check(MIN(59) + SEC(59), "59:59");
}
TEST_END()

TEST_BEGIN(sub_second_is_truncated)
{
    check(SEC(3) + 999999ULL, "0:03");
}
TEST_END()

TEST_BEGIN(hours)
{
    check(HOUR(1), "1:00:00");
    check(HOUR(2) + MIN(3) + SEC(4), "2:03:04");
    check(HOUR(23) + MIN(59) + SEC(59), "23:59:59");
}
TEST_END()

TEST_BEGIN(days)
{
    check(DAY(1), "1:00:00:00");
    check(DAY(2) + HOUR(5) + MIN(6) + SEC(7), "2:05:06:07");
}
TEST_END()

TEST_BEGIN(truncates_and_terminates)
{
    char buf[4];
    memset(buf, 'x', sizeof(buf));
    ui_format_time(buf, sizeof(buf), HOUR(1));
    ASSERT_INT_EQ(buf[3], 0); /* "1:0" + NUL, never overruns */
    ASSERT_INT_EQ((int)strlen(buf), 3);

    ui_format_time(buf, 0, SEC(1)); /* cap 0: must not touch the buffer */
    ASSERT_INT_EQ(buf[0], '1');
}
TEST_END()
