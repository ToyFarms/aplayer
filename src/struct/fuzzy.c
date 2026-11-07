#include "fuzzy.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  ifdef _MSC_VER
#    include <intrin.h>
#  endif
#else
#  include <time.h>
#endif

/*
 * See fuzzy.h for the design. Layout of this file:
 *
 *   folding        code point -> (folded code point, character class)
 *   index          decoded + folded haystack, one 64-bit mask per string
 *   query          needle terms
 *   scoring        prefilter, forward/backward window, DP (+ backtrack)
 *   search         incremental, time-sliced scan and the O(n) ordering
 */

#define FZ_MAX_TEXT     512 /* code points kept per haystack string */
#define FZ_MAX_TERMS    8
#define FZ_MAX_TERM_LEN 64

/* scoring (the numbers are fzf's) */
#define SC_MATCH       16
#define SC_GAP_START   (-3)
#define SC_GAP_EXT     (-1)
#define BONUS_BOUNDARY 8
#define BONUS_CONSEC   4
#define FIRST_MULT     2
#define NEG            (-30000)
#define NEG_LIM        (-15000)

enum fz_class
{
    C_OTHER = 0, /* punctuation, symbols */
    C_SPACE,
    C_DELIM, /* / \ | : ; , */
    C_LOWER,
    C_UPPER,
    C_DIGIT,
    C_LETTER, /* word characters without case: CJK, kana, hangul, ... */
    C_COUNT,
};

static uint64_t fz_now_ns(void)
{
#ifdef _WIN32
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)now.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static inline int fz_ctz64(uint64_t b)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(b);
#elif defined(_MSC_VER)
    unsigned long i;
    _BitScanForward64(&i, b);
    return (int)i;
#else
    int i = 0;
    while (!(b & 1))
    {
        b >>= 1;
        i++;
    }
    return i;
#endif
}

/* ------------------------------------------------------------------------ */
/* folding                                                                  */
/* ------------------------------------------------------------------------ */

