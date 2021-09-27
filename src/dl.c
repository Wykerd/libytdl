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
    ytdl_http_client_t *http = handle->data;
    ytdl_dl_ctx_t *ctx = http->data;
    if (ctx->on_close != NULL)
        ctx->on_close(ctx);
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

    ctx->queue.len--;

    if (ctx->queue.len > 0)
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
    {
        ytdl__dl_finalize(ctx, player);
    }    
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

    ctx->on_close = NULL;

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
    memcpy(&ctx->queue.videos[ctx->queue.len - 1]->id[0], &id[0], YTDL_ID_SIZE);
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
{   
    ytdl_dl_player_t *player;
    hashmap_foreach_data(player, &ctx->player_map) {
        fputs(player->player_path, fd);
        fputc(0, fd);
        ytdl_sig_actions_save_file(&player->sig_actions, fd);
    }
}

int ytdl_dl_player_cache_load_file(ytdl_dl_ctx_t *ctx, FILE *fd)
{   
    char player_path[100] = {0};
    int pos = 0;
    for (;;)
    {
        do {
            if (pos > 99)
                return -1;
        
            player_path[pos++] = fgetc(fd);
        } while (player_path[pos - 1] != 0);

        ytdl_dl_player_t *player = malloc(sizeof(ytdl_dl_player_t));

        player->player_path = strdup(player_path);
        ytdl_sig_actions_init(&player->sig_actions);
        ytdl_sig_actions_load_file(&player->sig_actions, fd);

        hashmap_put(&ctx->player_map, player->player_path, player);

        int c = fgetc(fd);
        if (c == EOF)
            return 0;
        player_path[0] = c;
        pos = 1;
    }
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

static int ytdl__media_chunk_complete_cb (llhttp_t* parser);
static int ytdl__media_status_cb (llhttp_t* parser);

static void ytdl__media_close_redirect_cb (uv_handle_t* handle) 
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)handle->data)->data;

    http_parser_url_init(&ctx->url);

    if (!ytdl_http_client_init(ctx->http.loop, &ctx->http))
    {
        return; //TODO:
    }

    ctx->http.data = ctx;
    ctx->http.settings.keep_alive = 1;
    ctx->http.parser_settings.on_status_complete = ytdl__media_status_cb;
    ctx->http.parser_settings.on_body = ytdl__media_echo_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__media_chunk_complete_cb;

    if (http_parser_parse_url(ctx->format_url, strlen(ctx->format_url), 0, &ctx->url))
        return; //TODO : BETTER

    ytdl_dl_media_ctx_connect(ctx);
}

static int ytdl__media_header_value_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->redirect_was_location)
    {
        free(ctx->format_url);
        ctx->format_url = malloc(length + 1);
        memcpy(ctx->format_url, at, length);
        ctx->format_url[length] = 0;

        ctx->http.parser_settings.on_header_field = NULL;
        ctx->http.parser_settings.on_header_value = NULL;

        ctx->want_redirect = 0;
        ctx->redirect_was_location = 0;

        ytdl_http_client_shutdown(&ctx->http, ytdl__media_close_redirect_cb);
    };

    return 0;
}

static int ytdl__media_header_field_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;
    if (ctx->want_redirect)
    {
        if (!strncasecmp(at, "location", length))
        {
            ctx->redirect_was_location = 1;
            ctx->http.parser_settings.on_header_value = ytdl__media_header_value_cb;
        }
    };

    return 0;
}

static int ytdl__media_status_cb (llhttp_t* parser)
{
    ytdl_dl_media_ctx_t *ctx = (ytdl_dl_media_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if ((parser->status_code == 301) || (parser->status_code == 302))
    {
        ctx->want_redirect = 1;
        ctx->http.parser_settings.on_header_field = ytdl__media_header_field_cb;
        return 0;
    }

    if (parser->status_code != 200)
    {
        // TODO: status
        return 1;
    }

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
    ctx->http.parser_settings.on_status_complete = ytdl__media_status_cb;
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

static int ytdl__dash_manifest_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->manifest.len + length > ctx->manifest.size)
    {
        // TODO: memory check
        ytdl_buf_realloc(&ctx->manifest, ctx->manifest.len + length);
    }

    memcpy(&ctx->manifest.base[ctx->manifest.len], at, length);
    ctx->manifest.len += length;

    return 0;
}

