#include "base_test.h"

INCLUDE_BEGIN
#include <assert.h>
#include "dict.h"
#include <string.h>
INCLUDE_END

CFLAGS_BEGIN/*
-Isrc/include
src/struct/dict.c
*/CFLAGS_END

TEST_BEGIN(init)
{
    dict_t dict = dict_create();
    ASSERT_NOTNULL(dict.buckets);
    ASSERT_INT_EQ(dict.length, 0);
    ASSERT_INT_GT(dict.capacity, 0);
}
TEST_END()

TEST_BEGIN(_free)
{
    dict_t dict = dict_create();
    dict_free(&dict);
    ASSERT_NULL(dict.buckets);
    ASSERT_INT_EQ(dict.capacity, 0);
    ASSERT_INT_EQ(dict.length, 0);
    ASSERT_MEM_EQ(&dict, &(dict_t){0}, sizeof(dict_t));
}
TEST_END()

TEST_BEGIN(free_null, EXPECT_FAIL)
{
    dict_t dict = {0};
    dict_free(&dict);
}
TEST_END()

TEST_BEGIN(free_null2, EXPECT_FAIL)
{
    dict_free(NULL);
}
TEST_END()
