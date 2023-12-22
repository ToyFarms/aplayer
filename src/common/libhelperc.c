#include "libhelper.h"

static int prev_level;

void av_log_turn_off()
{
    prev_level = av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
}

void av_log_turn_on()
{
    av_log_set_level(prev_level);
}