static int ytdl__dash_echo_cb (llhttp_t* parser, const char *at, size_t length)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->on_data)
        ctx->on_data(ctx, at, length);

    return 0;
}

static int ytdl__dash_segment_complete_cb (llhttp_t* parser)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    if (ctx->on_segment_complete)
        ctx->on_segment_complete(ctx);

    xmlChar *segment = ctx->is_video ?
        ytdl_dash_next_video_segment(&ctx->dash) :
        ytdl_dash_next_audio_segment(&ctx->dash);

    if (segment)
    {
        ytdl_buf_t buf;
        ytdl_net_request_segment(&buf, 
            ctx->path, ctx->path_len,
            segment, strlen(segment), 
            ctx->host, ctx->host_len);
        ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
        ytdl_buf_free(&buf);
    }
    else
        ctx->on_complete(ctx);
    
    return 0;
}

static void ytdl__dash_segment_connected_cb (ytdl_http_client_t *client)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)client->data;

    ctx->http.parser_settings.on_body = ytdl__dash_echo_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__dash_segment_complete_cb;

    xmlChar *segment = ctx->is_video ?
        ytdl_dash_next_video_segment(&ctx->dash) :
        ytdl_dash_next_audio_segment(&ctx->dash);

    if (segment)
    {
        ytdl_buf_t buf;
        ytdl_net_request_segment(&buf, 
            ctx->path, ctx->path_len,
            segment, strlen(segment), 
            ctx->host, ctx->host_len);
        ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
        ytdl_buf_free(&buf);
    }
    else
        ctx->on_complete(ctx);
}

void ytdl_dl_dash_load_fork (ytdl_dl_dash_ctx_t *ctx)
{
    struct http_parser_url url;
    http_parser_url_init(&url);

    ytdl_dash_ctx_init(&ctx->dash, ctx->manifest.base, ctx->manifest.len);
    ctx->is_dash_init = 1;

    ytdl_dash_get_format(&ctx->dash, ctx->on_pick_filter);

    xmlChar *u = ctx->is_video ? ctx->dash.v_base_url : ctx->dash.a_base_url;

    if (http_parser_parse_url(u, strlen(u), 0, &url))
    {
        // TODO: ctx->on_status 
        return;
    }

    ctx->host = malloc(url.field_data[UF_HOST].len + 1);
    ctx->host_len = url.field_data[UF_HOST].len;
    memcpy(ctx->host, u + url.field_data[UF_HOST].off, url.field_data[UF_HOST].len);
    ctx->host[url.field_data[UF_HOST].len] = 0;

    ctx->path = malloc(url.field_data[UF_PATH].len + 1);
    ctx->path_len = url.field_data[UF_PATH].len;
    memcpy(ctx->path, u + url.field_data[UF_PATH].off, url.field_data[UF_PATH].len);
    ctx->path[url.field_data[UF_PATH].len] = 0;

    ytdl_http_client_connect(&ctx->http, 1, ctx->host, "443", ytdl__status_cb, ytdl__dash_segment_connected_cb);
}

static void ytdl__dash_close_cb (uv_handle_t* handle) 
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)handle->data)->data;

    ctx->is_shutdown = 0;
    ytdl_http_client_init(ctx->http.loop, &ctx->http);

    if (ctx->on_manifest)
        ctx->on_manifest(ctx);

    ytdl_dl_dash_load_fork(ctx);
}

static int ytdl__dash_manifest_complete_cb (llhttp_t* parser)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)parser->data)->data;

    ctx->is_shutdown = 1;
    ytdl_http_client_shutdown(&ctx->http, ytdl__dash_close_cb);
    
    return 0;
}

int ytdl_dl_dash_ctx_init (uv_loop_t *loop, ytdl_dl_dash_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(ytdl_dl_dash_ctx_t));
    if (!ytdl_http_client_init(loop, &ctx->http))
    {
        return 1;
    }

    ctx->http.data = ctx;
    ctx->http.settings.keep_alive = 1;
    ctx->http.parser_settings.on_body = ytdl__dash_manifest_cb;
    ctx->http.parser_settings.on_message_complete = ytdl__dash_manifest_complete_cb;

    if (!ytdl_buf_alloc(&ctx->manifest, 1))
        return 1;

    return 0;
}

