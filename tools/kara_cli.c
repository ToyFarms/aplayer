#define _POSIX_C_SOURCE 200809L
/* Dev tool: runs the karaoke job on a media file and prints the result.
 *
 *   kara_cli <media> [lang] < lines.tsv       lines.tsv: start_ms<TAB>text
 *
 * Prints one line per lyric line: its index, then every unit as
 * start_ms/end_ms. Timing of the phases goes to stderr. */
#include "karaoke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    kara_source_line *lines = NULL;
    int n = 0, cap = 0;
    char buf[8192];
    while (fgets(buf, sizeof(buf), stdin))
    {
        buf[strcspn(buf, "\r\n")] = '\0';
        char *tab = strchr(buf, '\t');
        if (tab == NULL)
            continue;
        *tab = '\0';
        if (n == cap)
        {
            cap = cap ? cap * 2 : 64;
            lines = realloc(lines, (size_t)cap * sizeof(*lines));
        }
        lines[n].start_ms = atoi(buf);
        lines[n].text = strdup(tab + 1);
        n++;
    }

    double t0 = now();
    karaoke *k = kara_create(argv[1], lines, n, argc > 2 ? argv[2] : NULL);
    if (k == NULL)
        return 1;
    double first = -1;
    kara_status s;
    do
    {
        struct timespec ts = {0, 5 * 1000 * 1000};
        nanosleep(&ts, NULL);
        s = kara_get_status(k, NULL);
        if (first < 0 && kara_lines_done(k) > 0)
            first = now() - t0;
    } while (s != KARA_DONE && s != KARA_FAILED && s != KARA_SKIPPED);
    double total = now() - t0;
    fprintf(stderr, "status %d, first line after %.2fs, all done after %.2fs (300 ms of it is the start delay)\n",
            (int)s, first, total);

    for (int i = 0; i < n; i++)
    {
        const kara_line *l = kara_line_get(k, i);
        if (l == NULL)
            continue;
        printf("%d", i);
        for (int j = 0; j < l->n; j++)
            printf(" %d/%d", l->syl[j].start_ms, l->syl[j].end_ms);
        printf("\n");
    }
    kara_destroy(k);
    return 0;
}
