#ifndef __RING_BUF_H
#define __RING_BUF_H

typedef struct ring_buf_t
{
    void *buf;
    int capacity;
    int item_size;
    int length;
    int read_idx;
    int write_idx;
} ring_buf_t;

ring_buf_t ring_buf_create(int capacity, int item_size);
void ring_buf_free(ring_buf_t *rbuf);
int ring_buf_write(ring_buf_t *rbuf, void *mem, int item);
int ring_buf_read(ring_buf_t *rbuf, int req_item, void *out);

#endif /* __RING_BUF_H */
