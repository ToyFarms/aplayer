#include "libfile.h"

void file_free(File file)
{
    if (file.filename)
        free(file.filename);
    
    if (file.path)
        free(file.path);
}

char *file_read(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = (char *)malloc(fsize + 1);
    fread(string, fsize, 1, f);
    string[fsize] = '\0';

    return string;
}

bool file_exists(const char *path)
{
    int f = open(path, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
    bool exists = false;
    if (f < 0)
        exists = errno == EEXIST;
    else
        exists = false;
    
    return exists;
}

bool file_empty(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return true;

    fseek(f, 0, SEEK_END);
    bool empty = ftell(f) == 0;
    fclose(f);

    return empty;
}