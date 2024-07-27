#include "base_test.h"

INCLUDE_BEGIN
#include "queue.h"
INCLUDE_END

CFLAGS_BEGIN/*
-Isrc/include
src/struct/queue.c
*/CFLAGS_END

TEST_BEGIN(init)
{
    queue_t q = queue_create();
    ASSERT_INT_EQ(q.len, 0);
    ASSERT_NULL(q.head);
    ASSERT_NULL(q.tail);
}
TEST_END()

TEST_BEGIN(_free)
{
    queue_t q = queue_create();
    queue_free(&q);
}
TEST_END()

TEST_BEGIN(free_item_stack, EXPECT_FAIL)
{
    queue_t q = queue_create();
    q.free = free;

    int data[] = {23, 45, 67};
    queue_push(&q, &data[0]);
    queue_push(&q, &data[1]);
    queue_push(&q, &data[2]);

    queue_free(&q);
}
TEST_END()

TEST_BEGIN(free_item_stack_no_free)
{
    queue_t q = queue_create();

    int data[] = {23, 45, 67};
    queue_push(&q, &data[0]);
    queue_push(&q, &data[1]);
    queue_push(&q, &data[2]);

    queue_free(&q);
}
TEST_END()

TEST_BEGIN(free_item_heap)
{
    queue_t q = queue_create();
    q.free = free;

    int *data = malloc(3 * sizeof(int));
    ASSERT_NOTNULL(data);

    queue_push(&q, data);

    queue_free(&q);

    ASSERT_MEM_EQ(&q, &(queue_t){0}, sizeof(queue_t));
}
TEST_END()

TEST_BEGIN(push)
{
    queue_t q = queue_create();
    int data = 54;
    int ret = queue_push(&q, &data);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(q.len, 1);
    ASSERT_NOTNULL(q.head);
}
TEST_END()

TEST_BEGIN(push_3item)
{
    queue_t q = queue_create();
    int data[] = {23, 45, 67};
    int ret = queue_push(&q, &data[0]);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(q.len, 1);
    ret = queue_push(&q, &data[1]);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(q.len, 2);
    ret = queue_push(&q, &data[2]);
    ASSERT_INT_EQ(ret, 0);
    ASSERT_INT_EQ(q.len, 3);

    ASSERT_NOTNULL(q.head);
    ASSERT_NOTNULL(q.tail);
}
TEST_END()

TEST_BEGIN(pop)
{
    queue_t q = queue_create();
    int data = 54;
    queue_push(&q, &data);

    int *res = queue_pop(&q);
    ASSERT_NOTNULL(res);
    ASSERT_MEM_EQ(res, &data, sizeof(int));
    ASSERT_INT_EQ(q.len, 0);
    ASSERT_NULL(q.head);
}
TEST_END()

TEST_BEGIN(pop_3item)
{
    queue_t q = queue_create();
    int data[] = {23, 45, 67};
    queue_push(&q, &data[0]);
    queue_push(&q, &data[1]);
    queue_push(&q, &data[2]);

    int *res = queue_pop(&q);
    ASSERT_NOTNULL(res);
    ASSERT_MEM_EQ(res, &data[0], sizeof(int));
    ASSERT_INT_EQ(q.len, 2);

    res = queue_pop(&q);
    ASSERT_NOTNULL(res);
    ASSERT_MEM_EQ(res, &data[1], sizeof(int));
    ASSERT_INT_EQ(q.len, 1);

    res = queue_pop(&q);
    ASSERT_NOTNULL(res);
    ASSERT_MEM_EQ(res, &data[2], sizeof(int));
    ASSERT_INT_EQ(q.len, 0);
}
TEST_END()

TEST_BEGIN(pop_empty)
{
    queue_t q = queue_create();

    int *res = queue_pop(&q);
    ASSERT_NULL(res);
    ASSERT_INT_EQ(q.len, 0);
}
TEST_END()

TEST_BEGIN(pop_empty3times)
{
    queue_t q = queue_create();

    void *res = queue_pop(&q);
    ASSERT_NULL(res);
    ASSERT_INT_EQ(q.len, 0);
    res = queue_pop(&q);
    ASSERT_NULL(res);
    ASSERT_INT_EQ(q.len, 0);
    res = queue_pop(&q);
    ASSERT_NULL(res);
    ASSERT_INT_EQ(q.len, 0);
}
TEST_END()
