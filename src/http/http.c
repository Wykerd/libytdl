#include <ytdl/http/http.h>
/* Standard Library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static 
void close_free_cb (uv_handle_t* handle)
{
    free(handle);
};

void ytdl_http_client_parse_read (ytdl_http_client_t* client, const uv_buf_t *buf) {
    llhttp_errno_t err = llhttp_execute(&client->parser, buf->base, buf->len);
    if (err != HPE_OK) {
        ytdl_net_status_t error = {
            .type = YTDL_NET_E_PARSE,
            .code = err
        };

        client->status_cb(client, &error);
    }

    if (client->parser.upgrade == 1) {
        ytdl_net_status_t error = {
            .type = YTDL_NET_E_PROTOCOL_UPGRADE,
            .code = err
        };

        client->status_cb(client, &error);
    }
};

static void ytdl__http_client_write_cb (uv_write_t* req, int status) {
    ytdl_http_client_status_cb cb = req->data;
    ytdl_http_client_t *client = req->handle->data;

    if (status != 0) {
        ytdl_net_status_t error = {
            .type = YTDL_NET_E_WRITE_CB,
            .code = status
        };

        cb(client, &error);

        uv_close((uv_handle_t *)req, close_free_cb);

        return;
    }

    ytdl_net_status_t error = {
        .type = YTDL_NET_E_OK,
        .code = status
    };

    cb(client, &error);

    uv_close((uv_handle_t *)req, close_free_cb);
};

void ytdl_http_client_write (ytdl_http_client_t *client, uv_buf_t *buf, ytdl_http_client_status_cb write_cb) {
    if (client->ssl != NULL) {
        client->tls_write_queue.bufs = realloc(client->tls_write_queue.bufs, (++client->tls_write_queue.len) * sizeof(ytdl_https_write_job_t *));
    
        client->tls_write_queue.bufs[client->tls_write_queue.len - 1] = malloc(sizeof(ytdl_https_write_job_t));

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->base = malloc(buf->len);

        memcpy(client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->base, buf->base, buf->len);

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->len = buf->len;

        client->tls_write_queue.bufs[client->tls_write_queue.len - 1]->cb = write_cb;
    } else {
        uv_write_t *write_req = malloc(sizeof(uv_write_t));

        write_req->data = write_cb;

        int r = uv_write(write_req, (uv_stream_t*)&client->tcp, buf, 1, *ytdl__http_client_write_cb);

        if (r != 0) {
            ytdl_net_status_t error = {
                .type = YTDL_NET_E_WRITE_INIT,
                .code = r
            };

            write_cb(client, &error);
        }
    }
}

int ytdl_http_client_init (uv_loop_t *loop, ytdl_http_client_t *client) {
    if (!ytdl_tcp_client_init(loop, (ytdl_tcp_client_t *)client))
        return 0;

    /* Clear all the pointers */
    client->tls_ctx = NULL;
    client->ssl = NULL;
    client->ready_cb = NULL;
    client->read_cb = ytdl_http_client_parse_read;
    client->hostname = NULL;

    /* Initialize parser */
    llhttp_settings_init(&client->parser_settings);
    llhttp_init(&client->parser, HTTP_RESPONSE, &client->parser_settings);
    client->parser.data = client;

    /* Write queue */
    client->tls_write_queue.bufs = malloc(sizeof(ytdl_https_write_job_t *));
    client->tls_write_queue.len = 0;

    client->tls_read_buf = NULL;

    client->is_tls = 0;

    return 1;
};

