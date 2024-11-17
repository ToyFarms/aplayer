#include "arena_allocator.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static arena_block *arena_block_create(size_t size)
{
    void *mem = calloc(sizeof(arena_block) + size, 1);
    arena_block *block = (arena_block *)mem;
    if (block == NULL)
        return NULL;

    block->next = NULL;
    block->capacity = size;
    block->len = 0;
    block->data = mem + sizeof(arena_block);

    return block;
}

void arena_free(arena_allocator *arena)
{
    if (arena == NULL)
        return;

    arena_block *block = arena->first;
    while (block)
    {
        arena_block *next = block->next;
        free(block);
        block = next;
    }

    memset(arena, 0, sizeof(*arena));
}

arena_allocator arena_create(size_t block_size)
{
    arena_allocator arena = {0};
    arena.block_size = block_size;

    arena.first = NULL;
    arena.current = NULL;

    return arena;
}

void *arena_alloc(arena_allocator *arena, size_t size)
{
    if (arena->current == NULL ||
        (size > arena->current->capacity - arena->current->len))
    {
        size_t block_size = arena->block_size;
        if (size > block_size)
            block_size = size;

        arena_block *block = arena_block_create(block_size);
        if (block == NULL)
            return NULL;

        if (arena->current == NULL)
        {
            arena->first = block;
            arena->current = block;
        }
        else
        {
            arena->current->next = block;
            arena->current = block;
        }
    }

    void *ptr = arena->current->data + arena->current->len;
    assert(ptr);

    arena->current->len += size;

    return ptr;
}

void *arena_memdup(arena_allocator *arena, void *mem, size_t size)
{
    if (mem == NULL)
        return NULL;

    void *ptr = arena_alloc(arena, size);
    if (ptr == NULL)
        return NULL;

    memcpy(ptr, mem, size);

    return ptr;
}
