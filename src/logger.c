#include "logger.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static const char *log_level_str[] = {
    "QUIET", "WARNING", "ERROR", "FATAL", "INFO", "DEBUG",
};

static const char *log_level_col[] = {
    // None, Yellow, Red, Magenta, Green, Cyan
    "", "\x1b[1;33m", "\x1b[1;31m", "\x1b[1;35m", "\x1b[1;32m", "\x1b[1;34m"};

typedef struct logger_output
{
    int level;
    FILE *fd;
    int flags;
} logger_output;

typedef struct logger_ctx
{
    int level;
    logger_output out[LOGGER_MAX_OUTPUT];
    int nb_outputs;
    bool use_color;
} logger_ctx;

static logger_ctx gctx = {
    .level = LOG_INFO,
    .nb_outputs = 0,
    .use_color = true,
};

void logger_set_level(int level)
{
    gctx.level = level;
}

static bool atexit_registered = false;

static void close_output()
{
    for (int i = 0; i < gctx.nb_outputs; i++)
    {
        if (gctx.out[i].flags & LOG_DEFER_CLOSE && gctx.out[i].fd != NULL)
            fclose(gctx.out[i].fd);
    }
}

void logger_add_output(int level, FILE *fd, int flags)
{
    assert(fd);
    if (gctx.nb_outputs >= LOGGER_MAX_OUTPUT)
        return;

    gctx.out[gctx.nb_outputs++] =
        (logger_output){.level = level >= 0 ? level : gctx.level, .fd = fd, .flags = flags};

    if (!atexit_registered)
    {
        atexit(close_output);
        atexit_registered = true;
    }
}

void logger_use_color(bool enable)
{
    gctx.use_color = enable;
}

void logger_log(int level, const char *filename, const char *fn_name, int line,
                const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logger_logv(level, filename, fn_name, line, fmt, args);
    va_end(args);
}

void logger_logv(int level, const char *filename, const char *fn_name, int line,
                 const char *fmt, va_list args)
{
    if (level > LOG_DEBUG)
        level = LOG_DEBUG;

    if (level <= LOG_QUIET || level > gctx.level)
        return;

    time_t t = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%F %T", localtime(&t));

    va_list args_copy;
    for (int i = 0; i < gctx.nb_outputs; i++)
    {
        logger_output out = gctx.out[i];
        if (level > out.level)
            continue;

        va_copy(args_copy, args);

        bool use_color = gctx.use_color && (out.flags & LOG_USE_COLOR);

        fprintf(out.fd, "%s %s%-7s%s [%s] %s:%d: ", timestr,
                use_color ? log_level_col[level] : "", log_level_str[level],
                use_color ? "\x1b[0m" : "", fn_name, filename, line);
        vfprintf(out.fd, fmt, args_copy);
        fflush(out.fd);

        va_end(args_copy);
    }
}
