#include "kara_text.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* utf-8 and character classes                                              */
/* ------------------------------------------------------------------------ */

typedef struct chr
{
    uint32_t cp;
    int off, len; /* bytes in the line's text */
} chr;

static size_t u8dec(const char *s, size_t n, uint32_t *cp)
{
    uint8_t c = (uint8_t)s[0];
    if (c < 0x80)
    {
        *cp = c;
        return 1;
    }
    size_t need = (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (need == 0 || need > n)
    {
        *cp = 0xFFFD;
        return 1;
    }
    uint32_t v = c & (0xFFu >> (need + 1));
    for (size_t k = 1; k < need; k++)
    {
        if (((uint8_t)s[k] & 0xC0) != 0x80)
        {
            *cp = 0xFFFD;
            return 1;
        }
        v = (v << 6) | ((uint8_t)s[k] & 0x3F);
    }
    *cp = v;
    return need;
}

/* All characters of text[from, to). Caller frees. */
static chr *decode_range(const char *text, int from, int to, int *count)
{
    chr *out = malloc((size_t)(to - from + 1) * sizeof(chr));
    int n = 0;
    if (out == NULL)
    {
        *count = 0;
        return NULL;
    }
    for (int i = from; i < to;)
    {
        uint32_t cp;
        size_t k = u8dec(text + i, (size_t)(to - i), &cp);
        out[n++] = (chr){cp, i, (int)k};
        i += (int)k;
    }
    *count = n;
    return out;
}

/* Python's \s */
static bool is_space(uint32_t cp)
{
    return cp == ' ' || (cp >= 9 && cp <= 13) || cp == 0x85 || cp == 0xA0 ||
           cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 ||
           cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000 ||
           (cp >= 0x1C && cp <= 0x1F);
}

/* [A-Za-zÀ-ÿ] */
static bool is_letter(uint32_t cp)
{
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
           (cp >= 0xC0 && cp <= 0xFF);
}

static uint32_t lower(uint32_t cp)
{
    if (cp >= 'A' && cp <= 'Z')
        return cp + 32;
    if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7)
        return cp + 32;
    return cp;
}

static bool is_hira(uint32_t cp)
{
    return cp >= 0x3041 && cp <= 0x3096;
}
static bool is_kata(uint32_t cp)
{
    return cp >= 0x30A1 && cp <= 0x30F6;
}
static bool is_kana(uint32_t cp)
{
    return is_hira(cp) || is_kata(cp) || cp == 0x30FC;
}
static bool is_kanji(uint32_t cp)
{
    return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
           cp == 0x3005 || (cp >= 0xF900 && cp <= 0xFAFF);
}
static bool is_hangul(uint32_t cp)
{
    return cp >= 0xAC00 && cp <= 0xD7AF;
}
static bool is_cjk_ideograph(uint32_t cp)
{
    return cp >= 0x4E00 && cp <= 0x9FFF;
}

/* str.isalnum(), near enough: letters and digits of any script, not the
 * punctuation, symbols and music notes lyrics files use for gaps */
static bool is_alnum_cp(uint32_t cp)
{
    if (cp < 0x80)
        return isalnum((int)cp) != 0;
    if (cp < 0xC0)
        return cp == 0xAA || cp == 0xB5 || cp == 0xBA || cp == 0xB2 ||
               cp == 0xB3 || cp == 0xB9 || (cp >= 0xBC && cp <= 0xBE);
    if (cp == 0xD7 || cp == 0xF7)
        return false;
    if (cp >= 0x2000 && cp <= 0x2BFF)
        return false; /* punctuation, symbols, arrows, music */
    if (cp >= 0x3000 && cp <= 0x303F && cp != 0x3005 && cp != 0x3006)
        return false;
    if (cp >= 0xFE00 && cp <= 0xFE0F)
        return false;
    if (cp >= 0xFF00 && cp <= 0xFF0F)
        return false;
    if (cp >= 0xFF1A && cp <= 0xFF20)
        return false;
    if (cp >= 0x1F000)
        return false;
    if (cp == 0x200B || cp == 0xFEFF || cp == 0x3099 || cp == 0x309A)
        return false;
    return true;
}

/* ------------------------------------------------------------------------ */
/* language                                                                 */
/* ------------------------------------------------------------------------ */

