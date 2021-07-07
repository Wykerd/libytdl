#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <ytdl/cli.h>
#include <ytdl/dl.h>
#include <ytdl/info.h>
#include <ytdl/mux.h>

const char cli_usage[] =
    "Usage: %s [-qLo:vD:] URL [URL..]\n\n"
    "Options:\n"
    "  -q, --quiet                  Silence output to console.\n"
    //"  -f, --format FORMAT          Download format selector. See cli documentation for info (default \"bestvideo+bestaudio/best\")\n"
    //"  -F, --format-index NUMBER    Download format by index\n"
    "  -L, --log-formats            Log formats to console. Do not download.\n"
    "  -o, --output TEMPLATE        Output filename template. See cli documentation for info (default \"%%2$s-%%1$s.mkv\")\n"
    "Debugging:\n"
    "  -v, --verbose                Verbose output for debugging.\n"
    "  -D, --debug-log FILENAME     Creates a complete log file with all downloaded resources for debugging.\n";

static void ytdl__media_close_cb (ytdl_dl_media_ctx_t *ctx)
{
    free(ctx);
}

static void ytdl__dash_close_cb (ytdl_dl_dash_ctx_t *ctx)
{
    free(ctx);
}

static void ytdl__mux_and_free (ytdl_cli_video_t *video)
{
    if (video->opts->log_level > YTDL_LOG_LEVEL_NONE)
        putc('\n', stdout);
    ytdl_mux_files(video->audio_path, video->video_path, video->o_path, 
        video->opts->log_level == YTDL_MUX_LOG_QUIET ? YTDL_MUX_LOG_ERRORS : YTDL_MUX_LOG_VERBOSE);
    if (!video->opts->is_debug)
    {
        unlink(video->audio_path);
        unlink(video->video_path);
    }
    free(video->audio_path);
    free(video->video_path);
    free(video->o_path);
    free(video); 
}

static void ytdl__dash_complete (ytdl_dl_dash_ctx_t *ctx)
{
    ytdl_cli_video_t *video = ctx->data;
    if (ctx->is_video)
        video->is_video_done = 1;
    else
        video->is_audio_done = 1;
    if (video->is_audio_done && video->is_video_done)
    {
        ytdl__mux_and_free(video);
    }
    ytdl_dl_dash_shutdown(ctx, ytdl__dash_close_cb);
}

static void ytdl__media_complete (ytdl_dl_media_ctx_t *ctx)
{
    ytdl_cli_ctx_t *media = ctx->data;
    *media->is_done = 1;
    close(media->fd);
    if (media->video->is_audio_done && media->video->is_video_done)
    {
        ytdl__mux_and_free(media->video);
    }
    free(media);
    ytdl_dl_media_shutdown(ctx, ytdl__media_close_cb);
}

static void ytdl__dash_pipe (ytdl_dl_dash_ctx_t *ctx, const char *buf, size_t len)
{
    ytdl_cli_video_t *video = ctx->data;
    write(ctx->is_video ? video->video_fd : video->audio_fd, buf, len);
}

static void ytdl__dash_segment_complete (ytdl_dl_dash_ctx_t *ctx)
{
    ytdl_cli_video_t *video = ctx->data;
    if (ctx->is_video)
        video->video_count++;
    else
        video->audio_count++;

    if (video->opts->log_level > YTDL_LOG_LEVEL_NONE)
    {
        printf(
            "[info] %s: %3.1f %% downloaded.\r",
            video->id,
            ((1.0 * (video->audio_count + video->video_count)) 
            / (ctx->dash.a_segment_count + ctx->dash.v_segment_count)) * 100
        );
        fflush(stdout);
    }
}

static void ytdl__media_pipe (ytdl_dl_media_ctx_t *ctx, const char *buf, size_t len)
{
    ytdl_cli_ctx_t *media = ctx->data;
    *media->count += len;
    if (media->video->opts->log_level > YTDL_LOG_LEVEL_NONE)
    {
        printf(
            "[info] %s: %3.1f %% downloaded\r",
            media->video->id,
            ((1.0 * (media->video->audio_count + media->video->video_count)) 
            / (media->video->audio_count_total + media->video->video_count_total)) * 100
        );
        fflush(stdout);
    }
    write(media->fd, buf, len);
}

static void ytdl__dash_manifest_done (ytdl_dl_dash_ctx_t *ctx)
{
    ytdl_dl_dash_ctx_t *dash = malloc(sizeof(ytdl_dl_dash_ctx_t));
    ytdl_dl_dash_ctx_init(ctx->http.loop, dash);
    ytdl_dl_dash_ctx_fork(ctx, dash);
    dash->is_video = 1;
    dash->data = ctx->data;
    ctx->is_video = 0;
    ctx->data = ctx->data;
    ytdl_dl_dash_load_fork(dash);
}

