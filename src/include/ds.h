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

str_t string_create();
str_t string_new(const char *s);
str_t string_alloc(size_t capacity);
void string_free(str_t *str);
str_t *string_resize(str_t *str, size_t new);

str_t *string_cat(str_t *str, const char *s);
str_t *string_catch(str_t *str, char ch);
str_t *string_catlen(str_t *str, const char *s, size_t len);
str_t *string_cat_str(str_t *dst, const str_t *src);
// faster version only for number concatenation
str_t *string_catf_d(str_t *str, const char *fmt, ...);
str_t *string_catf(str_t *str, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
str_t *string_catfv(str_t *str, const char *fmt, va_list args);
str_t *string_catw(str_t *str, wchar_t *ws);
str_t *string_catwch(str_t *str, wchar_t wc);

#endif /* __DS_H */