bool kara_lang_from_tag(const char *tag, kara_lang *out)
{
    if (tag == NULL || tag[0] == '\0' || tag[1] == '\0')
        return false;
    char a = (char)tolower((unsigned char)tag[0]);
    char b = (char)tolower((unsigned char)tag[1]);
    if (a == 'i' && b == 'd')
        *out = KARA_LANG_ID;
    else if (a == 'm' && b == 's')
        *out = KARA_LANG_ID;
    else if (a == 'e' && b == 'n')
        *out = KARA_LANG_EN;
    else if (a == 'j' && b == 'a')
        *out = KARA_LANG_JA;
    else if (a == 'k' && b == 'o')
        *out = KARA_LANG_KO;
    else if (a == 'z' && b == 'h')
        *out = KARA_LANG_ZH;
    else
        *out = KARA_LANG_OTHER;
    return true;
}

static bool scan(const char *text, bool (*pred)(uint32_t))
{
    for (size_t i = 0, n = strlen(text); i < n;)
    {
        uint32_t cp;
        i += u8dec(text + i, n - i, &cp);
        if (pred(cp))
            return true;
    }
    return false;
}

static bool is_kana_block(uint32_t cp)
{
    return cp >= 0x3040 && cp <= 0x30FF;
}

bool kara_text_has_kana(const char *text)
{
    return scan(text, is_kana_block);
}

kara_lang kara_guess_latin_lang(const char *all_text)
{
    static const char *id_stop[] = {"yang", "dan",   "di",    "ke",   "dari",
                                    "ini",  "itu",   "untuk", "dengan", "tak",
                                    "tidak", "aku",  "kau",   "ku",   "mu",
                                    "kamu", "saya",  "ada",   "pada", "akan",
                                    "juga", "atau",  "tapi",  "saat"};
    static const char *en_stop[] = {"the", "and", "you", "i",  "to",  "of",
                                    "a",   "in",  "it",  "is", "that", "my",
                                    "me",  "your", "for", "but", "was", "on",
                                    "be",  "with", "all", "just", "we"};
    int ni = 0, ne = 0;
    size_t n = strlen(all_text);
    char word[64];
    size_t wl = 0;
    for (size_t i = 0; i <= n; i++)
    {
        char c = i < n ? (char)tolower((unsigned char)all_text[i]) : ' ';
        if ((c >= 'a' && c <= 'z') || c == '\'')
        {
            if (wl < sizeof(word) - 1)
                word[wl++] = c;
            else
                wl = sizeof(word); /* too long to be a stop word */
            continue;
        }
        if (wl > 0 && wl < sizeof(word))
        {
            word[wl] = '\0';
            for (size_t k = 0; k < sizeof(id_stop) / sizeof(*id_stop); k++)
                ni += strcmp(word, id_stop[k]) == 0;
            for (size_t k = 0; k < sizeof(en_stop) / sizeof(*en_stop); k++)
                ne += strcmp(word, en_stop[k]) == 0;
        }
        wl = 0;
    }
    return ni > ne ? KARA_LANG_ID : KARA_LANG_EN;
}

kara_lang kara_detect_lang(const char *text, kara_lang hint, bool has_hint)
{
    if (scan(text, is_kana_block))
        return KARA_LANG_JA;
    if (scan(text, is_hangul))
        return KARA_LANG_KO;
    if (scan(text, is_cjk_ideograph))
        return (has_hint && hint == KARA_LANG_JA) ? KARA_LANG_JA : KARA_LANG_ZH;
    if (has_hint && hint != KARA_LANG_JA)
        return hint;
    /* No hint, or Japanese tagged but the line is Latin ("oh oh oh"): vote */
    return kara_guess_latin_lang(text);
}

bool kara_is_instrumental(const char *text)
{
    for (size_t i = 0, n = strlen(text); i < n;)
    {
        uint32_t cp;
        i += u8dec(text + i, n - i, &cp);
        if (is_alnum_cp(cp))
            return false;
    }
    return true;
}

/* ------------------------------------------------------------------------ */
/* unit list                                                                */
/* ------------------------------------------------------------------------ */

typedef struct ub
{
    kara_unit *u;
    int n, cap;
    bool oom;
} ub;

