#include "base_test.h"

INCLUDE_BEGIN
#include "ring_buf.h"
INCLUDE_END

CFLAGS_BEGIN/*
-Isrc/include
src/struct/ring_buf.c
*/CFLAGS_END

TEST_BEGIN(init)
{
    ring_buf_t rbuf = ring_buf_create(8, 1);
    ASSERT_INT_EQ(errno, 0);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 0);
    ASSERT_INT_EQ(rbuf.capacity, 8);
    ASSERT_INT_EQ(rbuf.length, 0);
    ASSERT_INT_EQ(rbuf.item_size, 1);
    ASSERT_NOTNULL(rbuf.buf);
}
TEST_END()

TEST_BEGIN(init_illegal, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(0, 1);
}
TEST_END()

TEST_BEGIN(init_illegal2, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, 0);
}
TEST_END()

TEST_BEGIN(write)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    int ret = ring_buf_write(&rbuf, data, 5);
    ASSERT_INT_EQ(ret, 0);

    ASSERT_INT_EQ(rbuf.length, 5);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 5);
    ASSERT_INT_EQ(rbuf.capacity, 8);
    ASSERT_MEM_EQ(rbuf.buf, data, 5 * sizeof(int));
}
TEST_END()

TEST_BEGIN(write_wrap)
{
    ring_buf_t rbuf = ring_buf_create(4, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    int ret = ring_buf_write(&rbuf, data, 5);
    ASSERT_INT_EQ(ret, 0);

    ASSERT_INT_EQ(rbuf.length, 4);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 1);
    ASSERT_INT_EQ(rbuf.capacity, 4);

    int expected[] = {5, 2, 3, 4};
    ASSERT_MEM_EQ(rbuf.buf, expected, 4 * sizeof(int));
}
TEST_END()

TEST_BEGIN(write_null, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    int ret = ring_buf_write(NULL, data, 5);
}
TEST_END()

TEST_BEGIN(write_null2, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int ret = ring_buf_write(&rbuf, NULL, 5);
}
TEST_END()

TEST_BEGIN(write_illegal_size, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    int ret = ring_buf_write(&rbuf, data, -1);
}
TEST_END()

TEST_BEGIN(write_buf_null, EXPECT_FAIL)
{
    ring_buf_t rbuf = {0};

    int data[] = {1, 2, 3, 4, 5};
    int ret = ring_buf_write(&rbuf, data, 5);
}
TEST_END()

TEST_BEGIN(read)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    ring_buf_write(&rbuf, data, 5);

    int expected[] = {1, 2, 3, 4, 5};
    int actual[5] = {0};

    int ret = ring_buf_read(&rbuf, 5, actual);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_MEM_EQ(expected, actual, 5 * sizeof(int));
    ASSERT_INT_EQ(rbuf.length, 0);
    ASSERT_INT_EQ(rbuf.read_idx, 5);
    ASSERT_INT_EQ(rbuf.write_idx, 5);
    ASSERT_INT_EQ(rbuf.capacity, 8);
}
TEST_END()

TEST_BEGIN(read_wrap)
{
    ring_buf_t rbuf = ring_buf_create(4, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    ring_buf_write(&rbuf, data, 5);

    int expected[] = {5, 2, 3, 4};
    int actual[4] = {0};

    int ret = ring_buf_read(&rbuf, 4, actual);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_MEM_EQ(expected, actual, 4 * sizeof(int));
    ASSERT_INT_EQ(rbuf.length, 0);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 1);
    ASSERT_INT_EQ(rbuf.capacity, 4);
}
TEST_END()

TEST_BEGIN(read_too_much)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));
    int data[] = {1, 2, 3, 4, 5};
    ring_buf_write(&rbuf, data, 5);

    int expected[7] = {0};
    int actual[7] = {0};
    int ret = ring_buf_read(&rbuf, 7, actual);
    ASSERT_INT_NEQ(ret, 0);
    ASSERT_MEM_EQ(expected, actual, 7 * sizeof(int));
}
TEST_END()

TEST_BEGIN(read_null, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));

    int out[8] = {0};
    int ret = ring_buf_read(NULL, 5, out);
}
TEST_END()

TEST_BEGIN(read_null2, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));

    int ret = ring_buf_read(&rbuf, 5, NULL);
}
TEST_END()

TEST_BEGIN(read_illegal_size, EXPECT_FAIL)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));

    int out[8] = {0};
    int ret = ring_buf_read(&rbuf, -1, out);
}
TEST_END()

TEST_BEGIN(read_buf_null, EXPECT_FAIL)
{
    ring_buf_t rbuf = {0};

    int out[8] = {0};
    int ret = ring_buf_read(&rbuf, 8, out);
}
TEST_END()

