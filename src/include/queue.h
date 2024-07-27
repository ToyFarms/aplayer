#ifndef __QUEUE_H
#define __QUEUE_H

#include "struct.h"

#include <pthread.h>

typedef struct queue_entry_t
{
    struct queue_entry_t *next;
    void *data;
} queue_entry_t;

typedef struct queue_t
{
    queue_entry_t *head;
    queue_entry_t *tail;
    int len;
    pthread_mutex_t mutex;
    FREE_DEF(free);
    FREEP_DEF(freep);
} queue_t;

#define QUEUE_IS_EMPTY(q) ((q)->head == NULL || (q)->len <= 0)

queue_t queue_create();
void queue_free(queue_t *q);
int queue_push(queue_t *q, void *data);
void *queue_pop(queue_t *q);
void queue_clear(queue_t *q);

#endif /* __QUEUE_H */
