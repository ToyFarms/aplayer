#ifndef _WIN32
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L /* readlink, even under -std=c17 */
#  endif
#endif

#include "ui_font.h"

#include "_math.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <dirent.h>
#  include <unistd.h>
#endif

#define CACHE_CAP  4096 /* glyph bitmaps, power of two */
#define MAX_PPEM   1024
#define MISSING_EM 0.5f /* advance of a glyph no font has */

/* ------------------------------------------------------------------------ */
/* the font: files, loaded on demand                                        */
/* ------------------------------------------------------------------------ */

enum face_state
{
    FACE_UNLOADED,
    FACE_READY,
    FACE_BAD,
};

typedef struct face
{
    char *path;
    FT_Face ft;
    int ppem; /* pixel size currently set on `ft`; 0: none */
    enum face_state state;
} face;

typedef struct centry
{
    int face, glyph, ppem;
    int adv; /* whole pixels */
    int w, h, xo, yo;
    uint8_t *bm; /* w * h, 0 or 255 (or the gray levels of a gray bitmap) */
    bool used, rendered;
} centry;

struct ui_font
{
    FT_Library lib;
    face *faces;
    int n;
    int weight;
    centry *cache;
    int cache_used;
};

static char *dup_str(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d == NULL)
        abort();
    memcpy(d, s, n);
    return d;
}

static void cache_clear(ui_font *f)
{
    for (int i = 0; i < CACHE_CAP; i++)
        free(f->cache[i].bm);
    memset(f->cache, 0, CACHE_CAP * sizeof(centry));
    f->cache_used = 0;
}

/* Points the weight axis of a variable font at `f->weight`; every other axis
 * stays at its default. */
static void apply_weight(const ui_font *f, face *fc)
{
    fc->ppem = 0; /* sizes depend on the variation */
    if (!FT_HAS_MULTIPLE_MASTERS(fc->ft))
        return;
    FT_MM_Var *mm = NULL;
    if (FT_Get_MM_Var(fc->ft, &mm) != 0 || mm == NULL)
        return;

    FT_Fixed *coords = malloc((mm->num_axis ? mm->num_axis : 1) * sizeof(*coords));
    if (coords == NULL)
        abort();
    bool has_wght = false;
    for (FT_UInt i = 0; i < mm->num_axis; i++)
    {
        const FT_Var_Axis *ax = &mm->axis[i];
        coords[i] = ax->def;
        if (ax->tag == FT_MAKE_TAG('w', 'g', 'h', 't'))
        {
            has_wght = true;
            if (f->weight > 0)
            {
                FT_Fixed w = (FT_Fixed)f->weight * 65536;
                coords[i] = w < ax->minimum   ? ax->minimum
                            : w > ax->maximum ? ax->maximum
                                              : w;
            }
        }
    }
    if (has_wght)
        FT_Set_Var_Design_Coordinates(fc->ft, mm->num_axis, coords);
    free(coords);
    FT_Done_MM_Var(f->lib, mm);
}

static bool face_load(ui_font *f, face *fc)
{
    if (fc->state != FACE_UNLOADED)
        return fc->state == FACE_READY;
    fc->state = FACE_BAD;

    FT_Face ft = NULL;
    if (FT_New_Face(f->lib, fc->path, 0, &ft) != 0 || ft == NULL)
        return false;
    /* bitmap-only fonts (colour emoji, old PCF) have no outlines to size */
    if (!FT_IS_SCALABLE(ft) || ft->num_glyphs <= 0)
    {
        FT_Done_Face(ft);
        return false;
    }
    fc->ft = ft;
    fc->state = FACE_READY;
    apply_weight(f, fc);
    return true;
}

ui_font *ui_font_open(const char *const *paths, int n)
{
    if (paths == NULL || n <= 0)
        return NULL;
    ui_font *f = calloc(1, sizeof(*f));
    if (f == NULL)
        abort();
    if (FT_Init_FreeType(&f->lib) != 0)
    {
        free(f);
        return NULL;
    }
    f->faces = calloc((size_t)n, sizeof(face));
    f->cache = calloc(CACHE_CAP, sizeof(centry));
    if (f->faces == NULL || f->cache == NULL)
        abort();
    for (int i = 0; i < n; i++)
        f->faces[i].path = dup_str(paths[i] ? paths[i] : "");
    f->n = n;

    /* the primary must exist now; the others wait until a character needs
     * them */
    for (int i = 0; i < n; i++)
        if (face_load(f, &f->faces[i]))
            return f;
    ui_font_close(f);
    return NULL;
}

