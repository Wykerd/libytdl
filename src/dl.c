#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>

static int ytdl__watch_body_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    ytdl_buf_t *buf = &ctx->queue.videos[0]->response;
    
    if (buf->size < (buf->len + length))
        ytdl_buf_realloc(buf, buf->len + length);

    memcpy(buf->base + buf->len, at, length);

    buf->len += length;
    return 0;
}

static int yt__player_body_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    
    if (ctx->player_js.size < (ctx->player_js.len + length))
        ytdl_buf_realloc(&ctx->player_js, ctx->player_js.len + length);

    memcpy(ctx->player_js.base + ctx->player_js.len, at, length);

    ctx->player_js.len += length;
    return 0;
}

static void ytdl__write_cb (ytdl_http_client_t *client, ytdl_net_status_t *status)
{
    if (status->type != YTDL_NET_E_OK)
        fprintf(stderr, "Write error: %u %zd\n", status->type, status->code);
}

static void ytdl__status_cb (ytdl_http_client_t *client, ytdl_net_status_t *status)
{
    if (status->type != YTDL_NET_E_OK)
        fprintf(stderr, "HTTP error: %u %zd\n", status->type, status->code);
}

static void ytdl__close_cb (uv_handle_t* handle) 
{
    // noop
}

static int ytdl__watch_html_complete (llhttp_t* parser);
static int ytdl__player_js_complete (llhttp_t* parser);

static void ytdl__dl_wake(ytdl_dl_ctx_t *ctx)
{
    if (!(ctx->status & YTDL_DL_IS_IDLE))
        return;
    
    if (!ctx->queue.len)
        return;

    ctx->http.parser_settings.on_body = ytdl__watch_body_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__watch_html_complete;

    ytdl_buf_t buf;
    ytdl_net_request_watch_html(&buf, ctx->queue.videos[0]->id);
    ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
    ytdl_buf_free(&buf);
    ctx->status ^= YTDL_DL_IS_IDLE;
}

static int ytdl__dl_finalize (ytdl_dl_ctx_t *ctx, ytdl_dl_player_t *player)
{
    ytdl_dl_video_t *vid = ctx->queue.videos[0];
    ytdl_info_set_sig_actions(&vid->info, &player->sig_actions);

    vid->on_complete(ctx, vid);

    ytdl_info_ctx_free(&vid->info);
    ytdl_buf_free(&vid->response);
    free(vid);

    for (size_t i = 0; i < ctx->queue.len - 1; i++)
    {
        ctx->queue.videos[i] = ctx->queue.videos[i + 1];
    };

    ctx->status |= YTDL_DL_IS_IDLE;

    if (--ctx->queue.len > 0)
        ytdl__dl_wake(ctx);

    // ytdl_http_client_shutdown(&ctx->http, ytdl__close_cb);
}

static int ytdl__player_js_complete (llhttp_t* parser)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    ytdl_dl_video_t *vid = ctx->queue.videos[0];
    
    ytdl_dl_player_t *player = malloc(sizeof(ytdl_dl_player_t));
    if (!player)
    {
        // TODO: ERROR malloc
        return 0;
    }

    player->player_path = strdup(vid->info.player_url);

    if (ytdl_sig_actions_extract(&player->sig_actions, (uint8_t *)ctx->player_js.base, 
                                 ctx->player_js.len))
    {
        // FAIL
        perror("Sig extract error");
        return 0;
    }

    if (hashmap_put(&ctx->player_map, player->player_path, player) < 0)
    {
        // TODO: error
        perror("map error");
        return 0;
    }

    ctx->player_js.len = 0;
    ytdl__dl_finalize(ctx, player);

    return 0;
}

static int ytdl__watch_html_complete (llhttp_t* parser)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    ytdl_dl_video_t *vid = ctx->queue.videos[0];

    int ret = ytdl_info_extract_watch_html(&vid->info, (uint8_t *)vid->response.base, 
                                           vid->response.len);
    if (ret)
    {
        // FAIL
        fprintf(stderr, "Watch extract error: %d\n", ret);
        return 0;
    }

    if (ytdl_info_get_playability_status(&vid->info))
    {
        fputs(ytdl_info_get_playability_status_message(&vid->info), stderr);
        return 0;
    }

    ytdl_dl_player_t *player = hashmap_get(&ctx->player_map, vid->info.player_url);

    if (player)
        ytdl__dl_finalize(ctx, player);
    else    
    {
        ytdl_buf_t buf;
        ctx->http.parser_settings.on_body = yt__player_body_cb;
        ctx->http.parser_settings.on_message_complete = ytdl__player_js_complete;
        ytdl_net_request_player_js(&buf, vid->info.player_url);
        ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
        ytdl_buf_free(&buf);
    }

    return 0;
}

