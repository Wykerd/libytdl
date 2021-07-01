#ifndef FA_TCP_H
#define FA_TCP_H
#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*imp_tcp_client_status_cb)(imp_tcp_client_t *client, imp_net_status_t *status);
typedef void (*imp_tcp_client_read_cb)(imp_tcp_client_t *client, uv_buf_t *buf);
typedef void (*imp_tcp_client_connect_cb)(imp_tcp_client_t *client);

#define FA_TCP_CLIENT_SETTINGS_FIELDS \
    int keep_alive; \
    unsigned int keep_alive_secs;

#define FA_TCP_CLIENT_FIELDS \
    void *data; \
    uv_loop_t *loop; \
    imp_url_t *url; \
    imp_tcp_client_settings_t settings; \
    uv_tcp_t tcp; \
    imp_tcp_client_connect_cb connect_cb;

#define FA_TCP_CLIENT_CB_FIELDS \
    imp_tcp_client_status_cb status_cb; \
    imp_tcp_client_read_cb read_cb;

typedef struct imp_tcp_client_settings_s {
    FA_TCP_CLIENT_SETTINGS_FIELDS
} imp_tcp_client_settings_t;

struct imp_tcp_client_s {
    FA_TCP_CLIENT_FIELDS
    FA_TCP_CLIENT_CB_FIELDS
};

int imp_tcp_client_init (uv_loop_t *loop, imp_tcp_client_t *client);
int imp_tcp_client_connect (imp_tcp_client_t *client, imp_tcp_client_status_cb status_cb, imp_tcp_client_connect_cb connect_cb);

#ifdef __cplusplus
}
#endif
#endif