void ui_font_close(ui_font *f)
{
    if (f == NULL)
        return;
    if (f->cache)
        cache_clear(f);
    free(f->cache);
    for (int i = 0; i < f->n; i++)
    {
        free(f->faces[i].path);
        if (f->faces[i].ft)
            FT_Done_Face(f->faces[i].ft);
    }
    free(f->faces);
    if (f->lib)
        FT_Done_FreeType(f->lib);
    free(f);
}

const char *ui_font_primary_path(const ui_font *f)
{
    if (f == NULL)
        return NULL;
    for (int i = 0; i < f->n; i++)
        if (f->faces[i].state == FACE_READY)
            return f->faces[i].path;
    return NULL;
}

void ui_font_set_weight(ui_font *f, int weight)
{
    if (f == NULL)
        return;
    weight = MATH_CLAMP(weight, 0, 1000);
    if (weight == f->weight)
        return;
    f->weight = weight;
    for (int i = 0; i < f->n; i++)
        if (f->faces[i].state == FACE_READY)
            apply_weight(f, &f->faces[i]);
    cache_clear(f);
}

/* ------------------------------------------------------------------------ */
/* default font files                                                       */
/* ------------------------------------------------------------------------ */

#ifdef _WIN32

static const char *const SYSTEM_FONTS[] = {
    "segoeuib.ttf", /* Latin, Greek, Cyrillic */
    "arialbd.ttf",  "msyhbd.ttc", /* Chinese */
    "YuGothB.ttc",                /* Japanese */
    "malgunbd.ttf",               /* Korean */
    "seguisym.ttf",
};

static const char *const UNIFONT_SYSTEM[] = {NULL};

#else

/* Bold faces first: strokes of a regular weight are about a pixel wide at
 * lyric sizes and break up. CJK follows the Latin fonts as a fallback. */
static const char *const SYSTEM_FONTS[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Bold.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
    "/usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
};

static const char *const UNIFONT_SYSTEM[] = {
    "/usr/share/fonts/opentype/unifont/unifont.otf",
    "/usr/share/fonts/truetype/unifont/unifont.ttf",
    "/usr/share/fonts/unifont/unifont.otf",
    "/usr/share/fonts/unifont/unifont.ttf",
    "/usr/share/fonts/misc/unifont.otf",
    "/usr/share/fonts/TTF/unifont.ttf",
    "/usr/share/unifont/unifont.otf",
    NULL,
};

#endif

#define N_SYSTEM_FONTS ((int)(sizeof(SYSTEM_FONTS) / sizeof(SYSTEM_FONTS[0])))

typedef struct plist
{
    char **v;
    int n, cap;
} plist;

static void plist_add(plist *l, const char *s)
{
    if (l->n == l->cap)
    {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->v = realloc(l->v, (size_t)l->cap * sizeof(*l->v));
        if (l->v == NULL)
            abort();
    }
    l->v[l->n++] = dup_str(s);
}

static void plist_free(plist *l)
{
    for (int i = 0; i < l->n; i++)
        free(l->v[i]);
    free(l->v);
    *l = (plist){0};
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool ci_prefix(const char *s, const char *prefix)
{
    for (; *prefix; s++, prefix++)
        if (*s == '\0' || tolower((unsigned char)*s) != *prefix)
            return false;
    return true;
}

static bool is_font_file(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return false;
    static const char *const exts[] = {".ttf", ".otf", ".ttc", ".otc"};
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
    {
        const char *e = exts[i];
        const char *p = dot;
        while (*e && *p && tolower((unsigned char)*p) == *e)
            e++, p++;
        if (*e == '\0' && *p == '\0')
            return true;
    }
    return false;
}

/* The font files of `dir`, sorted by name: GNU Unifont (a file called
 * unifont*) goes to `last`, everything else to `main`. A missing directory is
 * nothing, not an error. */
static void scan_dir(const char *dir, plist *main, plist *last)
{
    if (dir == NULL || dir[0] == '\0')
        return;
    plist names = {0};

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            is_font_file(fd.cFileName))
            plist_add(&names, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    const char sep = '\\';
#else
    DIR *d = opendir(dir);
    if (d == NULL)
        return;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
        if (is_font_file(e->d_name))
            plist_add(&names, e->d_name);
    closedir(d);
    const char sep = '/';
#endif

    if (names.n > 1)
        qsort(names.v, (size_t)names.n, sizeof(*names.v), cmp_names);
    for (int i = 0; i < names.n; i++)
    {
        char path[2048];
        snprintf(path, sizeof(path), "%s%c%s", dir, sep, names.v[i]);
        plist_add(ci_prefix(names.v[i], "unifont") ? last : main, path);
    }
    plist_free(&names);
}

/* the directory of the running executable, or "" */
static void exe_dir(char *out, size_t cap)
{
    out[0] = '\0';
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)cap);
    if (n == 0 || n >= cap)
    {
        out[0] = '\0';
        return;
    }
    char *slash = strrchr(out, '\\');
