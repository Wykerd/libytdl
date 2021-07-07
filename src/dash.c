#include <string.h>
#include <libxml/tree.h>

#include <ytdl/dash.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree must be enabled"
#endif

int ytdl_dash_get_best_representation (ytdl_dash_ctx_t *ctx, xmlNode *adaptation, 
                                       xmlNode *representation, int is_video)
{
    xmlChar *attr = xmlGetProp(representation, "bandwidth");
    if (!attr) 
        return 0; 
    long long bw = atoll(attr);
    xmlFree(attr);
    if (is_video)
    {
        if (bw > ctx->v_bandwidth)
        {
            ctx->v_bandwidth = bw;
            return 1;
        };
    }
    else
    {
        if (bw > ctx->a_bandwidth)
        {
            ctx->a_bandwidth = bw;
            return 1;
        };
    }
    
    return 0;
}

static
void ytdl__dash_representation_pick (ytdl_dash_ctx_t *ctx, xmlNode *adaptation, int is_video)
{
    xmlNode *rep_node = adaptation->children;

    for (; rep_node; rep_node = rep_node->next) 
    {
        if (rep_node->type == XML_ELEMENT_NODE)
        {
            if (!strcmp(rep_node->name, "Representation"))
                if (ctx->on_pick_filter(ctx, adaptation, rep_node, is_video))
                    if (is_video)
                        ctx->v_rep = rep_node;
                    else
                        ctx->a_rep = rep_node;
        }
    }
}

int ytdl_dash_get_format (ytdl_dash_ctx_t *ctx, 
                          ytdl_dash_representation_select_cb on_pick_filter)
{
    xmlNode *root_el, *period_el;

    ctx->on_pick_filter = on_pick_filter;

    root_el = xmlDocGetRootElement(ctx->doc);

    if (strcmp(root_el->name, "MPD"))
        return -1;

    period_el = root_el->children;
    for (; period_el; period_el = period_el->next) 
        if (period_el->type == XML_ELEMENT_NODE) 
            break;

    if (strcmp(period_el->name, "Period"))
        return -1;

    xmlNode *adapt_node = period_el->children;

    // Traverse the Period for AdaptationSet elements
    for (; adapt_node; adapt_node = adapt_node->next) 
    {
        if (adapt_node->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(adapt_node->name, "AdaptationSet"))
            {
                xmlAttr *attr = adapt_node->properties;
                for (; attr; attr = attr->next) 
                {
                    if (!strcmp(attr->name, "mimeType"))
                    {
                        xmlChar *mimeType = xmlNodeListGetString(ctx->doc, attr->children, 1);
                        if (!strncmp("audio", mimeType, 5))
                        {
                            ytdl__dash_representation_pick(ctx, adapt_node, 0);
                        }
                        else if (!strncmp("video", mimeType, 5))
                        {
                            ytdl__dash_representation_pick(ctx, adapt_node, 1);
                        }
                        xmlFree(mimeType);
                    }
                }
            }
        }
    }

    if (!(ctx->v_rep && ctx->a_rep))
        return -1;

    xmlNode *rep_v = ctx->v_rep->children;
    for (; rep_v; rep_v = rep_v->next) 
    {
        if (rep_v->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(rep_v->name, "BaseURL"))
                ctx->v_base_url = xmlNodeListGetString(ctx->doc, rep_v->children, 1);
            if (!strcmp(rep_v->name, "SegmentList"))
                ctx->v_segment_list = rep_v;
        }
    }

    xmlNode *rep_a = ctx->a_rep->children;
    for (; rep_a; rep_a = rep_a->next) 
    {
        if (rep_a->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(rep_a->name, "BaseURL"))
                ctx->a_base_url = xmlNodeListGetString(ctx->doc, rep_a->children, 1);
            if (!strcmp(rep_a->name, "SegmentList"))
                ctx->a_segment_list = rep_a;
        }
    }

    if (!(ctx->a_base_url && ctx->a_segment_list && 
          ctx->v_segment_list && ctx->v_base_url))
    {
        puts("FAIL");
        return -1;
    }

    xmlNode *v_list = ctx->v_segment_list->children;
    for (; v_list; v_list = v_list->next) 
    {
        if (v_list->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(v_list->name, "Initialization"))
            {
                ctx->v_initial_segment = xmlGetProp(v_list, "sourceURL");
                ctx->v_segment_count++;
            }
            else if (!strcmp(v_list->name, "SegmentURL"))
            {
                ctx->v_segment_count++;
            }
        }
    }

    xmlNode *a_list = ctx->a_segment_list->children;
    for (; a_list; a_list = a_list->next) 
    {
        if (a_list->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(a_list->name, "Initialization"))
            {
                ctx->a_initial_segment = xmlGetProp(a_list, "sourceURL");
                ctx->a_segment_count++;
            }
            else if (!strcmp(a_list->name, "SegmentURL"))
            {
                ctx->a_segment_count++;
            }
        }
    }

    ctx->a_segment_list = ctx->a_segment_list->children;
    ctx->v_segment_list = ctx->v_segment_list->children;

    return 0;
}

