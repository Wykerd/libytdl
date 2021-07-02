#include <ytdl/buffer.h>

#include <stdlib.h>
#include <string.h>

char *ytdl_buf_alloc (ytdl_buf_t *buf, size_t size)
{
    buf->base = malloc(size);
    buf->size = size;
    buf->len = 0;
    return buf->base;
}

char *ytdl_buf_realloc (ytdl_buf_t *buf, size_t size)
{
    buf->base = realloc(buf->base, size);
    buf->size = size;
    return buf->base;
}

char *ytdl_buf_grow (ytdl_buf_t *buf, double grow_factor)
{
    buf->size *= grow_factor; 
    buf->base = realloc(buf->base, buf->size);
    return buf->base;
}

void ytdl_buf_free (ytdl_buf_t *buf)
{
    if (buf->base != NULL)
        free(buf->base);
    buf->base = NULL;
}