/* generated: Latin letters with diacritics -> base ASCII letter (bit 7 = upper case) */
static const uint8_t FZ_LATIN_A[400] = {
    0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe1, 0xe3, 0xe5, 0xe5, 0xe5, 0xe5, 0xe9, 0xe9, 0xe9, 0xe9,
    0xe4, 0xee, 0xef, 0xef, 0xef, 0xef, 0xef, 0x00, 0xef, 0xf5, 0xf5, 0xf5, 0xf5, 0xf9, 0xf4, 0x73,
    0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x63, 0x65, 0x65, 0x65, 0x65, 0x69, 0x69, 0x69, 0x69,
    0x64, 0x6e, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0x00, 0x6f, 0x75, 0x75, 0x75, 0x75, 0x79, 0x74, 0x79,
    0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe3, 0x63, 0xe3, 0x63, 0xe3, 0x63, 0xe3, 0x63, 0xe4, 0x64,
    0xe4, 0x64, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe7, 0x67, 0xe7, 0x67,
    0xe7, 0x67, 0xe7, 0x67, 0xe8, 0x68, 0xe8, 0x68, 0xe9, 0x69, 0xe9, 0x69, 0xe9, 0x69, 0xe9, 0x69,
    0xe9, 0x69, 0xe9, 0x69, 0xea, 0x6a, 0xeb, 0x6b, 0x6b, 0xec, 0x6c, 0xec, 0x6c, 0xec, 0x6c, 0x00,
    0x00, 0xec, 0x6c, 0xee, 0x6e, 0xee, 0x6e, 0xee, 0x6e, 0x6e, 0xee, 0x6e, 0xef, 0x6f, 0xef, 0x6f,
    0xef, 0x6f, 0xef, 0x6f, 0xf2, 0x72, 0xf2, 0x72, 0xf2, 0x72, 0xf3, 0x73, 0xf3, 0x73, 0xf3, 0x73,
    0xf3, 0x73, 0xf4, 0x74, 0xf4, 0x74, 0xf4, 0x74, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75,
    0xf5, 0x75, 0xf5, 0x75, 0xf7, 0x77, 0xf9, 0x79, 0xf9, 0xfa, 0x7a, 0xfa, 0x7a, 0xfa, 0x7a, 0x73,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xef, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf5,
    0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0x61, 0xe9,
    0x69, 0xef, 0x6f, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0x00, 0xe1, 0x61,
    0xe1, 0x61, 0x00, 0x00, 0x00, 0x00, 0xe7, 0x67, 0xeb, 0x6b, 0xef, 0x6f, 0xef, 0x6f, 0x00, 0x00,
    0x6a, 0x00, 0x00, 0x00, 0xe7, 0x67, 0x00, 0x00, 0xee, 0x6e, 0xe1, 0x61, 0x00, 0x00, 0x00, 0x00,
    0xe1, 0x61, 0xe1, 0x61, 0xe5, 0x65, 0xe5, 0x65, 0xe9, 0x69, 0xe9, 0x69, 0xef, 0x6f, 0xef, 0x6f,
    0xf2, 0x72, 0xf2, 0x72, 0xf5, 0x75, 0xf5, 0x75, 0xf3, 0x73, 0xf4, 0x74, 0x00, 0x00, 0xe8, 0x68,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0x61, 0xe5, 0x65, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f,
    0xef, 0x6f, 0xf9, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t FZ_LATIN_B[256] = {
    0xe1, 0x61, 0xe2, 0x62, 0xe2, 0x62, 0xe2, 0x62, 0xe3, 0x63, 0xe4, 0x64, 0xe4, 0x64, 0xe4, 0x64,
    0xe4, 0x64, 0xe4, 0x64, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe6, 0x66,
    0xe7, 0x67, 0xe8, 0x68, 0xe8, 0x68, 0xe8, 0x68, 0xe8, 0x68, 0xe8, 0x68, 0xe9, 0x69, 0xe9, 0x69,
    0xeb, 0x6b, 0xeb, 0x6b, 0xeb, 0x6b, 0xec, 0x6c, 0xec, 0x6c, 0xec, 0x6c, 0xec, 0x6c, 0xed, 0x6d,
    0xed, 0x6d, 0xed, 0x6d, 0xee, 0x6e, 0xee, 0x6e, 0xee, 0x6e, 0xee, 0x6e, 0xef, 0x6f, 0xef, 0x6f,
    0xef, 0x6f, 0xef, 0x6f, 0xf0, 0x70, 0xf0, 0x70, 0xf2, 0x72, 0xf2, 0x72, 0xf2, 0x72, 0xf2, 0x72,
    0xf3, 0x73, 0xf3, 0x73, 0xf3, 0x73, 0xf3, 0x73, 0xf3, 0x73, 0xf4, 0x74, 0xf4, 0x74, 0xf4, 0x74,
    0xf4, 0x74, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf6, 0x76, 0xf6, 0x76,
    0xf7, 0x77, 0xf7, 0x77, 0xf7, 0x77, 0xf7, 0x77, 0xf7, 0x77, 0xf8, 0x78, 0xf8, 0x78, 0xf9, 0x79,
    0xfa, 0x7a, 0xfa, 0x7a, 0xfa, 0x7a, 0x68, 0x74, 0x77, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61,
    0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe1, 0x61, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65,
    0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe5, 0x65, 0xe9, 0x69, 0xe9, 0x69, 0xef, 0x6f, 0xef, 0x6f,
    0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f, 0xef, 0x6f,
    0xef, 0x6f, 0xef, 0x6f, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75, 0xf5, 0x75,
    0xf5, 0x75, 0xf9, 0x79, 0xf9, 0x79, 0xf9, 0x79, 0xf9, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


/* Folds one code point. Returns false for code points that are dropped
 * (combining marks: NFD file names from macOS carry "e" + U+0301). */
static bool fz_prepare(uint32_t c, uint32_t *folded, uint8_t *cls)
{
    if ((c >= 0x300 && c <= 0x36F) || c == 0x3099 || c == 0x309A)
        return false;
    if (c == 0x3000 || c == 0xA0 || (c >= 0x2000 && c <= 0x200A))
    {
        *folded = ' ';
        *cls = C_SPACE;
        return true;
    }
    if (c >= 0xFF01 && c <= 0xFF5E) /* full-width ASCII, keeps its case */
        c -= 0xFEE0;

    if (c < 0x80)
    {
        *folded = c;
        if (c >= 'a' && c <= 'z')
            *cls = C_LOWER;
        else if (c >= 'A' && c <= 'Z')
        {
            *folded = c + 32;
            *cls = C_UPPER;
        }
        else if (c >= '0' && c <= '9')
            *cls = C_DIGIT;
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            *cls = C_SPACE;
        else if (c == '/' || c == '\\' || c == '|' || c == ':' || c == ';' ||
                 c == ',')
            *cls = C_DELIM;
        else
            *cls = C_OTHER;
        return true;
    }

    const uint8_t *t = NULL;
    if (c >= 0xC0 && c <= 0x24F)
        t = &FZ_LATIN_A[c - 0xC0];
    else if (c >= 0x1E00 && c <= 0x1EFF)
        t = &FZ_LATIN_B[c - 0x1E00];
    if (t != NULL && *t != 0)
    {
        *folded = *t & 0x7F;
        *cls = (*t & 0x80) ? C_UPPER : C_LOWER;
        return true;
    }

    *folded = c;
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) /* Greek */
    {
        *folded = c + 32;
        *cls = C_UPPER;
    }
    else if (c == 0x3C2) /* final sigma */
    {
        *folded = 0x3C3;
        *cls = C_LOWER;
    }
    else if (c >= 0x3B1 && c <= 0x3C9)
        *cls = C_LOWER;
    else if (c >= 0x400 && c <= 0x40F) /* Cyrillic */
    {
        *folded = c + 80;
        *cls = C_UPPER;
    }
    else if (c >= 0x410 && c <= 0x42F)
    {
        *folded = c + 32;
        *cls = C_UPPER;
    }
    else if (c >= 0x430 && c <= 0x45F)
        *cls = C_LOWER;
    else if (c >= 0x30A1 && c <= 0x30F6) /* katakana -> hiragana */
    {
        *folded = c - 0x60;
        *cls = C_LETTER;
    }
    else if (c < 0xC0 || (c >= 0x2010 && c <= 0x206F) ||
             (c >= 0x3001 && c <= 0x303F) || (c >= 0xFE30 && c <= 0xFE6F) ||
             (c >= 0xFF00 && c <= 0xFF65) || c == 0xD7 || c == 0xF7 ||
             c == 0xFFFD)
        *cls = C_OTHER;
    else
        *cls = C_LETTER;
    return true;
}

static size_t fz_decode(const char *s, size_t len, uint32_t *cp)
{
    const unsigned char *p = (const unsigned char *)s;
    if (len == 0)
        return 0;
    unsigned c = p[0];
    if (c < 0x80)
    {
        *cp = c;
        return 1;
    }
    size_t need = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 ? 2 : 0;
    if (need == 0 || c > 0xF4 || need > len)
        goto bad;
    uint32_t v = c & (0x3Fu >> (need - 1));
    for (size_t i = 1; i < need; i++)
    {
        if ((p[i] & 0xC0) != 0x80)
            goto bad;
        v = (v << 6) | (p[i] & 0x3F);
    }
    if ((need == 3 && v < 0x800) || (need == 4 && v < 0x10000) ||
        (v >= 0xD800 && v <= 0xDFFF) || v > 0x10FFFF)
        goto bad;
    *cp = v;
    return need;
bad:
    *cp = 0xFFFD;
    return 1;
}

static inline uint64_t fz_bit(uint32_t f)
{
    if (f >= 'a' && f <= 'z')
        return 1ull << (f - 'a');
    if (f >= '0' && f <= '9')
        return 1ull << (26 + (f - '0'));
    if (f == ' ')
        return 0;
    return 1ull << (36 + ((uint32_t)(f * 2654435761u) >> 27) % 28);
}

/* bonus of a character given the class of the one before it */
static int fz_bonus_calc(uint8_t p, uint8_t c)
{
    if (c == C_SPACE || c == C_DELIM || c == C_OTHER)
        return 4;
    if (p == C_SPACE)
        return 10;
    if (p == C_DELIM)
        return 9;
    if (p == C_OTHER)
        return 8;
    if (c == C_UPPER && p == C_LOWER)
        return 7;
    if (c == C_DIGIT && p != C_DIGIT)
        return 7;
    return 0;
}

static int8_t FZ_BONUS[C_COUNT][C_COUNT];

static void fz_init_tables(void)
{
    static bool done;
    if (done)
        return;
    for (int p = 0; p < C_COUNT; p++)
        for (int c = 0; c < C_COUNT; c++)
            FZ_BONUS[p][c] = (int8_t)fz_bonus_calc((uint8_t)p, (uint8_t)c);
    done = true;
}

/* ------------------------------------------------------------------------ */
/* index                                                                    */
/* ------------------------------------------------------------------------ */

typedef struct fz_entry
{
    uint32_t off;
    uint16_t len;
    uint16_t pad;
    uint64_t mask;
} fz_entry;

struct fuzzy_index
{
    uint32_t *cp; /* folded code points of every string, back to back */
    uint8_t *cls;
    size_t n_cp, cap_cp;
    fz_entry *e;
    size_t n, cap_e;
};

static void *fz_realloc(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (q == NULL && n != 0)
        abort();
    return q;
}

fuzzy_index *fuzzy_index_new(void)
{
    fz_init_tables();
    return calloc(1, sizeof(fuzzy_index));
}

void fuzzy_index_free(fuzzy_index *idx)
{
    if (idx == NULL)
        return;
    free(idx->cp);
    free(idx->cls);
    free(idx->e);
    free(idx);
}

static void index_grow_cp(fuzzy_index *idx, size_t need)
{
    if (idx->n_cp + need <= idx->cap_cp)
        return;
    size_t cap = idx->cap_cp ? idx->cap_cp : 4096;
    while (cap < idx->n_cp + need)
        cap *= 2;
    idx->cp = fz_realloc(idx->cp, cap * sizeof(*idx->cp));
    idx->cls = fz_realloc(idx->cls, cap * sizeof(*idx->cls));
    idx->cap_cp = cap;
}

static void index_grow_e(fuzzy_index *idx, size_t need)
{
    if (idx->n + need <= idx->cap_e)
        return;
    size_t cap = idx->cap_e ? idx->cap_e : 256;
    while (cap < idx->n + need)
        cap *= 2;
    idx->e = fz_realloc(idx->e, cap * sizeof(*idx->e));
    idx->cap_e = cap;
}

void fuzzy_index_reserve(fuzzy_index *idx, size_t items, size_t avg_len)
{
    index_grow_e(idx, items);
    index_grow_cp(idx, items * avg_len);
}

/* Fold `utf8` into `cp`/`cls` (capacity FZ_MAX_TEXT); `orig`, if given, gets the
 * index of each kept character in the original string. Returns the length. */
static int fz_fold_text(const char *utf8, size_t len, uint32_t *cp, uint8_t *cls,
                        uint16_t *orig)
{
    int n = 0;
    size_t i = 0;
    uint32_t oi = 0;
    while (i < len && n < FZ_MAX_TEXT)
    {
        uint32_t c;
        i += fz_decode(utf8 + i, len - i, &c);
        uint32_t f;
        uint8_t k;
        if (fz_prepare(c, &f, &k))
        {
            cp[n] = f;
            cls[n] = k;
            if (orig)
                orig[n] = (uint16_t)(oi < 0xFFFF ? oi : 0xFFFF);
            n++;
        }
        oi++;
    }
    return n;
}

uint32_t fuzzy_index_add(fuzzy_index *idx, const char *utf8, size_t len)
{
    index_grow_e(idx, 1);
    index_grow_cp(idx, len < FZ_MAX_TEXT ? len : FZ_MAX_TEXT);
    uint32_t *cp = idx->cp + idx->n_cp;
    uint8_t *cls = idx->cls + idx->n_cp;
    int n = fz_fold_text(utf8, len, cp, cls, NULL);

    uint64_t mask = 0;
    for (int i = 0; i < n; i++)
        mask |= fz_bit(cp[i]);
    idx->e[idx->n] = (fz_entry){(uint32_t)idx->n_cp, (uint16_t)n, 0, mask};
    idx->n_cp += (size_t)n;
    return (uint32_t)idx->n++;
}

size_t fuzzy_index_count(const fuzzy_index *idx)
{
    return idx->n;
}

/* ------------------------------------------------------------------------ */
/* query                                                                    */
/* ------------------------------------------------------------------------ */

typedef struct fz_term
{
    uint32_t cp[FZ_MAX_TERM_LEN];
    uint8_t cls[FZ_MAX_TERM_LEN];
    int len;
    bool sens; /* smart-case: has an upper-case letter */
} fz_term;

struct fuzzy_query
{
    char *text;
    fz_term term[FZ_MAX_TERMS];
    int nterms;
    uint64_t mask; /* characters every match must contain */
};

static bool fz_is_blank(uint32_t c)
{
    return c <= 0x20 || c == 0x3000 || c == 0xA0;
}

fuzzy_query *fuzzy_query_new(const char *utf8)
{
    fz_init_tables();
    if (utf8 == NULL)
        utf8 = "";
    size_t len = strlen(utf8);
    fuzzy_query *q = calloc(1, sizeof(*q));
    if (q == NULL)
        abort();
    q->text = fz_realloc(NULL, len + 1);
    memcpy(q->text, utf8, len + 1);

    fz_term *cur = NULL;
    size_t i = 0;
    while (i < len)
    {
        uint32_t c;
        i += fz_decode(utf8 + i, len - i, &c);
        if (fz_is_blank(c))
        {
            cur = NULL;
            continue;
        }
        if (cur == NULL)
        {
            if (q->nterms == FZ_MAX_TERMS)
                break;
            cur = &q->term[q->nterms++];
        }
        uint32_t f;
        uint8_t k;
        if (!fz_prepare(c, &f, &k) || cur->len == FZ_MAX_TERM_LEN)
            continue;
        cur->cp[cur->len] = f;
        cur->cls[cur->len] = k;
        cur->len++;
        if (k == C_UPPER)
            cur->sens = true;
        q->mask |= fz_bit(f);
    }
    /* a term made only of dropped characters matches everything: remove it */
    int w = 0;
    for (int t = 0; t < q->nterms; t++)
        if (q->term[t].len > 0)
            q->term[w++] = q->term[t];
    q->nterms = w;
    return q;
}

void fuzzy_query_free(fuzzy_query *q)
{
    if (q == NULL)
        return;
    free(q->text);
    free(q);
}

bool fuzzy_query_empty(const fuzzy_query *q)
{
    return q->nterms == 0;
}

const char *fuzzy_query_text(const fuzzy_query *q)
{
    return q->text;
}

bool fuzzy_query_extends(const fuzzy_query *newer, const fuzzy_query *older)
{
    size_t n = strlen(older->text);
    return strncmp(newer->text, older->text, n) == 0;
}

/* ------------------------------------------------------------------------ */
/* scoring                                                                  */
/* ------------------------------------------------------------------------ */

/* DP tables, grown on demand and reused for every candidate of a search. */
typedef struct fz_scratch
{
    int16_t *M, *G, *Pj, *Gk;
    int8_t *K;
    size_t cap;
} fz_scratch;

static void scratch_free(fz_scratch *s)
{
    free(s->M);
    free(s->G);
    free(s->Pj);
    free(s->Gk);
    free(s->K);
    memset(s, 0, sizeof(*s));
}

static void scratch_need(fz_scratch *s, size_t cells)
{
    if (cells <= s->cap)
        return;
    size_t cap = s->cap ? s->cap : 1024;
    while (cap < cells)
        cap *= 2;
    s->M = fz_realloc(s->M, cap * sizeof(*s->M));
    s->G = fz_realloc(s->G, cap * sizeof(*s->G));
    s->Pj = fz_realloc(s->Pj, cap * sizeof(*s->Pj));
    s->Gk = fz_realloc(s->Gk, cap * sizeof(*s->Gk));
    s->K = fz_realloc(s->K, cap * sizeof(*s->K));
    s->cap = cap;
}

static inline bool fz_eq(const fz_term *t, int i, uint32_t cp, uint8_t cls)
{
    if (t->cp[i] != cp)
        return false;
    if (!t->sens)
        return true;
    uint8_t qc = t->cls[i];
    return (qc != C_UPPER && qc != C_LOWER) || qc == cls;
}

static inline int fz_bonus_at(const uint8_t *cls, int j)
{
    return FZ_BONUS[j == 0 ? C_SPACE : cls[j - 1]][cls[j]];
}

static inline int imax(int a, int b) { return a > b ? a : b; }

/* Best score of one term in the text, or -1. If `pos` is given it receives the
 * matched text indices (t->len of them, ascending). */
static int fz_match_term(const fz_term *t, const uint32_t *cp,
                         const uint8_t *cls, int n, fz_scratch *sc, int *pos)
{
    const int m = t->len;
    if (m > n)
        return -1;

    /* forward: earliest place the match can start, and whether it exists */
    int first = -1, j = 0;
    for (int i = 0; i < n; i++)
    {
        if (fz_eq(t, j, cp[i], cls[i]))
        {
            if (j == 0)
                first = i;
            if (++j == m)
                break;
        }
    }
    if (j < m)
        return -1;

    if (m == 1)
    {
        int best = -1, bi = first;
        for (int i = first; i < n; i++)
        {
            if (!fz_eq(t, 0, cp[i], cls[i]))
                continue;
            int s = SC_MATCH + fz_bonus_at(cls, i) * FIRST_MULT;
            if (s > best)
            {
                best = s;
                bi = i;
            }
        }
        if (pos)
            pos[0] = bi;
        return best;
    }

    /* backward: the last place the final character can sit */
    int last = n - 1;
    j = m - 1;
    for (int i = n - 1; i >= first; i--)
    {
        if (fz_eq(t, j, cp[i], cls[i]))
        {
            if (j == m - 1)
                last = i;
            if (--j < 0)
                break;
        }
    }

    const int w = last - first + 1;
    scratch_need(sc, (size_t)m * (size_t)w);
    int16_t *M = sc->M, *G = sc->G, *Pj = sc->Pj, *Gk = sc->Gk;
    int8_t *K = sc->K;
#define AT(i, j) ((size_t)(i) * (size_t)w + (size_t)(j))

    for (int i = 0; i < m; i++)
    {
        for (int c = 0; c < w; c++)
        {
            const int tj = first + c;
            int mv = NEG, kv = 0, pj = -1;
            if (fz_eq(t, i, cp[tj], cls[tj]))
            {
                const int b = fz_bonus_at(cls, tj);
                if (i == 0)
                {
                    mv = SC_MATCH + b * FIRST_MULT;
                    kv = b;
                }
                else if (c > 0)
                {
                    const int a = M[AT(i - 1, c - 1)];
                    const int g = G[AT(i - 1, c - 1)];
                    if (a > NEG_LIM)
                    {
                        const int kp = K[AT(i - 1, c - 1)];
                        const int eff = imax(imax(b, kp), BONUS_CONSEC);
                        mv = a + SC_MATCH + eff;
                        kv = (b >= BONUS_BOUNDARY && b > kp) ? b : kp;
                        pj = c - 1;
                    }
                    if (g > NEG_LIM && g + SC_MATCH + b > mv)
                    {
                        mv = g + SC_MATCH + b;
                        kv = b;
                        pj = Gk[AT(i - 1, c - 1)];
                    }
                    if (mv <= NEG_LIM)
                        mv = NEG;
                }
            }
            M[AT(i, c)] = (int16_t)mv;
            K[AT(i, c)] = (int8_t)kv;
            Pj[AT(i, c)] = (int16_t)pj;

            int gv = NEG, gk = -1;
            if (c > 0)
            {
                const int pm = M[AT(i, c - 1)], pg = G[AT(i, c - 1)];
                if (pm > NEG_LIM)
                {
                    gv = pm + SC_GAP_START;
                    gk = c - 1;
                }
                if (pg > NEG_LIM && pg + SC_GAP_EXT > gv)
                {
                    gv = pg + SC_GAP_EXT;
                    gk = Gk[AT(i, c - 1)];
                }
            }
            G[AT(i, c)] = (int16_t)gv;
            Gk[AT(i, c)] = (int16_t)gk;
        }
    }

    int best = NEG, bc = -1;
    for (int c = 0; c < w; c++)
    {
        if (M[AT(m - 1, c)] > best)
        {
            best = M[AT(m - 1, c)];
            bc = c;
        }
    }
    if (bc < 0 || best <= NEG_LIM)
        return -1;

    if (pos)
    {
        int c = bc;
        for (int i = m - 1; i >= 0; i--)
        {
            pos[i] = first + c;
            c = Pj[AT(i, c)];
        }
    }
#undef AT
    return best;
}

/* All terms: sum of scores, or -1. `pos` (capacity FZ_MAX_TERMS*FZ_MAX_TERM_LEN)
 * receives every matched text index, `npos` how many. */
static int fz_match_all(const fuzzy_query *q, const uint32_t *cp,
                        const uint8_t *cls, int n, fz_scratch *sc, int *pos,
                        int *npos)
{
    int total = 0, np = 0;
    for (int t = 0; t < q->nterms; t++)
    {
        int s = fz_match_term(&q->term[t], cp, cls, n, sc, pos ? pos + np : NULL);
        if (s < 0)
            return -1;
        total += s;
        np += q->term[t].len;
    }
    if (npos)
        *npos = np;
    return total;
}

bool fuzzy_query_match(const fuzzy_query *q, const char *utf8, size_t len,
                       int32_t *score)
{
    uint32_t cp[FZ_MAX_TEXT];
    uint8_t cls[FZ_MAX_TEXT];
    int n = fz_fold_text(utf8, len, cp, cls, NULL);
    fz_scratch sc = {0};
    int s = fz_match_all(q, cp, cls, n, &sc, NULL, NULL);
    scratch_free(&sc);
    if (s < 0)
        return false;
    if (score)
        *score = s;
    return true;
}

int fuzzy_query_positions(const fuzzy_query *q, const char *utf8, size_t len,
                          uint16_t *out, int cap)
{
    uint32_t cp[FZ_MAX_TEXT];
    uint8_t cls[FZ_MAX_TEXT];
    uint16_t orig[FZ_MAX_TEXT];
    int n = fz_fold_text(utf8, len, cp, cls, orig);

    int pos[FZ_MAX_TERMS * FZ_MAX_TERM_LEN];
    int np = 0;
    fz_scratch sc = {0};
    int s = fz_match_all(q, cp, cls, n, &sc, pos, &np);
    scratch_free(&sc);
    if (s < 0)
        return -1;

    uint8_t seen[FZ_MAX_TEXT] = {0};
    for (int i = 0; i < np; i++)
        seen[pos[i]] = 1;
    int count = 0;
    for (int i = 0; i < n && count < cap; i++)
        if (seen[i] && (count == 0 || out[count - 1] != orig[i]))
            out[count++] = orig[i];
    return count;
}

/* ------------------------------------------------------------------------ */
/* search                                                                   */
/* ------------------------------------------------------------------------ */

struct fuzzy_search
{
    const fuzzy_index *idx;
    fuzzy_query *q;
    uint32_t *cands; /* ascending ids to scan; NULL = every id */
    size_t total, pos;
    fuzzy_hit *hits;
    size_t nhits, caphits;
    bool done, refined;
    fz_scratch sc;
};

fuzzy_search *fuzzy_search_new(const fuzzy_index *idx, const char *query,
                               const fuzzy_search *parent)
{
    fuzzy_search *s = calloc(1, sizeof(*s));
    if (s == NULL)
        abort();
    s->idx = idx;
    s->q = fuzzy_query_new(query);
    s->total = idx->n;

    if (parent != NULL && parent->done && parent->idx == idx &&
        !fuzzy_query_empty(parent->q) && !fuzzy_query_empty(s->q) &&
        fuzzy_query_extends(s->q, parent->q) && parent->nhits < idx->n)
    {
        /* only the parent's matches can still match: list them in id order
         * through a bitset (cheaper than sorting them) */
        size_t words = (idx->n + 63) / 64;
        uint64_t *bits = calloc(words ? words : 1, sizeof(*bits));
        if (bits == NULL)
            abort();
        for (size_t i = 0; i < parent->nhits; i++)
            bits[parent->hits[i].id >> 6] |= 1ull << (parent->hits[i].id & 63);
        s->cands = fz_realloc(NULL, (parent->nhits ? parent->nhits : 1) *
                                        sizeof(*s->cands));
        size_t k = 0;
        for (size_t w = 0; w < words; w++)
            for (uint64_t b = bits[w]; b; b &= b - 1)
                s->cands[k++] = (uint32_t)(w * 64 + (size_t)fz_ctz64(b));
        free(bits);
        s->total = k;
        s->refined = true;
    }
    return s;
}

void fuzzy_search_free(fuzzy_search *s)
{
    if (s == NULL)
        return;
    fuzzy_query_free(s->q);
    free(s->cands);
    free(s->hits);
    scratch_free(&s->sc);
    free(s);
}

static void hits_push(fuzzy_search *s, uint32_t id, int score)
{
    if (s->nhits == s->caphits)
    {
        s->caphits = s->caphits ? s->caphits * 2 : 256;
        s->hits = fz_realloc(s->hits, s->caphits * sizeof(*s->hits));
    }
    s->hits[s->nhits++] = (fuzzy_hit){id, score};
}

/* Stable counting sorts, least significant key first:
 *   1. length ascending (shorter strings win ties)
 *   2. score descending
 * `hits` arrives in ascending id order, so ties end up in id order. O(n+range). */
static void sort_hits(fuzzy_search *s)
{
    const size_t n = s->nhits;
    if (n < 2)
        return;
    const fz_entry *e = s->idx->e;
    fuzzy_hit *tmp = fz_realloc(NULL, n * sizeof(*tmp));

    size_t cnt[257] = {0};
    for (size_t i = 0; i < n; i++)
    {
        unsigned l = e[s->hits[i].id].len;
        cnt[(l > 255 ? 255 : l) + 1]++;
    }
    for (int i = 0; i < 256; i++)
        cnt[i + 1] += cnt[i];
    for (size_t i = 0; i < n; i++)
    {
        unsigned l = e[s->hits[i].id].len;
        tmp[cnt[l > 255 ? 255 : l]++] = s->hits[i];
    }

    int32_t lo = tmp[0].score, hi = tmp[0].score;
    for (size_t i = 1; i < n; i++)
    {
        if (tmp[i].score < lo)
            lo = tmp[i].score;
        if (tmp[i].score > hi)
            hi = tmp[i].score;
    }
    size_t range = (size_t)(hi - lo) + 1;
    size_t *c2 = calloc(range + 1, sizeof(*c2));
    if (c2 == NULL)
        abort();
    for (size_t i = 0; i < n; i++)
        c2[(size_t)(hi - tmp[i].score) + 1]++;
    for (size_t i = 0; i < range; i++)
        c2[i + 1] += c2[i];
    for (size_t i = 0; i < n; i++)
        s->hits[c2[(size_t)(hi - tmp[i].score)]++] = tmp[i];
    free(c2);
    free(tmp);
}

bool fuzzy_search_run(fuzzy_search *s, uint64_t budget_ns)
{
    if (s->done)
        return true;

    const fuzzy_index *idx = s->idx;
    const uint64_t qmask = s->q->mask;
    const uint64_t t0 = budget_ns == UINT64_MAX ? 0 : fz_now_ns();
    const size_t CHUNK = 256;

    do
    {
        size_t end = s->pos + CHUNK < s->total ? s->pos + CHUNK : s->total;
        for (; s->pos < end; s->pos++)
        {
            uint32_t id = s->cands ? s->cands[s->pos] : (uint32_t)s->pos;
            const fz_entry *e = &idx->e[id];
            if (qmask & ~e->mask)
                continue; /* lacks a character the query needs */
            int sc = fz_match_all(s->q, idx->cp + e->off, idx->cls + e->off,
                                  e->len, &s->sc, NULL, NULL);
            if (sc >= 0)
                hits_push(s, id, sc);
        }
        if (s->pos >= s->total)
            break;
    } while (budget_ns == UINT64_MAX || fz_now_ns() - t0 < budget_ns);

    if (s->pos >= s->total)
    {
        sort_hits(s);
        s->done = true;
    }
    return s->done;
}

bool fuzzy_search_done(const fuzzy_search *s)
{
    return s->done;
}

const fuzzy_hit *fuzzy_search_hits(const fuzzy_search *s, size_t *count)
{
    if (count)
        *count = s->nhits;
    return s->hits;
}

size_t fuzzy_search_scanned(const fuzzy_search *s)
{
    return s->pos;
}

size_t fuzzy_search_total(const fuzzy_search *s)
{
    return s->total;
}

const fuzzy_query *fuzzy_search_query(const fuzzy_search *s)
{
    return s->q;
}

bool fuzzy_search_was_refined(const fuzzy_search *s)
{
    return s->refined;
}