static void push(ub *b, int off, int len, float w, bool word_start, int sub,
                 int nsub)
{
    if (b->oom)
        return;
    if (b->n == b->cap)
    {
        int cap = b->cap ? b->cap * 2 : 32;
        kara_unit *p = realloc(b->u, (size_t)cap * sizeof(*p));
        if (p == NULL)
        {
            b->oom = true;
            return;
        }
        b->u = p;
        b->cap = cap;
    }
    b->u[b->n++] = (kara_unit){off, len, w, word_start, (uint8_t)sub,
                               (uint8_t)nsub};
}

/* ------------------------------------------------------------------------ */
/* Latin scripts                                                            */
/* ------------------------------------------------------------------------ */

static bool is_vowel_orth(uint32_t c)
{
    static const uint32_t v[] = {'a',  'e',  'i',  'o',  'u',  'y',  0xE1,
                                 0xE0, 0xE2, 0xE4, 0xE9, 0xE8, 0xEA, 0xEB,
                                 0xED, 0xEC, 0xEE, 0xEF, 0xF3, 0xF2, 0xF4,
                                 0xF6, 0xFA, 0xF9, 0xFB, 0xFC};
    for (size_t i = 0; i < sizeof(v) / sizeof(*v); i++)
        if (v[i] == c)
            return true;
    return false;
}

/* Orthographic split of w (lower case) into n pieces: vowel-group nuclei, the
 * V-CV rule, then merged or split to reach n. Writes the piece lengths in
 * characters to plen (room for wn + 1) and returns how many. */
static int split_orth(const uint32_t *w, int wn, int n, int *plen)
{
    int *ns = malloc((size_t)(wn + 1) * sizeof(int) * 2);
    if (ns == NULL)
    {
        plen[0] = wn;
        return 1;
    }
    int *ne = ns + wn + 1;
    int nn = 0;
    for (int i = 0; i < wn;)
    {
        if (!is_vowel_orth(w[i]))
        {
            i++;
            continue;
        }
        int j = i;
        while (j < wn && is_vowel_orth(w[j]))
            j++;
        ns[nn] = i;
        ne[nn] = j;
        nn++;
        i = j;
    }
    if (nn == 0)
    {
        free(ns);
        plen[0] = wn;
        return 1;
    }
    int cnt = nn;
    int prev = 0;
    for (int k = 0; k + 1 < nn; k++)
    {
        int cons = ns[k + 1] - ne[k];
        int cut = ne[k] + (cons <= 1 ? 0 : cons / 2);
        plen[k] = cut - prev;
        prev = cut;
    }
    plen[nn - 1] = wn - prev;
    free(ns);

    while (cnt > n && cnt > 1)
    {
        int best = 0, bl = plen[0] + plen[1];
        for (int i = 1; i + 1 < cnt; i++)
            if (plen[i] + plen[i + 1] < bl)
            {
                bl = plen[i] + plen[i + 1];
                best = i;
            }
        plen[best] += plen[best + 1];
        for (int i = best + 1; i + 1 < cnt; i++)
            plen[i] = plen[i + 1];
        cnt--;
    }
    while (cnt < n)
    {
        int best = 0;
        for (int i = 1; i < cnt; i++)
            if (plen[i] > plen[best])
                best = i;
        int p = plen[best];
        if (p < 2)
            break;
        for (int i = cnt; i > best + 1; i--)
            plen[i] = plen[i - 1];
        plen[best] = p / 2;
        plen[best + 1] = p - p / 2;
        cnt++;
    }
    return cnt;
}

/* Indonesian and Malay: vowels are nuclei; ai / au / oi are a diphthong only
 * at the end of a word. Digraph consonants ng ny sy kh count as one. Writes
 * the index (into `letters`) where each syllable starts; returns the count. */
