#ifndef __AP_UTILS_H
#define __AP_UTILS_H

#include "ap_os.h"
#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include "ap_array.h"
#include "wcwidth.h"

#define ARRLEN(arr)     (sizeof((arr)) / sizeof((arr)[0]))
#define NOT_IMPLEMENTED assert(0 && "Not Implemented")
#define PTR_PTR_CHECK(ptr, ret)                                                \
    {                                                                          \
        if (!(ptr))                                                            \
            return ret;                                                        \
        if (!(*(ptr)))                                                         \
            return ret;                                                        \
    }
#define MSTOUS(ms) ((ms) * 1000)
#define USTOMS(us) ((us) / 1000)

#if OS_TYPE == OS_WINDOWS
#    include <windows.h>
#endif

typedef struct APFileStat
{
    uint16_t st_mode;
    long st_size;
    int64_t st_atime;
    int64_t st_mtime;
    int64_t st_ctime;
} APFileStat;

typedef struct APFile
{
    char *directory;
    char *filename;
    APFileStat stat;
    wchar_t *filenamew;
} APFile;

// not a limitation, just an arbitrary cap
#define MAX_PATH_LEN 2048

char *wchartombs(const wchar_t *strw, int strwlen, int *strlen_out);
wchar_t *mbstowchar(const char *str, int strlen, int *strwlen_out);
char *file_read(const char *path, bool byte_mode, int *out_size);
APArrayT(APFile) *read_directory(const char *dir);
char *path_normalize(const char *path, int *len_out);
wchar_t *path_normalizew(const wchar_t *path, int *len_out);
bool path_compare(const char *a, const char *b);
bool path_comparew(const wchar_t *a, const wchar_t *b);
APFileStat file_get_stat(const char *filename, int *success);
APFileStat file_get_statw(const wchar_t *filename, int *success);
bool is_path_file(const char *path);
bool is_ascii(const char *str, int str_len);
int strw_fit_into_column(const wchar_t *strw, int strwlen, int target_column, int *column_out);

#endif /* __AP_UTILS_H */
