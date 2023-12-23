#include "libfile.h"

FileStat get_file_stat(wchar_t *filename)
{
    struct _stat stat;
    _wstat(filename, &stat);

    return (FileStat){
        .st_mode = stat.st_mode,
        .st_size = stat.st_size,
        .st_atime = stat.st_atime,
        .st_mtime = stat.st_mtime,
        .st_ctime = stat.st_ctime,
    };
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
            wchar_t filepath[MAX_PATH];
            wcscpy_s(filepath, MAX_PATH, wide_directory);
            wcscat_s(filepath, MAX_PATH, L"\\");
            wcscat_s(filepath, MAX_PATH, ffd.cFileName);

            files[count] = (File){
                .path = wchar2mbs(filepath),
                .filename = wchar2mbs(ffd.cFileName),
                .stat = get_file_stat(ffd.cFileName),
            };
            count++;
        }
    } while (FindNextFileW(find, &ffd) != 0 && count < MAX_FILES);

    FindClose(find);

    if (out_size)
        *out_size = count;

    return files;
}