#else
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    if (n <= 0)
        return;
    out[n] = '\0';
    char *slash = strrchr(out, '/');
#endif
    if (slash != NULL)
        *slash = '\0';
    else
        out[0] = '\0';
}

ui_font *ui_font_open_default(const char *preferred)
{
    plist list = {0}, last = {0};

    if (preferred != NULL && preferred[0] != '\0')
        plist_add(&list, preferred);
    const char *env = getenv("APLAYER_LYRICS_FONT");
    if (env != NULL && env[0] != '\0')
        plist_add(&list, env);

    /* the fonts folder */
    const char *dir_env = getenv("APLAYER_FONT_DIR");
    scan_dir(dir_env, &list, &last);
    char exe[1024];
    exe_dir(exe, sizeof(exe));
    if (exe[0] != '\0')
    {
        char dir[1100];
#ifdef _WIN32
        snprintf(dir, sizeof(dir), "%s\\fonts", exe);
#else
        snprintf(dir, sizeof(dir), "%s/fonts", exe);
#endif
        scan_dir(dir, &list, &last);
    }
    scan_dir("fonts", &list, &last);

    /* what the platform ships */
#ifdef _WIN32
    const char *win = getenv("WINDIR");
    for (int i = 0; i < N_SYSTEM_FONTS; i++)
    {
        char path[320];
        snprintf(path, sizeof(path), "%s\\Fonts\\%s", win ? win : "C:\\Windows",
                 SYSTEM_FONTS[i]);
        plist_add(&list, path);
    }
#else
    for (int i = 0; i < N_SYSTEM_FONTS; i++)
        plist_add(&list, SYSTEM_FONTS[i]);
#endif

    /* and Unifont, last of all */
    for (int i = 0; i < last.n; i++)
        plist_add(&list, last.v[i]);
    for (int i = 0; UNIFONT_SYSTEM[i] != NULL; i++)
        plist_add(&list, UNIFONT_SYSTEM[i]);

    ui_font *f = ui_font_open((const char *const *)list.v, list.n);
    plist_free(&list);
    plist_free(&last);
    return f;
}

/* ------------------------------------------------------------------------ */
/* characters -> glyphs                                                     */
/* ------------------------------------------------------------------------ */

static size_t decode(const char *s, size_t len, uint32_t *cp)
{
    const unsigned char *p = (const unsigned char *)s;
    if (len == 0)
        return 0;
    if (p[0] < 0x80)
    {
        *cp = p[0];
        return 1;
    }
    size_t n = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : p[0] >= 0xC0 ? 2 : 0;
    if (n == 0 || n > len)
    {
        *cp = 0xFFFD;
        return 1;
    }
    uint32_t c = p[0] & (0xFFu >> (n + 1));
    for (size_t k = 1; k < n; k++)
    {
        if ((p[k] & 0xC0) != 0x80)
        {
            *cp = 0xFFFD;
            return 1;
        }
        c = (c << 6) | (p[k] & 0x3F);
    }
    *cp = c;
    return n;
}

/* Characters that are never seen and take no room: variation selectors, the
 * joiners, zero-width spaces and the like. Fonts that map some of them give
 * them an empty glyph of some width, which is not wanted either. */
static bool invisible(uint32_t cp)
{
    return cp == 0x00AD || cp == 0x034F || cp == 0x061C || cp == 0x180E ||
           (cp >= 0x180B && cp <= 0x180D) || (cp >= 0x200B && cp <= 0x200F) ||
           (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2060 && cp <= 0x206F) ||
           (cp >= 0xFE00 && cp <= 0xFE0F) || cp == 0xFEFF ||
           (cp >= 0xFFF9 && cp <= 0xFFFB) || (cp >= 0xE0000 && cp <= 0xE0FFF);
}

