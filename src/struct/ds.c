#include "ds.h"
#include "logger.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
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

    str_resize(str, capacity);
}

static void append_null(str_t *str)
{
    ensure_size(str, 1);
    str->buf[str->len] = '\0';
}

str_t str_create()
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

str_t str_new(const char *s)
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

str_t str_alloc(size_t capacity)
{
    str_t str = {0};

    str.len = 0;
    str.capacity = capacity;
    str.buf = calloc(1, str.capacity);
    if (str.buf == NULL)
        errno = -ENOMEM;

    return str;
}

void str_free(str_t *str)
{
    if (str == NULL)
        return;

    free(str->buf);
    memset(str, 0, sizeof(*str));
}

str_t *str_resize(str_t *str, size_t new)
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

str_t *str_cat(str_t *str, const char *s)
{
    assert(str && s);

    size_t len = strlen(s);
    ensure_size(str, len + 1);
    memcpy(offset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *str_catch(str_t *str, char ch)
{
    assert(str);

    ensure_size(str, 2);
    str->buf[str->len++] = ch;
    str->buf[str->len] = '\0';

    return str;
}

str_t *str_catlen(str_t *str, const char *s, size_t len)
{
    assert(str && s);

    ensure_size(str, len + 1);
    memcpy(offset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *str_cat_strlen(str_t *dst, const str_t *src, size_t len)
{
    return str_catlen(dst, src->buf, len);
}

str_t *str_cat_str(str_t *dst, const str_t *src)
{
    assert(dst);

    ensure_size(dst, src->len + 1);
    memcpy(offset(dst), src->buf, src->len);

    dst->len += src->len;
    append_null(dst);

    return dst;
}

str_t *str_catf(str_t *str, const char *fmt, ...)
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

str_t *str_catfv(str_t *str, const char *fmt, va_list args)
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

str_t *str_catf_d(str_t *str, const char *fmt, ...)
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
            str_catlen(str, digit, len);

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

str_t *str_catwcs(str_t *str, const wchar_t *ws)
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

str_t *str_catwch(str_t *str, const wchar_t wc)
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

#ifdef _WIN32
wchar_t *str_decode(const str_t *str)
{
    int needed =
        MultiByteToWideChar(CP_UTF8, 0, str->buf, (int)str->len, NULL, 0);
    if (!needed)
    {
        errno = GetLastError();
        return NULL;
    }

    wchar_t *buf = malloc((needed + 1) * sizeof(wchar_t));
    if (buf == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    int len =
        MultiByteToWideChar(CP_UTF8, 0, str->buf, (int)str->len, buf, needed);
    buf[len] = L'\0';
    return buf;
}

str_t str_encode(const wchar_t *utf16)
{
    str_t mbs = {0};

    int needed = WideCharToMultiByte(CP_UTF8, 0, utf16, (int)wcslen(utf16),
                                     NULL, 0, NULL, NULL);
    if (!needed)
    {
        errno = GetLastError();
        return str_create();
    }

    mbs.capacity = needed + 1;
    mbs.buf = calloc(mbs.capacity, sizeof(char));
    if (mbs.buf == NULL)
    {
        errno = ENOMEM;
        return mbs;
    }

    mbs.len = WideCharToMultiByte(CP_UTF8, 0, utf16, (int)wcslen(utf16),
                                  mbs.buf, needed, NULL, NULL);
    mbs.buf[mbs.len] = '\0';

    return mbs;
}
#else
wchar_t *str_decode(const str_t *str)
{
    size_t needed = mbstowcs(NULL, str->buf, 0);
    if (needed == (size_t)-1)
    {
        errno = EILSEQ;
        return NULL;
    }

    wchar_t *buf = malloc((needed + 1) * sizeof(wchar_t));
    if (buf == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    mbstowcs(buf, str->buf, needed + 1);
    buf[needed] = L'\0';
    return buf;
}

str_t str_encode(const wchar_t *utf16)
{
    str_t mbs = {0};

    size_t needed = wcstombs(NULL, utf16, 0);
    if (needed == (size_t)-1)
    {
        errno = EILSEQ;
        return str_create();
    }

    mbs.capacity = needed + 1;
    mbs.buf = calloc(mbs.capacity, sizeof *mbs.buf);
    if (!mbs.buf)
    {
        errno = ENOMEM;
        return mbs;
    }

    size_t len = wcstombs(mbs.buf, utf16, needed + 1);
    mbs.len = (len == (size_t)-1 ? 0 : len);
    mbs.buf[mbs.len] = '\0';

    return mbs;
}
#endif // _WIN32

str_t *str_repeat_char(str_t *str, char ch, size_t n, const char *sep)
{
    assert(str);
    if (n == 0)
        return str;
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t total = n + sep_len * (n - 1);
    ensure_size(str, total + 1);
    for (size_t i = 0; i < n; ++i)
    {
        offset(str)[0] = ch;
        str->len++;
        if (sep_len && i + 1 < n)
        {
            memcpy(offset(str), sep, sep_len);
            str->len += sep_len;
        }
    }
    append_null(str);
    return str;
}

str_t *str_repeat_wchar(str_t *str, wchar_t wc, size_t n, const wchar_t *wsep)
{
    assert(str);
    if (n == 0)
        return str;
    char buf_wc[MB_CUR_MAX];
    int wc_len = wctomb(buf_wc, wc);
    assert(wc_len > 0);
    size_t sep_len = 0;
    char *buf_sep = NULL;
    if (wsep)
    {
        sep_len = wcstombs(NULL, wsep, 0);
        buf_sep = malloc(sep_len + 1);
        wcstombs(buf_sep, wsep, sep_len + 1);
    }
    size_t total = n * (size_t)wc_len + sep_len * (n - 1);
    ensure_size(str, total + 1);
    for (size_t i = 0; i < n; ++i)
    {
        memcpy(offset(str), buf_wc, wc_len);
        str->len += wc_len;
        if (sep_len && i + 1 < n)
        {
            memcpy(offset(str), buf_sep, sep_len);
            str->len += sep_len;
        }
    }
    append_null(str);
    free(buf_sep);
    return str;
}

str_t *str_repeat_cstr(str_t *str, const char *s, size_t n, const char *sep)
{
    assert(str && s);
    if (n == 0)
        return str;
    size_t s_len = strlen(s);
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t total = n * s_len + sep_len * (n - 1);
    ensure_size(str, total + 1);
    for (size_t i = 0; i < n; ++i)
    {
        memcpy(offset(str), s, s_len);
        str->len += s_len;
        if (sep_len && i + 1 < n)
        {
            memcpy(offset(str), sep, sep_len);
            str->len += sep_len;
        }
    }
    append_null(str);
    return str;
}

str_t *str_repeat_wcs(str_t *str, const wchar_t *ws, size_t n,
                      const wchar_t *wsep)
{
    assert(str && ws);
    if (n == 0)
        return str;
    size_t ws_len = wcstombs(NULL, ws, 0);
    char *buf_ws = malloc(ws_len + 1);
    wcstombs(buf_ws, ws, ws_len + 1);
    size_t sep_len = 0;
    char *buf_sep = NULL;
    if (wsep)
    {
        sep_len = wcstombs(NULL, wsep, 0);
        buf_sep = malloc(sep_len + 1);
        wcstombs(buf_sep, wsep, sep_len + 1);
    }
    size_t total = n * ws_len + sep_len * (n - 1);
    ensure_size(str, total + 1);
    for (size_t i = 0; i < n; ++i)
    {
        memcpy(offset(str), buf_ws, ws_len);
        str->len += ws_len;
        if (sep_len && i + 1 < n)
        {
            memcpy(offset(str), buf_sep, sep_len);
            str->len += sep_len;
        }
    }
    append_null(str);
    free(buf_ws);
    free(buf_sep);
    return str;
}

str_t *str_repeat_str(str_t *str, const str_t *src, size_t n, const char *sep)
{
    assert(str && src);
    if (n == 0)
        return str;
    size_t s_len = src->len;
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t total = n * s_len + sep_len * (n - 1);
    ensure_size(str, total + 1);
    for (size_t i = 0; i < n; ++i)
    {
        memcpy(offset(str), src->buf, s_len);
        str->len += s_len;
        if (sep_len && i + 1 < n)
        {
            memcpy(offset(str), sep, sep_len);
            str->len += sep_len;
        }
    }
    append_null(str);
    return str;
}

void str_tokenizer_init(str_tokenizer_t *tok, const str_t *s, const char *delim)
{
    assert(tok && s && delim);
    tok->next = s->buf;
    tok->delim = delim;
    tok->delim_len = strlen(delim);
    tok->initial = true;
}

bool str_tokenizer_next(str_tokenizer_t *tok, strview_t *view)
{
    if (tok->next == NULL || *tok->next == '\0')
    {
        view->buf = NULL;
        view->len = 0;
        return false;
    }

    char *start = tok->next;
    char *pos = strstr(start, tok->delim);
    if (pos)
    {
        view->buf = start;
        view->len = (size_t)(pos - start);
        *pos = '\0';
        tok->next = pos + tok->delim_len;
        tok->initial = false;
    }
    else if (!tok->initial)
    {
        view->buf = start;
        view->len = strlen(start);
        tok->next = NULL;
    }
    else
    {
        view->buf = NULL;
        view->len = 0;
        return false;
    }

    return true;
}

long str_parse(strview_t view, int base)
{
    errno = 0;
    char tmp = view.buf[view.len];
    view.buf[view.len] = '\0';
    long val = strtol(view.buf, NULL, base);
    view.buf[view.len] = tmp;
    if (errno != 0)
        return 0;
    return val;
}

float str_parsef(strview_t view)
{
    errno = 0;
    char tmp = view.buf[view.len];
    view.buf[view.len] = '\0';
    float val = strtof(view.buf, NULL);
    view.buf[view.len] = tmp;
    if (errno != 0)
        return 0;
    return val;
}

long double str_parseld(strview_t view)
{
    errno = 0;
    char tmp = view.buf[view.len];
    view.buf[view.len] = '\0';
    long double val = strtold(view.buf, NULL);
    view.buf[view.len] = tmp;
    if (errno != 0)
        return 0;
    return val;
}
