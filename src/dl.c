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

    ytdl_sig_actions_init(&player->sig_actions);

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
    return ytdl_http_client_connect(&ctx->http, 1, "www.youtube.com", "443", 
                                    ytdl__status_cb, ytdl__connected_cb);
}

static 
void close_free_cb (uv_handle_t* handle)
{
    free(handle);
};

static void ytdl__dl_shutdown (uv_idle_t* handle) 
{
    ytdl_dl_ctx_t *ctx = handle->data;

    if (ctx->status & YTDL_DL_IS_CONNECTED)
        ytdl_http_client_shutdown(&ctx->http, ytdl__close_cb);

    ytdl_buf_free(&ctx->player_js);

    ytdl_dl_player_t *player;
    hashmap_foreach_data(player, &ctx->player_map) {
        ytdl_sig_actions_free(&player->sig_actions);
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
    uv_close((uv_handle_t*)handle, close_free_cb);
}

void ytdl_dl_shutdown (ytdl_dl_ctx_t *ctx)
{
    // finish the current event before killing everything
    uv_idle_t *idle = malloc(sizeof(uv_async_t));
    idle->data = ctx;
    uv_idle_init(ctx->http.loop, idle);
    uv_idle_start(idle, ytdl__dl_shutdown);
}

void ytdl_dl_player_cache_save_file(ytdl_dl_ctx_t *ctx, FILE *fd)
{   /*
    ytdl_dl_player_t *player;
    hashmap_foreach_data(player, &ctx->player_map) {
        fputs(player->player_path, fd);
        fputc(0, fd);
        fwrite(&player->sig_actions, sizeof(ytdl_sig_actions_t), 1, fd);
    }*/
}

int ytdl_dl_player_cache_load_file(ytdl_dl_ctx_t *ctx, FILE *fd)
{   /*
    char player_path[100] = {0};
    int pos = 0,
        off = 0;
    for (;;)
    {
        do {
            if (pos > 99)
                return -1;
        
            player_path[off + pos++] = fgetc(fd);
        } while (player_path[off + pos - 1] != 0);

        off += pos + sizeof(ytdl_dl_player_t);

        ytdl_dl_player_t *player = malloc(sizeof(ytdl_dl_player_t));

        player->player_path = strdup(player_path);
        fread(&player->sig_actions, sizeof(ytdl_sig_actions_t), 1, fd);

        hashmap_put(&ctx->player_map, player->player_path, player);

        int c = fgetc(fd);
        if (c == EOF)
            return 0;
        player_path[0] = c;
        pos = 1;
    }*/
}

static int ytdl__media_echo_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->on_data)
        ctx->on_data(ctx, at, length);

    return 0;
}

static int ytdl__media_chunk_final_complete_cb (llhttp_t* parser)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->on_complete)
        ctx->on_complete(ctx);

    return 0;
}

static int ytdl__media_chunk_complete_cb (llhttp_t* parser)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->is_chunked)
    {
        ytdl_buf_t buf;
        if (ctx->last_chunk_end + YTDL_DL_CHUNK_SIZE >= ctx->format_content_length)
        {
            ctx->http.parser_settings.on_message_complete = ytdl__media_chunk_final_complete_cb;
            ytdl_net_request_media_chunk(&buf, 
                ctx->format_url + ctx->url.field_data[UF_PATH].off, ctx->url.field_data[UF_PATH].len,
                ctx->format_url + ctx->url.field_data[UF_QUERY].off, ctx->url.field_data[UF_QUERY].len,
                ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len,
                ctx->last_chunk_end + 1, 0
            );
        }
        else 
        {
            ytdl_net_request_media_chunk(&buf, 
                ctx->format_url + ctx->url.field_data[UF_PATH].off, ctx->url.field_data[UF_PATH].len,
                ctx->format_url + ctx->url.field_data[UF_QUERY].off, ctx->url.field_data[UF_QUERY].len,
                ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len,
                ctx->last_chunk_end + 1, ctx->last_chunk_end + YTDL_DL_CHUNK_SIZE
            );
            ctx->last_chunk_end += YTDL_DL_CHUNK_SIZE;
        };
        ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb); //TODO: error handle
        ytdl_buf_free(&buf);
    }
    else if (ctx->on_complete)
        ctx->on_complete(ctx);
    
    return 0;
}

