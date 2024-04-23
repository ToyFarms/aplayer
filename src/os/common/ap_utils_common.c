#include "ap_utils.h"

char *file_read(const char *path, bool byte_mode, int *out_size)
{
    FILE *fd = fopen(path, byte_mode ? "rb" : "r");
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