static void ytdl__dash_manifest_connected_cb (ytdl_http_client_t *client)
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)client->data;

    ytdl_buf_t buf;
    ytdl_net_request_generic(&buf, ctx->manifest_path, ctx->manifest_path_len, ctx->manifest_host, ctx->manifest_host_len);
    ytdl_http_client_write(&ctx->http, (uv_buf_t *)&buf, ytdl__write_cb);
    ytdl_buf_free(&buf);
}

int ytdl_dl_dash_ctx_connect (ytdl_dl_dash_ctx_t *ctx, const char *manifest_url)
{
    struct http_parser_url url;
    http_parser_url_init(&url);

    if (http_parser_parse_url(manifest_url, strlen(manifest_url), 0, &url))
        return -1;

    ctx->manifest_host_len = url.field_data[UF_HOST].len;
    ctx->manifest_host = malloc(ctx->manifest_host_len + 1);
    memcpy(ctx->manifest_host, manifest_url + url.field_data[UF_HOST].off, url.field_data[UF_HOST].len);
    ctx->manifest_host[url.field_data[UF_HOST].len] =  0;

    ctx->manifest_path_len = url.field_data[UF_PATH].len;
    ctx->manifest_path = malloc(ctx->manifest_path_len + 1);
    memcpy(ctx->manifest_path, manifest_url + url.field_data[UF_PATH].off, url.field_data[UF_PATH].len);
    ctx->manifest_path[url.field_data[UF_PATH].len] =  0;

    ytdl_http_client_connect(&ctx->http, 1, ctx->manifest_host, "443", ytdl__status_cb, ytdl__dash_manifest_connected_cb);

    return 0;
}

static void ytdl__dash_close_f_cb (uv_handle_t* handle) 
{
    ytdl_dl_dash_ctx_t *ctx = (ytdl_dl_dash_ctx_t *)((ytdl_http_client_t *)handle->data)->data;

    ctx->on_close(ctx);
}

void ytdl__dl_dash_shutdown (uv_idle_t* handle) 
{
    ytdl_dl_dash_ctx_t *ctx = handle->data;

    uv_idle_stop(handle);
    uv_close((uv_handle_t*)handle, close_free_cb);

    if (ctx->path)
        free(ctx->path);
    if (ctx->host)
        free(ctx->host);
    if (ctx->manifest_host)
        free(ctx->manifest_host);
    if (ctx->manifest_path)
        free(ctx->manifest_path);
    
    if (ctx->is_dash_init)
        ytdl_dash_ctx_free(&ctx->dash);
    
    ytdl_buf_free(&ctx->manifest);

    if (!ctx->is_shutdown)
        ytdl_http_client_shutdown(&ctx->http, ytdl__dash_close_f_cb);
}

int ytdl_dl_dash_ctx_fork (ytdl_dl_dash_ctx_t *ctx, ytdl_dl_dash_ctx_t *fork)
{
    if (!ytdl_buf_realloc(&fork->manifest, ctx->manifest.size))
        return 1;
    memcpy(fork->manifest.base, ctx->manifest.base, ctx->manifest.len);
    fork->manifest.len = ctx->manifest.len;

    fork->on_data = ctx->on_data;
    fork->on_complete = ctx->on_complete;
    fork->on_status = ctx->on_status;
    fork->on_close = ctx->on_close;
    fork->on_manifest = ctx->on_manifest;
    fork->on_pick_filter = ctx->on_pick_filter;
    fork->on_segment_complete = ctx->on_segment_complete;

    fork->manifest_path = strdup(ctx->manifest_path);
    fork->manifest_host = strdup(ctx->manifest_host);

    fork->manifest_host_len = ctx->manifest_host_len;
    fork->manifest_path_len = ctx->manifest_path_len;

    fork->is_shutdown = ctx->is_shutdown;
    fork->is_dash_init = ctx->is_dash_init;
    fork->is_video = ctx->is_video;
    fork->data = ctx->data;

    return 0;
}

void ytdl_dl_dash_shutdown (ytdl_dl_dash_ctx_t *ctx, ytdl_dl_dash_cb on_close)
{
    // finish the current event before killing everything
    ctx->on_close = on_close;
    uv_idle_t *idle = malloc(sizeof(uv_async_t));
    idle->data = ctx;
    uv_idle_init(ctx->http.loop, idle);
    uv_idle_start(idle, ytdl__dl_dash_shutdown);
}
