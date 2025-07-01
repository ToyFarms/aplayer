#ifndef __DS_H
#define __DS_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef struct str_t
{
    char *buf;
    size_t len;
    size_t capacity;
} str_t;

typedef struct strview_t
{
    char *buf;
    size_t len;
} strview_t;

typedef struct str_tokenizer_t
{
    bool initial;
    char *next;
    const char *delim;
    size_t delim_len;
} str_tokenizer_t;

#define strv(view)                                                             \
    ((str_t){.buf = (view).buf, .len = (view).len, .capacity = 0})
#define view(str) ((strview_t){.buf = (str).buf, .len = (str).len})

#define STR_SPLIT(s, token, delim)                                             \
    for (str_tokenizer_t __tok,                                                \
         *__ptok = (str_tokenizer_init(&__tok, &s, delim), &__tok);            \
         __ptok != NULL; __ptok = NULL)                                        \
        for (strview_t token; str_tokenizer_next(__ptok, &token); (void)0)

str_t str_create();
str_t str_new(const char *s);
str_t str_alloc(size_t capacity);
void str_free(str_t *str);
str_t *str_resize(str_t *str, size_t new);

str_t *str_cat(str_t *str, const char *s);
str_t *str_catch(str_t *str, char ch);
str_t *str_catlen(str_t *str, const char *s, size_t len);
str_t *str_cat_str(str_t *dst, const str_t *src);
str_t *str_cat_strlen(str_t *dst, const str_t *src, size_t len);
// faster version only for number concatenation
str_t *str_catf_d(str_t *str, const char *fmt, ...);
str_t *str_catf(str_t *str, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
str_t *str_catfv(str_t *str, const char *fmt, va_list args);
str_t *str_catwcs(str_t *str, const wchar_t *ws);
str_t *str_catwch(str_t *str, const wchar_t wc);

wchar_t *str_decode(const str_t *str);
str_t str_encode(const wchar_t *utf16);

str_t *str_repeat_char(str_t *str, char ch, size_t n, const char *sep);
str_t *str_repeat_wchar(str_t *str, wchar_t wc, size_t n, const wchar_t *sep);
str_t *str_repeat_cstr(str_t *str, const char *s, size_t n, const char *sep);
str_t *str_repeat_wcs(str_t *str, const wchar_t *ws, size_t n,
                      const wchar_t *sep);
str_t *str_repeat_str(str_t *str, const str_t *src, size_t n, const char *sep);

void str_tokenizer_init(str_tokenizer_t *tok, const str_t *s,
                        const char *delim);
bool str_tokenizer_next(str_tokenizer_t *tok, strview_t *view);

long str_parse(strview_t view, int base);
float str_parsef(strview_t view);
long double str_parseld(strview_t view);

#endif /* __DS_H */
