#ifndef FA_HTTP_H
#define FA_HTTP_H
#include "tcp.h"
/* Parsers */
#include <llhttp.h>
/* TLS */
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FA_HTTPS_BUF_LEN 8192

typedef struct imp_http_header_s {
    uv_buf_t field;
    uv_buf_t value;
} imp_http_header_t;

typedef struct imp_http_headers_s {
    imp_http_header_t **base;
    size_t len;
} imp_http_headers_t;

typedef struct imp_http_client_s imp_http_client_t;

typedef struct imp_http_request_s {
    imp_http_headers_t headers;
    char* method;
    void *data; // opaque
} imp_http_request_t;

typedef void (*imp_http_client_status_cb)(imp_http_client_t *client, imp_net_status_t *status);
typedef void (*imp_http_client_read_cb)(imp_http_client_t* client, const uv_buf_t *buf);
typedef void (*imp_http_client_ready_cb)(imp_http_client_t *client);

typedef struct imp_https_write_job_s {
    char* base;
    size_t len;
    imp_http_client_status_cb cb;
} imp_https_write_job_t;

typedef struct imp_https_write_queue_s {
    imp_https_write_job_t **bufs;
    size_t len;
} imp_https_write_queue_t;

// Must be compatible with FA_TCP_CLIENT_CB_FIELDS
#define FA_HTTP_CLIENT_CB_FIELDS \
    imp_http_client_status_cb status_cb; \
    imp_http_client_read_cb read_cb; \
    imp_http_client_ready_cb ready_cb;

#define FA_HTTP_PARSER_FIELDS \
    llhttp_t parser; \
    llhttp_settings_t parser_settings;

#define FA_HTTPS_CLIENT_FIELDS \
    SSL *ssl; \
    SSL_CTX *tls_ctx; \
    char* tls_read_buf; \
    imp_https_write_queue_t tls_write_queue; \
    uv_poll_t tls_poll; \
    uv_close_cb tls_close_cb;

struct imp_http_client_s {
    /* Inherited */
    FA_TCP_CLIENT_FIELDS
    /* Callbacks */
    FA_HTTP_CLIENT_CB_FIELDS
    /* HTTP Parser */
    FA_HTTP_PARSER_FIELDS
    /* TLS */
    FA_HTTPS_CLIENT_FIELDS
};

void imp_http_client_parse_read (imp_http_client_t* client, const uv_buf_t *buf);
int imp_http_client_init (uv_loop_t *loop, imp_http_client_t *client);
int imp_http_client_connect (imp_http_client_t *client, imp_http_client_status_cb status_cb, imp_http_client_ready_cb ready_cb);
void imp_http_client_shutdown (imp_http_client_t *client, uv_close_cb close_cb);
void imp_http_client_write (imp_http_client_t *client, uv_buf_t *buf, imp_http_client_status_cb write_cb);
int imp_http_client_set_url (imp_http_client_t *client, const char* url);

int imp_http_header_is_equal (const char* src, const size_t src_len, const char* target, const size_t target_len);

typedef enum imp_http_request_err {
    FA_HR_E_OK,
    FA_HR_E_FIELD_NAME, // INVALID FIELD NAME
} imp_http_request_err_t;

void imp_http_headers_init (imp_http_headers_t *headers);
imp_http_request_err_t imp_http_headers_push_buf (imp_http_headers_t *headers, uv_buf_t *field, uv_buf_t *value);
imp_http_request_err_t imp_http_headers_push (imp_http_headers_t *headers, char* field, char* value);
void imp_http_headers_free (imp_http_headers_t *headers);

imp_http_request_t *imp_http_request_init (const char* method);
uv_buf_t *imp_http_request_header_with_path (imp_http_request_t *req, imp_url_path_t *path, 
                                             int include_content_type, const char* host_name);
uv_buf_t *imp_http_request_serialize_with_path (imp_http_request_t *req, uv_buf_t *body, 
                                                imp_url_path_t *path, const char* host_name);
uv_buf_t *imp_http_request_serialize_with_url (imp_http_request_t *req, uv_buf_t *body, imp_url_t *url);
void imp_http_request_serialize_free (uv_buf_t *buf);
void imp_http_request_free (imp_http_request_t *req);

#ifdef __cplusplus
}
#endif
#endif