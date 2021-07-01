#ifndef YTDL_HTTP_H
#define YTDL_HTTP_H
#include "tcp.h"
/* Parsers */
#include <llhttp.h>
/* TLS */
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YTDL_HTTPS_BUF_LEN 8192

typedef struct ytdl_http_header_s {
    uv_buf_t field;
    uv_buf_t value;
} ytdl_http_header_t;

typedef struct ytdl_http_headers_s {
    ytdl_http_header_t **base;
    size_t len;
} ytdl_http_headers_t;

typedef struct ytdl_http_client_s ytdl_http_client_t;

typedef struct ytdl_http_request_s {
    ytdl_http_headers_t headers;
    char* method;
    void *data; // opaque
} ytdl_http_request_t;

typedef void (*ytdl_http_client_status_cb)(ytdl_http_client_t *client, ytdl_net_status_t *status);
typedef void (*ytdl_http_client_read_cb)(ytdl_http_client_t* client, const uv_buf_t *buf);
typedef void (*ytdl_http_client_ready_cb)(ytdl_http_client_t *client);

typedef struct ytdl_https_write_job_s {
    char* base;
    size_t len;
    ytdl_http_client_status_cb cb;
} ytdl_https_write_job_t;

typedef struct ytdl_https_write_queue_s {
    ytdl_https_write_job_t **bufs;
    size_t len;
} ytdl_https_write_queue_t;

// Must be compatible with YTDL_TCP_CLIENT_CB_FIELDS
#define YTDL_HTTP_CLIENT_CB_FIELDS \
    ytdl_http_client_status_cb status_cb; \
    ytdl_http_client_read_cb read_cb; \
    ytdl_http_client_ready_cb ready_cb;

#define YTDL_HTTP_PARSER_FIELDS \
    llhttp_t parser; \
    llhttp_settings_t parser_settings;

#define YTDL_HTTPS_CLIENT_FIELDS \
    SSL *ssl; \
    SSL_CTX *tls_ctx; \
    char* tls_read_buf; \
    ytdl_https_write_queue_t tls_write_queue; \
    uv_poll_t tls_poll; \
    uv_close_cb tls_close_cb;

struct ytdl_http_client_s {
    /* Inherited */
    YTDL_TCP_CLIENT_FIELDS
    /* Callbacks */
    YTDL_HTTP_CLIENT_CB_FIELDS
    /* HTTP Parser */
    YTDL_HTTP_PARSER_FIELDS
    /* TLS */
    YTDL_HTTPS_CLIENT_FIELDS
};

void ytdl_http_client_parse_read (ytdl_http_client_t* client, const uv_buf_t *buf);
int ytdl_http_client_init (uv_loop_t *loop, ytdl_http_client_t *client);
int ytdl_http_client_connect (ytdl_http_client_t *client, ytdl_http_client_status_cb status_cb, ytdl_http_client_ready_cb ready_cb);
void ytdl_http_client_shutdown (ytdl_http_client_t *client, uv_close_cb close_cb);
void ytdl_http_client_write (ytdl_http_client_t *client, uv_buf_t *buf, ytdl_http_client_status_cb write_cb);
int ytdl_http_client_set_url (ytdl_http_client_t *client, const char* url);

int ytdl_http_header_is_equal (const char* src, const size_t src_len, const char* target, const size_t target_len);

#ifdef __cplusplus
}
#endif
#endif