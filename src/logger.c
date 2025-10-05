#include "logger.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *log_level_str[] = {
    "QUIET", "WARNING", "ERROR", "FATAL", "INFO", "DEBUG",
};

static const char *log_level_col[] = {
    // None, Yellow,  Red,          Magenta,      Green,        Cyan
    "", "\x1b[1;33m", "\x1b[1;31m", "\x1b[1;35m", "\x1b[1;32m", "\x1b[1;34m"};

typedef struct logger_output
{
    int level;
    FILE *fd;
    int flags;
} logger_output;

typedef struct logger_callback
{
    int level;
    logger_cb_fun callback;
    int flags;
} logger_callback;

typedef struct logger_ctx
{
    int level;
    logger_output out[LOGGER_MAX_OUTPUT];
    int nb_outputs;
    logger_callback cb[LOGGER_MAX_CALLBACK];
    int nb_callbacks;
    bool use_color;
} logger_ctx;

static logger_ctx g_ctx = {
    .level = LOG_INFO,
    .nb_outputs = 0,
    .use_color = true,
};

void logger_set_level(int level)
{
    g_ctx.level = level;
}

static bool atexit_registered = false;

static void close_output()
{
    for (int i = 0; i < g_ctx.nb_outputs; i++)
    {
        if (g_ctx.out[i].flags & LOG_DEFER_CLOSE && g_ctx.out[i].fd != NULL)
            fclose(g_ctx.out[i].fd);
    }
}

void logger_add_output(int level, FILE *fd, int flags)
{
    assert(fd);
    if (g_ctx.nb_outputs >= LOGGER_MAX_OUTPUT)
        return;

    g_ctx.out[g_ctx.nb_outputs++] = (logger_output){
        .level = level >= 0 ? level : g_ctx.level, .fd = fd, .flags = flags};

    if (!atexit_registered)
    {
        atexit(close_output);
        atexit_registered = true;
    }
}

void logger_add_callback(int level, logger_cb_fun callback, int flags)
{
    assert(callback);

    if (g_ctx.nb_callbacks >= LOGGER_MAX_CALLBACK)
        return;

    g_ctx.cb[g_ctx.nb_callbacks++] =
        (logger_callback){.level = level >= 0 ? level : g_ctx.level,
                          .callback = callback,
                          .flags = flags};
}

void logger_remove_callback(logger_cb_fun callback)
{
    int index = -1;
    for (int i = 0; i < g_ctx.nb_callbacks; i++)
    {
        if (g_ctx.cb[i].callback == callback)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
        return;

    int to_move = g_ctx.nb_callbacks - index - 1;
    if (to_move > 0)
        memmove(&g_ctx.cb[index], &g_ctx.cb[index + 1],
                to_move * sizeof(g_ctx.cb[0]));

    g_ctx.nb_callbacks--;
}

void logger_use_color(bool enable)
{
    g_ctx.use_color = enable;
}

void logger_log(int level, const char *filename, const char *fn_name, int line,
                const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logger_logv(level, filename, fn_name, line, fmt, args);
    va_end(args);
}

// void logger_logv(int level, const char *filename, const char *fn_name, int
// line,
//                  const char *fmt, va_list args)
// {
//     if (level > LOG_DEBUG)
//         level = LOG_DEBUG;
//
//     if (level <= LOG_QUIET || level > g_ctx.level)
//         return;
//
//     time_t t = time(NULL);
//     char timestr[64] = {0};
//     strftime(timestr, sizeof(timestr), "%F %T", localtime(&t));
//
//     va_list args_copy;
//     for (int i = 0; i < g_ctx.nb_outputs; i++)
//     {
//         logger_output out = g_ctx.out[i];
//         if (level > out.level)
//             continue;
//
//         va_copy(args_copy, args);
//
//         bool use_color = g_ctx.use_color && (out.flags & LOG_USE_COLOR);
//
//         fprintf(out.fd, "%s %s%-7s%s [%s] %s:%d: ", timestr,
//                 use_color ? log_level_col[level] : "", log_level_str[level],
//                 use_color ? "\x1b[0m" : "", fn_name, filename, line);
//         vfprintf(out.fd, fmt, args_copy);
//         fflush(out.fd);
//
//         va_end(args_copy);
//     }
//
//     for (int i = 0; i < g_ctx.nb_callbacks; i++)
//     {
//         logger_callback cb = g_ctx.cb[i];
//         if (level > cb.level)
//             continue;
//
//         va_copy(args_copy, args);
//
//         bool use_color = g_ctx.use_color && (cb.flags & LOG_USE_COLOR);
//
//         char prefix[1024];
//         int prefix_size = snprintf(
//             prefix, 1024, "%s %s%-7s%s [%s] %s:%d: ", timestr,
//             use_color ? log_level_col[level] : "", log_level_str[level],
//             use_color ? "\x1b[0m" : "", fn_name, filename, line);
//
//         int needed = vsnprintf(NULL, 0, fmt, args_copy) + prefix_size;
//         char log[needed + 1];
//         memset(log, 0, needed + 1);
//
//         int log_size = vsnprintf(log + prefix_size, needed + 1, fmt,
//         args_copy); memcpy(log + prefix_size, log, log_size);
//
//         cb.callback(log);
//
//         va_end(args_copy);
//     }
// }

void logger_logv(int level, const char *filename, const char *fn_name, int line,
                 const char *fmt, va_list args)
{
    if (level > LOG_DEBUG)
        level = LOG_DEBUG;

    if (level <= LOG_QUIET || level > g_ctx.level)
        return;

    time_t t = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%F %T", localtime(&t));

    char prefix[1024];
    bool use_color_global = g_ctx.use_color;
    const char *lvlcol = log_level_col[level];
    const char *lvlstr = log_level_str[level];
    int prefix_size =
        snprintf(prefix, sizeof(prefix), "%s %s%-7s%s [%s] %s:%d: ", timestr,
                 use_color_global ? lvlcol : "", lvlstr,
                 use_color_global ? "\x1b[0m" : "", fn_name, filename, line);
    if (prefix_size < 0 || prefix_size >= (int)sizeof(prefix))
    {
        prefix_size =
            snprintf(prefix, sizeof(prefix), "%s [%s]: ", timestr, lvlstr);
    }

    for (int i = 0; i < g_ctx.nb_outputs; i++)
    {
        logger_output out = g_ctx.out[i];
        if (level > out.level)
            continue;

        va_list args_copy;
        va_copy(args_copy, args);

        fprintf(out.fd, "%.*s", prefix_size, prefix);
        vfprintf(out.fd, fmt, args_copy);
        if (fmt[strlen(fmt) - 1] != '\n')
            fputc('\n', out.fd);
        fflush(out.fd);

        va_end(args_copy);
    }

    for (int i = 0; i < g_ctx.nb_callbacks; i++)
    {
        logger_callback cb = g_ctx.cb[i];
        if (level > cb.level)
            continue;

        va_list args_sz;
        va_copy(args_sz, args);
        int msg_len = vsnprintf(NULL, 0, fmt, args_sz);
        va_end(args_sz);

        if (msg_len < 0)
            continue;

        size_t total = (size_t)prefix_size + (size_t)msg_len + 1;
        char *buf = malloc(total);
        if (!buf)
            continue;

        memcpy(buf, prefix, (size_t)prefix_size);

        va_list args_fmt;
        va_copy(args_fmt, args);
        vsnprintf(buf + prefix_size, (size_t)msg_len + 1, fmt, args_fmt);
        va_end(args_fmt);

        cb.callback(buf);

        free(buf);
    }
}
