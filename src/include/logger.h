#ifndef __LOGGER_H
#define __LOGGER_H

#include <stdbool.h>
#include <stdio.h>

#define LOG_QUIET   0
#define LOG_WARNING 1
#define LOG_ERROR   2
#define LOG_FATAL   3
#define LOG_INFO    4
#define LOG_DEBUG   5

#define LOGGER_MAX_OUTPUT 32
#define LOG_USE_COLOR     (1 << 0)
#define LOG_DEFER_CLOSE   (1 << 1)

#ifndef LOGGER_BASEPATH_LENGTH
#    define LOGGER_BASEPATH_LENGTH 0
#endif // LOGGER_BASEPATH_LENGTH

#define __FILENAME__ (&__FILE__[LOGGER_BASEPATH_LENGTH])

#define log_warning(fmt, ...)                                                  \
    logger_log(LOG_WARNING, __FILENAME__, __FUNCTION__, __LINE__, fmt,         \
               ##__VA_ARGS__)
#define log_error(fmt, ...)                                                    \
    logger_log(LOG_ERROR, __FILENAME__, __FUNCTION__, __LINE__, fmt,           \
               ##__VA_ARGS__)
#define log_fatal(fmt, ...)                                                    \
    logger_log(LOG_FATAL, __FILENAME__, __FUNCTION__, __LINE__, fmt,           \
               ##__VA_ARGS__)
#define log_info(fmt, ...)                                                     \
    logger_log(LOG_INFO, __FILENAME__, __FUNCTION__, __LINE__, fmt,            \
               ##__VA_ARGS__)
#define log_debug(fmt, ...)                                                    \
    logger_log(LOG_DEBUG, __FILENAME__, __FUNCTION__, __LINE__, fmt,           \
               ##__VA_ARGS__)
#define log_log(level, fmt, ...)                                               \
    logger_log(level, __FILENAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

void logger_set_level(int level);
void logger_add_output(int level, FILE *fd, int flags);
void logger_use_color(bool enable);
void logger_log(int level, const char *filename, const char *fn_name, int line,
                const char *fmt, ...);
void logger_logv(int level, const char *filename, const char *fn_name, int line,
                 const char *fmt, va_list args);

#endif /* __LOGGER_H */
