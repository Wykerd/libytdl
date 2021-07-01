#ifndef YTDL_TCP_H
#define YTDL_TCP_H
#include "net.h"
#include <uriparser/Uri.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ytdl_tcp_client_status_cb)(ytdl_tcp_client_t *client, ytdl_net_status_t *status);
typedef void (*ytdl_tcp_client_read_cb)(ytdl_tcp_client_t *client, uv_buf_t *buf);
typedef void (*ytdl_tcp_client_connect_cb)(ytdl_tcp_client_t *client);

#define YTDL_TCP_CLIENT_SETTINGS_FIELDS \
    int keep_alive; \
    unsigned int keep_alive_secs;

#define YTDL_TCP_CLIENT_FIELDS \
    void *data; \
    uv_loop_t *loop; \
    UriUriA url; \
    ytdl_tcp_client_settings_t settings; \
    uv_tcp_t tcp; \
    ytdl_tcp_client_connect_cb connect_cb;

#define YTDL_TCP_CLIENT_CB_FIELDS \
    ytdl_tcp_client_status_cb status_cb; \
    ytdl_tcp_client_read_cb read_cb;

typedef struct ytdl_tcp_client_settings_s {
    YTDL_TCP_CLIENT_SETTINGS_FIELDS
} ytdl_tcp_client_settings_t;

struct ytdl_tcp_client_s {
    YTDL_TCP_CLIENT_FIELDS
    YTDL_TCP_CLIENT_CB_FIELDS
};

int ytdl_tcp_client_init (uv_loop_t *loop, ytdl_tcp_client_t *client);
int ytdl_tcp_client_connect (ytdl_tcp_client_t *client, ytdl_tcp_client_status_cb status_cb, ytdl_tcp_client_connect_cb connect_cb);

#ifdef __cplusplus
}
#endif
#endif