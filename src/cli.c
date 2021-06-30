#include <stdio.h>
#include <stdlib.h>
#include <ytdl/sig.h>
#include <ytdl/info.h>

int main () {
    FILE *fd;
    
    fd = fopen("/Users/wykerd/Documents/ytdl/player.js", "rb");

    fseek(fd, 0, SEEK_END);
    size_t buf_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);  /* same as rewind(f); */

    uint8_t *buf = malloc(buf_size + 1);
    fread(buf, 1, buf_size, fd);
    fclose(fd);

    ytdl_sig_actions_t actions;

    ytdl_sig_actions_extract(&actions, buf, buf_size);

    free(buf);

    fd = fopen("/Users/wykerd/Documents/ytdl/watch.html", "rb");

    fseek(fd, 0, SEEK_END);
    buf_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);  /* same as rewind(f); */

    buf = malloc(buf_size + 1);
    fread(buf, 1, buf_size, fd);
    fclose(fd);

    ytdl_info_ctx_t info;
    ytdl_info_extract_watch_html(&info, buf, buf_size);

    ytdl_info_set_sig_actions(&info, &actions);
    ytdl_info_extract_formats(&info);

    printf("%s\n", ytdl_info_get_format_url(&info, 0));

    free(buf);
}