#ifndef __DS_H
#define __DS_H

#include <stdio.h>
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

str_t str_create();
str_t str_new(const char *s);
str_t str_alloc(size_t capacity);
void str_free(str_t *str);
str_t *str_resize(str_t *str, size_t new);

str_t *str_cat(str_t *str, const char *s);
str_t *str_catch(str_t *str, char ch);
str_t *str_catlen(str_t *str, const char *s, size_t len);
str_t *str_cat_str(str_t *dst, const str_t *src);
// faster version only for number concatenation
str_t *str_catf_d(str_t *str, const char *fmt, ...);
str_t *str_catf(str_t *str, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
str_t *str_catfv(str_t *str, const char *fmt, va_list args);
str_t *str_catw(str_t *str, wchar_t *ws);
str_t *str_catwch(str_t *str, wchar_t wc);

#endif /* __DS_H */
