#include "base_test.h"

INCLUDE_BEGIN
#include "array.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
INCLUDE_END

CFLAGS_BEGIN/*
-Isrc/include
src/struct/array.c
*/CFLAGS_END

TEST_BEGIN(init)
{
    array_t arr = array_create(1, 1);
    ASSERT_COND(arr.data);
}
TEST_END()

TEST_BEGIN(init_illegal_size, EXPECT_FAIL)
{
    array_t arr = array_create(-1, 1);
}
TEST_END()

TEST_BEGIN(init_illegal_size2, EXPECT_FAIL)
{
    array_t arr = array_create(1, -1);
}
TEST_END()

TEST_BEGIN(append_static_not_enough_space)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, 3);
    ASSERT_INT_EQ(ret, -ENOMEM);
    ASSERT_INT_EQ(arr.length, 0);
    ASSERT_INT_EQ(arr.capacity, 1);
}
TEST_END()

TEST_BEGIN(append_static_illegal_size, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, -1);
}
TEST_END()

TEST_BEGIN(append_static_null_data, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, NULL, 3);
}
TEST_END()

TEST_BEGIN(append_static_null_data2, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(NULL, data, 3);
}
TEST_END()

TEST_BEGIN(append_static_null_item)
{
    array_t arr = {0};

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, 3);
    ASSERT_INT_EQ(ret, -ENOMEM);
}
TEST_END()

TEST_BEGIN(append_static)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, 3);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 3);
    ASSERT_INT_EQ(arr.capacity, 3);
    ASSERT_MEM_EQ(arr.data, data, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_static_partial)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, 1);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 1);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {1, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_static_partial2)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, 2);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 2);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {1, 2, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_static_item_overflow)
{
    array_t arr = array_create(3, 1);
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {400, 2, 3};
    int ret = array_append_static(&arr, data, 1);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 1);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {400 - 256, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_illegal_size, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, -1);
}
TEST_END()

TEST_BEGIN(append_dynamic_null_data, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, NULL, 3);
}
TEST_END()

TEST_BEGIN(append_dynamic_null_data2, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(NULL, data, 3);
}
TEST_END()

TEST_BEGIN(append_dynamic_null_item)
{
    array_t arr = {0};

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, 3);
    ASSERT_INT_EQ(ret, -EINVAL);
}
TEST_END()

TEST_BEGIN(append_dynamic)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, 3);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 3);
    ASSERT_INT_EQ(arr.capacity, 3);
    ASSERT_MEM_EQ(arr.data, data, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_resize2x)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3, 4, 5, 6};
    int ret = array_append(&arr, data, 6);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 6);
    ASSERT_INT_EQ(arr.capacity, 6);
    ASSERT_MEM_EQ(arr.data, data, 6 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_resize3x)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    int ret = array_append(&arr, data, 12);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 12);
    ASSERT_INT_EQ(arr.capacity, 12);
    ASSERT_MEM_EQ(arr.data, data, 12 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_resize_power2)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int ret = array_append(&arr, data, 10);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 10);
    ASSERT_INT_EQ(arr.capacity, 12);
    ASSERT_MEM_EQ(arr.data, data, 10 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_partial)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, 1);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 1);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {1, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_partial2)
{
    array_t arr = array_create(3, sizeof(int));
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, 2);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 2);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {1, 2, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(append_dynamic_item_overflow)
{
    array_t arr = array_create(3, 1);
    ASSERT_COND(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {400, 2, 3};
    int ret = array_append_static(&arr, data, 1);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 1);
    ASSERT_INT_EQ(arr.capacity, 3);
    const int expected[] = {400 - 256, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, 3 * sizeof(int));
}
TEST_END()

TEST_BEGIN(_remove)
{
    array_t arr = array_create(16, sizeof(int));

    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    array_append(&arr, data, 10);

    int ret = array_remove(&arr, 5, 2);

    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 8);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 4, 5, 8, 9, 10};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(remove2)
{
    array_t arr = array_create(10, sizeof(int));

    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    array_append(&arr, data, 10);

    int ret = array_remove(&arr, 5, 6);

    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(arr.length, 5);
    ASSERT_INT_EQ(arr.capacity, 10);

    int expected[] = {1, 2, 3, 4, 5, 6};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()
