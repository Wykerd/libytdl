#include "img-panda/http/http.h"
#include "img-panda/b64.h"
/* Standard Library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void imp_http_client_parse_read (imp_http_client_t* client, const uv_buf_t *buf) {
    llhttp_errno_t err = llhttp_execute(&client->parser, buf->base, buf->len);
    if (err != HPE_OK) {
        imp_net_status_t error = {
            .type = FA_NET_E_PARSE,
            .code = err
        };

        client->status_cb(client, &error);
    }

    if (client->parser.upgrade == 1) {
        imp_net_status_t error = {
            .type = FA_NET_E_PROTOCOL_UPGRADE,
            .code = err
        };

        client->status_cb(client, &error);
    }
};

static void imp__http_client_write_cb (uv_write_t* req, int status) {
    imp_http_client_status_cb cb = req->data;
    imp_http_client_t *client = req->handle->data;

    if (status != 0) {
        imp_net_status_t error = {
            .type = FA_NET_E_WRITE_CB,
            .code = status
        };

        cb(client, &error);

        free(req);

        return;
    }

    imp_net_status_t error = {
        .type = FA_NET_E_OK,
        .code = status
    };

    cb(client, &error);

    free(req);
};

void imp_http_client_write (imp_http_client_t *client, uv_buf_t *buf, imp_http_client_status_cb write_cb) {
    if (client->ssl != NULL) {
        client->tls_write_queue.bufs = realloc(client->tls_write_queue.bufs, (++client->tls_write_queue.len) * sizeof(imp_https_write_job_t *));
    
        client->tls_write_queue.bufs[client->tls_write_queue.len - 1] = malloc(sizeof(imp_https_write_job_t));

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->base = malloc(buf->len);

        memcpy(client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->base, buf->base, buf->len);

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->len = buf->len;

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->cb = write_cb;
    } else {
        uv_write_t *write_req = malloc(sizeof(uv_write_t));

        write_req->data = write_cb;

        int r = uv_write(write_req, (uv_stream_t*)&client->tcp, buf, 1, *imp__http_client_write_cb);

        if (r != 0) {
            imp_net_status_t error = {
                .type = FA_NET_E_WRITE_INIT,
                .code = r
            };

            write_cb(client, &error);
        }
    }
}

int imp_http_client_init (uv_loop_t *loop, imp_http_client_t *client) {
    imp_tcp_client_init(loop, (imp_tcp_client_t *)client);

    /* Clear all the pointers */
    client->tls_ctx = NULL;
    client->ssl = NULL;
    client->ready_cb = NULL;
    client->read_cb = imp_http_client_parse_read;

    /* Initialize parser */
    llhttp_settings_init(&client->parser_settings);
    llhttp_init(&client->parser, HTTP_RESPONSE, &client->parser_settings);
    client->parser.data = client;

    /* Write queue */
    client->tls_write_queue.bufs = malloc(sizeof(imp_https_write_job_t *));
    client->tls_write_queue.len = 0;

    return 1;
};

static void imp__http_client_tls_shutdown_cb (uv_idle_t* handle) {
    imp_http_client_t *client = handle->data;

    int err = SSL_shutdown(client->ssl);

    if (err < 0) {
        if (err == SSL_ERROR_WANT_READ ||
            err == SSL_ERROR_WANT_WRITE ||
            err == SSL_ERROR_WANT_X509_LOOKUP) {
            return;
        } else {
            goto free_ssl;
        }
    } else {
free_ssl:
        uv_idle_stop(handle);
        free(handle);

        SSL_free(client->ssl);
        if (client->tls_ctx != NULL) {
            SSL_CTX_free(client->tls_ctx);
        };

        int err = uv_tcp_close_reset(&client->tcp, client->tls_close_cb);
        if (err)
            client->tls_close_cb((uv_handle_t *)&client->tcp);
        return;
    }
}

void imp_http_client_shutdown (imp_http_client_t *client, uv_close_cb close_cb) {
    for (size_t i = 0; i < client->tls_write_queue.len; i++) {
        free(client->tls_write_queue.bufs[i]);
    }
    free(client->tls_write_queue.bufs);

    if (client->url != NULL) {
        imp_url_free(client->url);
        client->url = NULL;
    }

    client->tcp.data = client;

    int err = 0;
    if (client->ssl != NULL) {
        uv_poll_stop(&client->tls_poll);

        uv_idle_t *sht = malloc(sizeof(uv_idle_t));

        uv_idle_init(client->loop, sht);

        sht->data = client;

        client->tls_close_cb = close_cb;

        err = uv_idle_start(sht, imp__http_client_tls_shutdown_cb);
    } else {
        err = uv_tcp_close_reset(&client->tcp, close_cb);
    }
    if (err) {
        close_cb((uv_handle_t *)&client->tcp);
    }
}

