#ifndef _LIBFILE_H
#define _LIBFILE_H

#include <stdio.h>
#include <libavutil/log.h>

#include "libos.h"
#include "libhelper.h"

#ifdef AP_WINDOWS

#define MAX_FILES 1000

#elif defined(AP_MACOS)
#elif defined(LINUX)

#endif // AP_WINDOWS

char **list_directory(char *directory, int *out_size);

#endif // _LIBFILE_H
