#ifndef __RING_BUF_H
#define __RING_BUF_H

#include <pthread.h>

typedef struct ring_buf_t
{
    void *buf;
    int capacity;
    int item_size;
    int length;
    int read_idx;
    int write_idx;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} ring_buf_t;

ring_buf_t ring_buf_create(int capacity, int item_size);
void ring_buf_free(ring_buf_t *rbuf);
int ring_buf_write(ring_buf_t *rbuf, const void *mem, int items);
int ring_buf_read(ring_buf_t *rbuf, int req_item, void *out);
int ring_buf_try_read(ring_buf_t *rbuf, int req_item, void *out);
void ring_buf_reset(ring_buf_t *rbuf);

#endif /* __RING_BUF_H */
