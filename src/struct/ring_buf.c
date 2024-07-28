#include "ring_buf.h"
#include "_math.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

ring_buf_t ring_buf_create(int capacity, int item_size)
{
    assert(capacity > 0 && item_size > 0);

    ring_buf_t rbuf = {0};
    rbuf.capacity = capacity;
    rbuf.item_size = item_size;

    rbuf.buf = calloc(item_size, capacity);
    if (rbuf.buf == NULL)
        errno = -ENOMEM;

    return rbuf;
}

void ring_buf_free(ring_buf_t *rbuf)
{
    if (rbuf == NULL)
        return;

    if (rbuf->buf != NULL)
        free(rbuf->buf);
    rbuf->buf = NULL;

    memset(rbuf, 0, sizeof(*rbuf));
}

int ring_buf_write(ring_buf_t *rbuf, void *mem, int item)
{
    assert(rbuf != NULL && rbuf->buf != NULL && mem != NULL && item > 0);
    if (item == 0)
        return 0;

    int items_to_write = item;
    const char *src = (const char *)mem;

    while (items_to_write > 0)
    {
        int space_left = rbuf->capacity - rbuf->write_idx;
        int items_now = (items_to_write < space_left) ? items_to_write : space_left;

        memcpy(rbuf->buf + (rbuf->write_idx * rbuf->item_size), src, items_now * rbuf->item_size);

        items_to_write -= items_now;
        src += items_now * rbuf->item_size;
        rbuf->write_idx = (rbuf->write_idx + items_now) % rbuf->capacity;
        rbuf->length = MATH_MIN(rbuf->length + items_now, rbuf->capacity);
    }

    return 0;
}

int ring_buf_read(ring_buf_t *rbuf, int req_item, void *out)
{
    assert(rbuf != NULL && rbuf->buf != NULL && req_item > 0 && out != NULL);
    if (req_item > rbuf->length)
        return -ENODATA;

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

    return 0;
}
