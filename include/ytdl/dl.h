#ifndef YTDL_DL_H
#define YTDL_DL_H

#include <uv.h>
#include <ytdl/net.h>
#include <ytdl/info.h>
#include <ytdl/sig.h>
#include <ytdl/buffer.h>
#include <ytdl/http/http.h>
#include <ytdl/hashmap.h>
#include <ytdl/url_parser.h>
#include <ytdl/dash.h>

#ifdef __cplusplus
extern "C" {
#endif
#define YTDL_DL_IS_CONNECTED    (1)
#define YTDL_DL_IS_SHUTDOWN     (1 << 1)
#define YTDL_DL_IS_IDLE         (1 << 2)

typedef struct ytdl_dl_ctx_s ytdl_dl_ctx_t;
typedef struct ytdl_dl_video_s ytdl_dl_video_t;

typedef void (*ytdl_dl_complete_cb)(ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* video);

struct ytdl_dl_video_s {
    char id[YTDL_ID_SIZE];
    ytdl_info_ctx_t info;
    ytdl_buf_t response;
    ytdl_dl_complete_cb on_complete;
};

typedef struct ytdl_dl_player_s {
    char *player_path;
    ytdl_sig_actions_t sig_actions;
} ytdl_dl_player_t;

typedef HASHMAP(char, ytdl_dl_player_t) ytdl_dl_player_map_t;

typedef struct ytdl_dl_queue_s {
    ytdl_dl_video_t **videos;
    size_t len;
    size_t size;
} ytdl_dl_queue_t;

struct ytdl_dl_ctx_s {
    ytdl_dl_queue_t queue;

    ytdl_http_client_t http;
    ytdl_buf_t player_js;

    ytdl_dl_player_map_t player_map;

    int status;
    void *data;
};

int ytdl_dl_ctx_init (uv_loop_t *loop, ytdl_dl_ctx_t *ctx);
int ytdl_dl_ctx_connect (ytdl_dl_ctx_t *ctx);
int ytdl_dl_get_info (ytdl_dl_ctx_t *ctx, const char id[YTDL_ID_SIZE],
                      ytdl_dl_complete_cb on_complete);
void ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx);

void ytdl_dl_player_cache_save_file(ytdl_dl_ctx_t *ctx, FILE *fd);
int ytdl_dl_player_cache_load_file(ytdl_dl_ctx_t *ctx, FILE *fd);

typedef struct ytdl_dl_media_ctx_s ytdl_dl_media_ctx_t;

typedef void (*ytdl_dl_media_data_cb)(ytdl_dl_media_ctx_t *ctx, const char *buf, size_t len);
typedef void (*ytdl_dl_media_cb)(ytdl_dl_media_ctx_t *ctx);
typedef void (*ytdl_dl_media_status_cb)(ytdl_dl_media_ctx_t *ctx, ytdl_net_status_t *status);

struct ytdl_dl_media_ctx_s {
    ytdl_http_client_t http;
    struct http_parser_url url;
    char *format_url;
    long long format_content_length;

    ytdl_dl_media_data_cb on_data;
    ytdl_dl_media_cb on_complete;
    // Relays http status
    ytdl_dl_media_status_cb on_status;
    ytdl_dl_media_cb on_close;

    int is_chunked;
    int want_redirect;
    int redirect_was_location;
    size_t last_chunk_end;

    // opaque data
    void *data;
};

int ytdl_dl_media_ctx_init (uv_loop_t *loop, ytdl_dl_media_ctx_t *ctx, 
                            ytdl_info_format_t *format, ytdl_info_ctx_t *info);
int ytdl_dl_media_ctx_connect (ytdl_dl_media_ctx_t *ctx);
void ytdl_dl_media_shutdown (ytdl_dl_media_ctx_t *ctx, ytdl_dl_media_cb on_close);

typedef struct ytdl_dl_dash_ctx_s ytdl_dl_dash_ctx_t;

typedef void (*ytdl_dl_dash_data_cb)(ytdl_dl_dash_ctx_t *ctx, const char *buf, size_t len);
typedef void (*ytdl_dl_dash_cb)(ytdl_dl_dash_ctx_t *ctx);
typedef void (*ytdl_dl_dash_status_cb)(ytdl_dl_dash_ctx_t *ctx, ytdl_net_status_t *status);

struct ytdl_dl_dash_ctx_s {
    ytdl_http_client_t http;
    ytdl_buf_t manifest;

    ytdl_dl_dash_data_cb on_data;
    ytdl_dl_dash_cb on_segment_complete;
    ytdl_dl_dash_cb on_complete;
    // Relays http status
    ytdl_dl_dash_status_cb on_status;
    ytdl_dl_dash_cb on_close;
    ytdl_dl_dash_cb on_manifest;
    ytdl_dash_representation_select_cb on_pick_filter;

    ytdl_dash_ctx_t dash;

    char *manifest_path;
    char *manifest_host;
    size_t manifest_path_len;
    size_t manifest_host_len;

    char *host;
    size_t host_len;
    
    char *path;
    size_t path_len;

    int is_shutdown;
    int is_dash_init;
    int is_video;
    // opaque data
    void *data;
};

int ytdl_dl_dash_ctx_init (uv_loop_t *loop, ytdl_dl_dash_ctx_t *ctx);
int ytdl_dl_dash_ctx_connect (ytdl_dl_dash_ctx_t *ctx, const char *manifest_url);
void ytdl_dl_dash_shutdown (ytdl_dl_dash_ctx_t *ctx, ytdl_dl_dash_cb on_close);
void ytdl_dl_dash_load_fork (ytdl_dl_dash_ctx_t *ctx);
/**
 * To be called only within on_manifest
 * 
 * Used to create a clone of the manifest so that you can download another format
 */
int ytdl_dl_dash_ctx_fork (ytdl_dl_dash_ctx_t *ctx, ytdl_dl_dash_ctx_t *fork);

#ifdef __cplusplus
}
#endif
#endif
