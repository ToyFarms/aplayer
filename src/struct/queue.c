#include "queue.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

queue_t queue_create()
{
    queue_t q = {0};

    pthread_mutex_init(&q.mutex, NULL);

    return q;
}

void queue_free(queue_t *q)
{
    if (q == NULL)
        return;

    pthread_mutex_destroy(&q->mutex);
    queue_clear(q);
    memset(q, 0, sizeof(*q));
}

int queue_push(queue_t *q, void *data)
{
    queue_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return -1;
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
