#include "libhelper.h"

/* Will be leaked on exit */
static char **win32_argv_utf8 = NULL;
static int win32_argc = 0;

/**
 * Prepare command line arguments for executable.
 * For Windows - perform wide-char to UTF-8 conversion.
 * Input arguments should be main() function arguments.
 * @param argc_ptr Arguments number (including executable)
 * @param argv_ptr Arguments list.
 */
// From https://github.com/FFmpeg/FFmpeg/blob/4fee63b241e0dd254436bf38df8b0635b2b666d8/fftools/cmdutils.c#L178
void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    char *argstr_flat;
    wchar_t **argv_w;
    int i, buffsize = 0, offset = 0;

    if (win32_argv_utf8)
    {
        *argc_ptr = win32_argc;
        *argv_ptr = win32_argv_utf8;
        return;
    }

    win32_argc = 0;
    argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
    if (win32_argc <= 0 || !argv_w)
        return;

    /* determine the UTF-8 buffer size (including NULL-termination symbols) */
    for (i = 0; i < win32_argc; i++)
        buffsize += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                        NULL, 0, NULL, NULL);

    win32_argv_utf8 = av_mallocz(sizeof(char *) * (win32_argc + 1) + buffsize);
    argstr_flat = (char *)win32_argv_utf8 + sizeof(char *) * (win32_argc + 1);
    if (!win32_argv_utf8)
    {
        LocalFree(argv_w);
        return;
    }

    for (i = 0; i < win32_argc; i++)
    {
        win32_argv_utf8[i] = &argstr_flat[offset];
        offset += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                      &argstr_flat[offset],
                                      buffsize - offset, NULL, NULL);
    }
    win32_argv_utf8[i] = NULL;
    LocalFree(argv_w);

    *argc_ptr = win32_argc;
    *argv_ptr = win32_argv_utf8;
}

static char *get_last_error_string()
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

char *wchar2mbs(const wchar_t *strw)
{
    if (!strw)
        return NULL;

    int bufsize = WideCharToMultiByte(CP_UTF8, 0, &strw[0], -1, NULL, 0, NULL, NULL);
    if (bufsize == 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Error converting wchar. %s.\n", get_last_error_string());
        return NULL;
    }

    char *char_utf8 = (char *)malloc(bufsize * sizeof(char));
    if (!char_utf8)
    {
        av_log(NULL, AV_LOG_FATAL, "wchar2mbs: Could not allocate memory.\n");
        return NULL;
    }

    bufsize = WideCharToMultiByte(CP_UTF8, 0, &strw[0], -1, &char_utf8[0], bufsize, NULL, NULL);
    if (bufsize == 0)
    {
        av_log(NULL, AV_LOG_FATAL, "Error converting wchar. %s.\n", get_last_error_string());
        free(char_utf8);
        return NULL;
    }

    return char_utf8;
}

wchar_t *mbs2wchar(const char *str, size_t wsize, int *strwlen_out)
{
    if (!str)
        return NULL;

    wchar_t *strw = (wchar_t *)malloc(wsize * sizeof(wchar_t));
    if (!strw)
    {
        av_log(NULL, AV_LOG_FATAL, "mbs2wchar: Could not allocate memory.\n");
        return NULL;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, strw, wsize);
    if (strwlen_out)
        *strwlen_out = len;

    return strw;
}