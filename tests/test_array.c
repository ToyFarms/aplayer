#include "base_test.h"

INCLUDE_BEGIN
#include "array.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/struct/array.c
 src/logger.c
 */ CFLAGS_END

TEST_BEGIN(init)
{
    array_t arr = array_create(1, 1);
    ASSERT_TRUE(arr.data);
}
TEST_END()

TEST_BEGIN(init_check_zero_alloc)
{
    array_t arr = array_create(4096, 1);

    char b;
    ARR_FOREACH(arr, b, i)
    {
        ASSERT_INT_EQ((int)b, 0);
    }
}
TEST_END()

TEST_BEGIN(init_check_zero_alloc2)
{
    array_t arr = array_create(2048, sizeof(int));

    int b;
    ARR_FOREACH(arr, b, i)
    {
        ASSERT_INT_EQ(b, 0);
    }
}
TEST_END()

TEST_BEGIN(init_illegal_size, EXPECT_FAIL)
{
    array_t arr = array_create(-1, 1);
    (void)arr;
}
TEST_END()

TEST_BEGIN(init_illegal_size2, EXPECT_FAIL)
{
    array_t arr = array_create(1, -1);
    (void)arr;
}
TEST_END()

TEST_BEGIN(append_static_not_enough_space)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(&arr, data, -1);
    ASSERT_INT_EQ(ret, -EINVAL);
}
TEST_END()

TEST_BEGIN(append_static_null_data, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    int ret = array_append_static(&arr, NULL, 3);
    ASSERT_INT_EQ(ret, -EINVAL)
}
TEST_END()

TEST_BEGIN(append_static_null_data2, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append_static(NULL, data, 3);
    ASSERT_INT_EQ(ret, -EINVAL)
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(&arr, data, -1);
    ASSERT_INT_EQ(ret, -EINVAL)
}
TEST_END()

TEST_BEGIN(append_dynamic_null_data, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    int ret = array_append(&arr, NULL, 3);
    ASSERT_INT_EQ(ret, -EINVAL)
}
TEST_END()

TEST_BEGIN(append_dynamic_null_data2, EXPECT_FAIL)
{
    array_t arr = array_create(1, sizeof(int));
    ASSERT_TRUE(arr.data);
    ASSERT_INT_EQ(errno, 0);

    const int data[] = {1, 2, 3};
    int ret = array_append(NULL, data, 3);
    ASSERT_INT_EQ(ret, -EINVAL)
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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
    ASSERT_TRUE(arr.data);
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

TEST_BEGIN(remove)
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

TEST_BEGIN(insert)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 6);
    array_insert(&arr, data, 2, 3);

    ASSERT_INT_EQ(arr.length, 8);
    ASSERT_INT_EQ(arr.capacity, 8);

    int expected[] = {1, 2, 3, 1, 2, 4, 5, 6};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert2)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int index = 0;

    array_insert(&arr, data + 0, 1, index++);
    array_insert(&arr, data + 1, 1, index++);
    array_insert(&arr, data + 2, 1, index++);
    array_insert(&arr, data + 3, 1, index++);
    array_insert(&arr, data + 4, 1, index++);
    array_insert(&arr, data + 5, 1, index++);
    array_insert(&arr, data + 6, 1, index++);
    array_insert(&arr, data + 7, 1, index++);

    ASSERT_INT_EQ(arr.length, 8);
    ASSERT_INT_EQ(arr.capacity, 8);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_full)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 3, 0);

    ASSERT_INT_EQ(arr.length, 11);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_full2)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 1, 0);

    ASSERT_INT_EQ(arr.length, 9);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_at_length)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 4);
    array_insert(&arr, data, 2, arr.length);

    ASSERT_INT_EQ(arr.length, 6);
    ASSERT_INT_EQ(arr.capacity, 8);

    int expected[] = {1, 2, 3, 4, 1, 2};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_at_end)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 1, 8);

    ASSERT_INT_EQ(arr.length, 9);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 1};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_at_end2)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 8, 8);

    ASSERT_INT_EQ(arr.length, 16);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_at_end_min1)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 1, 7);

    ASSERT_INT_EQ(arr.length, 9);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 1, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_at_end2_min1)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 2, 7);

    ASSERT_INT_EQ(arr.length, 10);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 1, 2, 8};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(insert_far)
{
    array_t arr = array_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    array_append(&arr, data, 8);
    array_insert(&arr, data, 2, 16);

    ASSERT_INT_EQ(arr.length, 10);
    ASSERT_INT_EQ(arr.capacity, 32);

    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0, 0,
                      1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(check_zero_realloc)
{
    array_t arr = array_create(4, sizeof(int));
    int data[] = {1, 2, 3, 4};

    array_insert(&arr, data, 4, 8);

    ASSERT_INT_EQ(arr.length, 4);
    ASSERT_INT_EQ(arr.capacity, 16);

    int expected[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0, 0};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(random_shuffle_integrity)
{
    array_t arr = array_create(10, sizeof(int));
    for (int i = 0; i < 10; ++i)
        array_append(&arr, &i, 1);

    bool exists[10];
    for (int iter = 0; iter < 100; ++iter)
    {
        memset(exists, 0, sizeof(exists));
        array_shuffle(&arr);
        int x;
        ARR_FOREACH(arr, x, i)
        {
            exists[x] = true;
        }

        for (int i = 0; i < 10; i++)
            ASSERT_TRUE(exists[i]);
    }

    ASSERT_INT_EQ(arr.length, 10);
    ASSERT_INT_EQ(arr.capacity, 10);
}
TEST_END()

TEST_BEGIN(reverse)
{
    array_t arr = array_create(6, sizeof(int));
    int data[] = {10, 20, 30, 40, 50, 60};
    array_append(&arr, data, 6);

    array_reverse(&arr);

    int expected[] = {60, 50, 40, 30, 20, 10};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
    ASSERT_INT_EQ(arr.length, 6);
    ASSERT_INT_EQ(arr.capacity, 6);
}
TEST_END()

TEST_BEGIN(reverse2x)
{
    array_t arr = array_create(6, sizeof(int));
    int data[] = {10, 20, 30, 40, 50, 60};
    array_append(&arr, data, 6);

    array_reverse(&arr);
    array_reverse(&arr);

    int expected[] = {10, 20, 30, 40, 50, 60};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
    ASSERT_INT_EQ(arr.length, 6);
    ASSERT_INT_EQ(arr.capacity, 6);
}
TEST_END()

TEST_BEGIN(reverse3x)
{
    array_t arr = array_create(6, sizeof(int));
    int data[] = {10, 20, 30, 40, 50, 60};
    array_append(&arr, data, 6);

    array_reverse(&arr);
    array_reverse(&arr);
    array_reverse(&arr);

    int expected[] = {60, 50, 40, 30, 20, 10};
    ASSERT_MEM_EQ(arr.data, expected, sizeof(expected));
    ASSERT_INT_EQ(arr.length, 6);
    ASSERT_INT_EQ(arr.capacity, 6);
}
TEST_END()
