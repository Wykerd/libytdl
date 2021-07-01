#include "img-panda/http/tcp.h"
/* Standard Library */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int imp_tcp_client_init (uv_loop_t *loop, imp_tcp_client_t *client) {
    client->loop = loop;

    /* Clear all the pointers */
    client->url = NULL;
    client->connect_cb = NULL;
    client->status_cb = NULL;
    client->read_cb = NULL;

    /* Default http client settings */
    client->settings.keep_alive = 0;
    client->settings.keep_alive_secs = 1;

    return 1;
};

static void imp__tcp_client_connect_cb (
    uv_connect_t* req, 
    int status
) {
    imp_tcp_client_t *client = req->data;
    free(req);

    if (status != 0) {
        imp_net_status_t stat = {
            .type = FA_NET_E_TCP_CONNECT,
            .code = status
        };

        client->status_cb(client, &stat);

        return;
    };

    // Keep alive
    if (client->settings.keep_alive) {
        uv_tcp_keepalive(&client->tcp, 1, client->settings.keep_alive_secs);
    };

    client->connect_cb(client);
};

static void imp__tcp_client_connect (
    imp_tcp_client_t *client,
    struct sockaddr *addr
) {
    int r;
    r = uv_tcp_init(client->loop, &client->tcp);

    if (r != 0) {
        imp_net_status_t stat = {
            .type = FA_NET_E_TCP_INIT,
            .code = r
        };

        client->status_cb(client, &stat);

        return;
    };

    client->tcp.data = client;

    uv_connect_t *con_req = malloc(sizeof(uv_connect_t));

    con_req->data = client;

    r = uv_tcp_connect(con_req, &client->tcp, addr, *imp__tcp_client_connect_cb);

    if (r != 0) {
        imp_net_status_t stat = {
            .type = FA_NET_E_CONNECT_INIT,
            .code = r
        };

        client->status_cb(client, &stat);

        return;
    };
}

static void imp__tcp_client_getaddrinfo_cb (
    uv_getaddrinfo_t* req,
    int status,
    struct addrinfo* res
) {
    if (status < 0) {
        imp_tcp_client_t *client = req->data;

        imp_net_status_t stat = {
            .type = FA_NET_E_GETADDRINFO,
            .code = status
        };

        client->status_cb(client, &stat);

        goto cleanup;
    };

    imp__tcp_client_connect(req->data, res->ai_addr);
cleanup:
    uv_freeaddrinfo(res);
    free(req);
}

int imp_tcp_client_connect (imp_tcp_client_t *client, imp_tcp_client_status_cb status_cb, imp_tcp_client_connect_cb connect_cb) {
    client->status_cb = status_cb;
    client->connect_cb = connect_cb;

    client->tcp.data = client;

    if (unlikely(client->status_cb == NULL)) {
        return -1;
    };

    if (unlikely(client->url == NULL)) {
        return -2;
    };

    int numeric_host_v = imp_net_is_numeric_host_v(client->url->host);

    if (unlikely(numeric_host_v)) {
        int r;
        switch (numeric_host_v)
        {
        case 4:
            {
                struct sockaddr_in dst;
                r = uv_ip4_addr(client->url->host, atoi(client->url->port), &dst);
                imp__tcp_client_connect(client, (struct sockaddr *)&dst);
            }
            break;
        
        case 6:
            {
                struct sockaddr_in6 dst;
                r = uv_ip6_addr(client->url->host, atoi(client->url->port), &dst);
                imp__tcp_client_connect(client, (struct sockaddr *)&dst);
            }
            break;

        default:
            return -3;
        }

        if (unlikely(r != 0)) return -4;
    } else {
        uv_getaddrinfo_t *getaddrinfo_req = malloc(sizeof(uv_getaddrinfo_t));

        getaddrinfo_req->data = client;

        struct addrinfo hints;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        uv_getaddrinfo(
            client->loop,
            getaddrinfo_req,
            *imp__tcp_client_getaddrinfo_cb,
            client->url->host,
            client->url->port,
            &hints
        );
    }

    return 0;
}