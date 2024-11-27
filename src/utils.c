#include "utils.h"
#include "ds.h"

#include <ctype.h>
#include <string.h>

void print_raw(const char *str)
{
    str_t s = str_alloc(strlen(str));

    char c = 0;
    while ((c = *str++))
    {
        switch ((int)c)
        {
        case '\t':
            str_catlen(&s, "\\t", 2);
            break;
        case '\n':
            str_catlen(&s, "\\n", 2);
            break;
        default:
            if (isprint(c))
                str_catch(&s, c);
            else
                str_catf(&s, "\\x%02x", c);
            break;
        }
    }

    printf("%s", s.buf);
    str_free(&s);
}
