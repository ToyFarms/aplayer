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
//     ASSERT_COND(0 == 0);
// }
// TEST_END()

#ifndef __BASE_H
#define __BASE_H

#define TEST_BEGIN(name, ...) void name()
#define TEST_END(...)
#define CFLAGS_BEGIN
#define CFLAGS_END
#define INCLUDE_BEGIN
#define INCLUDE_END

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_memory(const void *addr, size_t n)
{
    const unsigned char *ptr = addr;

    for (size_t i = 0; i < n; i++)
        fprintf(stderr, "%02X ", ptr[i]);
}

#define ASSERT_COND(a) assert(a)
#define _ASSERT_INT(a, b, op, nop, name)                                       \
    if ((a)nop(b))                                                             \
    {                                                                          \
        fprintf(stderr, name "(" #a "[%d] " #nop " " #b "[%d]) %s:%d", (a),     \
                (b), __FILE__, __LINE__);                                      \
        exit(1);                                                               \
    }
#define ASSERT_INT_EQ(a, b)  _ASSERT_INT(a, b, ==, !=, "INT_EQ")
#define ASSERT_INT_NEQ(a, b) _ASSERT_INT(a, b, !=, ==, "INT_NEQ")
#define ASSERT_INT_GT(a, b)  _ASSERT_INT(a, b, >, <=, "INT_GT")
#define ASSERT_INT_GTE(a, b) _ASSERT_INT(a, b, >=, <, "INT_GTE")
#define ASSERT_INT_LT(a, b)  _ASSERT_INT(a, b, <, >=, "INT_LT")
#define ASSERT_INT_LTE(a, b) _ASSERT_INT(a, b, <=, >, "INT_LTE")
#define ASSERT_MEM_EQ(a, b, n)                                                 \
    if (memcmp((a), (b), (n)) != 0)                                            \
    {                                                                          \
        fprintf(stderr, "MEM_EQ(%s[", #a);                                     \
        print_memory((a), (n));                                                \
        fprintf(stderr, "] != %s[", #b);                                       \
        print_memory((b), (n));                                                \
        fprintf(stderr, "]) %s:%d", __FILE__, __LINE__);                       \
        exit(1);                                                               \
    }
#define ASSERT_NULL(a)                                                         \
    if ((a) != NULL)                                                           \
    {                                                                          \
        fprintf(stderr, #a " is not NULL %s:%d", __FILE__, __LINE__);          \
        exit(1);                                                               \
    }
#define ASSERT_NOTNULL(a)                                                      \
    if ((a) == NULL)                                                           \
    {                                                                          \
        fprintf(stderr, #a " is NULL %s:%d", __FILE__, __LINE__);              \
        exit(1);                                                               \
    }

#endif /* __BASE_H */
