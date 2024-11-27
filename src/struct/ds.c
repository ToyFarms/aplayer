#include "ds.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define offset(str) ((str)->buf + (str)->len)

static size_t get_avail(str_t *str)
{
    return str->capacity - str->len;
}

static void ensure_size(str_t *str, size_t required)
{
    if (get_avail(str) >= required)
        return;

    size_t capacity = str->capacity;
    while (capacity - str->len < required)
        capacity *= 2;

    string_resize(str, capacity);
}

static void append_null(str_t *str)
{
    ensure_size(str, 1);
    str->buf[str->len] = '\0';
}

str_t string_create()
{
    errno = 0;
    str_t str = {0};

    str.len = 0;
    str.capacity = 256;
    str.buf = calloc(1, str.capacity);
    if (str.buf == NULL)
        errno = -ENOMEM;

    return str;
}

str_t string_new(const char *s)
{
    errno = 0;
    str_t str = {0};

    str.len = strlen(s);
    str.capacity = str.len * 2;
    str.buf = strdup(s);
    if (str.buf == NULL)
        errno = -ENOMEM;

    return str;
}

str_t string_alloc(size_t capacity)
{
    str_t str = {0};

    str.len = 0;
    str.capacity = capacity;
    str.buf = calloc(1, str.capacity);
    if (str.buf == NULL)
        errno = -ENOMEM;

    return str;
}

void string_free(str_t *str)
{
    if (str == NULL)
        return;

    free(str->buf);
    memset(str, 0, sizeof(*str));
}

str_t *string_resize(str_t *str, size_t new)
{
    assert(str);

    if (str->capacity == new)
        return str;

    if (str->len > new)
        str->len = new;
    str->capacity = new;
    str->buf = realloc(str->buf, str->capacity);
    if (str->buf == NULL)
        errno = -ENOMEM;

    return str;
}

str_t *string_cat(str_t *str, const char *s)
{
    assert(str && s);

    size_t len = strlen(s);
    ensure_size(str, len + 1);
    memcpy(offset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *string_catch(str_t *str, char ch)
{
    assert(str);

    ensure_size(str, 2);
    str->buf[str->len++] = ch;
    str->buf[str->len] = '\0';

    return str;
}

str_t *string_catlen(str_t *str, const char *s, size_t len)
{
    assert(str && s);

    ensure_size(str, len + 1);
    memcpy(offset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *string_cat_str(str_t *dst, const str_t *src)
{
    assert(dst);

    ensure_size(dst, src->len + 1);
    memcpy(offset(dst), src->buf, src->len);

    dst->len += src->len;
    append_null(dst);

    return dst;
}

str_t *string_catf(str_t *str, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    size_t needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    va_start(args, fmt);

    ensure_size(str, needed + 1);
    vsprintf(offset(str), fmt, args);
    str->len += needed;

    va_end(args);

    return str;
}

str_t *string_catfv(str_t *str, const char *fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    size_t needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    ensure_size(str, needed + 1);

    vsprintf(offset(str), fmt, args);
    str->len += needed;

    return str;
}

static int int_to_str(long long int value, char *buffer)
{
    int len = 0;

    if (value < 0)
    {
        buffer[len++] = '-';
        value = -value;
    }

    char temp[68];
    int temp_len = 0;
    do
    {
        temp[temp_len++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    for (int i = temp_len - 1; i >= 0; --i)
        buffer[len++] = temp[i];

    return len;
}

str_t *string_catf_d(str_t *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    const char *f = fmt;
    char c = 0;
    char n = 0;
    char digit[68] = {0};

    while ((c = *f))
    {
        n = *(f + 1);

        if (c == '%' && n == 'd')
        {
            int v = va_arg(args, int);
            int len = int_to_str(v, digit);
            string_catlen(str, digit, len);

            f += 2;
        }
        else if (c == '%' && n == '%')
        {
            ensure_size(str, 1);
            str->buf[str->len++] = '%';
            f += 2;
        }
        else if (c == '%')
        {
            ensure_size(str, 2);
            str->buf[str->len++] = '%';
            str->buf[str->len++] = n;
            f += 2;
        }
        else
        {
            ensure_size(str, 1);
            str->buf[str->len++] = c;
            f++;
        }
    }

    append_null(str);
    va_end(args);

    return str;
}

str_t *string_catw(str_t *str, wchar_t *ws)
{
    size_t len = wcstombs(NULL, ws, 0);
    if (len == (size_t)-1)
    {
        log_error("Failed to get the required length for an mbs\n");
        errno = -EINVAL;
        return str;
    }

    ensure_size(str, len + 1);

    size_t length = wcstombs(offset(str), ws, len + 1);
    if (length == (size_t)-1)
    {
        log_error("Failed to convert wide string to mbs\n");
        errno = -EINVAL;
        return str;
    }

    str->len += length;

    return str;
}

str_t *string_catwch(str_t *str, wchar_t wc)
{
    ensure_size(str, MB_CUR_MAX + 1);

    size_t length = wctomb(offset(str), wc);
    if (length < 0)
    {
        log_error("Failed to convert wide char to mbs\n");
        errno = -EINVAL;
        return str;
    }

    str->len += length;
    append_null(str);

    return str;
}
