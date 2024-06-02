#ifndef __AP_QUEUE_H
#define __AP_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

#define AP_QUEUE_IS_EMPTY(q) (q->head == NULL || q->len <= 0)

typedef struct APQueueEntry
{
    struct APQueueEntry *next;
    void *data;
} APQueueEntry;

typedef struct APQueue
{
    APQueueEntry *head;
    APQueueEntry *tail;
    int len;
    pthread_mutex_t mutex;
} APQueue;

void ap_queue_init(APQueue *q);
APQueue *ap_queue_alloc();
void ap_queue_free(APQueue **q);
int ap_queue_push(APQueue *q, void *data);
void *ap_queue_pop(APQueue *q);
void ap_queue_clear(APQueue *q);

#endif /* __AP_QUEUE_H */
