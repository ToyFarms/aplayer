#ifndef _LIBFILE_H
#define _LIBFILE_H

#include <stdio.h>
#include <libavutil/log.h>
#include <sys/stat.h>

#include "libos.h"
#include "libhelper.h"

#define MAX_FILES 1000

typedef struct FileStat
{
    uint16_t st_mode;
    long st_size;
    int64_t st_atime;
    int64_t st_mtime;
    int64_t st_ctime;
} FileStat;

typedef struct File
{
    char *path;
    char *filename;
    FileStat stat;
} File;

#ifdef AP_WINDOWS

#include <windows.h>

#elif defined(AP_MACOS)
#elif defined(LINUX)
#endif // AP_WINDOWS

File *list_directory(char *directory, int *out_size);

#endif // _LIBFILE_H
