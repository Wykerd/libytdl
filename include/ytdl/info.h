#ifndef YTDL_INFO_H
#define YTDL_INFO_H

#include <inttypes.h>
#include <stddef.h>
#include <ytdl/buffer.h>
#include <ytdl/yyjson.h>
#include <ytdl/sig.h>

typedef struct ytdl_info_format_s {
    yyjson_val *val;
    char *url;
} ytdl_info_format_t;

typedef struct ytdl_info_ctx_s {
    char cver[20];
    char player_url[150];
    
    yyjson_doc *init_pr_doc;
    yyjson_doc *init_d_doc;
    yyjson_doc *watch_doc;
    yyjson_val *player_response;
    yyjson_val *response;

    ytdl_info_format_t **formats;
    size_t formats_size;

    ytdl_sig_actions_t *sig_actions;
} ytdl_info_ctx_t;

int ytdl_info_extract_watch_html (ytdl_info_ctx_t *info, 
                                  const uint8_t *buf, size_t buf_len);

int ytdl_info_extract_formats (ytdl_info_ctx_t *info);

void ytdl_info_set_sig_actions (ytdl_info_ctx_t *info, ytdl_sig_actions_t *sig_actions);

char *ytdl_info_get_format_url (ytdl_info_ctx_t *info, size_t idx);

#endif