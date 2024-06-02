#include "ap_queue.h"

#include <stdlib.h>

void ap_queue_init(APQueue *q)
{
    pthread_mutex_init(&q->mutex, NULL);
}

APQueue *ap_queue_alloc()
{
    APQueue *q = calloc(1, sizeof(*q));
    if (!q)
        return NULL;

    ap_queue_init(q);

    return q;
}

void ap_queue_free(APQueue **q)
{
    if (!q || !*q)
        return;

    pthread_mutex_destroy(&(*q)->mutex);
    ap_queue_clear(*q);

    free(*q);
    *q = NULL;
}

int ap_queue_push(APQueue *q, void *data)
{
    APQueueEntry *entry = calloc(1, sizeof(*entry));
    if (!entry)
        return -1;
    entry->data = data;

    pthread_mutex_lock(&q->mutex);

    if (AP_QUEUE_IS_EMPTY(q))
        q->head = entry;
    else
        q->tail->next = entry;

    q->tail = entry;
    q->len++;

    pthread_mutex_unlock(&q->mutex);

    return 0;
}

void *ap_queue_pop(APQueue *q)
{
    pthread_mutex_lock(&q->mutex);

    if (AP_QUEUE_IS_EMPTY(q))
        return NULL;

    void *data = q->head->data;

    APQueueEntry *next = q->head->next;
    if (!next)
        q->tail = NULL;

    free(q->head);
    q->head = next;
    q->len--;

    pthread_mutex_unlock(&q->mutex);

    return data;
}

void ap_queue_clear(APQueue *q)
{
    pthread_mutex_lock(&q->mutex);

    APQueueEntry **entry = &q->head;
    q->head = NULL;
    q->tail = NULL;
    q->len = 0;

    pthread_mutex_unlock(&q->mutex);

    while (*entry)
    {
        APQueueEntry **next = &(*entry)->next;
        free(*entry);
        *entry = NULL;
        entry = next;
    }
}
