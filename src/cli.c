#include <stdio.h>
#include <stdlib.h>

#include <ytdl/dl.h>

int main ()
{
    ytdl_dl_ctx_t ctx;
    ytdl_dl_ctx_init(uv_default_loop(), &ctx);
    ytdl_dl_set_video_url(&ctx, "https://www.youtube.com/watch?v=qquhXmDtynM&list=PLASU6lE9dlHDtAc-OUsDIO45V-3A0Fxwd&index=6");
    ytdl_dl_ctx_run(&ctx);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    ytdl_dl_shutdown(&ctx);
}

/*
#include <ytdl/sig.h>
#include <ytdl/info.h>
#include <ytdl/net.h>
#include <ytdl/http/http.h>

#include <uriparser/Uri.h>

void status (ytdl_http_client_t *client, ytdl_net_status_t *status)
{
    printf("STATUS CALLED %lld %s\n", status->type, status->code);
}

void connected_cb (ytdl_http_client_t *client)
{
    puts("CONNECTED");
    ytdl_buf_t buf;
    ytdl_net_request_watch_html(&buf, "2KrzvBlsS8k");
    ytdl_http_client_write(client, &buf, status);
    ytdl_buf_free(&buf);
}

int body_cb (llhttp_t* parser, const char *at, size_t length)
{
    printf("BODY\n%.*s\n\n", length, at);
    return 0;
}

int imp__pool_on_message_complete (llhttp_t* parser)
{
    puts("FU");
    return 0;
}

int main () {
    puts("TRYING TO CONNECT");

    ytdl_ssl_init();

    ytdl_http_client_t *http = malloc(sizeof(ytdl_http_client_t));
    ytdl_http_client_init(uv_default_loop(), http);
    http->parser_settings.on_body = body_cb;
    http->parser_settings.on_message_complete = imp__pool_on_message_complete;
    http->settings.keep_alive = 1;
    ytdl_http_client_connect(http, 1, "www.youtube.com", "443", status, connected_cb);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

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
    free(buf);
}
*/