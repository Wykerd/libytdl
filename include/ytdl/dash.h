#ifndef YTDL_DASH_H
#define YTDL_DASH_H
#include <libxml/parser.h>

typedef struct ytdl_dash_ctx_s ytdl_dash_ctx_t;
typedef int (*ytdl_dash_representation_select_cb) (ytdl_dash_ctx_t *ctx, xmlNode *adaptation, 
                                                   xmlNode *representation, int is_video);

struct ytdl_dash_ctx_s {
    xmlDoc *doc;
    
    xmlNode *a_rep;
    long long a_bandwidth;
    xmlChar *a_base_url;
    xmlChar *a_segment_path;
    xmlChar *a_initial_segment;
    xmlNode *a_segment_list;
    size_t a_chunk_count;

    xmlNode *v_rep;
    long long v_bandwidth;
    xmlChar *v_base_url;
    xmlChar *v_segment_path;
    xmlChar *v_initial_segment;
    xmlNode *v_segment_list;
    size_t v_chunk_count;

    ytdl_dash_representation_select_cb on_pick_filter;
};

int ytdl_dash_get_best_representation (ytdl_dash_ctx_t *ctx, xmlNode *adaptation, 
                                       xmlNode *representation, int is_video);

int ytdl_dash_get_format (ytdl_dash_ctx_t *ctx, 
                          ytdl_dash_representation_select_cb on_pick_filter);

char *ytdl_dash_next_video_segment (ytdl_dash_ctx_t *ctx);

char *ytdl_dash_next_audio_segment (ytdl_dash_ctx_t *ctx);

int ytdl_dash_ctx_init (ytdl_dash_ctx_t *ctx, uint8_t *buf, size_t buflen);

void ytdl_dash_ctx_free (ytdl_dash_ctx_t *ctx);

#endif
