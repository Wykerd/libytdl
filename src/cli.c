#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <ytdl/dl.h>
#include <ytdl/info.h>
#include <ytdl/mux.h>

const char cli_usage[] =
    "Usage: %s [-qfFLovD] URL [URL..]\n\n"
    "Options:\n"
    "  -q, --quiet                  Silence output to console.\n"
    "  -f, --format FORMAT          Download format selector. See cli documentation for info (default \"bestvideo+bestaudio/best\")\n"
    "  -F, --format-index NUMBER    Download format by index\n"
    "  -L, --log-formats            Log formats to console. Do not download.\n"
    "  -o, --output TEMPLATE        Output filename template. See cli documentation for info (default \"%%2$s-%%1$s.mkv\")\n"
    "Debugging:\n"
    "  -v, --verbose                Verbose output for debugging.\n"
    "  -D, --debug-log FILENAME     Creates a complete log file with all downloaded resources for debugging.\n";
/*
int main(int argc, char *argv[])
{
    char id[YTDL_ID_SIZE];
    int opt, long_index, idx, audio_only = 0;
    ytdl_mux_loglevel loglevel = YTDL_MUX_LOG_ERRORS;

    static struct option long_options[] = {
        { "quiet",          no_argument,        0, 'q' },
        //{ "format",         required_argument,  0, 'f' },
        //{ "log-formats",    no_argument,        0, 'L' },
        //{ "format-index",   required_argument,  0, 'F' },
        { "output",         required_argument,  0, 'o' },
        { 0,        0,           0, 0   }
    };

    if (argc < 2)
    {
        printf(cli_usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt_long(argc, argv, "qfFLovD", long_options, &long_index)) != -1)
    {
        switch (opt)
        {
        case 'f':
            audio_only = 1;
            break;

        case 'q':
            loglevel = YTDL_MUX_LOG_QUIET;
            break;
        
        default:
            printf(cli_usage, argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    for (idx = optind; idx < argc - 1; idx++) {
        printf("Non-option argument %s\n", argv[idx]);
    }

    ytdl_sanitize_filename_inplace(argv[idx]);

    printf("COOL argument %s\n", argv[idx]);

    printf("%3$s\n", 5, 123, "TRUE");

    return 0;
}
//*/
///*
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

static void ytdl__dash_pipe (ytdl_dl_dash_ctx_t *ctx, const char *buf, size_t len)
{
    // puts("WRITING");
    content_length_true += len;
    printf("[ytdl] %lld bytes downloaded\r", content_length_true);
    fflush(stdout);
    fwrite(buf, 1, len, ctx->data);
}

static void ytdl__dash_close_cb (ytdl_dl_dash_ctx_t *ctx)
{
    free(ctx);
}

static void ytdl__dash_complete (ytdl_dl_dash_ctx_t *ctx)
{
    fclose(ctx->data);
    ytdl_dl_dash_shutdown(ctx, ytdl__dash_close_cb);
}

static void ytdl__dash_manifest_done (ytdl_dl_dash_ctx_t *ctx)
{
    ytdl_dl_dash_ctx_t *dash = malloc(sizeof(ytdl_dl_dash_ctx_t));
    ytdl_dl_dash_ctx_init(ctx->http.loop, dash);
    ytdl_dl_dash_ctx_fork(ctx, dash);
    dash->is_video = 1;
    dash->data = fopen("./video_ff", "wb");
    ctx->is_video = 0;
    ctx->data = fopen("./audio_ff", "wb");
    ytdl_dl_dash_load_fork(dash);
}

static void on_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_info_extract_formats(&vid->info);

    ytdl_info_extract_video_details(&vid->info);

    printf("[ytdl] Video title: %s\n", vid->info.title);

    printf("[ytdl] Downloading media for: %s\n", vid->id);

    if (vid->info.dash_manifest_url)
    {
        printf("[ytdl] Using dash manifest\n");
        ytdl_dl_dash_ctx_t *dash = malloc(sizeof(ytdl_dl_dash_ctx_t));
        ytdl_dl_dash_ctx_init(ctx->http.loop, dash);
        dash->on_data = ytdl__dash_pipe;
        dash->on_complete = ytdl__dash_complete;
        dash->on_pick_filter = ytdl_dash_get_best_representation;
        dash->on_manifest = ytdl__dash_manifest_done;
        ytdl_dl_dash_ctx_connect(dash, vid->info.dash_manifest_url);
    }
    else
    {
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
}

static void on_complete_f (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    on_complete(ctx, vid);

    FILE *fd = fopen("./ytdl_cache.bin", "wb");
    ytdl_dl_player_cache_save_file(ctx, fd);
    //ytdl_sig_actions_save_file(vid->info.sig_actions, fd);
    fclose(fd);

    ytdl_dl_shutdown(ctx);
}

int main (int argc, char **argv)
{
    char id[YTDL_ID_SIZE];
    ytdl_dl_ctx_t ctx;

    if (argc < 2)
    {
        printf("Usage: %s URL [URL..]", argv[0]);
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
//*/
