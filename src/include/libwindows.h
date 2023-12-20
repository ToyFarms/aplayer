#ifndef _LIBWINDOWS_H
#define _LIBWINDOWS_H

#include <windows.h>
#include <stdio.h>
#include <libavutil/log.h>

#define MAX_FILES 1000

char *wchar2mbs(const wchar_t *wideCharStr);
char **list_directory(char *directory, int *out_size);

#endif // _LIBWINDOWS_H