TEST_BEGIN(read_write)
{
    ring_buf_t rbuf = ring_buf_create(32, sizeof(int));

    int data[] = {1, 2, 3, 4, 5};
    int out[32] = {0};
    ring_buf_write(&rbuf, data, 5);
    //   1 2 3 4 5 ...
    // r         w
    ASSERT_INT_EQ(rbuf.length, 5);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 5);

    ring_buf_write(&rbuf, data, 2);
    //   1 2 3 4 5 1 2 ...
    // r             w
    ASSERT_INT_EQ(rbuf.length, 7);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 7);

    ring_buf_read(&rbuf, 4, out);
    // 1 2 3 4 5 1 2 ...
    //       r     w
    ASSERT_INT_EQ(rbuf.length, 3);
    ASSERT_INT_EQ(rbuf.read_idx, 4);
    ASSERT_INT_EQ(rbuf.write_idx, 7);
    {
        int expected[] = {1, 2, 3, 4};
        ASSERT_MEM_EQ(out, expected, 4 * sizeof(int));
    }

    ring_buf_write(&rbuf, data, 5);
    // 1 2 3 4 5 1 2 1 2 3 4 5 ...
    //       r               w
    ASSERT_INT_EQ(rbuf.length, 8);
    ASSERT_INT_EQ(rbuf.read_idx, 4);
    ASSERT_INT_EQ(rbuf.write_idx, 12);

    ring_buf_read(&rbuf, 7, out);
    // 1 2 3 4 5 1 2 1 2 3 4 5 ...
    //                     r w
    ASSERT_INT_EQ(rbuf.length, 1);
    ASSERT_INT_EQ(rbuf.read_idx, 11);
    ASSERT_INT_EQ(rbuf.write_idx, 12);
    {
        int expected[] = {5, 1, 2, 1, 2, 3, 4};
        ASSERT_MEM_EQ(out, expected, 7 * sizeof(int));
    }

    int ret = ring_buf_read(&rbuf, 5, out);
    ASSERT_INT_NEQ(ret, 0);
    // 1 2 3 4 5 1 2 1 2 3 4 5
    //                     r w
    ASSERT_INT_EQ(rbuf.length, 1);
    ASSERT_INT_EQ(rbuf.read_idx, 11);
    ASSERT_INT_EQ(rbuf.write_idx, 12);
    {
        int expected[] = {5, 1, 2, 1, 2, 3, 4};
        ASSERT_MEM_EQ(out, expected, 7 * sizeof(int));
    }
}
TEST_END()

TEST_BEGIN(read_write_overflow)
{
    ring_buf_t rbuf = ring_buf_create(8, sizeof(int));

    int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    int out[8] = {0};

    ring_buf_write(&rbuf, data, 5);
    // 1 2 3 4 5 0 0 0
    // r       w
    ASSERT_INT_EQ(rbuf.length, 5);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 5);
    {
        memcpy(out, rbuf.buf, 8 * sizeof(int));
        int expected[] = {1, 2, 3, 4, 5, 0, 0, 0};
        ASSERT_MEM_EQ(out, expected, 8 * sizeof(int));
    }

    ring_buf_write(&rbuf, data, 10);
    // 1 2 3 4 5 1 2  3
    // 4 5 6 7 8 9 10 3
    // r           w
    ASSERT_INT_EQ(rbuf.length, 8);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 7);
    {
        memcpy(out, rbuf.buf, 8 * sizeof(int));
        int expected[] = {4, 5, 6, 7, 8, 9, 10, 3};
        ASSERT_MEM_EQ(out, expected, 8 * sizeof(int));
    }

    ring_buf_read(&rbuf, 3, out);
    // 4 5 6 7 8 9 10 3
    //     r       w
    ASSERT_INT_EQ(rbuf.length, 5);
    ASSERT_INT_EQ(rbuf.read_idx, 3);
    ASSERT_INT_EQ(rbuf.write_idx, 7);
    {
        int expected[] = {4, 5, 6};
        ASSERT_MEM_EQ(out, expected, 3 * sizeof(int));
    }

    ring_buf_write(&rbuf, data, 20);
    // 4  5  6  7  8  9  10 1
    // 2  3  4  5  6  7  8  9
    // 10 11 12 13 14 15 16 17
    // 18 19 20 13 14 15 16 17
    //       rw
    ASSERT_INT_EQ(rbuf.length, 8);
    ASSERT_INT_EQ(rbuf.read_idx, 3);
    ASSERT_INT_EQ(rbuf.write_idx, 3);
    {
        memcpy(out, rbuf.buf, 8 * sizeof(int));
        int expected[] = {18, 19, 20, 13, 14, 15, 16, 17};
        ASSERT_MEM_EQ(out, expected, 8 * sizeof(int));
    }

    ring_buf_read(&rbuf, 5, out);
    //   18 19 20 13 14 15 16 17
    // r       w
    ASSERT_INT_EQ(rbuf.length, 3);
    ASSERT_INT_EQ(rbuf.read_idx, 0);
    ASSERT_INT_EQ(rbuf.write_idx, 3);
    {
        int expected[] = {13, 14, 15, 16, 17};
        ASSERT_MEM_EQ(out, expected, 5 * sizeof(int));
    }

    ring_buf_read(&rbuf, 1, out);
    // 18 19 20 13 14 15 16 17
    // r     w
    ASSERT_INT_EQ(rbuf.length, 2);
    ASSERT_INT_EQ(rbuf.read_idx, 1);
    ASSERT_INT_EQ(rbuf.write_idx, 3);
    {
        int expected[] = {18};
        ASSERT_MEM_EQ(out, expected, 1 * sizeof(int));
    }

    ring_buf_read(&rbuf, 2, out);
    // 18 19 20 13 14 15 16 17
    //       rw
    ASSERT_INT_EQ(rbuf.length, 0);
    ASSERT_INT_EQ(rbuf.read_idx, 3);
    ASSERT_INT_EQ(rbuf.write_idx, 3);
    {
        int expected[] = {19, 20};
        ASSERT_MEM_EQ(out, expected, 2 * sizeof(int));
    }
}
TEST_END()
