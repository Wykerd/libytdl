#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>
#include <ytdl/info.h>
#include <ytdl/mux.h>

long long content_length = 0;
long long content_length_true = 0;

static void ytdl__media_pipe (ytdl_dl_media_ctx_t *ctx, const char *buf, size_t len)
{
    // puts("WRITING");
    content_length_true += len;
    printf("[ytdl] %3.1f %% downloaded\r", 
        ((1.0 * content_length_true) / content_length) * 100);
    fflush(stdout);
    fwrite(buf, 1, len, ctx->data);
}

static void ytdl__media_close_cb (ytdl_dl_media_ctx_t *ctx)
{
    free(ctx);
}

static void ytdl__media_complete (ytdl_dl_media_ctx_t *ctx)
{
    fclose(ctx->data);
    ytdl_dl_media_shutdown(ctx, ytdl__media_close_cb);
}

static void on_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_info_extract_formats(&vid->info);

    printf("[ytdl] Downloading media for: %s\n", vid->id);

    ytdl_info_format_t *vid_fmt = vid->info.formats[ytdl_info_get_best_video_format(&vid->info)];

    ytdl_info_format_t *aud_fmt = vid->info.formats[ytdl_info_get_best_audio_format(&vid->info)];

    content_length = aud_fmt->content_length + vid_fmt->content_length;

    ytdl_dl_media_ctx_t *vid_m = malloc(sizeof(ytdl_dl_media_ctx_t));
    ytdl_dl_media_ctx_init(ctx->http.loop, vid_m, vid_fmt, &vid->info);
    vid_m->on_data = ytdl__media_pipe;
    vid_m->on_complete = ytdl__media_complete;
    vid_m->data = fopen("./video_ff", "wb");
    ytdl_dl_media_ctx_connect(vid_m);

    ytdl_dl_media_ctx_t *aud_m = malloc(sizeof(ytdl_dl_media_ctx_t));
    ytdl_dl_media_ctx_init(ctx->http.loop, aud_m, aud_fmt, &vid->info);
    aud_m->on_data = ytdl__media_pipe;
    aud_m->on_complete = ytdl__media_complete;
    aud_m->data = fopen("./audio_ff", "wb");
    ytdl_dl_media_ctx_connect(aud_m);
}

static void on_complete_f (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    on_complete(ctx, vid);

    FILE *fd = fopen("./ytdl_cache.bin", "wb");
    ytdl_dl_player_cache_save_file(ctx, fd);
    fclose(fd);

    ytdl_dl_shutdown(ctx);
}

int main (int argc, char **argv)
{
    char id[YTDL_ID_SIZE];
    ytdl_dl_ctx_t ctx;

    if (argc < 2)
    {
        printf("Usage: %s [LINK]...\n", argv[0]);
        return 0;
    }

    ytdl_dl_ctx_init(uv_default_loop(), &ctx);

    // Load JS Player Cache
    FILE *fd = fopen("./ytdl_cache.bin", "rb");
    if (fd) 
    {
        ytdl_dl_player_cache_load_file(&ctx, fd);
        fclose(fd);
    } 
    else if (errno != ENOENT)
    {
        perror("JS Player Cache Error.");
        return 1;
    }

    for (size_t i = 1; i < argc - 1; i++)
    {
        ytdl_net_get_id_from_url(argv[i], strlen(argv[i]), id);
        ytdl_dl_get_info (&ctx, id, on_complete);
    }
    ytdl_net_get_id_from_url(argv[argc - 1], strlen(argv[argc - 1]), id);
    ytdl_dl_get_info (&ctx, id, on_complete_f);
    
    ytdl_dl_ctx_connect(&ctx);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    putc('\n', stdout);
    ytdl_mux_files("./audio_ff", "./video_ff", "./output_video.mkv", YTDL_MUX_LOG_VERBOSE);
}