/* Combining marks no font has: they would have taken no room either. */
static bool combining(uint32_t cp)
{
    return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}

/* the first font that has the character; fallbacks are opened on first use */
static bool resolve(ui_font *f, uint32_t cp, int *face_i, int *glyph)
{
    for (int i = 0; i < f->n; i++)
    {
        if (!face_load(f, &f->faces[i]))
            continue;
        FT_UInt g = FT_Get_Char_Index(f->faces[i].ft, (FT_ULong)cp);
        if (g != 0)
        {
            *face_i = i;
            *glyph = (int)g;
            return true;
        }
    }
    return false;
}

/* sizes snap to whole pixels, everywhere, so that what was measured is what is
 * drawn */
static int size_of_px(float px)
{
    return MATH_CLAMP((int)floorf(px + 0.5f), 1, MAX_PPEM);
}

static bool use_size(face *fc, int ppem)
{
    if (fc->ppem == ppem)
        return true;
    if (FT_Set_Pixel_Sizes(fc->ft, 0, (FT_UInt)ppem) != 0)
    {
        fc->ppem = 0;
        return false;
    }
    fc->ppem = ppem;
    return true;
}

/* ------------------------------------------------------------------------ */
/* glyph cache                                                              */
/* ------------------------------------------------------------------------ */

static uint32_t hash_key(int face, int glyph, int ppem)
{
    uint32_t h = (uint32_t)face * 2654435761u;
    h ^= (uint32_t)glyph * 2246822519u + 0x9E3779B9u + (h << 6) + (h >> 2);
    h ^= (uint32_t)ppem * 3266489917u + 0x7F4A7C15u + (h << 6) + (h >> 2);
    return h;
}

/* Fills in the metrics of the glyph at this size and, when asked, its bitmap:
 * hinted, monochrome. */
static void fill_entry(ui_font *f, centry *e, bool want_bm)
{
    face *fc = &f->faces[e->face];
    free(e->bm);
    e->bm = NULL;
    e->w = e->h = e->xo = e->yo = 0;
    e->adv = (int)floorf((float)e->ppem * MISSING_EM + 0.5f);
    e->rendered = want_bm;

    FT_Int32 flags = FT_LOAD_TARGET_MONO | FT_LOAD_NO_BITMAP;
    if (want_bm)
        flags |= FT_LOAD_RENDER;
    if (!use_size(fc, e->ppem) ||
        FT_Load_Glyph(fc->ft, (FT_UInt)e->glyph, flags) != 0)
        return;

    FT_GlyphSlot slot = fc->ft->glyph;
    e->adv = (int)((slot->advance.x + 32) >> 6);
    if (!want_bm || slot->format != FT_GLYPH_FORMAT_BITMAP)
        return;

    const FT_Bitmap *bmp = &slot->bitmap;
    int w = (int)bmp->width, h = (int)bmp->rows;
    if (w <= 0 || h <= 0 || bmp->buffer == NULL)
        return;
    bool mono = bmp->pixel_mode == FT_PIXEL_MODE_MONO;
    if (!mono && bmp->pixel_mode != FT_PIXEL_MODE_GRAY)
        return;

    e->bm = malloc((size_t)w * (size_t)h);
    if (e->bm == NULL)
        abort();
    for (int y = 0; y < h; y++)
    {
        const uint8_t *src = bmp->buffer + (ptrdiff_t)y * bmp->pitch;
        if (bmp->pitch < 0) /* bottom-up bitmaps: not what FreeType makes here,
                               but cheap to be right about */
            src = bmp->buffer + (ptrdiff_t)(h - 1 - y) * -bmp->pitch;
        for (int x = 0; x < w; x++)
            e->bm[y * w + x] =
                mono ? ((src[x >> 3] >> (7 - (x & 7))) & 1 ? 255 : 0) : src[x];
    }
    e->w = w;
    e->h = h;
    e->xo = slot->bitmap_left;
    e->yo = -slot->bitmap_top;
}

/* The cached glyph (with its bitmap if `want_bm`). Valid until the next call:
 * a cache that is getting full starts over. */
