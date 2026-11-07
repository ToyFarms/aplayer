#include "base_test.h"

INCLUDE_BEGIN
#include "arena_allocator.h"
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/struct/arena_allocator.c
 src/logger.c
 */ CFLAGS_END

TEST_BEGIN(init)
{
    arena_allocator arena = arena_create(1024);
    ASSERT_NULL(arena.first)
    ASSERT_NULL(arena.current)
    ASSERT_INT_EQ((int)arena.block_size, 1024);
}
TEST_END()

TEST_BEGIN(free)
{
    arena_allocator arena = arena_create(1024);
    arena_free(&arena);
    ASSERT_MEM_EQ(&arena, &(arena_allocator){0}, sizeof(arena_allocator));
}
TEST_END()

TEST_BEGIN(free_invalid)
{
    arena_allocator *arena = NULL;
    arena_free(arena);
    ASSERT_NULL(arena);
}
TEST_END()

TEST_BEGIN(init_invalid_size)
{
    arena_allocator arena = arena_create(-1);
    ASSERT_NULL(arena.first)
    ASSERT_NULL(arena.current)
    ASSERT_TRUE(arena.block_size == (size_t)-1);
}
TEST_END()

TEST_BEGIN(alloc)
{
    arena_allocator arena = arena_create(1024);
    void *mem = arena_alloc(&arena, 64);
    ASSERT_NOTNULL(mem);
    ASSERT_NOTNULL(arena.current);
    ASSERT_NOTNULL(arena.first);
    ASSERT_TRUE(arena.current == arena.first);
}
TEST_END()

TEST_BEGIN(alloc_same_size)
{
    arena_allocator arena = arena_create(1024);
    void *mem = arena_alloc(&arena, 1024);
    ASSERT_NOTNULL(mem);
    ASSERT_NOTNULL(arena.current);
    ASSERT_NOTNULL(arena.first);
    ASSERT_NULL(arena.first->next);
    ASSERT_TRUE(arena.current == arena.first);
}
TEST_END()

TEST_BEGIN(alloc_larger)
{
    arena_allocator arena = arena_create(1024);
    void *mem = arena_alloc(&arena, 2048);
    ASSERT_NOTNULL(mem);
    ASSERT_NOTNULL(arena.current);
    ASSERT_NOTNULL(arena.first);
    ASSERT_TRUE(arena.current == arena.first);
}
TEST_END()

TEST_BEGIN(alloc_overflow)
{
    arena_allocator arena = arena_create(1024);
    void *mem;

    mem = arena_alloc(&arena, 64);
    mem = arena_alloc(&arena, 1024);
    ASSERT_NOTNULL(mem);
    ASSERT_NOTNULL(arena.current);
    ASSERT_NOTNULL(arena.first);
    ASSERT_NOTNULL(arena.first->next);
    ASSERT_TRUE(arena.first->next == arena.current);
    ASSERT_TRUE(arena.current != arena.first);
}
TEST_END()

TEST_BEGIN(alloc_overflow2)
{
    arena_allocator arena = arena_create(1024);
    void *mem;

    mem = arena_alloc(&arena, 64);
    mem = arena_alloc(&arena, 2048);
    mem = arena_alloc(&arena, 1024);
    ASSERT_NOTNULL(mem);
    ASSERT_NOTNULL(arena.current);
    ASSERT_NOTNULL(arena.first);
    ASSERT_NOTNULL(arena.first->next);
    ASSERT_NOTNULL(arena.first->next->next);
    ASSERT_TRUE(arena.first->next->next == arena.current);
    ASSERT_TRUE(arena.current != arena.first);
}
TEST_END()

TEST_BEGIN(alloc_memdup)
{
    arena_allocator arena = arena_create(1024);
    void *mem;

    char *str = "Hello World!";
    mem = arena_memdup(&arena, str, strlen(str));

    ASSERT_NOTNULL(mem);
    ASSERT_MEM_EQ(mem, str, strlen(str));
}
TEST_END()

TEST_BEGIN(alloc_memdup_overflow)
{
    arena_allocator arena = arena_create(4);
    void *mem;

    char *str = "Hello World!";
    mem = arena_memdup(&arena, str, strlen(str));

    ASSERT_NULL(arena.first->next);
    ASSERT_TRUE(arena.first == arena.current);
    ASSERT_NOTNULL(mem);
    ASSERT_MEM_EQ(mem, str, strlen(str));
}
TEST_END()

TEST_BEGIN(alloc_memdup_overflow2)
{
    arena_allocator arena = arena_create(4);
    void *mem;

    char *str = "Hello World!";
    mem = arena_memdup(&arena, str, strlen(str));

    ASSERT_NOTNULL(mem);
    ASSERT_MEM_EQ(mem, str, strlen(str));

    mem = arena_memdup(&arena, str, strlen(str));

    ASSERT_NOTNULL(arena.first->next);
    ASSERT_TRUE(arena.first != arena.current);
    ASSERT_TRUE(arena.first->next == arena.current);
    ASSERT_NOTNULL(mem);
    ASSERT_MEM_EQ(mem, str, strlen(str));
}
TEST_END()
