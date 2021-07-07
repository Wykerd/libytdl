#ifndef YTDL_CLI_H
#define YTDL_CLI_H

#include <stdio.h>

#include <ytdl/dl.h>
#include <ytdl/info.h>
#include <ytdl/mux.h>

typedef enum ytdl_cli_log_level {
    YTDL_LOG_LEVEL_NONE,
    YTDL_LOG_LEVEL_INFO,
    YTDL_LOG_LEVEL_VERBOSE
} ytdl_cli_log_level;

typedef struct ytdl_cli_options_s {
    char *o_templ;
    char *v_intr_templ;
    char *a_intr_templ;
    int log_formats;
    int is_debug;
    FILE *debug_log_fd;
    ytdl_cli_log_level log_level;
} ytdl_cli_options_t;

typedef struct ytdl_cli_video_s {
    ytdl_cli_options_t *opts;
    ytdl_dl_ctx_t *ctx;
    char id[YTDL_ID_SIZE];

    char *o_path;

    int audio_fd;
    int video_fd;
    char *audio_path;
    char *video_path;
    int is_video_done;
    int is_audio_done;

    int video_count;
    int audio_count;
    int video_count_total;
    int audio_count_total;
} ytdl_cli_video_t;

typedef struct ytdl_cli_ctx_s {
    ytdl_cli_video_t *video;
    ytdl_dl_media_ctx_t *media;
    int fd;
    int *is_done;
    int *count;
} ytdl_cli_ctx_t;

#endif