int imp_http_client_set_url (imp_http_client_t *client, const char* url) {
    client->url = imp_parse_url(url, strlen(url));
    return client->url != NULL;
};

static void imp__http_client_alloc_cb (
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf
) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

static void imp__http_client_read_cb (uv_stream_t *tcp, ssize_t nread, const uv_buf_t * buf) {
    imp_http_client_t *client = tcp->data;

    if (nread > 0) {
        uv_buf_t out = {
            .base = buf->base,
            .len = (size_t)nread
        };

        client->read_cb(client, &out);
    } else {
        if (nread != UV_EOF) {
            imp_net_status_t error = {
                .type = FA_NET_E_READ_CB,
                .code = nread
            };

            // kill reading
            uv_read_stop(tcp);

            client->status_cb(client, &error);
        } else {
            // kill reading
            uv_read_stop(tcp);

            imp_net_status_t stat = {
                .type = FA_NET_E_TCP_EOF,
                .code = nread
            };

            client->status_cb(client, &stat);
        }
    }

    free(buf->base);
}

static void imp__http_client_tls_poll_cb (uv_poll_t* handle, int status, int events) {
    imp_http_client_t *client = handle->data;

    if (status < 0) {
        uv_poll_stop(handle);

        imp_net_status_t stat = {
            .type = FA_NET_E_POLL,
            .code = status
        };

        (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

        return;
    };

    int err, free_handle = 0, has_notified_of_closed = 0;
    size_t nread;
    int success;
    if (events & UV_READABLE) {
read_loop:
        nread = 0;
        success = SSL_read_ex(client->ssl, client->tls_read_buf, FA_HTTPS_BUF_LEN * sizeof(char), &nread);
        
        err = SSL_get_error(client->ssl, success);

        if (!success) {
            if (err == SSL_ERROR_WANT_READ ||
                err == SSL_ERROR_WANT_WRITE ||
                err == SSL_ERROR_WANT_X509_LOOKUP) {
                goto exit_read;
            } else if (err == SSL_ERROR_ZERO_RETURN) {
                free_handle = 1;
                has_notified_of_closed = 1;

                imp_net_status_t stat = {
                    .type = FA_NET_E_TLS_SESSION_CLOSED,
                    .code = err
                };

                (*(imp_http_client_status_cb)client->status_cb)(client, &stat);
            } else {
                free_handle = 1;

                if (err = SSL_ERROR_SYSCALL) {
                    // if (!uv_is_active(&client->tcp)) puts("\n\nINACTIVE");
                    char c;
                    ssize_t x = recv(SSL_get_fd(client->ssl), &c, 1, MSG_PEEK);
                    if (x == 0) {
                        free_handle = 1;
                        has_notified_of_closed = 1;

                        imp_net_status_t stat = {
                            .type = FA_NET_E_TLS_SESSION_CLOSED,
                            .code = err
                        };

                        (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

                        puts("FIN\n\n");

                        goto write_exit;
                    }
                }

                imp_net_status_t stat = {
                    .type = FA_NET_E_TLS_SESSION_READ_FAILED,
                    .code = err
                };

                client->status_cb(client, &stat);
            }
        } else {
            uv_buf_t read_buf = {
                .base = client->tls_read_buf,
                .len = nread
            };

            client->read_cb(client, &read_buf);

            if (SSL_pending(client->ssl) > 0) {
                goto read_loop;
            } 
        };
    };

exit_read:
    #define FA_HTTP_POP_BUF \
        free(buf->base); \
        free(buf); \
        client->tls_write_queue.bufs = realloc(client->tls_write_queue.bufs, (--client->tls_write_queue.len) * sizeof(imp_https_write_job_t *));

    if (events & UV_WRITABLE) {
        if (client->tls_write_queue.len > 0) {
            imp_https_write_job_t *buf = client->tls_write_queue.bufs[client->tls_write_queue.len - 1];

            if (buf->len == 0) {
                FA_HTTP_POP_BUF
                goto exit_read;
            };
            
            err = SSL_write(client->ssl, buf->base, buf->len);

            err = SSL_get_error(client->ssl, err);

            if (err < 0) {
                if (err == SSL_ERROR_WANT_READ ||
                    err == SSL_ERROR_WANT_WRITE ||
                    err == SSL_ERROR_WANT_X509_LOOKUP) {
                    goto write_exit;
                } else if (err == SSL_ERROR_ZERO_RETURN) {
                    free_handle = 1;

                    imp_net_status_t stat = {
                        .type = FA_NET_E_TLS_SESSION_CLOSED,
                        .code = err
                    };

                    puts("CLOSING SESSION");

                    buf->cb(client, &stat);

                    if (!has_notified_of_closed) {
                        has_notified_of_closed = 1;
                        
                        (*(imp_http_client_status_cb)client->status_cb)(client, &stat);
                    };

                    FA_HTTP_POP_BUF
                } else {

                    imp_net_status_t stat = {
                        .type = FA_NET_E_TLS_SESSION_WRITE_FAILED,
                        .code = err
                    };

                    buf->cb(client, &stat);

                    FA_HTTP_POP_BUF
                }
            } else {
                imp_net_status_t stat = {
                    .type = FA_NET_E_OK,
                    .code = err
                };

                buf->cb(client, &stat);

                FA_HTTP_POP_BUF
            }
        }
    }

    #undef FA_HTTP_POP_BUF

write_exit:
    if (free_handle) {
        uv_poll_stop(handle);
    }
}

static void imp__http_client_tls_handshake_cb (uv_idle_t* handle) {
    imp_http_client_t *client = handle->data;

    int success = SSL_connect(client->ssl);
    
    if (success < 0) {
        int err = SSL_get_error(client->ssl, success);

        if (err == SSL_ERROR_WANT_READ ||
            err == SSL_ERROR_WANT_WRITE ||
            err == SSL_ERROR_WANT_X509_LOOKUP) {
            return; // Handshake still in progress
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            uv_idle_stop(handle);
            free(handle);
            
            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_SESSION_CLOSED,
                .code = err
            };

            client->status_cb(client, &stat);

            return;
        } else {
            uv_idle_stop(handle);
            free(handle);

            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_SESSION_HANDSHAKE_FAILED,
                .code = err
            };

            client->status_cb(client, &stat);

            return;
        }
    } else {
        // Handshake is complete
        uv_idle_stop(handle);
        free(handle);
        // Start polling for I/O
        
        // Get socket file descriptor
        uv_os_fd_t sock_fd; 
        uv_fileno((uv_handle_t *)&client->tcp, &sock_fd);

        int err;
        err = uv_poll_init_socket(client->loop, &client->tls_poll, sock_fd);

        if (err) {
            imp_net_status_t stat = {
                .type = FA_NET_E_POLL_INIT,
                .code = err
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        client->tls_poll.data = client;

        err = uv_poll_start(&client->tls_poll, UV_READABLE | UV_WRITABLE, imp__http_client_tls_poll_cb);

        if (err) {
            imp_net_status_t stat = {
                .type = FA_NET_E_POLL_INIT,
                .code = err
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        client->ready_cb(client);
    }
}

static void imp__http_client_tcp_connect_cb (
    imp_tcp_client_t *tcp_client
) {
    imp_http_client_t *client = (imp_http_client_t *)tcp_client;

    int r;

    if (!strcmp(client->url->schema, "https")) {
        // reset ssl if client was used before
        if (client->ssl != NULL) {
            SSL_free(client->ssl);
            if (client->tls_ctx != NULL) {
                SSL_CTX_free(client->tls_ctx);
            };
        }

        // Setup OpenSSL session
        const SSL_METHOD *meth;
        
        ERR_clear_error();

        meth = TLS_client_method();

        if (meth == NULL) {
            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_CTX_METHOD,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        ERR_clear_error();

        client->tls_ctx = SSL_CTX_new(meth);

        if (client->tls_ctx == NULL) {
            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_CTX_NEW,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };

        ERR_clear_error();

        if (!SSL_CTX_set_default_verify_paths(client->tls_ctx)) {
            SSL_CTX_free(client->tls_ctx); // deref

            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_CTX_VERIFY_PATHS,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        SSL_CTX_set_verify(client->tls_ctx, SSL_VERIFY_PEER, NULL);

        ERR_clear_error();

        client->ssl = SSL_new(client->tls_ctx);
        if (client->ssl == NULL) {
            SSL_CTX_free(client->tls_ctx); // deref

            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_NEW,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };
        
        ERR_clear_error();

        const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";

        if (!SSL_set_cipher_list(client->ssl, PREFERRED_CIPHERS)) {
            SSL_CTX_free(client->tls_ctx); // deref
            SSL_free(client->ssl);

            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_CTX_CIPHERS,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        ERR_clear_error();

        if (!imp_net_is_numeric_host(client->url->host)) {
            if (!SSL_set_tlsext_host_name(client->ssl, client->url->host)) {
                SSL_CTX_free(client->tls_ctx); // deref
                SSL_free(client->ssl);

                imp_net_status_t stat = {
                    .type = FA_NET_E_TLS_SESSION_HOST,
                    .code = ERR_get_error()
                };

                (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

                return;
            }
        };

        client->tls_read_buf = calloc(sizeof(char), FA_HTTPS_BUF_LEN);

        // Get socket file descriptor
        uv_os_fd_t sock_fd; 
        uv_fileno((uv_handle_t *)&client->tcp, &sock_fd);

        ERR_clear_error();

        if (!SSL_set_fd(client->ssl, sock_fd)) {
            SSL_CTX_free(client->tls_ctx); // deref
            SSL_free(client->ssl);
            client->ssl = NULL;

            imp_net_status_t stat = {
                .type = FA_NET_E_TLS_SET_FD,
                .code = ERR_get_error()
            };

            (*(imp_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };

        SSL_set_connect_state(client->ssl); // set ssl to work in client mode.

        uv_idle_t *handshake = malloc(sizeof(uv_idle_t));

        uv_idle_init(client->loop, handshake);

        handshake->data = client;

        uv_idle_start(handshake, imp__http_client_tls_handshake_cb);

        return;
    } else if (!strcmp(client->url->schema, "http")) {
        r = uv_read_start((uv_stream_t *)&client->tcp, *imp__http_client_alloc_cb, *imp__http_client_read_cb);

        if (r != 0) {
            uv_read_stop((uv_stream_t*)&client->tcp);

            imp_net_status_t error = {
                .type = FA_NET_E_READ_INIT,
                .code = r
            };

            client->status_cb(client, &error);

            return;
        }

        client->ready_cb(client);
    } else {
        imp_net_status_t stat = {
            .type = FA_NET_E_UNSUPPORTED_SCHEMA,
            .code = 0
        };

        (*(imp_http_client_status_cb)client->status_cb)(client, &stat);
    }
}

int imp_http_client_connect (imp_http_client_t *client, imp_http_client_status_cb status_cb, imp_http_client_ready_cb ready_cb) {
    client->ready_cb = ready_cb;

    return imp_tcp_client_connect((imp_tcp_client_t *)client, (imp_tcp_client_status_cb)status_cb, imp__http_client_tcp_connect_cb);;
}

static const char tokens[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
       ' ',     '!',      0,      '#',     '$',     '%',     '&',    '\'',
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        0,       0,      '*',     '+',      0,      '-',     '.',      0,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
       '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
       '8',     '9',      0,       0,       0,       0,       0,       0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
        0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
       'x',     'y',     'z',      0,       0,       0,      '^',     '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
       '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
       'x',     'y',     'z',      0,      '|',      0,      '~',       0 };

#define TOKEN(c) ((c == ' ') ? 0 : tokens[(unsigned char)c])

int imp_http_header_is_equal (const char* src, const size_t src_len, const char* target, const size_t target_len) {
    if (src_len != target_len) return 0;

    int is_equal = 1;

    for (size_t x = 0; x < target_len; x++) {
        is_equal = TOKEN(src[x]) == TOKEN(target[x]);
        if (!is_equal) return 0;
    };

    return 1;
}

static const char auth[] = "authorization";
static const char host[] = "host";
static const char content_type[] = "content-type";
static const char http_version[] = "HTTP/1.1";

static const uv_buf_t content_type_header[2] = {
    {
        .base = "Content-Type",
        .len = 12
    },
    {
        .base = "application/octet-stream",
        .len = 24
    }
};

static const uv_buf_t content_length_header = {
    .base = "Content-Length",
    .len = 14
};

void imp_http_headers_init (imp_http_headers_t *headers) {
    headers->base = malloc(sizeof(imp_http_header_t *));
    headers->len = 0;
};

imp_http_request_err_t imp_http_headers_push_buf (imp_http_headers_t *headers, uv_buf_t *field, uv_buf_t *value) {
    headers->base = realloc(headers->base, sizeof(imp_http_header_t *) * ++headers->len);
    
    headers->base[headers->len - 1] = malloc(sizeof(imp_http_header_t));

    headers->base[headers->len - 1]->field.base = malloc(sizeof(char) * (field->len + 1));
    headers->base[headers->len - 1]->value.base = malloc(sizeof(char) * (value->len + 1));
    headers->base[headers->len - 1]->field.len = field->len;
    headers->base[headers->len - 1]->value.len = value->len;

    for (size_t i = 0; i < field->len; i++) {
        char tok = TOKEN(field->base[i]);
        if (!tok) return FA_HR_E_FIELD_NAME;
        headers->base[headers->len - 1]->field.base[i] = field->base[i];
    };

    headers->base[headers->len - 1]->field.base[field->len] = 0;

    // TODO: Field Value validation
    memcpy(headers->base[headers->len - 1]->value.base, value->base, value->len);
    headers->base[headers->len - 1]->value.base[value->len] = 0;

    return FA_HR_E_OK;
};

imp_http_request_err_t imp_http_headers_push (imp_http_headers_t *headers, char* field, char* value) {
    uv_buf_t field_buf = {
        .base = field,
        .len = strlen(field)
    };

    uv_buf_t value_buf = {
        .base = value,
        .len = strlen(value)
    };
    
    return imp_http_headers_push_buf(headers, &field_buf, &value_buf);
}

void imp_http_headers_free (imp_http_headers_t *headers) {
    for (size_t i = 0; i < headers->len; i++) {
        free(headers->base[i]->field.base);
        free(headers->base[i]->value.base);
        free(headers->base[i]);
    }
    free(headers->base);
}

imp_http_request_t *imp_http_request_init (const char* method) {
    imp_http_request_t *req = malloc(sizeof(imp_http_request_t));

    imp_http_headers_init(&req->headers);

    size_t method_len = strlen(method);
    req->method = malloc(sizeof(char) * (method_len + 1));
    memcpy(req->method, method, method_len);
    req->method[method_len] = 0;

    return req;
};

uv_buf_t *imp_http_request_header_with_path (imp_http_request_t *req, imp_url_path_t *path, 
                                             int include_content_type, const char* host_name) 
{
    int has_host = 0, has_auth = 0, has_content_type = !include_content_type;

    uv_buf_t *header = malloc(sizeof(uv_buf_t));

    // Request line
    size_t method_len = strlen(req->method);
    size_t path_len = strlen(path->path);
    size_t query_len = strlen(path->query);

    // Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
    size_t request_line_len = method_len + 1 + (path_len ? path_len : 1) + (query_len ? query_len + 1 : 0) + 1 + 8 + 2;

    size_t header_len = request_line_len;

    // First pass calculate size + has host etc
    for (size_t i = 0; i < req->headers.len; i++) {
        header_len += req->headers.base[i]->field.len + 2 + req->headers.base[i]->value.len + 2;
    };

    // the header terminator CRLF
    header_len += 2;

    // allocate the memory
    header->len = header_len;
    header->base = malloc(header->len);

    size_t cursor = 0;

    // Add the request line
    memcpy(header->base, req->method, method_len);
    cursor += method_len;
    header->base[cursor++] = ' ';
    memcpy(header->base + cursor, path_len ? path->path : "/", (path_len ? path_len : 1));
    cursor += path_len ? path_len : 1;
    if (query_len) {
        header->base[cursor++] = '?';
        memcpy(header->base + cursor, path->query, query_len);
        cursor += query_len;
    };
    header->base[cursor++] = ' ';
    memcpy(header->base + cursor, http_version, 8);
    cursor += 8;
    header->base[cursor++] = '\r';
    header->base[cursor++] = '\n';

    // Add the headers
    for (size_t i = 0; i < req->headers.len; i++) {
        int is_auth = (req->headers.base[i]->field.len == 13) && (!has_auth), 
            is_host = (req->headers.base[i]->field.len == 4) && (!has_host),
            is_content_length = (req->headers.base[i]->field.len == 12) && (!has_content_type);
        
        for (size_t x = 0; x < req->headers.base[i]->field.len; x++) {
            if (is_auth) is_auth = TOKEN(req->headers.base[i]->field.base[x]) == auth[x]; 
            if (is_host) is_host = TOKEN(req->headers.base[i]->field.base[x]) == host[x];
            if (is_content_length) is_content_length = TOKEN(req->headers.base[i]->field.base[x]) == content_type[x];
            header->base[cursor++] = req->headers.base[i]->field.base[x];
        };

        has_host = has_host || is_host;
        has_auth = has_auth || is_auth;
        has_content_type = has_content_type || is_content_length;

        header->base[cursor++] = ':';
        header->base[cursor++] = ' ';
        memcpy(header->base + cursor, req->headers.base[i]->value.base, req->headers.base[i]->value.len);
        cursor += req->headers.base[i]->value.len;
        header->base[cursor++] = '\r';
        header->base[cursor++] = '\n';
    };

    size_t orig_header_len = req->headers.len;

    if (!has_host) 
        header_len += 4 + 2 + strlen(host_name) + 2;

    if (!has_auth) {
        size_t userinfo_len = strlen(path->userinfo);
        if (userinfo_len > 0) {
            // add basic auth
            size_t b64_len = base64_encoded_size(userinfo_len, FA_B64_MODE_NORMAL);

            char *b64_buf = calloc(sizeof(char), b64_len);

            if (!base64_encode(path->userinfo, userinfo_len, b64_buf, b64_len, FA_B64_MODE_NORMAL)) goto skip_auth;

            uv_buf_t field = {
                .base = "Authorization",
                .len = 13
            };

            uv_buf_t value;

            value.len = (sizeof(char) * 6) + b64_len;
            value.base = malloc(value.len);
            memcpy(value.base, "Basic ", sizeof(char) * 6);
            memcpy(value.base + (sizeof(char) * 6), b64_buf, b64_len);

            imp_http_headers_push_buf(&req->headers, &field, &value);

            free(b64_buf);
            free(value.base);

            header_len += field.len + 2 + strlen(value.base) + 2;
        };
    };

skip_auth:

    // If BODY != NULL this field will be set application/octet-stream as to rfc2616 spec
    if (!has_content_type) {
        imp_http_headers_push_buf(&req->headers, (uv_buf_t *)&content_type_header[0], (uv_buf_t *)&content_type_header[1]);
        header_len += content_type_header[0].len + 2 + content_type_header[1].len + 2;
    };

    size_t additional_len = header_len - header->len;

    if (additional_len) {
        header->len = header_len;
        header->base = realloc(header->base, header->len);

        for (size_t i = orig_header_len; i < req->headers.len; i++) {
            memcpy(header->base + cursor, req->headers.base[i]->field.base, req->headers.base[i]->field.len);
            cursor += req->headers.base[i]->field.len;
            header->base[cursor++] = ':';
            header->base[cursor++] = ' ';
            memcpy(header->base + cursor, req->headers.base[i]->value.base, req->headers.base[i]->value.len);
            cursor += req->headers.base[i]->value.len;
            header->base[cursor++] = '\r';
            header->base[cursor++] = '\n';
        }
    };

    if (!has_host) {
        memcpy(header->base + cursor, "Host", 4);
        cursor += 4;
        header->base[cursor++] = ':';
        header->base[cursor++] = ' ';
        size_t host_len = strlen(host_name);
        memcpy(header->base + cursor, host_name, host_len);
        cursor += host_len;
        header->base[cursor++] = '\r';
        header->base[cursor++] = '\n';
    };

    header->base[cursor++] = '\r';
    header->base[cursor++] = '\n';

    return header;
};

uv_buf_t *imp_http_request_serialize_with_path (imp_http_request_t *req, uv_buf_t *body, 
                                                imp_url_path_t *path, const char* host_name) 
{
    if ((body != NULL) && body->len) {
        uv_buf_t value;
        value.len = snprintf(NULL, 0, "%zu", body->len);
        value.base = calloc(sizeof(char), value.len + 1);
        snprintf(value.base, value.len + 1, "%zu", body->len);

        imp_http_headers_push_buf(&req->headers, (uv_buf_t *)&content_length_header, &value);

        free(value.base);
    }

    uv_buf_t *serial = imp_http_request_header_with_path(req, path, body != NULL, host_name);

    if ((body != NULL) && body->len) {
        size_t cursor = serial->len;
        serial->len += body->len;
        serial->base = realloc(serial->base, serial->len);
        memcpy(serial->base + cursor, body->base, body->len);
    };

    return serial;
};

uv_buf_t *imp_http_request_serialize_with_url (imp_http_request_t *req, uv_buf_t *body, imp_url_t *url) {
    return imp_http_request_serialize_with_path(req, body, (imp_url_path_t *)url, url->host);
};

void imp_http_request_serialize_free (uv_buf_t *buf) {
    free(buf->base);
    free(buf);
    buf = NULL;
};

void imp_http_request_free (imp_http_request_t *req) {
    imp_http_headers_free(&req->headers);
    free(req->method);
    free(req);
    req = NULL;
};
