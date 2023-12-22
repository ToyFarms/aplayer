#include "libfile.h"

char **list_directory(char *directory, int *out_size)
{
    WIN32_FIND_DATAW ffd;
    HANDLE find;
    int count = 0;
    char **filenames = (char **)malloc(MAX_FILES * MAX_PATH * sizeof(char));

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

            filenames[count] = wchar2mbs(filepath);
            count++;
        }
    } while (FindNextFileW(find, &ffd) != 0 && count < MAX_FILES);

    FindClose(find);

    if (out_size)
        *out_size = count;

    return filenames;
}