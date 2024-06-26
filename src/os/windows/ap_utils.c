#include "ap_utils.h"

#include <stdlib.h>
#include <sys/stat.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char *wchartombs(const wchar_t *strw, int strwlen, int *strlen_out)
{
    if (!strw)
        return NULL;

    int strlen =
        WideCharToMultiByte(CP_UTF8, 0, strw, strwlen, NULL, 0, NULL, NULL);
    if (strlen == 0)
        return NULL;

    char *str = (char *)malloc((strlen + 1) * sizeof(char));
    if (!str)
        return NULL;

    strlen =
        WideCharToMultiByte(CP_UTF8, 0, strw, strwlen, str, strlen, NULL, NULL);
    if (strlen == 0)
    {
        free(str);
        return NULL;
    }
    str[strlen] = '\0';

    if (strlen_out)
        *strlen_out = strlen;

    return str;
}

wchar_t *mbstowchar(const char *str, int strlen, int *strwlen_out)
{
    if (!str)
        return NULL;

    int strwlen = MultiByteToWideChar(CP_UTF8, 0, str, strlen, NULL, 0);
    if (strwlen == 0)
        return NULL;

    wchar_t *strw = (wchar_t *)malloc((strwlen + 1) * sizeof(wchar_t));
    if (!strw)
        return NULL;

    int len = MultiByteToWideChar(CP_UTF8, 0, str, strlen, strw, strwlen);
    if (len == 0)
    {
        free(strw);
        return NULL;
    }
    strw[strwlen] = L'\0';

    if (strwlen_out)
        *strwlen_out = len;

    return strw;
}

APArrayT(APFile) * read_directory(const char *dir)
{
    if (!dir || !strlen(dir))
        return NULL;

    WIN32_FIND_DATAW ffd;
    HANDLE find = NULL;
    APArray *files = NULL;
    wchar_t *dirw = NULL;
    char *dir_copy = NULL;

    int dirw_len;
    dirw = mbstowchar(dir, -1, &dirw_len);

    wchar_t search_path[dirw_len + 10];
    if (!dirw)
        goto cleanup;

    wcscpy_s(search_path, dirw_len + 10, dirw);
    wcscat_s(search_path, dirw_len + 10, L"\\*");

    find = FindFirstFileW(search_path, &ffd);
    if (find == INVALID_HANDLE_VALUE)
        goto cleanup;

    dir_copy = strdup(dir);
    if (!dir_copy)
        goto error;

    files = ap_array_alloc(256, sizeof(APFile));
    if (!files)
        goto cleanup;

    do
    {
        if (wcscmp(ffd.cFileName, L".") != 0 &&
            wcscmp(ffd.cFileName, L"..") != 0)
        {
            char *normalized_mbs = wchartombs(ffd.cFileName, -1, NULL);
            if (!normalized_mbs)
                continue;

            int fullpath_len = dirw_len + MAX_PATH;
            wchar_t fullpath[fullpath_len];
            memcpy_s(fullpath, fullpath_len, dirw, dirw_len * sizeof(wchar_t));
            wcscat_s(fullpath, fullpath_len, L"\\");
            wcscat_s(fullpath, fullpath_len, ffd.cFileName);

            ap_array_append_resize(
                files,
                &(APFile){.directory = dir_copy,
                          .filename = normalized_mbs,
                          .filenamew = wcsdup(ffd.cFileName),
                          .stat = file_get_statw(fullpath, NULL)},
                1);
        }
    } while (FindNextFileW(find, &ffd) != 0);

    goto cleanup;

error:
    if (dir_copy)
        free(dir_copy);
    ap_array_free(&files);

cleanup:
    FindClose(find);
    if (dirw)
        free(dirw);

    return files;
}

char *path_normalize(const char *path, int *len_out)
{
    char *normalized = (char *)malloc(MAX_PATH_LEN * sizeof(char));
    int len = GetFullPathName(path, MAX_PATH_LEN, normalized, NULL);
    if (len_out)
        *len_out = len;

    return normalized;
}

wchar_t *path_normalizew(const wchar_t *path, int *len_out)
{
    wchar_t *normalized = (wchar_t *)malloc(MAX_PATH_LEN * sizeof(wchar_t));
    int len = GetFullPathNameW(path, MAX_PATH_LEN, normalized, NULL);
    if (len_out)
        *len_out = len;

    return normalized;
}

bool path_compare(const char *a, const char *b)
{
    char expanded_a[MAX_PATH_LEN];
    GetFullPathName(a, MAX_PATH_LEN, expanded_a, NULL);

    char expanded_b[MAX_PATH_LEN];
    GetFullPathName(b, MAX_PATH_LEN, expanded_b, NULL);

    return strcmp(expanded_a, expanded_b) == 0;
}

bool path_comparew(const wchar_t *a, const wchar_t *b)
{
    wchar_t expanded_a[MAX_PATH_LEN];
    GetFullPathNameW(a, MAX_PATH_LEN, expanded_a, NULL);

    wchar_t expanded_b[MAX_PATH_LEN];
    GetFullPathNameW(b, MAX_PATH_LEN, expanded_b, NULL);

    return wcscmp(expanded_a, expanded_b) == 0;
}

static void ap_filestat_fill(APFileStat *dst, struct _stat src)
{
    dst->st_mode = src.st_mode;
    dst->st_size = src.st_size;
    dst->st_atime = src.st_atime;
    dst->st_mtime = src.st_mtime;
    dst->st_ctime = src.st_ctime;
}

APFileStat file_get_stat(const char *filename, int *success)
{
    struct _stat stat;
    APFileStat fstat = {0};
    int ret_code = 0;

    wchar_t *filenamew = mbstowchar(filename, -1, NULL);
    if (!filenamew)
        goto error;

    if (_wstat(filenamew, &stat) != 0)
        goto error;

    ap_filestat_fill(&fstat, stat);

    goto cleanup;

error:
    ret_code = -1;
cleanup:
    if (filenamew)
        free(filenamew);
    if (success)
        *success = ret_code;

    return fstat;
}

APFileStat file_get_statw(const wchar_t *filename, int *success)
{
    struct _stat stat;
    APFileStat fstat = {0};

    if (_wstat(filename, &stat) != 0)
    {
        if (success)
            *success = -1;
        return fstat;
    }

    ap_filestat_fill(&fstat, stat);

    if (success)
        *success = 0;

    return fstat;
}

bool is_path_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return st.st_mode & S_IFREG;
}
