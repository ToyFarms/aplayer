#ifndef _LIBFILE_H
#define _LIBFILE_H

#include <stdio.h>
#include <libavutil/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

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

void file_free(File file);
char *file_read(const char *path);
bool file_exists(const char *path);
bool file_empty(const char *path);

#ifdef AP_WINDOWS

#include <windows.h>

#elif defined(AP_MACOS)
#elif defined(LINUX)
#endif // AP_WINDOWS

File *list_directory(const char *directory, int *out_size);
bool path_compare(const char *a, const char *b);
bool path_comparew(const wchar_t *a, const wchar_t *b);
FileStat file_get_statw(const wchar_t *filename, int *success);
FileStat file_get_stat(const char *filename, int *success);
char *path_normalize(const char *path);
wchar_t *path_normalizew(const wchar_t *path);

#endif // _LIBFILE_H
