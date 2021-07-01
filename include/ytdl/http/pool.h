#ifndef IMP_HTTP_POOL_H
#define IMP_HTTP_POOL_H

#include "img-panda/common.h"
#include "http.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct imp_http_worker_s imp_http_worker_t;
typedef struct imp_http_pool_s imp_http_pool_t;

typedef void (*imp_http_pool_err_cb)(imp_http_worker_t *worker, imp_net_status_t *status);

typedef void (*imp_http_pool_status_cb)(imp_http_worker_t *worker, imp_http_pool_t *pool);

struct imp_http_worker_s {
    imp_http_client_t client;

    // Events
    imp_http_pool_err_cb on_error;
    imp_http_pool_status_cb on_response;

    // Redirect
    int should_redirect;
    int is_location_header;
    int redirect_new_host;
    imp_url_t *redirect_url;

    // HTTP
    imp_http_request_t *last_request;
    uv_buf_t *last_request_buf;
    uv_buf_t *last_request_body;
    void* last_request_data;
    imp_reusable_buf_t last_response;
    imp_net_status_t last_error;

    // Status
    int working;
    int is_connected;
    int is_idle;

    // Parent
    imp_http_pool_t *pool;
    size_t pos;
};

typedef struct imp_http_worker_list_s {
    imp_http_worker_t **workers;
    size_t len;
} imp_http_worker_list_t;

// requests are freed automatically
typedef struct imp_http_worker_request_s {
    imp_http_request_t *request; // buffer content and buffer itself is freed internally!
    imp_url_t *url; // url is to be duped as it is freed internally!
    imp_http_pool_err_cb on_error;
    imp_http_pool_status_cb on_response;
    uv_buf_t *body; // not freed internally - free on callback last_request_body
    void* data; // opaque
} imp_http_worker_request_t;

typedef struct imp_http_pool_s {
    uv_loop_t *loop;

    imp_http_worker_list_t working_workers;
    imp_http_worker_list_t idle_workers;
    size_t pool_size;

    // This callback is called when a worker moves into another state (idle <-> working)
    imp_http_pool_status_cb on_state_change;
    imp_http_pool_err_cb on_error_default;

    imp_http_worker_request_t **queue;
    size_t queue_len;
    size_t queue_size;
    void *data; // opaque
} imp_http_pool_t;

int imp_http_worker_init (imp_http_pool_t *pool, imp_http_worker_t *worker);
int imp_http_pool_init (uv_loop_t *loop, imp_http_pool_t *pool, size_t worker_count);

int imp_http_pool_request (imp_http_pool_t *pool, imp_http_worker_request_t *request);

// Shutdown of individual workers happen async
void imp_http_pool_shutdown (imp_http_pool_t *pool);

#define imp_http_pool_default_headers(x) \
    imp_http_headers_push(x, "Connection", "keep-alive"); \
    imp_http_headers_push(x, "User-Agent", "img-panda/0.1.0")

#ifdef __cplusplus
}
#endif

#endif