int ytdl_dl_ctx_init (uv_loop_t *loop, ytdl_dl_ctx_t *ctx) 
{
    if (!ytdl_buf_alloc(&ctx->player_js, 1))
        return 1;
    if (!ytdl_http_client_init(loop, &ctx->http))
    {
        ytdl_buf_free(&ctx->player_js);
        return 1;
    }

    ctx->queue.videos = malloc(sizeof(ytdl_dl_video_t *));
    if (!ctx->queue.videos)
    {
        ytdl_buf_free(&ctx->player_js);
        return 1;
    }

    ctx->queue.size = 1;
    ctx->queue.len = 0;

    hashmap_init(&ctx->player_map, hashmap_hash_string, strcmp);

    ctx->http.data = ctx;
    ctx->http.settings.keep_alive = 1;
    ctx->http.parser_settings.on_body = ytdl__watch_body_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__watch_html_complete;
    ctx->status = 0;

    return 0;
}

int ytdl_dl_get_info (ytdl_dl_ctx_t *ctx, const char id[YTDL_ID_SIZE],
                      ytdl_dl_complete_cb on_complete) 
{
    if (ctx->queue.size == ctx->queue.len)
    {
        ctx->queue.size *= 1.5;
        ctx->queue.videos = realloc(ctx->queue.videos, sizeof(ytdl_dl_video_t *) * ctx->queue.size);
        if (!ctx->queue.videos)
            return -1;
    }

    ctx->queue.videos[ctx->queue.len++] = malloc(sizeof(ytdl_dl_video_t));
    memcpy(ctx->queue.videos[ctx->queue.len - 1]->id, id, YTDL_ID_SIZE);
    if (!ytdl_buf_alloc(&ctx->queue.videos[ctx->queue.len - 1]->response, 1))
        return -1;
    ytdl_info_ctx_init(&ctx->queue.videos[ctx->queue.len - 1]->info);

    ctx->queue.videos[ctx->queue.len - 1]->on_complete = on_complete;

    ytdl__dl_wake(ctx);

    return 0;
}

static void ytdl__connected_cb (ytdl_http_client_t *client)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)client->data;
    ctx->status |= YTDL_DL_IS_CONNECTED | YTDL_DL_IS_IDLE;
    
    ytdl__dl_wake(ctx);
}

int ytdl_dl_ctx_connect (ytdl_dl_ctx_t *ctx) 
{
    ytdl_http_client_connect(&ctx->http, 1, "www.youtube.com", "443", 
                             ytdl__status_cb, ytdl__connected_cb);

    return 0;
}

static void ytdl__dl_shutdown (uv_idle_t* handle) 
{
    ytdl_dl_ctx_t *ctx = handle->data;

    if (ctx->status & YTDL_DL_IS_CONNECTED)
        ytdl_http_client_shutdown(&ctx->http, ytdl__close_cb);

    ytdl_buf_free(&ctx->player_js);

    ytdl_dl_player_t *player;
    hashmap_foreach_data(player, &ctx->player_map) {
        free(player->player_path);
        free(player);
    }

    hashmap_cleanup(&ctx->player_map);

    for (size_t i = 0; i < ctx->queue.len; i++) {
        ytdl_buf_free(&ctx->queue.videos[i]->response);
        ytdl_info_ctx_free(&ctx->queue.videos[i]->info);
        free(ctx->queue.videos[i]);
    }

    free(ctx->queue.videos);

    uv_idle_stop(handle);
    free(handle);
}

void ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx)
{
    // finish the current event before killing everything
    uv_idle_t *idle = malloc(sizeof(uv_async_t));
    idle->data = ctx;
    uv_idle_init(ctx->http.loop, idle);
    uv_idle_start(idle, ytdl__dl_shutdown);
}


