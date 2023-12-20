#include "libwindows.h"

char *get_last_error_string()
{
    DWORD error_code = GetLastError();
    if (error_code == 0)
        return "Success";

    LPVOID msg_buf;

    size_t size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL,
                                error_code,
                                MAKELANGID(LANG_NEUTRAL,
                                           SUBLANG_DEFAULT),
                                (LPSTR)&msg_buf,
                                0,
                                NULL);

    return (char *)msg_buf;
}

char *wchar2mbs(const wchar_t *wchar_str)
{
    if (!wchar_str)
        return NULL;

    int bufsize = WideCharToMultiByte(CP_UTF8, 0, &wchar_str[0], -1, NULL, 0, NULL, NULL);
    if (bufsize == 0)
    {
        wprintf(L"\x1b[38;2;255;0;0mError converting wide char \"%ls\": ", wchar_str);
        av_log(NULL, AV_LOG_FATAL, "%s.\n", get_last_error_string());
        return NULL;
    }

    char *char_utf8 = (char *)malloc(bufsize * sizeof(char));
    if (!char_utf8)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate memory");
        return NULL;
    }

    bufsize = WideCharToMultiByte(CP_UTF8, 0, &wchar_str[0], -1, &char_utf8[0], bufsize, NULL, NULL);
    if (bufsize == 0)
    {
        wprintf(L"\x1b[38;2;255;0;0mError converting wide char \"%ls\": ", wchar_str);
        av_log(NULL, AV_LOG_FATAL, "%s.\n", get_last_error_string());
        free(char_utf8);
        return NULL;
    }

    return char_utf8;
}

char **list_directory(char *directory, int *out_size)
{
    WIN32_FIND_DATAW ffd;
    HANDLE find;
    int count = 0;
    char **filenames = (char **)malloc(MAX_FILES * sizeof(char *));

    wchar_t wide_directory[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, directory, -1, wide_directory, MAX_PATH);

    wchar_t search_path[MAX_PATH];
    wcscpy_s(search_path, MAX_PATH, wide_directory);
    wcscat_s(search_path, MAX_PATH, L"\\*");

    find = FindFirstFileW(search_path, &ffd);

    if (find == INVALID_HANDLE_VALUE)
    {
        wprintf(L"\x1b[38;2;255;0;0m\"%ls\": ", search_path);
        av_log(NULL, AV_LOG_FATAL, "%s.\n", get_last_error_string());
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