static const centry *entry_of(ui_font *f, int face_i, int glyph, int ppem,
                              bool want_bm)
{
    uint32_t h = hash_key(face_i, glyph, ppem) & (CACHE_CAP - 1);
    for (;;)
    {
        centry *e = &f->cache[h];
        if (!e->used)
            break;
        if (e->face == face_i && e->glyph == glyph && e->ppem == ppem)
        {
            if (want_bm && !e->rendered)
                fill_entry(f, e, true);
            return e;
        }
        h = (h + 1) & (CACHE_CAP - 1);
    }

    /* full enough that probing gets long: start over, the sizes in use are
     * rasterized again as they are drawn */
    if (f->cache_used * 4 >= CACHE_CAP * 3)
    {
        cache_clear(f);
        h = hash_key(face_i, glyph, ppem) & (CACHE_CAP - 1);
    }

    centry *e = &f->cache[h];
    e->used = true;
    e->face = face_i;
    e->glyph = glyph;
    e->ppem = ppem;
    fill_entry(f, e, want_bm);
    f->cache_used++;
    return e;
}

/* ------------------------------------------------------------------------ */
/* placing                                                                  */
/* ------------------------------------------------------------------------ */

typedef struct placed
{
    int face, glyph; /* glyph < 0: nothing to draw */
    int kern, adv;   /* pixels: pen moves by kern, draws, moves by adv */
} placed;

/* Where the pen goes for `cp`, given the glyph before it (face/glyph, -1 for
 * none), which is updated. */
static placed place(ui_font *f, uint32_t cp, int ppem, int *prev_face,
                    int *prev_glyph)
{
    placed out = {-1, -1, 0, 0};
    if (cp == '\t')
        cp = ' ';
    if (cp < 0x20 || cp == 0x7F || invisible(cp))
        return out; /* takes no room */

    int fi, g;
    if (!resolve(f, cp, &fi, &g))
    {
        if (!combining(cp))
            out.adv = (int)floorf((float)ppem * MISSING_EM + 0.5f);
        *prev_face = -1;
        return out;
    }
    face *fc = &f->faces[fi];
    out.adv = entry_of(f, fi, g, ppem, false)->adv;
    if (*prev_face == fi && FT_HAS_KERNING(fc->ft) && use_size(fc, ppem))
    {
        FT_Vector k;
        if (FT_Get_Kerning(fc->ft, (FT_UInt)*prev_glyph, (FT_UInt)g,
                           FT_KERNING_DEFAULT, &k) == 0)
            out.kern = (int)lroundf((float)k.x / 64.0f);
    }
    out.face = fi;
    out.glyph = g;
    *prev_face = fi;
    *prev_glyph = g;
    return out;
}

void ui_font_metrics(ui_font *f, float px, float *ascent, float *descent)
{
    int ppem = size_of_px(px);
    face *fc = &f->faces[0];
    for (int i = 0; i < f->n; i++)
        if (f->faces[i].state == FACE_READY)
        {
            fc = &f->faces[i];
            break;
        }
    float a = (float)ppem * 0.8f, d = (float)ppem * 0.2f;
    if (fc->state == FACE_READY && use_size(fc, ppem))
    {
        const FT_Size_Metrics *m = &fc->ft->size->metrics;
        a = (float)m->ascender / 64.0f;
        d = (float)-m->descender / 64.0f;
    }
    if (ascent)
        *ascent = a;
    if (descent)
        *descent = d;
}

float ui_font_width(ui_font *f, const char *s, size_t len, float px)
{
    int ppem = size_of_px(px);
    int pf = -1, pg = -1;
    int pen = 0;
    for (size_t i = 0; i < len;)
    {
        uint32_t cp;
        size_t n = decode(s + i, len - i, &cp);
        if (n == 0)
            break;
        placed p = place(f, cp, ppem, &pf, &pg);
        pen += p.kern + p.adv;
        i += n;
    }
    return (float)pen;
}

/* ------------------------------------------------------------------------ */
/* wrapping                                                                 */
/* ------------------------------------------------------------------------ */

