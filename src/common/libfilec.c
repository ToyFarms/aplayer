#include "libfile.h"

void file_free(File file)
{
    if (file.filename)
        free(file.filename);
    
    if (file.path)
        free(file.path);
}
