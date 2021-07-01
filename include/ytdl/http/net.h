#ifndef YTDL_HTTP_NET_H
#define YTDL_HTTP_NET_H
/* Event loop */ 
#include <uv.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ytdl_tcp_client_s ytdl_tcp_client_t;

enum ytdl_net_err_type {
    YTDL_NET_E_OK, // Success.
    YTDL_NET_E_UNKNOWN,
    YTDL_NET_NOT_2XX, // Status is not 2XX
    YTDL_NET_E_GETADDRINFO, // Failed to resolve host.
    YTDL_NET_E_TCP_INIT, // Failed to initialize TCP handle.
    YTDL_NET_E_CONNECT_INIT, // Failed to initialize TCP connection.
    YTDL_NET_E_TCP_CONNECT, // Failed to establish TCP connection.
    YTDL_NET_E_TCP_EOF,
    YTDL_NET_E_READ_CB, 
    YTDL_NET_E_READ_POLL, 
    YTDL_NET_E_READ_INIT,
    YTDL_NET_E_WRITE_INIT,
    YTDL_NET_E_WRITE_CB,
    YTDL_NET_E_UNSUPPORTED_SCHEMA, // Unsupported schema.
    YTDL_NET_E_POLL_INIT,
    YTDL_NET_E_POLL,
    YTDL_NET_E_PARSE, // Error parsing HTTP response.
    YTDL_NET_E_PROTOCOL_UPGRADE,
    YTDL_NET_E_DISCONNECTED,
    YTDL_NET_E_TLS, // Generic TLS error.
    YTDL_NET_E_TLS_CTX_METHOD, // Could not get TLS method.
    YTDL_NET_E_TLS_CTX_NEW, // Could not create TLS context.
    YTDL_NET_E_TLS_CTX_VERIFY_PATHS,
    YTDL_NET_E_TLS_CTX_CIPHERS,
    YTDL_NET_E_TLS_NEW, // Could not create OpenSSL session.
    YTDL_NET_E_TLS_SET_FD,
    YTDL_NET_E_TLS_SESSION, // Generic Session error.
    YTDL_NET_E_TLS_SESSION_HOST,
    YTDL_NET_E_TLS_SESSION_CLOSED, // The TLS connection has been closed.
    YTDL_NET_E_TLS_SESSION_HANDSHAKE_FAILED, // TLS Handshake failed.
    YTDL_NET_E_TLS_SESSION_READ_FAILED, // Failed to read.
    YTDL_NET_E_TLS_SESSION_WRITE_FAILED,
};

#define ytdl_net_is_tls_err(x) (x >= YTDL_NET_E_TLS)
#define ytdl_net_is_tls_session_err(x) (x >= YTDL_NET_E_TLS_SESSION)

typedef struct ytdl_net_status_s {
    enum ytdl_net_err_type type;
    ssize_t code;
} ytdl_net_status_t;

void ytdl_ssl_init ();

int ytdl_net_is_numeric_host_af (const char *hostname, int family);
int ytdl_net_is_numeric_host_v6 (const char *hostname);
int ytdl_net_is_numeric_host_v4 (const char *hostname);
int ytdl_net_is_numeric_host_v (const char *hostname);
int ytdl_net_is_numeric_host (const char *hostname);

#ifdef __cplusplus
}
#endif
#endif