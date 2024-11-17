#include "base_test.h"

INCLUDE_BEGIN
#include "dict.h"
#include <assert.h>
#include <string.h>
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/struct/dict.c
 src/struct/array.c
 src/logger.c
 */ CFLAGS_END

TEST_BEGIN(init)
{
    dict_t dict = dict_create();
    ASSERT_INT_EQ(errno, 0);
    ASSERT_NOTNULL(dict.buckets);
    ASSERT_INT_EQ(dict.length, 0);
    ASSERT_INT_GT(dict.bucket_slot, 0);
}
TEST_END()

TEST_BEGIN(free)
{
    dict_t dict = dict_create();
    dict_free(&dict);
    ASSERT_NULL(dict.buckets);
    ASSERT_INT_EQ(dict.bucket_slot, 0);
    ASSERT_INT_EQ(dict.length, 0);
    ASSERT_MEM_EQ(&dict, &(dict_t){0}, sizeof(dict_t));
}
TEST_END()

TEST_BEGIN(free_null)
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

TEST_BEGIN(insert_get)
{
    dict_t dict = dict_create();
    int val[5] = {0};
    val[0] = 1;
    val[1] = 2;
    val[2] = 3;
    const char *key = "Hello World!";

    dict_insert(&dict, key, val);
    ASSERT_INT_EQ(dict.length, 1);

    int *ret = dict_get(&dict, key, NULL);

    ASSERT_MEM_EQ(val, ret, 5 * sizeof(int));
}
TEST_END()

TEST_BEGIN(insert_get_null)
{
    dict_t dict = dict_create();
    const char *key = "Hello World!";

    dict_insert(&dict, key, NULL);
    ASSERT_INT_EQ(dict.length, 1);

    char *ret = dict_get(&dict, key, NULL);
    ASSERT_NULL(ret);
}
TEST_END()

TEST_BEGIN(insert_null, EXPECT_FAIL)
{
    dict_t dict = dict_create();
    int val[5] = {0};
    val[0] = 1;
    val[1] = 2;
    val[2] = 3;
    const char *key = NULL;

    dict_insert(&dict, key, val);
    ASSERT_INT_EQ(dict.length, 1);

    int *ret = dict_get(&dict, key, NULL);

    ASSERT_MEM_EQ(val, ret, 5 * sizeof(int));
}
TEST_END()

TEST_BEGIN(get_null, EXPECT_FAIL)
{
    dict_t dict = dict_create();
    int val[5] = {0};
    val[0] = 1;
    val[1] = 2;
    val[2] = 3;
    const char *key = "Hello World!";
    dict_insert(&dict, key, val);

    int *ret = dict_get(&dict, NULL, NULL);

    ASSERT_MEM_EQ(val, ret, 5 * sizeof(int));
}
TEST_END()

TEST_BEGIN(get_notexist)
{
    dict_t dict = dict_create();
    int val[5] = {0};
    val[0] = 1;
    val[1] = 2;
    val[2] = 3;
    const char *key = "Hello World!";
    dict_insert(&dict, key, val);

    int *ret = dict_get(&dict, "Hello People!", NULL);
    ASSERT_INT_EQ(errno, -EINVAL);
    ASSERT_NULL(ret);
}
TEST_END()

TEST_BEGIN(insert_get10)
{
    dict_t dict = dict_create();

    for (int i = 0; i < 10; i++)
    {
        int *data = calloc(i + 1, sizeof(int));
        data[i] = i;

        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        dict_insert(&dict, key, data);
    }

    ASSERT_INT_EQ(dict.length, 10);

    int *ret;
    ret = dict_get(&dict, "Hello! 0", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[0], 0);

    ret = dict_get(&dict, "Hello! 1", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[1], 1);

    ret = dict_get(&dict, "Hello! 2", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[2], 2);

    ret = dict_get(&dict, "Hello! 3", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[3], 3);

    ret = dict_get(&dict, "Hello! 4", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[4], 4);

    ret = dict_get(&dict, "Hello! 5", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[5], 5);

    ret = dict_get(&dict, "Hello! 6", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[6], 6);

    ret = dict_get(&dict, "Hello! 7", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[7], 7);

    ret = dict_get(&dict, "Hello! 8", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[8], 8);

    ret = dict_get(&dict, "Hello! 9", NULL);
    ASSERT_NOTNULL(ret);
    ASSERT_INT_EQ(ret[9], 9);

    ret = dict_get(&dict, "Hello! 10", NULL);
    ASSERT_NULL(ret);

    ret = dict_get(&dict, "Hello! 11", NULL);
    ASSERT_NULL(ret);
}
TEST_END()

TEST_BEGIN(resize)
{
    dict_t dict = dict_create();

    for (int i = 0; i < 100; i++)
    {
        int *data = calloc(24, sizeof(int));
        data[i % 24] = i;

        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        dict_insert(&dict, key, data);
    }

    for (int i = 0; i < 100; i++)
    {
        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        int *ret = dict_get(&dict, key, NULL);
        ASSERT_NOTNULL(ret);
        ASSERT_INT_EQ(ret[i % 24], i);
    }

    dict_resize(&dict, 2);

    for (int i = 0; i < 100; i++)
    {
        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        int *ret = dict_get(&dict, key, NULL);
        ASSERT_NOTNULL(ret);
        ASSERT_INT_EQ(ret[i % 24], i);
    }
}
TEST_END()

TEST_BEGIN(stress)
{
    dict_t dict = dict_create();
    int item = 1000;

    for (int i = 0; i < item; i++)
    {
        int *data = calloc(10, sizeof(int));
        data[i % 10] = i;

        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        dict_insert(&dict, key, data);
    }

    for (int i = 0; i < item; i++)
    {
        char key[100];
        snprintf(key, 100, "Hello! %d", i);

        int *ret = dict_get(&dict, key, NULL);
        ASSERT_NOTNULL(ret);
        ASSERT_INT_EQ(ret[i % 10], i);
    }
}
TEST_END()

TEST_BEGIN(overwrite)
{
    dict_t dict = dict_create();
    char *data = "Hello World!";
    char *data2 = "Goodbye!";

    dict_insert(&dict, "TEST1", data);
    dict_insert(&dict, "TEST1", data2);

    ASSERT_INT_EQ(dict.length, 1);

    char *ret = dict_get(&dict, "TEST1", NULL);
    ASSERT_MEM_EQ(ret, data2, strlen(data2));
}
TEST_END()