static int id_syllables(const chr *letters, int ln, int *start)
{
    int *tok_at = malloc((size_t)(ln + 1) * sizeof(int) * 3);
    if (tok_at == NULL)
    {
        start[0] = 0;
        return 1;
    }
    int *tok_n = tok_at + ln + 1; /* letters in the token */
    int *tok_v = tok_n + ln + 1;  /* is a vowel */
    int nt = 0;
    for (int i = 0; i < ln;)
    {
        uint32_t a = lower(letters[i].cp);
        uint32_t b = i + 1 < ln ? lower(letters[i + 1].cp) : 0;
        int take = 0;
        bool vow = false;
        if ((a == 'n' && (b == 'g' || b == 'y')) || (a == 's' && b == 'y') ||
            (a == 'k' && b == 'h'))
            take = 2;
        else if ((a >= 'a' && a <= 'z') || a == 0xE9 || a == 0xE8)
        {
            take = 1;
            vow = a == 'a' || a == 'e' || a == 'i' || a == 'o' || a == 'u' ||
                  a == 0xE9 || a == 0xE8;
        }
        if (take == 0)
        {
            i++; /* findall skips what it cannot match */
            continue;
        }
        tok_at[nt] = i;
        tok_n[nt] = take;
        tok_v[nt] = vow;
        nt++;
        i += take;
    }

    int *nuc_first = malloc((size_t)(nt + 1) * sizeof(int) * 2);
    if (nuc_first == NULL)
    {
        free(tok_at);
        start[0] = 0;
        return 1;
    }
    int *nuc_last = nuc_first + nt + 1;
    int nn = 0;
    for (int i = 0; i < nt; i++)
    {
        if (!tok_v[i])
            continue;
        bool merge = false;
        if (nn > 0 && i == nuc_last[nn - 1] + 1 && i == nt - 1 && i > 0 &&
            tok_n[i] == 1 && tok_n[i - 1] == 1)
        {
            uint32_t p = lower(letters[tok_at[i - 1]].cp);
            uint32_t q = lower(letters[tok_at[i]].cp);
            merge = (p == 'a' && q == 'i') || (p == 'a' && q == 'u') ||
                    (p == 'o' && q == 'i');
        }
        if (merge)
            nuc_last[nn - 1] = i;
        else
        {
            nuc_first[nn] = i;
            nuc_last[nn] = i;
            nn++;
        }
    }

    int cnt = 0;
    if (nn == 0)
    {
        start[cnt++] = 0;
    }
    else
    {
        start[cnt++] = 0;
        for (int k = 0; k + 1 < nn; k++)
        {
            int gap = nuc_first[k + 1] - nuc_last[k] - 1;
            int cut = nuc_last[k] + 1 + (gap <= 1 ? 0 : gap / 2);
            start[cnt++] = cut < nt ? tok_at[cut] : ln;
        }
    }
    free(tok_at);
    free(nuc_first);
    return cnt;
}

static bool is_function_word(const char *w)
{
    static const char *f[] = {"a",   "an", "the", "of",   "to",   "in",  "on",
                              "at",  "and", "or", "but",  "is",   "it",  "its",
                              "i",   "i'm", "you", "your", "my",   "me",  "we",
                              "be",  "as", "for", "with", "that", "this"};
    for (size_t i = 0; i < sizeof(f) / sizeof(*f); i++)
        if (strcmp(w, f[i]) == 0)
            return true;
    return false;
}

static float en_weight(int stress)
{
    return stress == 1 ? 1.15f : stress == 2 ? 1.0f : 0.85f;
}

