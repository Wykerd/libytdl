#ifndef YTDL_DL_H
#define YTDL_DL_H

#include <uv.h>
#include <ytdl/net.h>
#include <ytdl/info.h>
#include <ytdl/sig.h>
#include <ytdl/buffer.h>
#include <ytdl/http/http.h>

#define YTDL_DL_IS_STARTED  (1)
#define YTDL_DL_IS_SHUTDOWN (1 << 1)

typedef struct ytdl_dl_ctx_s {
    char id[YTDL_ID_SIZE];
    ytdl_info_ctx_t info;
    ytdl_sig_actions_t sig_actions;

    ytdl_http_client_t http;
    ytdl_buf_t response;
    ytdl_buf_t player_js;

    int status;
} ytdl_dl_ctx_t;


int ytdl_dl_ctx_init (uv_loop_t *loop, ytdl_dl_ctx_t *ctx);
int ytdl_dl_ctx_run (ytdl_dl_ctx_t *ctx);
void ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx);
int ytdl_dl_set_video_url (ytdl_dl_ctx_t *ctx, const char *video_url);

#endif