#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>

static int ytdl__watch_body_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    
    if (ctx->response.size < (ctx->response.len + length))
        ytdl_buf_realloc(&ctx->response, ctx->response.len + length);

    memcpy(ctx->response.base + ctx->response.len, at, length);

    ctx->response.len += length;
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

static int ytdl__player_js_complete (llhttp_t* parser)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    
    if (ytdl_sig_actions_extract(&ctx->sig_actions, (uint8_t *)ctx->player_js.base, 
                                 ctx->player_js.len))
    {
        // FAIL
        perror("Sig extract error");
        return 0;
    }

    ctx->response.len = 0;
    ytdl_info_set_sig_actions(&ctx->info, &ctx->sig_actions);
    ytdl_info_extract_formats(&ctx->info);

    ytdl_buf_free(&ctx->player_js);

    printf("Best Audio: %s\n\nBest Video: %s\n\n", 
        ytdl_info_get_format_url(&ctx->info, ytdl_info_get_best_audio_format(&ctx->info)),
        ytdl_info_get_format_url(&ctx->info, ytdl_info_get_best_video_format(&ctx->info))
    );

    ytdl_http_client_shutdown(&ctx->http, ytdl__close_cb);
    ctx->status |= YTDL_DL_IS_SHUTDOWN;
    return 0;
}

static int ytdl__watch_html_complete (llhttp_t* parser)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    
    // printf("%.*s\n", ctx->response.len, (uint8_t *)ctx->response.base);

    int ret = ytdl_info_extract_watch_html(&ctx->info, (uint8_t *)ctx->response.base, 
                                           ctx->response.len);
    if (ret)
    {
        // FAIL
        fprintf(stderr, "Watch extract error: %d\n", ret);
        return 0;
    }

    if (ytdl_info_get_playability_status(&ctx->info))
    {
        fputs(ytdl_info_get_playability_status_message(&ctx->info), stderr);
        return 0;
    }

    ctx->response.len = 0;
    ctx->http.parser_settings.on_body = yt__player_body_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__player_js_complete;

    ytdl_buf_t buf;
    ytdl_net_request_player_js(&buf, ctx->info.player_url);
    ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
    ytdl_buf_free(&buf);

    return 0;
}

int ytdl_dl_ctx_init (uv_loop_t *loop, ytdl_dl_ctx_t *ctx) 
{
    if (!ytdl_http_client_init(loop, &ctx->http))
        return 1;
    if (!ytdl_buf_alloc(&ctx->response, 1))
        return 1;
    if (!ytdl_buf_alloc(&ctx->player_js, 1))
        return 1;
    ytdl_info_ctx_init(&ctx->info);
    ctx->http.data = ctx;
    ctx->http.settings.keep_alive = 1;
    ctx->http.parser_settings.on_body = ytdl__watch_body_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__watch_html_complete;
    ctx->status = 0;
    return 0;
}

int ytdl_dl_set_video_url (ytdl_dl_ctx_t *ctx, const char *video_url) 
{
    return ytdl_net_get_id_from_url(video_url, ctx->id);
}

static void ytdl__connected_cb (ytdl_http_client_t *client)
{
    ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)client->data;
    ytdl_buf_t buf;
    ytdl_net_request_watch_html(&buf, ctx->id);
    ytdl_http_client_write(client, &buf, ytdl__write_cb);
    ytdl_buf_free(&buf);
}

int ytdl_dl_ctx_run (ytdl_dl_ctx_t *ctx) 
{
    if (!ctx->id[0])
        return 1;

    ytdl_http_client_connect(&ctx->http, 1, "www.youtube.com", "443", 
                             ytdl__status_cb, ytdl__connected_cb);

    ctx->status |= YTDL_DL_IS_STARTED;

    return 0;
}

int ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx)
{
    if (ctx->status  == YTDL_DL_IS_STARTED)
        ytdl_http_client_shutdown(&ctx->http, ytdl__close_cb);

    ytdl_buf_free(&ctx->player_js);
    ytdl_buf_free(&ctx->response);
}