#include <stdio.h>
#include <stdlib.h>

#include <ytdl/sig.h>
#include <ytdl/info.h>
#include <ytdl/net.h>

int main () {
    // //
    FILE *fd = fopen("/Users/wykerd/Documents/ytdl/watch.html", "rb");
    fseek(fd, 0, SEEK_END);
    size_t buf_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);  /* same as rewind(f); */
    uint8_t *buf = malloc(buf_size + 1);
    fread(buf, 1, buf_size, fd);
    fclose(fd);
    // //

    ytdl_info_ctx_t info;
    ytdl_info_extract_watch_html(&info, buf, buf_size);

    printf("%s\n", info.player_url);

    // //
    FILE *fdd = fopen("/Users/wykerd/Documents/ytdl/player.js", "rb");
    fseek(fdd, 0, SEEK_END);
    size_t buff_size = ftell(fdd);
    fseek(fdd, 0, SEEK_SET);  /* same as rewind(f); */
    uint8_t *buff = malloc(buff_size + 1);
    fread(buff, 1, buff_size, fdd);
    fclose(fdd);
    // // //

    ytdl_sig_actions_t actions;
    ytdl_sig_actions_extract(&actions, buff, buff_size);

    free(buff);

    ytdl_info_set_sig_actions(&info, &actions);
    ytdl_info_extract_formats(&info);

    printf("%s\n", ytdl_info_get_format_url(&info, 0));

    char id[12];
    ytdl_net_get_id_from_url("https://www.youtube.com/watch?v=Y59vIAr8rAI", &id);

    puts(id);

    free(buf);
}