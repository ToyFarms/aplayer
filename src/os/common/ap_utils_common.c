#include "ap_utils.h"

char *file_read(const char *path, bool byte_mode, int *out_size)
{
    FILE *fd;
    fopen_s(&fd, path, byte_mode ? "rb" : "r");
    if (!fd)
        return NULL;

    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    char *buf = (char *)malloc(size + 1);
    buf[size] = '\0';

    fread(buf, 1, size, fd);

    fclose(fd);

    if (out_size)
        *out_size =  size;

    return buf;
}

bool is_ascii(const char *str, int str_len)
{
    for (int i = 0; i < (str_len < 0 ? strlen(str) : str_len); i++)
        if ((str[i] & ~0x7f) != 0)
            return false;
    return true;
}

int strw_fit_into_column(const wchar_t *strw, int strwlen, int target_column, int *column_out)
{
    int width = 0;
    int i;
    for (i = 0; i < (strwlen >= 0 ? strwlen : wcslen(strw)); i++)
    {
        wchar_t wchar = strw[i];
        int cwidth = mk_wcwidth(wchar);
        if (width + cwidth > target_column)
            break;
        width += cwidth;
    }

    if (column_out)
        *column_out = width;

    return i;
}
