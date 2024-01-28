#include "libfile.h"

FileStat get_file_stat(wchar_t *filename)
{
    struct _stat stat;
    FileStat fstat = {0};

    if (_wstat(filename, &stat) != 0)
    {
        av_log(NULL, AV_LOG_WARNING, "Could not get file stat.\n");
        return fstat;
    }

    fstat.st_mode = stat.st_mode;
    fstat.st_size = stat.st_size;
    fstat.st_atime = stat.st_atime;
    fstat.st_mtime = stat.st_mtime;
    fstat.st_ctime = stat.st_ctime;

    return fstat;
}

File *list_directory(char *directory, int *out_size)
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
        av_log(NULL, AV_LOG_FATAL, "FindFirstFileW Failed: %d.\n", GetLastError());
        return NULL;
    }

    do
    {
        if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0)
        {
            wchar_t filepath[1024];
            wcscpy_s(filepath, 1024, wide_directory);
            wcscat_s(filepath, 1024, L"\\");
            wcscat_s(filepath, 1024, ffd.cFileName);

            files[count] = (File){
                .path = wchar2mbs(filepath),
                .filename = wchar2mbs(ffd.cFileName),
                .stat = get_file_stat(filepath),
            };
            count++;
        }
    } while (FindNextFileW(find, &ffd) != 0 && count < MAX_FILES);

    FindClose(find);

    if (out_size)
        *out_size = count;

    return files;
}

bool path_compare(char *a, char *b)
{
    char expanded_a[2048];
    GetFullPathName(a, 2048, expanded_a, NULL);

    char expanded_b[2048];
    GetFullPathName(b, 2048, expanded_b, NULL);

    return strcmp(expanded_a, expanded_b) == 0;
}

bool path_comparew(wchar_t *a, wchar_t *b)
{
    wchar_t expanded_a[2048];
    GetFullPathNameW(a, 2048, expanded_a, NULL);

    wchar_t expanded_b[2048];
    GetFullPathNameW(b, 2048, expanded_b, NULL);

    return wcscmp(expanded_a, expanded_b) == 0;
}