/* Dev tool: reads "lang<TAB>text" lines on stdin, prints "n w1 w2 ..." per
 * line (unit count and weights), for comparison with the Python front-end. */
#include "kara_text.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    kara_lex *lex = kara_lex_open(argc > 1 ? argv[1] : NULL);
    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin))
    {
        buf[strcspn(buf, "\r\n")] = '\0';
        char *tab = strchr(buf, '\t');
        if (tab == NULL)
            continue;
        *tab = '\0';
        kara_lang lang = KARA_LANG_EN;
        kara_lang_from_tag(buf, &lang);
        kara_units *u = kara_units_build(tab + 1, lang, lex);
        printf("%d", u->n);
        for (int i = 0; i < u->n; i++)
            printf(" %.2f", u->u[i].weight);
        printf("\n");
        kara_units_free(u);
    }
    kara_lex_close(lex);
    return 0;
}
