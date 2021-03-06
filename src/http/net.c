#include <ytdl/http/net.h>
/* Standard Library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Networking */
#include <netdb.h>
/* TLS */
#include <openssl/ssl.h>
#include <openssl/err.h>

int ytdl_net_is_numeric_host_af (const char *hostname, int family) {
    struct in6_addr dst;
    return uv_inet_pton(family, hostname, &dst) == 1;
}

int ytdl_net_is_numeric_host_v6 (const char *hostname) {
    return ytdl_net_is_numeric_host_af(hostname, AF_INET6);
}

int ytdl_net_is_numeric_host_v4 (const char *hostname) {
    return ytdl_net_is_numeric_host_af(hostname, AF_INET);
}

int ytdl_net_is_numeric_host_v (const char *hostname) {
    int v = 0;
    if (ytdl_net_is_numeric_host_v4(hostname)) 
        v = 4;
    else if (ytdl_net_is_numeric_host_v6(hostname)) 
        v = 6;
    return v;
}

int ytdl_net_is_numeric_host (const char *hostname) {
    return ytdl_net_is_numeric_host_v6(hostname) ||
           ytdl_net_is_numeric_host_v4(hostname);
}

void ytdl_ssl_init () {
    /* Init openssl */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}
