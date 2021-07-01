#include <stdio.h>
#include <stdlib.h>

#include <ytdl/sig.h>
#include <ytdl/info.h>
#include <ytdl/net.h>
#include <ytdl/http/http.h>

#include <uriparser/Uri.h>

void status (ytdl_http_client_t *client, ytdl_net_status_t *status)
{
    puts("STATUS CALLED");
}

void connected_cb (ytdl_http_client_t *client)
{
    puts("CONNECTED");
}

int main () {
    /*
    // //
    FILE *fd = fopen("/Users/wykerd/Documents/ytdl/watch.html", "rb");
    fseek(fd, 0, SEEK_END);
    size_t buf_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    uint8_t *buf = malloc(buf_size + 1);
    fread(buf, 1, buf_size, fd);
    fclose(fd);
    // //

    ytdl_info_ctx_t info;
    ytdl_info_ctx_init(&info);
    ytdl_info_extract_watch_html(&info, buf, buf_size);

    if (ytdl_info_get_playability_status(&info))
    {
        puts(ytdl_info_get_playability_status_message(&info));
        return 1;
    }

    printf("%s\n", info.player_url);

    // //
    FILE *fdd = fopen("/Users/wykerd/Documents/ytdl/player.js", "rb");
    fseek(fdd, 0, SEEK_END);
    size_t buff_size = ftell(fdd);
    fseek(fdd, 0, SEEK_SET); 
    uint8_t *buff = malloc(buff_size + 1);
    fread(buff, 1, buff_size, fdd);
    fclose(fdd);
    // // //

    ytdl_sig_actions_t actions;
    ytdl_sig_actions_extract(&actions, buff, buff_size);

    free(buff);

    ytdl_info_set_sig_actions(&info, &actions);
    ytdl_info_extract_formats(&info);

    printf("Best Audio: %s\n\nBest Video: %s\n\n", 
        ytdl_info_get_format_url(&info, ytdl_info_get_best_audio_format(&info)),
        ytdl_info_get_format_url(&info, ytdl_info_get_best_video_format(&info))
    );

    char id[12];
    ytdl_net_get_id_from_url("https://www.youtube.com/watch?v=Y59vIAr8rAI", id);

    puts(id);

    ytdl_info_ctx_free(&info);
    free(buf);*/

    puts("TRYING TO CONNECT");

    ytdl_http_client_t http;
    ytdl_http_client_init(uv_default_loop(), &http);
    ytdl_http_client_set_url(&http, "https://example.com/");
    ytdl_http_client_connect(&http, status, connected_cb);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}