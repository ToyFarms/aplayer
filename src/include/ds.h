#ifndef __DS_H
#define __DS_H

#include <stdio.h>
#include <wchar.h>

typedef struct string_t
{
    char *buf;
    size_t len;
    size_t capacity;
} string_t;

string_t string_create();
string_t string_alloc(size_t capacity);
void string_free(string_t *str);
string_t *string_resize(string_t *str, size_t new);

string_t *string_cat(string_t *str, const char *s);
string_t *string_catlen(string_t *str, const char *s, size_t len);
string_t *string_cat_str(string_t *str, string_t *s);
string_t *string_catf_d(string_t *str, const char *fmt, ...);
__attribute__((format(printf, 2, 3))) string_t *string_catf(string_t *str,
                                                            const char *fmt,
                                                            ...);
string_t *string_catfv(string_t *str, const char *fmt, va_list args);
string_t *string_catw(string_t *str, wchar_t *ws);
string_t *string_catwch(string_t *str, wchar_t wc);

#endif /* __DS_H */
