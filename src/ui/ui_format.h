#ifndef __UI_FORMAT_H
#define __UI_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/* Formats a duration given in microseconds as "m:ss", "h:mm:ss" or
 * "d:hh:mm:ss", whichever is the shortest that fits. Always NUL-terminates
 * when cap > 0; output is truncated if it does not fit. */
void ui_format_time(char *out, size_t cap, uint64_t us);

#endif /* __UI_FORMAT_H */
