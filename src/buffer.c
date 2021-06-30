#include <ytdl/buffer.h>

#include <stdlib.h>
#include <string.h>

void ytdl_buf_alloc (ytdl_buf_t *buf, size_t size)
{
    buf->base = malloc(size);
    buf->size = size;
    buf->len = 0;
}

void ytdl_buf_grow (ytdl_buf_t *buf, double grow_factor)
{
    buf->size *= grow_factor; 
    buf->base = realloc(buf->base, buf->size);
}

void ytdl_buf_free (ytdl_buf_t *buf)
{
    free(buf->base);
}