/* Units of one hyphen-free piece of a word: text[ps, pe). */
static void piece_units(ub *b, const char *text, int ps, int pe,
                        kara_lang lang, const kara_lex *lex, bool *word_start)
{
    int cn;
    chr *cs = decode_range(text, ps, pe, &cn);
    if (cs == NULL)
        return;
    int *starts = malloc((size_t)(cn + 2) * sizeof(int));
    float *wts = malloc((size_t)(cn + 2) * sizeof(float));
    if (starts == NULL || wts == NULL)
        goto out;

    int n = 0;
    if (lang == KARA_LANG_EN)
    {
        /* the word as CMUdict sees it: a-z and ', lower case, no ' at the ends
         */
        chr *w = malloc((size_t)(cn + 1) * sizeof(chr));
        if (w == NULL)
            goto out;
        int wn = 0;
        for (int i = 0; i < cn; i++)
        {
            uint32_t c = cs[i].cp;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'')
            {
                w[wn] = cs[i];
                w[wn].cp = lower(c);
                wn++;
            }
        }
        int lo = 0, hi = wn;
        while (lo < hi && w[lo].cp == '\'')
            lo++;
        while (hi > lo && w[hi - 1].cp == '\'')
            hi--;
        /* function-word test uses the word with its apostrophes */
        char lw[64];
        int ln = 0;
        for (int i = 0; i < wn && ln < 62; i++)
            lw[ln++] = (char)w[i].cp;
        lw[ln] = '\0';

        if (hi > lo)
        {
            char key[96];
            int kn = 0;
            for (int i = lo; i < hi && kn < 94; i++)
                key[kn++] = (char)w[i].cp;
            key[kn] = '\0';

            uint8_t stress[32];
            int ns = kara_lex_en(lex, key, stress, 32);
            if (ns < 0)
            {
                char nokey[96];
                int nk = 0;
                for (int i = 0; i < kn; i++)
                    if (key[i] != '\'')
                        nokey[nk++] = key[i];
                nokey[nk] = '\0';
                if (nk != kn)
                    ns = kara_lex_en(lex, nokey, stress, 32);
            }
            if (ns > 32)
                ns = 32;
            if (ns < 0)
            {
                /* vowel groups, minus a silent final e */
                int m = hi - lo;
                uint32_t *ww = malloc((size_t)(m + 1) * sizeof(uint32_t));
                if (ww == NULL)
                {
                    free(w);
                    goto out;
                }
                for (int i = 0; i < m; i++)
                    ww[i] = w[lo + i].cp;
                int mm = m;
                if (m > 3 && ww[m - 1] == 'e' && m >= 2)
                {
                    uint32_t p = ww[m - 2];
                    if (!(p == 'a' || p == 'e' || p == 'i' || p == 'o' ||
                          p == 'u' || p == 'y'))
                        mm = m - 1;
                }
                int groups = 0;
                for (int i = 0; i < mm;)
                {
                    if (!is_vowel_orth(ww[i]))
                    {
                        i++;
                        continue;
                    }
                    while (i < mm && is_vowel_orth(ww[i]))
                        i++;
                    groups++;
                }
                free(ww);
                ns = groups < 1 ? 1 : groups;
                if (ns > 32)
                    ns = 32;
                for (int i = 0; i < ns; i++)
                    stress[i] = i == 0 ? 1 : 0;
            }

            uint32_t *ww = malloc((size_t)(hi - lo + 1) * sizeof(uint32_t));
            int *plen = malloc((size_t)(hi - lo + 2) * sizeof(int));
            if (ww == NULL || plen == NULL)
            {
                free(ww);
                free(plen);
                free(w);
                goto out;
            }
            for (int i = lo; i < hi; i++)
                ww[i - lo] = w[i].cp;
            int np = split_orth(ww, hi - lo, ns, plen);
            int at = lo;
            for (int k = 0; k < np; k++)
            {
                starts[n] = k == 0 ? 0 : w[at].off - ps;
                int st = k < ns ? stress[k] : 0;
                float wt = en_weight(st);
                if (np == 1 && is_function_word(lw))
                    wt = 0.75f;
                wts[n] = wt;
                n++;
                at += plen[k];
            }
            free(ww);
            free(plen);
        }
        free(w);
    }
    else
    {
        chr *l = malloc((size_t)(cn + 1) * sizeof(chr));
        if (l == NULL)
            goto out;
        int ln = 0;
        for (int i = 0; i < cn; i++)
            if (is_letter(cs[i].cp))
                l[ln++] = cs[i];

        if (lang == KARA_LANG_ID)
        {
            int *st = malloc((size_t)(ln + 2) * sizeof(int));
            if (st == NULL)
            {
                free(l);
                goto out;
            }
            int k = ln > 0 ? id_syllables(l, ln, st) : 0;
            for (int i = 0; i < k; i++)
            {
                starts[n] = i == 0 ? 0 : (st[i] < ln ? l[st[i]].off - ps : pe - ps);
                wts[n] = 1.0f;
                n++;
            }
            free(st);
        }
        else
        {
            uint32_t *ww = malloc((size_t)(ln + 1) * sizeof(uint32_t));
            int *plen = malloc((size_t)(ln + 2) * sizeof(int));
            if (ww == NULL || plen == NULL)
            {
                free(ww);
                free(plen);
                free(l);
                goto out;
            }
            int groups = 0;
            for (int i = 0; i < ln; i++)
                ww[i] = lower(l[i].cp);
            for (int i = 0; i < ln;)
            {
                if (!is_vowel_orth(ww[i]))
                {
                    i++;
                    continue;
                }
                while (i < ln && is_vowel_orth(ww[i]))
                    i++;
                groups++;
            }
            if (ln > 0)
            {
                int np = split_orth(ww, ln, groups < 1 ? 1 : groups, plen);
                int at = 0;
                for (int k = 0; k < np; k++)
                {
                    starts[n] = k == 0 ? 0 : l[at].off - ps;
                    wts[n] = 1.0f;
                    n++;
                    at += plen[k];
                }
            }
            free(ww);
            free(plen);
        }
        free(l);
    }

    for (int k = 0; k < n; k++)
    {
        int s = ps + starts[k];
        int e = k + 1 < n ? ps + starts[k + 1] : pe;
        push(b, s, e - s, wts[k], *word_start && k == 0, 0, 1);
    }
    if (n > 0)
        *word_start = false;
