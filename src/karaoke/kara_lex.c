#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L /* readlink, even under -std=c17 */
#endif

#include "kara_lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <unistd.h>
#endif

/* An open-addressing table of strings that live in one blob. */
typedef struct table
{
    char *blob;
    size_t blob_len;
    uint32_t *slot; /* offset into blob + 1 of the key; 0 empty */
    uint32_t *val;  /* offset into blob of the value */
    uint32_t mask;
    uint32_t count;
    int max_key_chars; /* longest key, in code points (Japanese lookup) */
} table;

struct kara_lex
{
    table en, ja;
};

static uint32_t fnv(const char *s, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ (uint8_t)s[i]) * 16777619u;
    return h;
}

static char *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > (long)256 * 1024 * 1024)
    {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n)
    {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[n] = '\0';
    *len = (size_t)n;
    return buf;
}

static int utf8_chars(const char *s, size_t n)
{
    int c = 0;
    for (size_t i = 0; i < n; i++)
        if (((uint8_t)s[i] & 0xC0) != 0x80)
            c++;
    return c;
}

/* Lines are `key <sep> value`; the blob is edited in place (separators and
 * newlines become NULs) so keys and values are plain C strings. */
static void table_load(table *t, const char *path, char sep)
{
    size_t len = 0;
    char *blob = slurp(path, &len);
    if (blob == NULL)
        return;

    size_t lines = 0;
    for (size_t i = 0; i < len; i++)
        lines += blob[i] == '\n';
    size_t cap = 16;
    while (cap < lines * 2 + 2)
        cap <<= 1;
    uint32_t *slot = calloc(cap, sizeof(uint32_t));
    uint32_t *val = calloc(cap, sizeof(uint32_t));
    if (slot == NULL || val == NULL)
    {
        free(blob);
        free(slot);
        free(val);
        return;
    }

    t->mask = (uint32_t)cap - 1;
    size_t pos = 0;
    while (pos < len)
    {
        size_t eol = pos;
        while (eol < len && blob[eol] != '\n')
            eol++;
        size_t end = eol;
        if (end > pos && blob[end - 1] == '\r')
            end--;
        size_t sp = pos;
        while (sp < end && blob[sp] != sep)
            sp++;
        if (sp > pos && sp < end && blob[pos] != '#')
        {
            size_t klen = sp - pos;
            uint32_t h = fnv(blob + pos, klen) & t->mask;
            while (slot[h] != 0)
                h = (h + 1) & t->mask;
            slot[h] = (uint32_t)pos + 1;
            val[h] = (uint32_t)(sp + 1);
            int kc = utf8_chars(blob + pos, klen);
            if (kc > t->max_key_chars)
                t->max_key_chars = kc;
            blob[sp] = '\0';
            blob[end] = '\0';
            t->count++;
        }
        pos = eol + 1;
    }
    t->blob = blob;
    t->blob_len = len;
    t->slot = slot;
    t->val = val;
}

static const char *table_get(const table *t, const char *key, size_t klen)
{
    if (t->slot == NULL)
        return NULL;
    uint32_t h = fnv(key, klen) & t->mask;
    while (t->slot[h] != 0)
    {
        const char *k = t->blob + (t->slot[h] - 1);
        if (strlen(k) == klen && memcmp(k, key, klen) == 0)
            return t->blob + t->val[h];
        h = (h + 1) & t->mask;
    }
    return NULL;
}

static void table_free(table *t)
{
    free(t->blob);
    free(t->slot);
    free(t->val);
    *t = (table){0};
}

