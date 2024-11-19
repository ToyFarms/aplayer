#include "utils.h"
#include "ds.h"

#include <ctype.h>
#include <string.h>

void print_raw(const char *str)
{
    string_t s = string_alloc(strlen(str));

    char c = 0;
    while ((c = *str++))
    {
        switch ((int)c)
        {
        case '\t':
            string_catlen(&s, "\\t", 2);
            break;
        case '\n':
            string_catlen(&s, "\\n", 2);
            break;
        default:
            if (isprint(c))
                string_catch(&s, c);
            else
                string_catf(&s, "\\x%02x", c);
            break;
        }
    }

    printf("%s", s.buf);
    string_free(&s);
}