out:
    free(starts);
    free(wts);
    free(cs);
}

static void latin_units(ub *b, const char *text, kara_lang lang,
                        const kara_lex *lex)
{
    int len = (int)strlen(text);
    int cn;
    chr *cs = decode_range(text, 0, len, &cn);
    if (cs == NULL)
        return;

    for (int i = 0; i < cn;)
    {
        if (is_space(cs[i].cp))
        {
            i++;
            continue;
        }
        int j = i;
        bool has_letter = false;
        while (j < cn && !is_space(cs[j].cp))
        {
            has_letter |= is_letter(cs[j].cp);
            j++;
        }
        if (has_letter)
        {
            bool word_start = true;
            /* hyphen, en dash, em dash split a token into pieces */
            int ps = i;
            for (int k = i; k <= j; k++)
            {
                bool sep = k < j && (cs[k].cp == '-' || cs[k].cp == 0x2013 ||
                                     cs[k].cp == 0x2014);
                if (k < j && !sep)
                    continue;
                bool piece_letter = false;
                for (int q = ps; q < k; q++)
                    piece_letter |= is_letter(cs[q].cp);
                if (piece_letter)
                {
                    int pb = cs[ps].off;
                    int pe = cs[k - 1].off + cs[k - 1].len;
                    piece_units(b, text, pb, pe, lang, lex, &word_start);
                }
                ps = k + 1;
            }
        }
        i = j;
    }
    free(cs);
}

/* ------------------------------------------------------------------------ */
/* Japanese                                                                 */
/* ------------------------------------------------------------------------ */

static uint32_t to_hira(uint32_t cp)
{
    return is_kata(cp) ? cp - 0x60 : cp;
}

static bool in_set(uint32_t c, const uint32_t *set, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (set[i] == c)
            return true;
    return false;
}

typedef struct mora
{
    int first, count; /* characters of the run */
    float weight;
} mora;

/* Groups kana into morae: small ya yu yo and small vowels join the kana
 * before them (small vowels not after a bare vowel), sokuon, moraic n and the
 * long vowel mark stand alone. Same rules as text_units.morae(). */
static int group_morae(const uint32_t *k, int n, mora *out)
{
    static const uint32_t small_y[] = {0x3083, 0x3085, 0x3087};
    static const uint32_t small_v[] = {0x3041, 0x3043, 0x3045, 0x3047, 0x3049};
    static const uint32_t vowel_only[] = {0x3042, 0x3044, 0x3046, 0x3048, 0x304A};
    static const uint32_t stop[] = {0x3063, 0x3093, 0x30FC};
    int m = 0;
    uint32_t last = 0;
    for (int i = 0; i < n; i++)
    {
        uint32_t c = to_hira(k[i]);
        bool join = false;
        if (m > 0)
        {
            bool prev_stop = in_set(last, stop, 3);
            if (in_set(c, small_y, 3) && !prev_stop &&
                !in_set(last, small_y, 3))
                join = true;
            else if (in_set(c, small_v, 5) && !prev_stop &&
                     !in_set(last, vowel_only, 5) && !in_set(last, small_v, 5))
                join = true;
        }
        if (join)
        {
            out[m - 1].count++;
            last = c;
        }
        else
        {
            out[m++] = (mora){i, 1, 1.0f};
            last = c;
        }
    }
    for (int i = 0; i < m; i++)
    {
        if (out[i].count == 1)
        {
            uint32_t c = to_hira(k[out[i].first]);
            if (c == 0x3063)
                out[i].weight = 0.6f; /* っ */
            else if (c == 0x3093)
                out[i].weight = 0.9f; /* ん */
        }
    }
    return m;
}

/* c characters sung over m units: with more units than characters a glyph
 * is shared (sub / nsub), with fewer a unit covers several glyphs. */