static void ytdl__http_client_tls_shutdown_cb (uv_idle_t* handle) {
    ytdl_http_client_t *client = handle->data;

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
        uv_close((uv_handle_t*)handle, close_free_cb);

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

void ytdl__tls_poll_close (uv_handle_t *handle) 
{
    ytdl_http_client_t *client = handle->data;
    int err = 0;
    uv_idle_t *sht = malloc(sizeof(uv_idle_t));

    uv_idle_init(client->loop, sht);

    sht->data = client;

    err = uv_idle_start(sht, ytdl__http_client_tls_shutdown_cb);
    if (err) {
        client->tls_close_cb((uv_handle_t *)&client->tcp);
    }
}

void ytdl_http_client_shutdown (ytdl_http_client_t *client, uv_close_cb close_cb) {
    for (size_t i = 0; i < client->tls_write_queue.len; i++) {
        free(client->tls_write_queue.bufs[i]);
    }
    free(client->tls_write_queue.bufs);

    if (client->tls_read_buf)
        free(client->tls_read_buf);

    client->tcp.data = client;

    if (client->hostname)
        free(client->hostname);

    int err = 0;
    if (client->ssl != NULL) {
        client->tls_close_cb = close_cb;
        uv_poll_stop(&client->tls_poll);
        uv_close((uv_handle_t*)&client->tls_poll, ytdl__tls_poll_close);
    } else {
        err = uv_tcp_close_reset(&client->tcp, close_cb);
    }
    if (err) {
        close_cb((uv_handle_t *)&client->tcp);
    }
}

static void ytdl__http_client_alloc_cb (
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf
) {
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

static void ytdl__http_client_read_cb (uv_stream_t *tcp, ssize_t nread, const uv_buf_t * buf) {
    ytdl_http_client_t *client = tcp->data;

    if (nread > 0) {
        uv_buf_t out = {
            .base = buf->base,
            .len = (size_t)nread
        };

        client->read_cb(client, &out);
    } else {
        if (nread != UV_EOF) {
            ytdl_net_status_t error = {
                .type = YTDL_NET_E_READ_CB,
                .code = nread
            };

            // kill reading
            uv_read_stop(tcp);

            client->status_cb(client, &error);
        } else {
            // kill reading
            uv_read_stop(tcp);

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TCP_EOF,
                .code = nread
            };

            client->status_cb(client, &stat);
        }
    }

    free(buf->base);
}

static void ytdl__http_client_tls_poll_cb (uv_poll_t* handle, int status, int events) {
    ytdl_http_client_t *client = handle->data;

    if (status < 0) {
        uv_poll_stop(handle);

        ytdl_net_status_t stat = {
            .type = YTDL_NET_E_POLL,
            .code = status
        };

        (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

        return;
    };

    int err, free_handle = 0, has_notified_of_closed = 0;
    size_t nread;
    int success;
    if (events & UV_READABLE) {
read_loop:
        nread = 0;
        success = SSL_read_ex(client->ssl, client->tls_read_buf, YTDL_HTTPS_BUF_LEN * sizeof(char), &nread);
        
        err = SSL_get_error(client->ssl, success);

        if (!success) {
            if (err == SSL_ERROR_WANT_READ ||
                err == SSL_ERROR_WANT_WRITE ||
                err == SSL_ERROR_WANT_X509_LOOKUP) {
                goto exit_read;
            } else if (err == SSL_ERROR_ZERO_RETURN) {
                free_handle = 1;
                has_notified_of_closed = 1;

                ytdl_net_status_t stat = {
                    .type = YTDL_NET_E_TLS_SESSION_CLOSED,
                    .code = err
                };

                (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);
            } else {
                free_handle = 1;

                if (err == SSL_ERROR_SYSCALL) {
                    // if (!uv_is_active(&client->tcp)) puts("\n\nINACTIVE");
                    char c;
                    ssize_t x = recv(SSL_get_fd(client->ssl), &c, 1, MSG_PEEK);
                    if (x == 0) {
                        free_handle = 1;
                        has_notified_of_closed = 1;

                        ytdl_net_status_t stat = {
                            .type = YTDL_NET_E_TLS_SESSION_CLOSED,
                            .code = err
                        };

                        (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

                        puts("FIN\n\n");

                        goto write_exit;
                    }
                }

                ytdl_net_status_t stat = {
                    .type = YTDL_NET_E_TLS_SESSION_READ_FAILED,
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
    #define YTDL_HTTP_POP_BUF \
        free(buf->base); \
        free(buf); \
        client->tls_write_queue.bufs = realloc(client->tls_write_queue.bufs, (--client->tls_write_queue.len) * sizeof(ytdl_https_write_job_t *));

    if (events & UV_WRITABLE) {
        if (client->tls_write_queue.len > 0) {
            ytdl_https_write_job_t *buf = client->tls_write_queue.bufs[client->tls_write_queue.len - 1];

            if (buf->len == 0) {
                YTDL_HTTP_POP_BUF
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

                    ytdl_net_status_t stat = {
                        .type = YTDL_NET_E_TLS_SESSION_CLOSED,
                        .code = err
                    };

                    puts("CLOSING SESSION");

                    buf->cb(client, &stat);

                    if (!has_notified_of_closed) {
                        has_notified_of_closed = 1;
                        
                        (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);
                    };

                    YTDL_HTTP_POP_BUF
                } else {

                    ytdl_net_status_t stat = {
                        .type = YTDL_NET_E_TLS_SESSION_WRITE_FAILED,
                        .code = err
                    };

                    buf->cb(client, &stat);

                    YTDL_HTTP_POP_BUF
                }
            } else {
                ytdl_net_status_t stat = {
                    .type = YTDL_NET_E_OK,
                    .code = err
                };

                buf->cb(client, &stat);

                YTDL_HTTP_POP_BUF
            }
        }
    }

    #undef YTDL_HTTP_POP_BUF

write_exit:
    if (free_handle) {
        uv_poll_stop(handle);
    }
}

static void ytdl__http_client_tls_handshake_cb (uv_idle_t* handle) {
    ytdl_http_client_t *client = handle->data;

    int success = SSL_connect(client->ssl);
    
    if (success < 0) {
        int err = SSL_get_error(client->ssl, success);

        if (err == SSL_ERROR_WANT_READ ||
            err == SSL_ERROR_WANT_WRITE ||
            err == SSL_ERROR_WANT_X509_LOOKUP) {
            return; // Handshake still in progress
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            uv_idle_stop(handle);
            uv_close((uv_handle_t*)handle, close_free_cb);
            
            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_SESSION_CLOSED,
                .code = err
            };

            client->status_cb(client, &stat);

            return;
        } else {
            uv_idle_stop(handle);
            uv_close((uv_handle_t*)handle, close_free_cb);

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_SESSION_HANDSHAKE_FAILED,
                .code = err
            };

            client->status_cb(client, &stat);

            return;
        }
    } else {
        // Handshake is complete
        uv_idle_stop(handle);
        uv_close((uv_handle_t*)handle, close_free_cb);
        // Start polling for I/O
        
        // Get socket file descriptor
        uv_os_fd_t sock_fd; 
        uv_fileno((uv_handle_t *)&client->tcp, &sock_fd);

        int err;
        err = uv_poll_init_socket(client->loop, &client->tls_poll, sock_fd);

        if (err) {
            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_POLL_INIT,
                .code = err
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        client->tls_poll.data = client;

        err = uv_poll_start(&client->tls_poll, UV_READABLE | UV_WRITABLE, ytdl__http_client_tls_poll_cb);

        if (err) {
            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_POLL_INIT,
                .code = err
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        client->ready_cb(client);
    }
}

static void ytdl__http_client_tcp_connect_cb (
    ytdl_tcp_client_t *tcp_client
) {
    ytdl_http_client_t *client = (ytdl_http_client_t *)tcp_client;

    int r;

    if (client->is_tls) {
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
            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_CTX_METHOD,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        ERR_clear_error();

        client->tls_ctx = SSL_CTX_new(meth);

        if (client->tls_ctx == NULL) {
            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_CTX_NEW,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };

        ERR_clear_error();

        if (!SSL_CTX_set_default_verify_paths(client->tls_ctx)) {
            SSL_CTX_free(client->tls_ctx); // deref

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_CTX_VERIFY_PATHS,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        SSL_CTX_set_verify(client->tls_ctx, SSL_VERIFY_PEER, NULL);

        ERR_clear_error();

        client->ssl = SSL_new(client->tls_ctx);
        if (client->ssl == NULL) {
            SSL_CTX_free(client->tls_ctx); // deref

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_NEW,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };
        
        ERR_clear_error();

        const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4";

        if (!SSL_set_cipher_list(client->ssl, PREFERRED_CIPHERS)) {
            SSL_CTX_free(client->tls_ctx); // deref
            SSL_free(client->ssl);

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_CTX_CIPHERS,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        }

        ERR_clear_error();

        if (!ytdl_net_is_numeric_host(client->hostname)) {
            if (!SSL_set_tlsext_host_name(client->ssl, client->hostname)) {
                SSL_CTX_free(client->tls_ctx); // deref
                SSL_free(client->ssl);

                ytdl_net_status_t stat = {
                    .type = YTDL_NET_E_TLS_SESSION_HOST,
                    .code = ERR_get_error()
                };

                (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

                return;
            }
        };

        client->tls_read_buf = calloc(sizeof(char), YTDL_HTTPS_BUF_LEN);

        // Get socket file descriptor
        uv_os_fd_t sock_fd; 
        uv_fileno((uv_handle_t *)&client->tcp, &sock_fd);

        ERR_clear_error();

        if (!SSL_set_fd(client->ssl, sock_fd)) {
            SSL_CTX_free(client->tls_ctx); // deref
            SSL_free(client->ssl);
            client->ssl = NULL;

            ytdl_net_status_t stat = {
                .type = YTDL_NET_E_TLS_SET_FD,
                .code = ERR_get_error()
            };

            (*(ytdl_http_client_status_cb)client->status_cb)(client, &stat);

            return;
        };

        SSL_set_connect_state(client->ssl); // set ssl to work in client mode.

        uv_idle_t *handshake = malloc(sizeof(uv_idle_t));

        uv_idle_init(client->loop, handshake);

        handshake->data = client;

        uv_idle_start(handshake, ytdl__http_client_tls_handshake_cb);

        return;
    } else {
        r = uv_read_start((uv_stream_t *)&client->tcp, *ytdl__http_client_alloc_cb, *ytdl__http_client_read_cb);

        if (r != 0) {
            uv_read_stop((uv_stream_t*)&client->tcp);

            ytdl_net_status_t error = {
                .type = YTDL_NET_E_READ_INIT,
                .code = r
            };

            client->status_cb(client, &error);

            return;
        }

        client->ready_cb(client);
    };
}

int ytdl_http_client_connect (ytdl_http_client_t *client, int is_tls, const char *host, const char *port, 
                              ytdl_http_client_status_cb status_cb, ytdl_http_client_ready_cb ready_cb)
{
    client->ready_cb = ready_cb;
    client->is_tls = is_tls;
    client->hostname = strdup(host);

    return ytdl_tcp_client_connect((ytdl_tcp_client_t *)client, host, port, (ytdl_tcp_client_status_cb)status_cb, ytdl__http_client_tcp_connect_cb);
}
