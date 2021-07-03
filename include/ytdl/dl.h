#ifndef YTDL_DL_H
#define YTDL_DL_H

#include <uv.h>
#include <ytdl/net.h>
#include <ytdl/info.h>
#include <ytdl/sig.h>
#include <ytdl/buffer.h>
#include <ytdl/http/http.h>
#include <ytdl/hashmap.h>

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
};

int ytdl_dl_ctx_init (uv_loop_t *loop, ytdl_dl_ctx_t *ctx);
int ytdl_dl_ctx_connect (ytdl_dl_ctx_t *ctx);
int ytdl_dl_get_info (ytdl_dl_ctx_t *ctx, const char id[YTDL_ID_SIZE],
                      ytdl_dl_complete_cb on_complete);
void ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx);

void ytdl_dl_player_cache_save_file(ytdl_dl_ctx_t *ctx, FILE *fd);
int ytdl_dl_player_cache_load_file(ytdl_dl_ctx_t *ctx, FILE *fd);

#endif
