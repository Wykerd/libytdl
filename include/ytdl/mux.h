#ifndef YTDL_MUX_H
#define YTDL_MUX_H

typedef enum ytdl_mux_loglevel_e {
    YTDL_MUX_LOG_QUIET,
    YTDL_MUX_LOG_ERRORS,
    YTDL_MUX_LOG_VERBOSE
} ytdl_mux_loglevel;

int ytdl_mux_files (const char *audio_path, const char *video_path, const char *output_path, ytdl_mux_loglevel loglevel);

#endif
