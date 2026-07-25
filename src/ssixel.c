#include "ssixel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#  include <omp.h>
#endif

typedef struct
{
    uint32_t rgb;
    uint32_t count;
} hist_entry_t;

#define HBITS    5
#define HLEVELS  (1 << HBITS)
#define HBUCKETS (HLEVELS * HLEVELS * HLEVELS)
#define HSHIFT   (8 - HBITS)

static inline int bucket_key(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> HSHIFT) << (2 * HBITS)) | ((g >> HSHIFT) << HBITS) |
           (b >> HSHIFT);
}

typedef struct
{
    uint32_t count;
    uint64_t sum_r, sum_g, sum_b;
} hist_accum_t;

static int build_histogram(const uint8_t *restrict rgb, int width, int height,
                           hist_entry_t **out_entries)
{
    int npx = width * height;

    hist_accum_t *acc = calloc((size_t)HBUCKETS, sizeof(hist_accum_t));

    for (int i = 0; i < npx; i++)
    {
        uint8_t r = rgb[i * 3 + 0];
        uint8_t g = rgb[i * 3 + 1];
        uint8_t b = rgb[i * 3 + 2];
        int key = bucket_key(r, g, b);

        hist_accum_t *a = &acc[key];
        a->count++;
        a->sum_r += r;
        a->sum_g += g;
        a->sum_b += b;
    }

    int nunique = 0;
    for (int k = 0; k < HBUCKETS; k++)
        if (acc[k].count)
            nunique++;

    hist_entry_t *entries = malloc(sizeof(hist_entry_t) * (size_t)nunique);
    int idx = 0;
    for (int k = 0; k < HBUCKETS; k++)
    {
        if (!acc[k].count)
            continue;

        uint32_t c = acc[k].count;
        uint8_t r = (uint8_t)(acc[k].sum_r / c);
        uint8_t g = (uint8_t)(acc[k].sum_g / c);
        uint8_t b = (uint8_t)(acc[k].sum_b / c);

        entries[idx].rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        entries[idx].count = c;
        idx++;
    }

    free(acc);

    *out_entries = entries;
    return nunique;
}

typedef struct
{
    int lo, hi;
} box_t;

static inline uint8_t chan_of(uint32_t rgb, int chan)
{
    return (uint8_t)((rgb >> (16 - 8 * chan)) & 0xFF);
}

static int g_sort_chan;

static int cmp_entry_by_chan(const void *a, const void *b)
{
    uint8_t va = chan_of(((const hist_entry_t *)a)->rgb, g_sort_chan);
    uint8_t vb = chan_of(((const hist_entry_t *)b)->rgb, g_sort_chan);
    return (int)va - (int)vb;
}

static int box_widest_channel(const hist_entry_t *entries, box_t box,
                              int *range_out)
{
    uint8_t mn[3] = {255, 255, 255};
    uint8_t mx[3] = {0, 0, 0};

    for (int i = box.lo; i < box.hi; i++)
    {
        uint32_t rgb = entries[i].rgb;
        uint8_t r = (uint8_t)(rgb >> 16), g = (uint8_t)(rgb >> 8),
                b = (uint8_t)rgb;
        if (r < mn[0])
            mn[0] = r;
        if (r > mx[0])
            mx[0] = r;
        if (g < mn[1])
            mn[1] = g;
        if (g > mx[1])
            mx[1] = g;
        if (b < mn[2])
            mn[2] = b;
        if (b > mx[2])
            mx[2] = b;
    }

    int best_c = 0;
    int best_range = -1;
    for (int c = 0; c < 3; c++)
    {
        int range = mx[c] - mn[c];
        if (range > best_range)
        {
            best_range = range;
            best_c = c;
        }
    }

    *range_out = best_range;
    return best_c;
}

static uint64_t box_weight(const hist_entry_t *entries, box_t box)
{
    uint64_t w = 0;
    for (int i = box.lo; i < box.hi; i++)
        w += entries[i].count;
    return w;
}

static int box_split(hist_entry_t *entries, box_t box, box_t *left,
                     box_t *right)
{
    if (box.hi - box.lo <= 1)
        return 0;

    int range;
    int chan = box_widest_channel(entries, box, &range);
    if (range <= 0)
        return 0;

    g_sort_chan = chan;
    qsort(entries + box.lo, (size_t)(box.hi - box.lo), sizeof(hist_entry_t),
          cmp_entry_by_chan);

    uint64_t total = box_weight(entries, box);
    uint64_t half = total / 2;

    uint64_t running = 0;
    int split = box.lo;
    for (int i = box.lo; i < box.hi; i++)
    {
        running += entries[i].count;
        split = i + 1;
        if (running >= half)
            break;
    }

    if (split <= box.lo)
        split = box.lo + 1;
    if (split >= box.hi)
        split = box.hi - 1;

    left->lo = box.lo;
    left->hi = split;
    right->lo = split;
    right->hi = box.hi;
    return 1;
}

