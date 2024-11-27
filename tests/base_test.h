// INCLUDE_BEGIN
// #include ...
// INCLUDE_END
//
// CFLAGS_BEGIN/*
// -Isrc/include
// */CFLAGS_END
//
// TEST_BEGIN(init)
// {
//     ASSERT_TRUE(0 == 0);
// }
// TEST_END()

#ifndef __BASE_H
#define __BASE_H

#define TEST_BEGIN(name, ...) void testns_##name()
#define TEST_END(...)
#define CFLAGS_BEGIN
#define CFLAGS_END
#define INCLUDE_BEGIN
#define INCLUDE_END

// #define __FILENAME__                                                           \
//     (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define test_init_setlocale setlocale(LC_ALL, "")
#define test_init_initlogger                                                   \
    logger_set_level(LOG_DEBUG);                                               \
    logger_add_output(-1, fopen("tests/logs/" __FILE_NAME__ ".log", "a"),      \
                      LOG_DEFER_CLOSE);

#include "logger.h"
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_memory(const void *addr, size_t n)
{
    const unsigned char *ptr = addr;

    for (size_t i = 0; i < n; i++)
    {
        if (isprint(ptr[i]))
            fprintf(stderr, "%c ", ptr[i]);
        else
            fprintf(stderr, "%02X ", ptr[i]);
    }
}

#define _assert(expr)                                                          \
    if (!(expr))                                                               \
    {                                                                          \
        fprintf(stderr, "Assertion failed: '%s' in %s:%d", #expr, __FILE__,    \
                __LINE__);                                                     \
        exit(EXIT_FAILURE);                                                    \
    }

#define ASSERT_TRUE(a) _assert(a)
#define _ASSERT_NUM(a, b, fmt, nop, name)                                      \
    if ((a)nop(b))                                                             \
    {                                                                          \
        fprintf(stderr, "%s(%s<%d> %s %s<%d>) %s:%d", name, #a, (a), #nop, #b, \
                (b), __FILE__, __LINE__);                                      \
        exit(EXIT_FAILURE);                                                    \
    }

#define ASSERT_INT_EQ(a, b)  _ASSERT_NUM(a, b, "%d", !=, "INT_EQ")
#define ASSERT_INT_NEQ(a, b) _ASSERT_NUM(a, b, "%d", ==, "INT_NEQ")
#define ASSERT_INT_GT(a, b)  _ASSERT_NUM(a, b, "%d", <=, "INT_GT")
#define ASSERT_INT_GTE(a, b) _ASSERT_NUM(a, b, "%d", <, "INT_GTE")
#define ASSERT_INT_LT(a, b)  _ASSERT_NUM(a, b, "%d", >=, "INT_LT")
#define ASSERT_INT_LTE(a, b) _ASSERT_NUM(a, b, "%d", >, "INT_LTE")

#define ASSERT_FLOAT_WITHIN(a, b, within)                                      \
    if (fabs((float)(a) - (float)(b)) > (float)(within))                       \
    {                                                                          \
        fprintf(stderr,                                                        \
                "FLOAT_WITHIN(%s<%f> is NOT within %f of %s<%f>) %s:%d", #a,   \
                (float)(a), (float)(within), #b, (float)(b), __FILE__,         \
                __LINE__);                                                     \
        exit(EXIT_FAILURE);                                                    \
    }

#define ASSERT_FLOAT_EQ(a, b)  ASSERT_FLOAT_WITHIN(a, b, 0.001f)
#define ASSERT_FLOAT_NEQ(a, b) _ASSERT_NUM(a, b, "%f", ==, "FLOAT_NEQ")
#define ASSERT_FLOAT_GT(a, b)  _ASSERT_NUM(a, b, "%f", <=, "FLOAT_GT")
#define ASSERT_FLOAT_GTE(a, b) _ASSERT_NUM(a, b, "%f", <, "FLOAT_GTE")
#define ASSERT_FLOAT_LT(a, b)  _ASSERT_NUM(a, b, "%f", >=, "FLOAT_LT")
#define ASSERT_FLOAT_LTE(a, b) _ASSERT_NUM(a, b, "%f", >, "FLOAT_LTE")

#define ASSERT_MEM_EQ(a, b, n)                                                 \
    if (memcmp((a), (b), (n)) != 0)                                            \
    {                                                                          \
        fprintf(stderr, "MEM_EQ(%s<[", #a);                                    \
        print_memory((a), (n));                                                \
        fprintf(stderr, "]> != %s<[", #b);                                     \
        print_memory((b), (n));                                                \
        fprintf(stderr, "]>) %s:%d", __FILE__, __LINE__);                      \
        exit(EXIT_FAILURE);                                                    \
    }
#define ASSERT_STR_EQ(a, b, n)                                                 \
    if (strncmp((a), (b), (n)) != 0)                                           \
    {                                                                          \
        fprintf(stderr, "STR_EQ(%s<\"%.*s\"> != %s<\"%.*s\">) %s:%d", #a,      \
                (int)(n), (a), #b, (int)(n), (b), __FILE__, __LINE__);         \
        exit(EXIT_FAILURE);                                                    \
    }
#define ASSERT_NULL(a)                                                         \
    if ((void *)(a) != NULL)                                                   \
    {                                                                          \
        fprintf(stderr, #a " is not NULL %s:%d", __FILE__, __LINE__);          \
        exit(EXIT_FAILURE);                                                    \
    }
#define ASSERT_NOTNULL(a)                                                      \
    if ((void *)(a) == NULL)                                                   \
    {                                                                          \
        fprintf(stderr, #a " is NULL %s:%d", __FILE__, __LINE__);              \
        exit(EXIT_FAILURE);                                                    \
    }

#define _MMIN(a, b) ((a) > (b) ? (b) : (a))
#define _MMAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* __BASE_H */