char *ytdl_dash_next_video_segment (ytdl_dash_ctx_t *ctx) 
{
    if (!ctx->v_segment_path && ctx->v_initial_segment)
    {
        size_t size_path = strlen(ctx->v_initial_segment);
        ctx->v_segment_path = xmlMalloc(size_path + 1);
        memcpy(ctx->v_segment_path, ctx->v_initial_segment, size_path);
        ctx->v_segment_path[size_path] = 0;
        return ctx->v_segment_path;
    };
    if (ctx->v_segment_path)
    {
        xmlFree(ctx->v_segment_path);
        ctx->v_segment_path = NULL;
    };
    for (; ctx->v_segment_list; ctx->v_segment_list = ctx->v_segment_list->next) 
    {
        if (ctx->v_segment_list->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(ctx->v_segment_list->name, "SegmentURL"))
            {
                if (ctx->v_segment_path)
                    xmlFree(ctx->v_segment_path);
                ctx->v_segment_path = xmlGetProp(ctx->v_segment_list, "media");
                ctx->v_segment_list = ctx->v_segment_list->next;
                return ctx->v_segment_path;
            }
        }
    }
    return NULL;
}

char *ytdl_dash_next_audio_segment (ytdl_dash_ctx_t *ctx) 
{
    if (!ctx->a_segment_path && ctx->a_initial_segment)
    {
        size_t size_path = strlen(ctx->a_initial_segment);
        ctx->a_segment_path = xmlMalloc(size_path + 1);
        memcpy(ctx->a_segment_path, ctx->a_initial_segment, size_path);
        ctx->a_segment_path[size_path] = 0;
        return ctx->a_segment_path;
    };
    if (ctx->a_segment_path)
    {
        xmlFree(ctx->a_segment_path);
        ctx->a_segment_path = NULL;
    };
    for (; ctx->a_segment_list; ctx->a_segment_list = ctx->a_segment_list->next) 
    {
        if (ctx->a_segment_list->type == XML_ELEMENT_NODE) 
        {
            if (!strcmp(ctx->a_segment_list->name, "SegmentURL"))
            {
                if (ctx->a_segment_path)
                    xmlFree(ctx->a_segment_path);
                ctx->a_segment_path = xmlGetProp(ctx->a_segment_list, "media");
                ctx->a_segment_list = ctx->a_segment_list->next;
                return ctx->a_segment_path;
            }
        }
    }
    return NULL;
}

int ytdl_dash_ctx_init (ytdl_dash_ctx_t *ctx, uint8_t *buf, size_t buflen)
{
    LIBXML_TEST_VERSION

    memset(ctx, 0, sizeof(ytdl_dash_ctx_t));

    ctx->doc = xmlReadMemory(buf, buflen, "dash.mpd", NULL, 0);
    if (ctx->doc == NULL) 
        return -1;

    return 0;
}

void ytdl_dash_ctx_free (ytdl_dash_ctx_t *ctx)
{
    xmlFreeDoc(ctx->doc);

    if (ctx->a_base_url)
        xmlFree(ctx->a_base_url);
    if (ctx->a_segment_path)
        xmlFree(ctx->a_segment_path);
    if (ctx->a_initial_segment)
        xmlFree(ctx->a_initial_segment);

    if (ctx->v_base_url)
        xmlFree(ctx->v_base_url);
    if (ctx->v_segment_path)
        xmlFree(ctx->v_segment_path);
    if (ctx->v_initial_segment)
        xmlFree(ctx->v_initial_segment);
    
    xmlCleanupParser();
}

/**
int
main(int argc, char **argv)
{
    ytdl_dash_ctx_t ctx;

    ytdl_dash_ctx_init(&ctx, NULL, 0);

    ytdl_dash_get_format(&ctx, ytdl_dash_get_best_representation);

    int i = 0;
    while (ytdl_dash_next_audio_segment(&ctx))
    {
        printf("curl \"%s%s\" -o out_%d\n", ctx.a_base_url, ctx.a_segment_path, i++);
    }

    return 0;
}
*/