static void spread(ub *b, const chr *ch, int c, int m, const float *w,
                   bool word_start)
{
    if (m <= 0 || c <= 0)
        return;
    if (m >= c)
    {
        int j = 0;
        for (int k = 0; k < c; k++)
        {
            int nk = ((k + 1) * m) / c - (k * m) / c;
            for (int s = 0; s < nk; s++, j++)
                push(b, ch[k].off, ch[k].len, w[j], word_start && j == 0, s,
                     nk);
        }
        return;
    }
    for (int j = 0; j < m; j++)
    {
        int k0 = (j * c + m - 1) / m;
        int k1 = ((j + 1) * c + m - 1) / m;
        if (k1 <= k0)
            k1 = k0 + 1;
        if (k1 > c)
            k1 = c;
        push(b, ch[k0].off, ch[k1 - 1].off + ch[k1 - 1].len - ch[k0].off, w[j],
             word_start && j == 0, 0, 1);
    }
}

#define JA_KANJI_FALLBACK 2 /* morae a kanji gets when no reading is known */

static void ja_units(ub *b, const char *text, const kara_lex *lex)
{
    int len = (int)strlen(text);
    int cn;
    chr *cs = decode_range(text, 0, len, &cn);
    if (cs == NULL)
        return;
    uint32_t *cps = malloc((size_t)(cn + 1) * sizeof(uint32_t));
    mora *mo = malloc((size_t)(cn + 64) * sizeof(mora));
    float *wts = malloc((size_t)(cn + 64) * sizeof(float));
    if (cps == NULL || mo == NULL || wts == NULL)
        goto out;
    for (int i = 0; i < cn; i++)
        cps[i] = cs[i].cp;

    bool joined = false; /* the previous character belonged to a unit */
    for (int i = 0; i < cn;)
    {
        uint32_t c = cps[i];
        if (is_kana(c))
        {
            int j = i;
            while (j < cn && is_kana(cps[j]))
                j++;
            int m = group_morae(cps + i, j - i, mo);
            for (int q = 0; q < m; q++)
            {
                int f = i + mo[q].first;
                int l = cs[f + mo[q].count - 1].off +
                        cs[f + mo[q].count - 1].len - cs[f].off;
                push(b, cs[f].off, l, mo[q].weight, !joined && q == 0, 0, 1);
            }
            joined = true;
            i = j;
        }
        else if (is_kanji(c))
        {
            uint32_t reading[64];
            int rn = 0;
            int used = kara_lex_ja(lex, cps + i, cn - i, reading, 64, &rn);
            int m;
            if (used > 0 && rn > 0)
            {
                m = group_morae(reading, rn, mo);
                for (int q = 0; q < m; q++)
                    wts[q] = mo[q].weight;
            }
            else
            {
                used = 1;
                m = JA_KANJI_FALLBACK;
                for (int q = 0; q < m; q++)
                    wts[q] = 1.0f;
            }
            spread(b, cs + i, used, m, wts, !joined);
            joined = true;
            i += used;
        }
        else
        {
            joined = false;
            i++;
        }
    }
out:
    free(cs);
    free(cps);
    free(mo);
    free(wts);
}

/* ------------------------------------------------------------------------ */
/* Korean, Chinese                                                          */
/* ------------------------------------------------------------------------ */

static void block_units(ub *b, const char *text, kara_lang lang)
{
    for (int i = 0, n = (int)strlen(text); i < n;)
    {
        uint32_t cp;
        int k = (int)u8dec(text + i, (size_t)(n - i), &cp);
        if (lang == KARA_LANG_KO ? is_hangul(cp) : is_cjk_ideograph(cp))
            push(b, i, k, 1.0f, true, 0, 1);
        i += k;
    }
}

/* ------------------------------------------------------------------------ */

kara_units *kara_units_build(const char *text, kara_lang lang,
                             const kara_lex *lex)
{
    kara_units *u = calloc(1, sizeof(*u));
    if (u == NULL)
        return NULL;
    u->lang = lang;

    ub b = {0};
    switch (lang)
    {
    case KARA_LANG_JA:
        ja_units(&b, text, lex);
        break;
    case KARA_LANG_KO:
    case KARA_LANG_ZH:
        block_units(&b, text, lang);
        break;
    default:
        latin_units(&b, text, lang, lex);
        break;
    }
    if (b.oom)
    {
        free(b.u);
        free(u);
        return NULL;
    }
    u->u = b.u;
    u->n = b.n;
    return u;
}

void kara_units_free(kara_units *u)
{
    if (u == NULL)
        return;
    free(u->u);
    free(u);
}