static void ytdl__info_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_cli_options_t *opts = ctx->data;
    
    ytdl_info_extract_formats(&vid->info);
    ytdl_info_extract_video_details(&vid->info);

    if (opts->debug_log_fd) {
        const char *func_val = JS_ToCString(vid->info.sig_actions->ctx, vid->info.sig_actions->func);

        fprintf(
            opts->debug_log_fd,
            "[debug] Actions { %d, %d, %d, %d }\n"
            "[debug] Action Args { %d, %d, %d, %d }\n"
            "[debug] Sig Function\n%s\n\n",
            vid->info.sig_actions->actions[0],
            vid->info.sig_actions->actions[1],
            vid->info.sig_actions->actions[2],
            vid->info.sig_actions->actions[3],
            vid->info.sig_actions->actions_arg[0],
            vid->info.sig_actions->actions_arg[1],
            vid->info.sig_actions->actions_arg[2],
            vid->info.sig_actions->actions_arg[3],
            func_val
        );

        JS_FreeCString(vid->info.sig_actions->ctx, func_val);
    }

    if (opts->log_formats)
    {
        printf("[info] %s: Formats\n", vid->id);
        for (size_t i = 0; i < vid->info.formats_size; i++)
        {
            ytdl_info_format_populate(vid->info.formats[i]);
            printf(
                "[info] Index: %zu\n"
                "[info] Itag: %d\n"
                "[info] MIME type: %s\n"
                "[info] Size: %lld bytes\n"
                "[info] Duration: %lld ms\n"
                "[info] Bitrate: %d\n"
                "[info] Average Bitrate: %d\n",
                i,
                vid->info.formats[i]->itag,
                vid->info.formats[i]->mime_type,
                vid->info.formats[i]->content_length,
                vid->info.formats[i]->approx_duration_ms,
                vid->info.formats[i]->bitrate,
                vid->info.formats[i]->average_bitrate
            );
            if (vid->info.formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID)
                printf(
                    "[info] Dimensions: %d x %d\n"
                    "[info] Quality: %s, %s\n"
                    "[info] FPS: %d\n",
                    vid->info.formats[i]->width,
                    vid->info.formats[i]->height,
                    vid->info.formats[i]->quality,
                    vid->info.formats[i]->quality_label,
                    vid->info.formats[i]->fps
                );
            if (vid->info.formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD)
                printf(
                    "[info] Audio Channels: %d\n"
                    "[info] Audio Quality: %s\n",
                    vid->info.formats[i]->audio_channels,
                    vid->info.formats[i]->audio_quality == YTLD_INFO_AUDIO_QUALITY_LOW ? "low" : "medium"
                );
            putc('\n', stdout);
        };
        if (vid->info.dash_manifest_url)
            printf("[info] %s: Has Dash Manifest\n\n", vid->id);
        return;
    }
    
    if (opts->log_level > YTDL_LOG_LEVEL_INFO)
        fprintf(
            opts->debug_log_fd ? opts->debug_log_fd : stdout,
            "\n"
            "[debug] %s: Video Info\n" 
            "[debug] Title: %s\n"
            "[debug] Length: %s seconds\n"
            "[debug] Channel ID: %s\n"
            "[debug] Rating: %.3f\n"
            "[debug] Views: %s\n"
            "[debug] Author: %s\n"
            "[debug] Description:\n%s\n"
            "\n",
            vid->id,
            vid->info.title, 
            vid->info.length_seconds, 
            vid->info.channel_id, 
            vid->info.average_rating, 
            vid->info.view_count, 
            vid->info.author, 
            vid->info.short_description
        );

    if (opts->log_level > YTDL_LOG_LEVEL_NONE)
        printf("[info] %s: Downloading media\n", vid->id);

    ytdl_cli_video_t *video = calloc(1, sizeof(ytdl_cli_video_t));
    if (!video)
    {
        fputs("[error] out of memory", stderr);
        exit(EXIT_FAILURE); // TODO: clean exit
    };
    memcpy(&video->id[0], &vid->id[0], YTDL_ID_SIZE);

    char *title = strdup(vid->info.title);
    ytdl_sanitize_filename_inplace(title);
    ssize_t bufsz = snprintf(NULL, 0, opts->o_templ,
        vid->id,
        title, 
        vid->info.length_seconds, 
        vid->info.channel_id, 
        vid->info.average_rating, 
        vid->info.view_count, 
        vid->info.author
    );
    video->o_path = malloc(bufsz + 1);
    snprintf(video->o_path, bufsz + 1,  opts->o_templ,
        vid->id,
        title, 
        vid->info.length_seconds, 
        vid->info.channel_id, 
        vid->info.average_rating, 
        vid->info.view_count, 
        vid->info.author
    );
    free(title);

    video->opts = opts;
    video->ctx = ctx;

    if (vid->info.dash_manifest_url)
    {
        video->video_path = strdup(opts->v_intr_templ);
        video->video_fd = mkstemp(video->video_path);
        video->audio_path = strdup(opts->a_intr_templ);
        video->audio_fd = mkstemp(video->audio_path);

        if (opts->log_level > YTDL_LOG_LEVEL_INFO)
            fprintf(
                opts->debug_log_fd ? opts->debug_log_fd : stdout,
                "\n"
                "[debug] %s: Video Temp Path: %s\n"
                "\n"
                "[debug] %s: Audio Temp Path: %s\n"
                "\n",
                vid->id,
                video->video_path,
                vid->id,
                video->audio_path
            );

        if (opts->log_level > YTDL_LOG_LEVEL_NONE)
            printf("[info] %s: Using dash manifest\n", vid->id);
        if (opts->log_level > YTDL_LOG_LEVEL_INFO)
            fprintf(
                opts->debug_log_fd ? opts->debug_log_fd : stdout,
                "[debug] %s: Manifest URL: %s\n", vid->id, vid->info.dash_manifest_url);
        ytdl_dl_dash_ctx_t *dash = malloc(sizeof(ytdl_dl_dash_ctx_t));
        if (!dash)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        }
        ytdl_dl_dash_ctx_init(ctx->http.loop, dash); // TODO: error
        dash->data = video;
        dash->on_data = ytdl__dash_pipe;
        dash->on_complete = ytdl__dash_complete;
        dash->on_segment_complete = ytdl__dash_segment_complete;
        dash->on_manifest = ytdl__dash_manifest_done;
        dash->on_pick_filter = ytdl_dash_get_best_representation;
        ytdl_dl_dash_ctx_connect(dash, vid->info.dash_manifest_url); // TODO: error */
    }
    else
    {
        ytdl_info_format_t *vid_fmt = vid->info.formats[ytdl_info_get_best_video_format(&vid->info)];
        ytdl_info_format_t *aud_fmt = vid->info.formats[ytdl_info_get_best_audio_format(&vid->info)];

        if (opts->log_level > YTDL_LOG_LEVEL_INFO)
            fprintf(
                opts->debug_log_fd ? opts->debug_log_fd : stdout,
                "\n"
                "[debug] %s: Video Format: %s\n"
                "\n"
                "[debug] %s: Audio Format: %s\n"
                "\n",
                vid->id,
                ytdl_info_get_format_url2(&vid->info, vid_fmt),
                vid->id,
                ytdl_info_get_format_url2(&vid->info, aud_fmt)
            );

        ytdl_dl_media_ctx_t *vid_m = malloc(sizeof(ytdl_dl_media_ctx_t));
        if (!vid_m)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        };

        ytdl_cli_ctx_t *dl_ctx = malloc(sizeof(ytdl_cli_ctx_t));
        if (!dl_ctx)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        };

        video->video_count_total = vid_fmt->content_length;
        video->audio_count_total = aud_fmt->content_length;

        dl_ctx->count = &video->video_count;
        dl_ctx->is_done = &video->is_video_done;
        dl_ctx->video = video;
        dl_ctx->media = vid_m;

        ytdl_dl_media_ctx_init(ctx->http.loop, vid_m, vid_fmt, &vid->info);
        vid_m->on_data = ytdl__media_pipe;
        vid_m->on_complete = ytdl__media_complete;
        vid_m->data = dl_ctx;

        video->video_path = strdup(opts->v_intr_templ);
        video->video_fd = mkstemp(video->video_path);

        dl_ctx->fd = video->video_fd;
        ytdl_dl_media_ctx_connect(vid_m);

        ytdl_dl_media_ctx_t *aud_m = malloc(sizeof(ytdl_dl_media_ctx_t));
        if (!aud_m)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        };

        ytdl_cli_ctx_t *dl_ctx_a = malloc(sizeof(ytdl_cli_ctx_t));
        if (!dl_ctx)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        };

        dl_ctx_a->count = &video->audio_count;
        dl_ctx_a->is_done = &video->is_audio_done;
        dl_ctx_a->video = video;
        dl_ctx_a->media = aud_m;

        ytdl_dl_media_ctx_init(ctx->http.loop, aud_m, aud_fmt, &vid->info);
        aud_m->on_data = ytdl__media_pipe;
        aud_m->on_complete = ytdl__media_complete;
        aud_m->data = dl_ctx_a;

        video->audio_path = strdup(opts->a_intr_templ);
        video->audio_fd = mkstemp(video->audio_path);

        dl_ctx_a->fd = video->audio_fd;
        ytdl_dl_media_ctx_connect(aud_m);

        if (opts->log_level > YTDL_LOG_LEVEL_INFO)
            fprintf(
                opts->debug_log_fd ? opts->debug_log_fd : stdout,
                "\n"
                "[debug] %s: Video Temp Path: %s\n"
                "\n"
                "[debug] %s: Audio Temp Path: %s\n"
                "\n",
                vid->id,
                video->video_path,
                vid->id,
                video->audio_path
            );
    }
}

