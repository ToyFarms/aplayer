#ifndef __ARENA_ALLOCATOR_H
#define __ARENA_ALLOCATOR_H

#include <stdio.h>

typedef struct arena_block
{
    struct arena_block *next;
    size_t capacity;
    size_t len;
    void *data;
} arena_block;

typedef struct arena_allocator
{
    arena_block *first;
    arena_block *current;
    size_t block_size;
} arena_allocator;

arena_allocator arena_create(size_t block_size);
void arena_free(arena_allocator *arena);
void *arena_alloc(arena_allocator *arena, size_t size);
void *arena_memdup(arena_allocator *arena, void *mem, size_t size);

#endif /* __ARENA_ALLOCATOR_H */
