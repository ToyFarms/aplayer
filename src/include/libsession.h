#ifndef _LIBSESSION_H 
#define _LIBSESSION_H 

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sb.h"
#include "libhelper.h"
#include "cJSON.h"

typedef struct SessionState
{
    int entry_size;
    File *entries;
    int playing;
    bool paused;
    float volume;
    int64_t timestamp;
} SessionState;

int session_write(const char *path, SessionState st);
SessionState session_read(const char *path, int *success);

#endif /* _LIBSESSION_H */