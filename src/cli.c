#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>
#include <ytdl/info.h>

static void on_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_info_extract_formats(&vid->info);

    printf("#####################\nVideo ID: %s\n\nBest Audio: %s\n\nBest Video: %s\n\n", 
        vid->id,
        ytdl_info_get_format_url(&vid->info, ytdl_info_get_best_audio_format(&vid->info)),
        ytdl_info_get_format_url(&vid->info, ytdl_info_get_best_video_format(&vid->info))
    );
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
}

