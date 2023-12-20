#ifndef _LIBFILE_H
#define _LIBFILE_H

#include "libos.h"

#if defined(AP_WINDOWS)

#include <windows.h>
#include <stdio.h>
#include <libavutil/log.h>

#define MAX_FILES 1000

#elif defined(AP_MACOS)
#elif defined(LINUX)
#endif

char **list_directory(char *directory, int *out_size);

#endif // _LIBFILE_H
