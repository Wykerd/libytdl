#ifndef FA_NET_H
#define FA_NET_H
/* Parsers */
#include "img-panda/parsers/url.h"
/* Event loop */ 
#include <uv.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct imp_tcp_client_s imp_tcp_client_t;

enum imp_net_err_type {
    FA_NET_E_OK, // Success.
    FA_NET_E_UNKNOWN,
    FA_NET_NOT_2XX, // Status is not 2XX
    FA_NET_E_GETADDRINFO, // Failed to resolve host.
    FA_NET_E_TCP_INIT, // Failed to initialize TCP handle.
    FA_NET_E_CONNECT_INIT, // Failed to initialize TCP connection.
    FA_NET_E_TCP_CONNECT, // Failed to establish TCP connection.
    FA_NET_E_TCP_EOF,
    FA_NET_E_READ_CB, 
    FA_NET_E_READ_POLL, 
    FA_NET_E_READ_INIT,
    FA_NET_E_WRITE_INIT,
    FA_NET_E_WRITE_CB,
    FA_NET_E_UNSUPPORTED_SCHEMA, // Unsupported schema.
    FA_NET_E_POLL_INIT,
    FA_NET_E_POLL,
    FA_NET_E_PARSE, // Error parsing HTTP response.
    FA_NET_E_PROTOCOL_UPGRADE,
    FA_NET_E_DISCONNECTED,
    FA_NET_E_TLS, // Generic TLS error.
    FA_NET_E_TLS_CTX_METHOD, // Could not get TLS method.
    FA_NET_E_TLS_CTX_NEW, // Could not create TLS context.
    FA_NET_E_TLS_CTX_VERIFY_PATHS,
    FA_NET_E_TLS_CTX_CIPHERS,
    FA_NET_E_TLS_NEW, // Could not create OpenSSL session.
    FA_NET_E_TLS_SET_FD,
    FA_NET_E_TLS_SESSION, // Generic Session error.
    FA_NET_E_TLS_SESSION_HOST,
    FA_NET_E_TLS_SESSION_CLOSED, // The TLS connection has been closed.
    FA_NET_E_TLS_SESSION_HANDSHAKE_FAILED, // TLS Handshake failed.
    FA_NET_E_TLS_SESSION_READ_FAILED, // Failed to read.
    FA_NET_E_TLS_SESSION_WRITE_FAILED,
};

#define imp_net_is_tls_err(x) (x >= FA_NET_E_TLS)
#define imp_net_is_tls_session_err(x) (x >= FA_NET_E_TLS_SESSION)

typedef struct imp_net_status_s {
    enum imp_net_err_type type;
    ssize_t code;
} imp_net_status_t;

void imp_ssl_init ();

int imp_net_is_numeric_host_af (const char *hostname, int family);
int imp_net_is_numeric_host_v6 (const char *hostname);
int imp_net_is_numeric_host_v4 (const char *hostname);
int imp_net_is_numeric_host_v (const char *hostname);
int imp_net_is_numeric_host (const char *hostname);

#ifdef __cplusplus
}
#endif
#endif