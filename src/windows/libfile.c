#include "libfile.h"

FileStat file_get_statw(const wchar_t *filename, int *success)
{
    struct _stat stat;
    FileStat fstat = {0};

    if (_wstat(filename, &stat) != 0)
    {
        char *filename_tmp = wchar2mbs(filename);
        if (filename_tmp)
        {
            av_log(NULL, AV_LOG_WARNING, "Could not get file stat: %s.\n", filename_tmp);
            free(filename_tmp);
        }

        if (success)
            *success = -1;
        return fstat;
    }

    fstat.st_mode = stat.st_mode;
    fstat.st_size = stat.st_size;
    fstat.st_atime = stat.st_atime;
    fstat.st_mtime = stat.st_mtime;
    fstat.st_ctime = stat.st_ctime;

    if (success)
        *success = 0;

    return fstat;
}

FileStat file_get_stat(const char *filename, int *success)
{
    struct _stat stat;
    FileStat fstat = {0};
    int ret_code = 0;

    wchar_t *filenamew = mbs2wchar(filename, 2048, NULL);
    if (!filenamew)
        goto error;

    if (_wstat(filenamew, &stat) != 0)
    {
        av_log(NULL, AV_LOG_WARNING, "Could not get file stat: %s.\n", filename);
        goto error;
    }

    fstat.st_mode = stat.st_mode;
    fstat.st_size = stat.st_size;
    fstat.st_atime = stat.st_atime;
    fstat.st_mtime = stat.st_mtime;
    fstat.st_ctime = stat.st_ctime;

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

File *list_directory(const char *directory, int *out_size)
{
    WIN32_FIND_DATAW ffd;
    HANDLE find;
    int count = 0;
    File *files = (File *)malloc(MAX_FILES * MAX_PATH * sizeof(File));

    wchar_t wide_directory[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, directory, -1, wide_directory, MAX_PATH);

    wchar_t search_path[MAX_PATH];
    wcscpy_s(search_path, MAX_PATH, wide_directory);
    wcscat_s(search_path, MAX_PATH, L"\\*");

    find = FindFirstFileW(search_path, &ffd);

    if (find == INVALID_HANDLE_VALUE)
    {
        av_log(NULL, AV_LOG_FATAL, "FindFirstFileW Failed: %ld.\n", GetLastError());
        return NULL;
    }

    do
    {
        if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0)
        {
            wchar_t filepath[2048];
            wcscpy_s(filepath, 2048, wide_directory);
            wcscat_s(filepath, 2048, L"\\");
            wcscat_s(filepath, 2048, ffd.cFileName);
            wchar_t *filepath_normalized = path_normalizew(filepath);

            files[count] = (File){
                .path = wchar2mbs(filepath_normalized),
                .filename = wchar2mbs(ffd.cFileName),
                .stat = file_get_statw(filepath, NULL),
            };

            free(filepath_normalized);
            count++;
        }
    } while (FindNextFileW(find, &ffd) != 0 && count < MAX_FILES);

    FindClose(find);

    if (out_size)
        *out_size = count;

    return files;
}

char *path_normalize(const char *path)
{
    char *normalized = (char *)malloc(2048 * sizeof(char));
    GetFullPathName(path, 2048, normalized, NULL);

    return normalized;
}

wchar_t *path_normalizew(const wchar_t *path)
{
    wchar_t *normalized = (wchar_t *)malloc(2048 * sizeof(wchar_t));
    GetFullPathNameW(path, 2048, normalized, NULL);

    return normalized;
}

bool path_compare(const char *a, const char *b)
{
    char expanded_a[2048];
    GetFullPathName(a, 2048, expanded_a, NULL);

    char expanded_b[2048];
    GetFullPathName(b, 2048, expanded_b, NULL);

    return strcmp(expanded_a, expanded_b) == 0;
}

bool path_comparew(const wchar_t *a, const wchar_t *b)
{
    wchar_t expanded_a[2048];
    GetFullPathNameW(a, 2048, expanded_a, NULL);

    wchar_t expanded_b[2048];
    GetFullPathNameW(b, 2048, expanded_b, NULL);

    return wcscmp(expanded_a, expanded_b) == 0;
}