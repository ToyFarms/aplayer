#include "ring_buf.h"
#include "_math.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

ring_buf_t ring_buf_create(int capacity, int item_size)
{
    assert(capacity > 0 && item_size > 0);
    errno = 0;
    ring_buf_t rbuf = {0};
    rbuf.capacity = capacity;
    rbuf.item_size = item_size;
    rbuf.buf = calloc(item_size, capacity);

    if (rbuf.buf == NULL)
    {
        errno = -ENOMEM;
        return rbuf;
    }

    if (pthread_mutex_init(&rbuf.mutex, NULL) != 0)
    {
        free(rbuf.buf);
        rbuf.buf = NULL;
        errno = -ENOMEM;
        return rbuf;
    }

    if (pthread_cond_init(&rbuf.cond_not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&rbuf.mutex);
        free(rbuf.buf);
        rbuf.buf = NULL;
        errno = -ENOMEM;
        return rbuf;
    }

    if (pthread_cond_init(&rbuf.cond_not_full, NULL) != 0)
    {
        pthread_cond_destroy(&rbuf.cond_not_empty);
        pthread_mutex_destroy(&rbuf.mutex);
        free(rbuf.buf);
        rbuf.buf = NULL;
        errno = -ENOMEM;
        return rbuf;
    }

    return rbuf;
}

void ring_buf_free(ring_buf_t *rbuf)
{
    if (rbuf == NULL)
        return;

    if (rbuf->buf != NULL)
    {
        pthread_mutex_lock(&rbuf->mutex);
        free(rbuf->buf);
        rbuf->buf = NULL;
        pthread_mutex_unlock(&rbuf->mutex);

        pthread_cond_destroy(&rbuf->cond_not_empty);
        pthread_cond_destroy(&rbuf->cond_not_full);
        pthread_mutex_destroy(&rbuf->mutex);
    }

    memset(rbuf, 0, sizeof(*rbuf));
}

int ring_buf_write(ring_buf_t *rbuf, const void *mem, int items)
{
    assert(rbuf != NULL && rbuf->buf != NULL && mem != NULL && items > 0);

    if (items == 0)
        return 0;

    pthread_mutex_lock(&rbuf->mutex);

    int items_to_write = items;
    const char *src = (const char *)mem;

    while (items_to_write > 0)
    {
        int space_left = rbuf->capacity - rbuf->write_idx;
        int items_now =
            (items_to_write < space_left) ? items_to_write : space_left;

        memcpy(rbuf->buf + (rbuf->write_idx * rbuf->item_size), src,
               items_now * rbuf->item_size);

        items_to_write -= items_now;
        src += items_now * rbuf->item_size;
        rbuf->write_idx = (rbuf->write_idx + items_now) % rbuf->capacity;
        rbuf->length = MATH_MIN(rbuf->length + items_now, rbuf->capacity);
    }

    pthread_cond_signal(&rbuf->cond_not_empty);
    pthread_mutex_unlock(&rbuf->mutex);

    return 0;
}

int ring_buf_read(ring_buf_t *rbuf, int req_item, void *out)
{
    assert(rbuf != NULL && rbuf->buf != NULL && out != NULL);

    pthread_mutex_lock(&rbuf->mutex);

    if (req_item <= 0 || req_item > rbuf->length)
    {
        pthread_mutex_unlock(&rbuf->mutex);
        return -ENODATA;
    }

    if (rbuf->read_idx + req_item <= rbuf->capacity)
    {
        memcpy(out, rbuf->buf + (rbuf->read_idx * rbuf->item_size),
               req_item * rbuf->item_size);
        rbuf->read_idx += req_item;
    }
    else
    {
        int overflow = (rbuf->read_idx + req_item) - rbuf->capacity;
        int fit = req_item - overflow;

        if (fit != 0)
            memcpy(out, rbuf->buf + (rbuf->read_idx * rbuf->item_size),
                   fit * rbuf->item_size);

        memcpy(out + (fit * rbuf->item_size), rbuf->buf,
               overflow * rbuf->item_size);
        rbuf->read_idx = overflow;
    }

    rbuf->length -= req_item;
    rbuf->read_idx = rbuf->read_idx % rbuf->capacity;

    pthread_cond_signal(&rbuf->cond_not_full);
    pthread_mutex_unlock(&rbuf->mutex);

    return 0;
}

int ring_buf_try_read(ring_buf_t *rbuf, int req_item, void *out)
{
    assert(rbuf != NULL && rbuf->buf != NULL && out != NULL);

    if (pthread_mutex_trylock(&rbuf->mutex) != 0)
        return -EBUSY;

    if (req_item <= 0 || req_item > rbuf->length)
    {
        pthread_mutex_unlock(&rbuf->mutex);
        return -ENODATA;
    }

    if (rbuf->read_idx + req_item <= rbuf->capacity)
    {
        memcpy(out, rbuf->buf + (rbuf->read_idx * rbuf->item_size),
               req_item * rbuf->item_size);
        rbuf->read_idx += req_item;
    }
    else
    {
        int overflow = (rbuf->read_idx + req_item) - rbuf->capacity;
        int fit = req_item - overflow;

        if (fit != 0)
            memcpy(out, rbuf->buf + (rbuf->read_idx * rbuf->item_size),
                   fit * rbuf->item_size);

        memcpy(out + (fit * rbuf->item_size), rbuf->buf,
               overflow * rbuf->item_size);
        rbuf->read_idx = overflow;
    }

    rbuf->length -= req_item;
    rbuf->read_idx = rbuf->read_idx % rbuf->capacity;

    pthread_cond_signal(&rbuf->cond_not_full);
    pthread_mutex_unlock(&rbuf->mutex);

    return 0;
}

void ring_buf_reset(ring_buf_t *rbuf)
{
    if (rbuf == NULL)
        return;

    pthread_mutex_lock(&rbuf->mutex);
    rbuf->write_idx = 0;
    rbuf->read_idx = 0;
    rbuf->length = 0;
    pthread_mutex_unlock(&rbuf->mutex);
}