static void ytdl__info_complete_final (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl__info_complete(ctx, vid);

    FILE *fd = fopen("./ytdl_cache.bin", "wb");
    ytdl_dl_player_cache_save_file(ctx, fd);
    fclose(fd);

    ytdl_dl_shutdown(ctx);
}

static void ytdl__http_client_read_log (ytdl_http_client_t* client, const uv_buf_t *buf)
{
    ytdl_dl_ctx_t *ctx = client->data;
    ytdl_cli_options_t *opts = ctx->data;
    ytdl_http_client_parse_read(client, buf);

    fwrite(buf->base, 1, buf->len, opts->debug_log_fd);
    putc('\n', opts->debug_log_fd);
}

int main (int argc, char **argv)
{
    char id[YTDL_ID_SIZE];
    int opt, long_index, idx;
    ytdl_dl_ctx_t ctx;
    ytdl_cli_options_t opts = {
        .o_templ = "%2$s-%1$s.mkv",
        .v_intr_templ = "video.XXXXXX",
        .a_intr_templ = "audio.XXXXXX",
        .is_debug = 0,
        .debug_log_fd = NULL,
        .log_level = YTDL_LOG_LEVEL_INFO,
        .log_formats = 0
    };

    if (argc < 2)
    {
        printf(cli_usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    static struct option long_options[] = {
        { "quiet",          no_argument,        0, 'q' },
        //{ "format",         required_argument,  0, 'f' },
        //{ "format-index",   required_argument,  0, 'F' },
        { "log-formats",    no_argument,        0, 'L' },
        { "output",         required_argument,  0, 'o' },
        { "verbose",        no_argument,        0, 'v' },
        { "debug-log",      required_argument,  0, 'D' },
        { 0,        0,           0, 0   }
    };

    while ((opt = getopt_long(argc, argv, "qLo:vD:", long_options, &long_index)) != -1)
    {
        switch (opt)
        {
        case 'L':
            opts.log_formats = 1;
            break;

        case 'q':
            opts.log_level = YTDL_LOG_LEVEL_NONE;
            break;

        case 'v':
            opts.log_level = YTDL_LOG_LEVEL_VERBOSE;
            break;

        case 'o':
            opts.o_templ = optarg;
            break;

        case 'D':
            {
                opts.is_debug = 1;
                opts.debug_log_fd = fopen(optarg, "wb");
                if (!opts.debug_log_fd)
                {
                    perror("[error] could not open debug log.");
                    exit(EXIT_FAILURE);
                }
            }
            break;

        default:
            printf(cli_usage, argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    ytdl_dl_ctx_init(uv_default_loop(), &ctx);

    if (opts.debug_log_fd)
        ctx.http.read_cb = ytdl__http_client_read_log;

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

    ctx.data = &opts;

    for (idx = optind; idx < argc - 1; idx++)
    {
        if (ytdl_net_get_id_from_url(argv[idx], strlen(argv[idx]), id))
        {
            printf("[error] invalid url %s\n", argv[idx]);
            exit(EXIT_FAILURE);
        }
        ytdl_dl_get_info (&ctx, id, ytdl__info_complete);
    }
    if (ytdl_net_get_id_from_url(argv[argc - 1], strlen(argv[argc - 1]), id))
    {
        printf("[error] invalid url %s\n", argv[argc - 1]);
        exit(EXIT_FAILURE);
    }
    ytdl_dl_get_info (&ctx, id, ytdl__info_complete_final); 

    ytdl_dl_ctx_connect(&ctx);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    if (opts.debug_log_fd)
        fclose(opts.debug_log_fd);

    return 0;
}