int ui_font_wrap(ui_font *f, const char *s, size_t len, float px, float max_w,
                 ui_font_span *out, int cap)
{
    if (cap <= 0)
        return 0;
    if (len == 0)
    {
        out[0] = (ui_font_span){0, 0};
        return 1;
    }

    int ppem = size_of_px(px);
    int count = 0;
    size_t start = 0;
    while (start < len)
    {
        if (count == cap - 1)
        {
            /* out of room: the rest goes on the last line */
            out[count++] = (ui_font_span){(int)start, (int)(len - start)};
            break;
        }

        size_t i = start, brk = 0, resume = 0;
        int w = 0;
        int pf = -1, pg = -1;
        bool fits_all = true;
        while (i < len)
        {
            uint32_t cp;
            size_t n = decode(s + i, len - i, &cp);
            if (n == 0)
                break;
            int kf = pf, kg = pg;
            placed p = place(f, cp, ppem, &kf, &kg);
            if ((float)(w + p.kern + p.adv) > max_w && i > start)
            {
                fits_all = false;
                break;
            }
            pf = kf;
            pg = kg;
            if (cp == ' ' && i > start)
            {
                brk = i;
                resume = i + n;
            }
            w += p.kern + p.adv;
            i += n;
        }

        size_t end = i, next = i;
        if (!fits_all && brk > start)
        {
            end = brk;
            next = resume;
        }
        while (end > start && s[end - 1] == ' ')
            end--;
        out[count++] = (ui_font_span){(int)start, (int)(end - start)};
        while (!fits_all && next < len && s[next] == ' ')
            next++;
        start = next > start ? next : len;
    }
    return count;
}

/* ------------------------------------------------------------------------ */
/* drawing                                                                  */
/* ------------------------------------------------------------------------ */

static void blit(ui_fb *fb, const centry *e, int ox, int oy, color_t color,
                 float alpha, int cx0, int cx1)
{
    for (int gy = 0; gy < e->h; gy++)
    {
        int y = oy + gy;
        if (y < 0 || y >= fb->h)
            continue;
        for (int gx = 0; gx < e->w; gx++)
        {
            int x = ox + gx;
            if (x < 0 || x >= fb->w || x < cx0 || x >= cx1)
                continue;
            float a = ((float)e->bm[gy * e->w + gx] / 255.0f) * alpha;
            if (a <= 0.0f)
                continue;

            uint8_t *d = fb->px + ((size_t)y * (size_t)fb->w + (size_t)x) * 4;
            float da = (float)d[3] / 255.0f;
            float oa = a + da * (1.0f - a);
            d[0] = (uint8_t)MATH_CLAMP(
                ((float)color.r * a + (float)d[0] * da * (1.0f - a)) / oa + 0.5f,
                0.0f, 255.0f);
            d[1] = (uint8_t)MATH_CLAMP(
                ((float)color.g * a + (float)d[1] * da * (1.0f - a)) / oa + 0.5f,
                0.0f, 255.0f);
            d[2] = (uint8_t)MATH_CLAMP(
                ((float)color.b * a + (float)d[2] * da * (1.0f - a)) / oa + 0.5f,
                0.0f, 255.0f);
            d[3] = (uint8_t)MATH_CLAMP(oa * 255.0f + 0.5f, 0.0f, 255.0f);
        }
    }
}

void ui_font_draw_clip(ui_font *f, ui_fb *fb, const char *s, size_t len,
                       float x, float baseline, float px, color_t color,
                       float alpha, float clip_x0, float clip_x1)
{
    /* below one pixel there is no text to draw (a whole pixel is the smallest
     * a glyph can be hinted to, and it would only be a speck) */
    if (f == NULL || fb == NULL || fb->px == NULL || alpha <= 0.0f || px < 1.0f)
        return;

    /* the clip is in whole pixel columns, like the glyphs */
    int cx0 = clip_x0 < -1e8f ? INT_MIN : (int)floorf(clip_x0 + 0.5f);
    int cx1 = clip_x1 > 1e8f ? INT_MAX : (int)floorf(clip_x1 + 0.5f);
    if (cx1 <= cx0)
        return;

    int ppem = size_of_px(px);
    int by = (int)floorf(baseline + 0.5f);
    int pf = -1, pg = -1;
    int pen = (int)floorf(x + 0.5f);
    for (size_t i = 0; i < len;)
    {
        uint32_t cp;
        size_t n = decode(s + i, len - i, &cp);
        if (n == 0)
            break;
        i += n;

        placed p = place(f, cp, ppem, &pf, &pg);
        pen += p.kern;
        if (p.glyph >= 0)
        {
            const centry *e = entry_of(f, p.face, p.glyph, ppem, true);
            if (e->bm != NULL)
                blit(fb, e, pen + e->xo, by + e->yo, color, alpha, cx0, cx1);
        }
        pen += p.adv;
    }
}

void ui_font_draw(ui_font *f, ui_fb *fb, const char *s, size_t len, float x,
                  float baseline, float px, color_t color, float alpha)
{
    ui_font_draw_clip(f, fb, s, len, x, baseline, px, color, alpha, -1e9f,
                      1e9f);
}
