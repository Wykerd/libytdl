#ifndef YTDL_BUF_H
#define YTDL_BUF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct ytdl_buf_s {
  char* base;
  size_t len;
  size_t size;
} ytdl_buf_t;

char *ytdl_buf_alloc (ytdl_buf_t *buf, size_t size);
char *ytdl_buf_realloc (ytdl_buf_t *buf, size_t size);
char *ytdl_buf_grow (ytdl_buf_t *buf, double grow_factor);
void ytdl_buf_free (ytdl_buf_t *buf);

#ifdef __cplusplus
}
#endif
#endif