#include "queue.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

queue_t queue_create()
{
    errno = 0;
    queue_t q = {0};

    pthread_mutex_init(&q.mutex, NULL);

    return q;
}

void queue_free(queue_t *q)
{
    if (q == NULL)
        return;

    queue_clear(q);
    pthread_mutex_destroy(&q->mutex);
    memset(q, 0, sizeof(*q));
}

int queue_push(queue_t *q, void *data)
{
    queue_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return -ENOMEM;
    entry->data = data;

    pthread_mutex_lock(&q->mutex);

    if (QUEUE_IS_EMPTY(q))
        q->head = entry;
    else
        q->tail->next = entry;

    q->tail = entry;
    q->len++;

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

// TODO: add test for queue_push_copy
int queue_push_copy(queue_t *q, const void *data, size_t size)
{
    void *heap = malloc(size);
    if (heap == NULL)
        return -ENOMEM;

    memcpy(heap, data, size);

    queue_push(q, heap);

    return 0;
}

// TODO: add popfront and popback, this one is popfront
void *queue_pop(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    void *data = NULL;

    if (QUEUE_IS_EMPTY(q))
        goto exit;

    data = q->head->data;

    queue_entry_t *next = q->head->next;
    if (!next)
        q->tail = NULL;

    free(q->head);
    q->head = next;
    q->len--;

exit:
    pthread_mutex_unlock(&q->mutex);

    return data;
}

void queue_clear(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    if (QUEUE_IS_EMPTY(q))
        goto exit;

    queue_entry_t *entry = q->head;
    while (entry)
    {
        queue_entry_t *next = entry->next;

        FREE_EITHER(q, entry->data);
        free(entry);

        entry = next;
    }

    q->len = 0;
    q->head = NULL;
    q->tail = NULL;

exit:
    pthread_mutex_unlock(&q->mutex);
}

bool queue_is_empty(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    bool empty = QUEUE_IS_EMPTY(q);
    pthread_mutex_unlock(&q->mutex);

    return empty;
}
