#include <ytdl/net.h>
#include <ytdl/url_parser.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

const char *yt_hosts[6] = {
    "youtube.com",
    "www.youtube.com",
    "youtu.be",
    "m.youtube.com",
    "music.youtube.com",
    "gaming.youtube.com"
};

const char yt_valid_id_map[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
    1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void ytdl_net_request_player_js (ytdl_buf_t *buf, const char* player_path) 
{
    buf->len = 0;
    if (!ytdl_buf_alloc(buf, YTDL_DL_PLAYER_JS_REQUEST_SIZE))
        return;

    snprintf(
        buf->base, 
        YTDL_DL_PLAYER_JS_REQUEST_SIZE,
        "GET %s HTTP/1.1\r\n"
        "Host: www.youtube.com\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: " YTDL_DL_USER_AGENT "\r\n"
        "Accept: */*\r\n\r\n",
        player_path
    );

    buf->len = strlen(buf->base);
}

void ytdl_net_request_watch_html (ytdl_buf_t *buf, const char id[YTDL_ID_SIZE]) 
{
    buf->len = 0;
    if (!ytdl_buf_alloc(buf, YTDL_DL_WATCH_HTML_REQUEST_SIZE))
        return;

    snprintf(
        buf->base, 
        YTDL_DL_WATCH_HTML_REQUEST_SIZE,
        "GET /watch?v=%s HTTP/1.1\r\n"
        "Host: www.youtube.com\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: " YTDL_DL_USER_AGENT "\r\n"
        "Accept: */*\r\n\r\n",
        id
    );

    buf->len = strlen(buf->base);
}

void ytdl_net_request_media_chunk (ytdl_buf_t *buf, const char* path_buf, size_t path_len, 
                                   const char* query_buf, size_t query_len, 
                                   const char* host_buf, size_t host_len, 
                                   size_t start_idx, size_t end_idx)
{
    size_t buf_size = 200 + sizeof(YTDL_DL_USER_AGENT) + path_len + host_len + query_len; // overallocate just in case
    buf->len = 0;
    if (!ytdl_buf_alloc(buf, buf_size))
        return;

    if (end_idx)
        snprintf(
            buf->base, 
            buf_size,
            "GET %.*s?%.*s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: keep-alive\r\n"
            "User-Agent: " YTDL_DL_USER_AGENT "\r\n"
            "Range: bytes=%zu-%zu\r\n"
            "Accept: */*\r\n\r\n",
            path_len, path_buf,
            query_len, query_buf,
            host_len, host_buf,
            start_idx, end_idx
        );
    else
        snprintf(
            buf->base, 
            buf_size,
            "GET %.*s?%.*s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: keep-alive\r\n"
            "User-Agent: " YTDL_DL_USER_AGENT "\r\n"
            "Range: bytes=%zu-\r\n"
            "Accept: */*\r\n\r\n",
            path_len, path_buf,
            query_len, query_buf,
            host_len, host_buf,
            start_idx
        );

    buf->len = strlen(buf->base);
}

void ytdl_net_request_media (ytdl_buf_t *buf, const char* path_buf, size_t path_len, 
                             const char* query_buf, size_t query_len, 
                             const char* host_buf, size_t host_len)
{
    size_t buf_size = 90 + sizeof(YTDL_DL_USER_AGENT) + path_len + host_len + query_len; 

    buf->len = 0;
    if (!ytdl_buf_alloc(buf, buf_size))
        return;

    snprintf(
        buf->base, 
        buf_size,
        "GET %.*s?%.*s HTTP/1.1\r\n"
        "Host: %.*s\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: " YTDL_DL_USER_AGENT "\r\n"
        "Accept: */*\r\n\r\n",
        path_len, path_buf,
        query_len, query_buf,
        host_len, host_buf
    );

    buf->len = strlen(buf->base);
}

void ytdl_net_get_watch_url (char url[YTDL_WATCH_URL_SIZE], char id[YTDL_ID_SIZE])
{
    sprintf(url, "%s%s", "https://www.youtube.com/watch?v=", id);
}

int ytdl_net_get_id_from_url (const char *url_buf, size_t url_len, char id[YTDL_ID_SIZE]) 
{
    struct http_parser_url u;

    http_parser_url_init(&u);
    if (http_parser_parse_url(url_buf, url_len, 0, &u))
        return -1;

    for (int i = 0; i < 6; i++)
        if (!strncmp(url_buf + u.field_data[UF_HOST].off, yt_hosts[i],
                     u.field_data[UF_HOST].len)) 
        {
            goto valid_host;
        }

fail:
    return 1;
valid_host:
    if (u.field_data[UF_PATH].len >= YTDL_ID_LEN + 1)
        if ((url_buf + u.field_data[UF_PATH].off + u.field_data[UF_PATH].len - YTDL_ID_LEN - 1)[0] == '/')
        {
            for (int i = 0; i < YTDL_ID_LEN; i++) 
                if (!yt_valid_id_map[(url_buf + u.field_data[UF_PATH].off + 1)[i]])
                    goto invalid_path_tail;

            memcpy(id, url_buf + u.field_data[UF_PATH].off + u.field_data[UF_PATH].len - YTDL_ID_LEN, YTDL_ID_LEN);
            id[YTDL_ID_LEN] = 0;
            goto match_found;
        }

invalid_path_tail:
    if (u.field_data[UF_QUERY].len >= YTDL_ID_LEN + 2) 
    {
        if (((url_buf + u.field_data[UF_QUERY].off)[0] == 'v') && ((url_buf + u.field_data[UF_QUERY].off)[1] == '='))
        {
            for (int i = 0; i < YTDL_ID_LEN; i++) 
                if (!yt_valid_id_map[(url_buf + u.field_data[UF_QUERY].off + 2)[i]])
                    goto fail;

            memcpy(id, url_buf + u.field_data[UF_QUERY].off + 2, YTDL_ID_LEN);
            id[YTDL_ID_LEN] = 0;
            goto match_found;
        }

        for (size_t x = 0; x < u.field_data[UF_QUERY].len - YTDL_ID_LEN; x++)
        {
            if (((url_buf + u.field_data[UF_QUERY].off)[x] == 'v') && ((url_buf + u.field_data[UF_QUERY].off)[x + 1] == '='))
            {
                for (int i = 0; i < YTDL_ID_LEN; i++) 
                    if (!yt_valid_id_map[(url_buf + u.field_data[UF_QUERY].off + x + 2)[i]])
                        goto fail;

                memcpy(id, url_buf + u.field_data[UF_QUERY].off + x + 2, YTDL_ID_LEN);
                id[YTDL_ID_LEN] = 0;
                goto match_found;
            }
        }
    }
    goto fail;
match_found:
    return 0;
}
