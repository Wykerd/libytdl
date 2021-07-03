#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>
#include <ytdl/info.h>

void on_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_info_extract_formats(&vid->info);

    printf("Best Audio: %s\n\nBest Video: %s\n\n", 
        ytdl_info_get_format_url(&vid->info, ytdl_info_get_best_audio_format(&vid->info)),
        ytdl_info_get_format_url(&vid->info, ytdl_info_get_best_video_format(&vid->info))
    );

    //ytdl_dl_shutdown(ctx);
}

int main ()
{
    ytdl_dl_ctx_t ctx;
    ytdl_dl_ctx_init(uv_default_loop(), &ctx);
    ytdl_dl_get_info (&ctx, "XDNFAujgJb0", on_complete);
    ytdl_dl_get_info (&ctx, "Y59vIAr8rAI", on_complete);
    ytdl_dl_ctx_connect(&ctx);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