typedef struct
{
    uint8_t r, g, b;
} rgb8_t;

static int median_cut(hist_entry_t *entries, int nentries, int max_colors,
                      rgb8_t *palette)
{
    if (nentries <= max_colors)
    {
        for (int i = 0; i < nentries; i++)
        {
            palette[i].r = chan_of(entries[i].rgb, 0);
            palette[i].g = chan_of(entries[i].rgb, 1);
            palette[i].b = chan_of(entries[i].rgb, 2);
        }
        return nentries;
    }

    box_t *boxes = malloc(sizeof(box_t) * (size_t)max_colors);
    int nboxes = 1;
    boxes[0].lo = 0;
    boxes[0].hi = nentries;

    while (nboxes < max_colors)
    {
        int best_i = -1;
        double best_score = -1;
        for (int i = 0; i < nboxes; i++)
        {
            if (boxes[i].hi - boxes[i].lo <= 1)
                continue;
            int range;
            box_widest_channel(entries, boxes[i], &range);
            double score =
                (double)range * (double)box_weight(entries, boxes[i]);
            if (score > best_score)
            {
                best_score = score;
                best_i = i;
            }
        }

        if (best_i < 0)
            break;

        box_t left, right;
        if (!box_split(entries, boxes[best_i], &left, &right))
            break;

        boxes[best_i] = left;
        boxes[nboxes] = right;
        nboxes++;
    }

    for (int i = 0; i < nboxes; i++)
    {
        uint64_t sr = 0, sg = 0, sb = 0, w = 0;
        for (int j = boxes[i].lo; j < boxes[i].hi; j++)
        {
            uint64_t c = entries[j].count;
            sr += chan_of(entries[j].rgb, 0) * c;
            sg += chan_of(entries[j].rgb, 1) * c;
            sb += chan_of(entries[j].rgb, 2) * c;
            w += c;
        }
        if (w == 0)
            w = 1;
        palette[i].r = (uint8_t)(sr / w);
        palette[i].g = (uint8_t)(sg / w);
        palette[i].b = (uint8_t)(sb / w);
    }

    free(boxes);
    return nboxes;
}

static inline int nearest_palette_index_brute(const rgb8_t *palette, int npal,
                                              int r, int g, int b)
{
    int best = 0;
    int best_d = 0x7fffffff;
    for (int i = 0; i < npal; i++)
    {
        int dr = r - palette[i].r;
        int dg = g - palette[i].g;
        int db = b - palette[i].b;
        int d = dr * dr + dg * dg + db * db;
        if (d < best_d)
        {
            best_d = d;
            best = i;
        }
    }
    return best;
}

static uint8_t *build_nn_lut(const rgb8_t *palette, int npal)
{
    uint8_t *lut = malloc((size_t)HBUCKETS);

    int key;
#ifdef _OPENMP
#  pragma omp parallel for schedule(static)
#endif
    for (key = 0; key < HBUCKETS; key++)
    {
        int r5 = (key >> (2 * HBITS)) & (HLEVELS - 1);
        int g5 = (key >> HBITS) & (HLEVELS - 1);
        int b5 = key & (HLEVELS - 1);

        int r = (r5 << HSHIFT) | (1 << (HSHIFT - 1));
        int g = (g5 << HSHIFT) | (1 << (HSHIFT - 1));
        int b = (b5 << HSHIFT) | (1 << (HSHIFT - 1));

        lut[key] = (uint8_t)nearest_palette_index_brute(palette, npal, r, g, b);
    }

    return lut;
}

static inline int nearest_palette_index_lut(const uint8_t *lut, int r, int g,
                                            int b)
{
    return lut[bucket_key((uint8_t)r, (uint8_t)g, (uint8_t)b)];
}