/* the directory of the running executable, or "" */
static void exe_dir(char *out, size_t cap)
{
    out[0] = '\0';
#if defined(_WIN32)
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
    {
        out[0] = '\0';
        return;
    }
    out[n] = '\0';
    char *slash = strrchr(out, '/');
#endif
    if (slash != NULL)
        *slash = '\0';
    else
        out[0] = '\0';
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

static int has_lexicon(const char *dir)
{
    char p[1200];
    snprintf(p, sizeof(p), "%s/en_syllables.txt", dir);
    if (file_exists(p))
        return 1;
    snprintf(p, sizeof(p), "%s/ja_readings.txt", dir);
    return file_exists(p);
}

kara_lex *kara_lex_open(const char *dir)
{
    kara_lex *lex = calloc(1, sizeof(*lex));
    if (lex == NULL)
        return NULL;

    char buf[1100];
    const char *use = NULL;
    if (dir != NULL && dir[0] != '\0' && has_lexicon(dir))
        use = dir;
    const char *env = getenv("APLAYER_LEXICON_DIR");
    if (use == NULL && env != NULL && env[0] != '\0' && has_lexicon(env))
        use = env;
    if (use == NULL)
    {
        char exe[1024];
        exe_dir(exe, sizeof(exe));
        if (exe[0] != '\0')
        {
            snprintf(buf, sizeof(buf), "%s/lexicon", exe);
            if (has_lexicon(buf))
                use = buf;
        }
    }
    if (use == NULL && has_lexicon("lexicon"))
        use = "lexicon";
    if (use == NULL)
        return lex;

    char p[1200];
    snprintf(p, sizeof(p), "%s/en_syllables.txt", use);
    table_load(&lex->en, p, ' ');
    snprintf(p, sizeof(p), "%s/ja_readings.txt", use);
    table_load(&lex->ja, p, '\t');
    return lex;
}

void kara_lex_close(kara_lex *lex)
{
    if (lex == NULL)
        return;
    table_free(&lex->en);
    table_free(&lex->ja);
    free(lex);
}

int kara_lex_has_en(const kara_lex *lex)
{
    return lex != NULL && lex->en.count > 0;
}

int kara_lex_has_ja(const kara_lex *lex)
{
    return lex != NULL && lex->ja.count > 0;
}

int kara_lex_en(const kara_lex *lex, const char *word, uint8_t *stress, int cap)
{
    if (!kara_lex_has_en(lex) || word == NULL)
        return -1;
    const char *v = table_get(&lex->en, word, strlen(word));
    if (v == NULL)
        return -1;
    int n = 0;
    for (; v[n] >= '0' && v[n] <= '2'; n++)
        if (n < cap)
            stress[n] = (uint8_t)(v[n] - '0');
    return n;
}

static size_t u8enc(uint32_t cp, char *o)
{
    if (cp < 0x80)
    {
        o[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800)
    {
        o[0] = (char)(0xC0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000)
    {
        o[0] = (char)(0xE0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    o[0] = (char)(0xF0 | (cp >> 18));
    o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    o[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static int u8dec_one(const char *s, uint32_t *cp)
{
    uint8_t c = (uint8_t)s[0];
    if (c < 0x80)
    {
        *cp = c;
        return 1;
    }
    if ((c >> 5) == 6)
    {
        *cp = ((uint32_t)(c & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
        return 2;
    }
    if ((c >> 4) == 14)
    {
        *cp = ((uint32_t)(c & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) |
              ((uint8_t)s[2] & 0x3F);
        return 3;
    }
    *cp = ((uint32_t)(c & 0x07) << 18) | (((uint8_t)s[1] & 0x3F) << 12) |
          (((uint8_t)s[2] & 0x3F) << 6) | ((uint8_t)s[3] & 0x3F);
    return 4;
}

int kara_lex_ja(const kara_lex *lex, const uint32_t *cps, int n,
                uint32_t *reading, int cap, int *reading_n)
{
    if (!kara_lex_has_ja(lex) || n <= 0)
        return 0;
    int maxn = lex->ja.max_key_chars < n ? lex->ja.max_key_chars : n;
    char key[4 * 32 + 4];
    if (maxn > 32)
        maxn = 32;
    size_t off[34];
    size_t len = 0;
    off[0] = 0;
    for (int i = 0; i < maxn; i++)
    {
        len += u8enc(cps[i], key + len);
        off[i + 1] = len;
    }
    for (int m = maxn; m >= 1; m--)
    {
        const char *v = table_get(&lex->ja, key, off[m]);
        if (v == NULL)
            continue;
        int rn = 0;
        for (const char *p = v; *p != '\0'; rn++)
        {
            uint32_t cp;
            p += u8dec_one(p, &cp);
            if (rn < cap)
                reading[rn] = cp;
        }
        *reading_n = rn < cap ? rn : cap;
        return m;
    }
    return 0;
}