int ytdl_dl_media_ctx_init (uv_loop_t *loop, ytdl_dl_media_ctx_t *ctx, 
                            ytdl_info_format_t *format, ytdl_info_ctx_t *info)
{
    if (!ytdl_http_client_init(loop, &ctx->http))
    {
        return 1;
    }

    ctx->http.data = ctx;
    ctx->http.settings.keep_alive = 1;
    ctx->http.parser_settings.on_body = ytdl__media_echo_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__media_chunk_complete_cb;

    ctx->is_chunked = !(format->flags & YTDL_INFO_FORMAT_HAS_AUD) || !(format->flags & YTDL_INFO_FORMAT_HAS_VID);

    http_parser_url_init(&ctx->url);
    ctx->format_url = strdup(ytdl_info_get_format_url2(info, format));
    ctx->format_content_length = format->content_length;

    if (http_parser_parse_url(ctx->format_url, strlen(ctx->format_url), 0, &ctx->url))
        return -1;

    return 0;
}

static void ytdl__media_connected_cb (ytdl_http_client_t *client)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)client->data;
    
    ytdl_buf_t buf;
    if (ctx->is_chunked)
    {
        if (YTDL_DL_CHUNK_SIZE >= ctx->format_content_length)
        {
            ctx->http.parser_settings.on_message_complete = ytdl__media_chunk_final_complete_cb;
            ytdl_net_request_media_chunk(&buf, 
                ctx->format_url + ctx->url.field_data[UF_PATH].off, ctx->url.field_data[UF_PATH].len,
                ctx->format_url + ctx->url.field_data[UF_QUERY].off, ctx->url.field_data[UF_QUERY].len,
                ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len,
                0, 0
            );
        }
        else 
        {
            ytdl_net_request_media_chunk(&buf, 
                ctx->format_url + ctx->url.field_data[UF_PATH].off, ctx->url.field_data[UF_PATH].len,
                ctx->format_url + ctx->url.field_data[UF_QUERY].off, ctx->url.field_data[UF_QUERY].len,
                ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len,
                0, YTDL_DL_CHUNK_SIZE
            );
            ctx->last_chunk_end = YTDL_DL_CHUNK_SIZE;
        };
    }
    else 
    {
        ytdl_net_request_media(&buf, 
                ctx->format_url + ctx->url.field_data[UF_PATH].off, ctx->url.field_data[UF_PATH].len,
                ctx->format_url + ctx->url.field_data[UF_QUERY].off, ctx->url.field_data[UF_QUERY].len,
                ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len
        );
    }
    ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb); //TODO: error handle
    ytdl_buf_free(&buf);
}

int ytdl_dl_media_ctx_connect (ytdl_dl_media_ctx_t *ctx)
{
    int ret;
    char *host = malloc(ctx->url.field_data[UF_HOST].len + 1);
    memcpy(host, ctx->format_url + ctx->url.field_data[UF_HOST].off, ctx->url.field_data[UF_HOST].len);
    host[ctx->url.field_data[UF_HOST].len] = 0;
    ret =  ytdl_http_client_connect(&ctx->http, 1, host, "443", 
                                    ytdl__status_cb, ytdl__media_connected_cb); //TODO: error handle
    free(host);
    return ret;
}

static void ytdl__media_close_cb (uv_handle_t* handle) 
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)handle->data)->data;

    ctx->on_close(ctx);
}

static void ytdl__dl_media_shutdown (uv_idle_t* handle) 
{
    ytdl_dl_media_ctx_t *ctx = handle->data;

    uv_idle_stop(handle);
    uv_close((uv_handle_t*)handle, close_free_cb);

    ytdl_http_client_shutdown(&ctx->http, ytdl__media_close_cb);

    free(ctx->format_url);
}

void ytdl_dl_media_shutdown (ytdl_dl_media_ctx_t *ctx, ytdl_dl_media_cb on_close)
{
    // finish the current event before killing everything
    ctx->on_close = on_close;
    uv_idle_t *idle = malloc(sizeof(uv_async_t));
    idle->data = ctx;
    uv_idle_init(ctx->http.loop, idle);
    uv_idle_start(idle, ytdl__dl_media_shutdown);
}