static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int itoa_fast(int v, char *buf)
{
    char tmp[12];
    int n = 0;
    if (v == 0)
    {
        buf[0] = '0';
        return 1;
    }
    while (v > 0)
    {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    return n;
}

static void emit_run(str_t *out, int count, uint8_t sixel_val)
{
    char ch = (char)('?' + sixel_val);

    if (count <= 1)
    {
        str_catch(out, ch);
        return;
    }
    if (count <= 3)
    {
        for (int i = 0; i < count; i++)
            str_catch(out, ch);
        return;
    }

    char buf[16];
    int n = 0;
    buf[n++] = '!';
    n += itoa_fast(count, buf + n);
    buf[n++] = ch;
    str_catlen(out, buf, n);
}

void sixel_encode_rgb(str_t *out, const uint8_t *rgb, int width, int height,
                      int max_colors)
{
    if (width <= 0 || height <= 0)
        return;

    if (max_colors <= 0)
        max_colors = 256;
    if (max_colors > 256)
        max_colors = 256;

    hist_entry_t *entries;
    int nentries = build_histogram(rgb, width, height, &entries);

    rgb8_t *palette = malloc(sizeof(rgb8_t) * (size_t)max_colors);
    int npal = median_cut(entries, nentries, max_colors, palette);
    free(entries);

    uint8_t *lut = build_nn_lut(palette, npal);

    uint8_t *indices = malloc((size_t)width * (size_t)height);

    int32_t *restrict work =
        malloc(sizeof(int32_t) * (size_t)width * (size_t)height * 3);
    for (int i = 0; i < width * height * 3; i++)
        work[i] = (int32_t)rgb[i] << 8;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = (y * width + x) * 3;
            int r = clampi(work[i + 0] >> 8, 0, 255);
            int g = clampi(work[i + 1] >> 8, 0, 255);
            int b = clampi(work[i + 2] >> 8, 0, 255);

            int pi = nearest_palette_index_lut(lut, r, g, b);
            indices[y * width + x] = (uint8_t)pi;

            int er = (r - palette[pi].r) << 8;
            int eg = (g - palette[pi].g) << 8;
            int eb = (b - palette[pi].b) << 8;

#define DIFFUSE(dx, dy, num)                                                   \
    do                                                                         \
    {                                                                          \
        int xx = x + (dx), yy = y + (dy);                                      \
        if (xx >= 0 && xx < width && yy >= 0 && yy < height)                   \
        {                                                                      \
            int j = (yy * width + xx) * 3;                                     \
            work[j + 0] += (er * (num)) >> 4;                                  \
            work[j + 1] += (eg * (num)) >> 4;                                  \
            work[j + 2] += (eb * (num)) >> 4;                                  \
        }                                                                      \
    } while (0)

            DIFFUSE(1, 0, 7);
            DIFFUSE(-1, 1, 3);
            DIFFUSE(0, 1, 5);
            DIFFUSE(1, 1, 1);

#undef DIFFUSE
        }
    }
    free(work);
    free(lut);

    char buf[128];
    str_cat(out, "\x1bPq");

    int n = snprintf(buf, sizeof(buf), "\"1;1;%d;%d", width, height);
    str_catlen(out, buf, n);

    for (int i = 0; i < npal; i++)
    {
        int r = (palette[i].r * 100 + 127) / 255;
        int g = (palette[i].g * 100 + 127) / 255;
        int b = (palette[i].b * 100 + 127) / 255;
        n = snprintf(buf, sizeof(buf), "#%d;2;%d;%d;%d", i, r, g, b);
        str_catlen(out, buf, n);
    }

    uint8_t *used = malloc((size_t)npal);
    int *slot_of = malloc(sizeof(int) * (size_t)npal);
    uint8_t *bandval = malloc((size_t)npal * (size_t)width);

    int nbands = (height + 5) / 6;

    for (int band = 0; band < nbands; band++)
    {
        int y0 = band * 6;
        int rows = (height - y0 < 6) ? (height - y0) : 6;

        memset(used, 0, (size_t)npal);
        for (int row = 0; row < rows; row++)
        {
            const uint8_t *idx_row = indices + (size_t)(y0 + row) * width;
            for (int x = 0; x < width; x++)
                used[idx_row[x]] = 1;
        }

        int nused = 0;
        for (int c = 0; c < npal; c++)
            if (used[c])
                slot_of[c] = nused++;

        memset(bandval, 0, (size_t)nused * (size_t)width);
        for (int row = 0; row < rows; row++)
        {
            const uint8_t *idx_row = indices + (size_t)(y0 + row) * width;
            uint8_t bit = (uint8_t)(1u << row);
            for (int x = 0; x < width; x++)
                bandval[slot_of[idx_row[x]] * width + x] |= bit;
        }

        int first = 1;
        for (int c = 0; c < npal; c++)
        {
            if (!used[c])
                continue;

            if (!first)
                str_catch(out, '$');
            first = 0;

            char nbuf[16];
            int nn = 0;
            nbuf[nn++] = '#';
            nn += itoa_fast(c, nbuf + nn);
            str_catlen(out, nbuf, nn);

            const uint8_t *vals = bandval + slot_of[c] * width;
            int run_len = 1;
            uint8_t run_val = vals[0];
            for (int x = 1; x < width; x++)
            {
                if (vals[x] == run_val)
                {
                    run_len++;
                }
                else
                {
                    emit_run(out, run_len, run_val);
                    run_val = vals[x];
                    run_len = 1;
                }
            }
            emit_run(out, run_len, run_val);
        }

        if (band != nbands - 1)
            str_catch(out, '-');
    }

    str_cat(out, "\x1b\\");

    free(bandval);
    free(slot_of);
    free(used);
    free(indices);
    free(palette);
}
