#ifndef __UTILS_H
#define __UTILS_H

#include "app.h"
#include <stdint.h>

typedef struct stream_mute_ctx
{
    int saved_fd;
    FILE *stream;
} stream_mute_ctx;

#define WITH_MUTED_STREAM(stream)                                              \
    for (stream_mute_ctx __mute_handle = mute_stream(stream);                  \
         __mute_handle.saved_fd >= 0;                                          \
         restore_stream(__mute_handle), __mute_handle.saved_fd = -1)

stream_mute_ctx mute_stream(FILE *stream);
void restore_stream(stream_mute_ctx h);

void print_raw(const char *str);
void play_next(app_instance *app);
void play_prev(app_instance *app);
void play_at_index(app_instance *app, int index);

#endif /* __UTILS_H */
