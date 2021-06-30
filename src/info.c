#include <ytdl/info.h>

#include <string.h>
#include <stdio.h>
#include <uriparser/Uri.h>

static 
char *ytdl__strrev(char *str)
{
    if (!str || ! *str)
        return str;

    int i = strlen(str) - 1, j = 0;

    char ch;
    while (i > j)
    {
        ch = str[i];
        str[i] = str[j];
        str[j] = ch;
        i--;
        j++;
    }
    return str;
}

void ytdl_info_ctx_free (ytdl_info_ctx_t *info) 
{
    if (info->init_pr_doc)
        yyjson_doc_free(info->init_pr_doc);
    
    if (info->init_d_doc)
        yyjson_doc_free(info->init_d_doc);

    if (info->watch_doc)
        yyjson_doc_free(info->watch_doc);

    if (info->formats) {
        for (size_t i = 0; i < info->formats_size; i++) 
        {
            if (info->formats[i]->url)
                free(info->formats[i]->url);
            free(info->formats[i]);
        }
        free(info->formats);
    }
}

int ytdl_info_extract_watch_html (ytdl_info_ctx_t *info, 
                                  const uint8_t *buf, size_t buf_len) 
{
    char *pos = strnstr((const char*)buf, "{\"key\":\"cver\",\"value\":\"", buf_len) + 23;

    for (char i = 0; i < 20; i++) {
        if (pos[i] == '"') {
            info->cver[i] = 0;
            break;
        }
        info->cver[i] = pos[i];
    };

    pos = strnstr((const char*)buf, "\"jsUrl\":\"", buf_len) + 9;

    for (int i = 0; i < 150; i++) {
        if (pos[i] == '"') {
            info->player_url[i] = 0;
            break;
        }
        info->player_url[i] = pos[i];
    };

    printf("JS Player path: %s\n", info->player_url);

    char *player_response_s = strnstr((const char *)buf, " ytInitialPlayerResponse = ", buf_len) + 27;
    size_t player_response_idx = (const uint8_t *)player_response_s - buf;
    char *player_response_e = strnstr(player_response_s, ";</script>", 
                                      buf_len - player_response_idx);

    info->init_pr_doc = yyjson_read(player_response_s, 
                                    player_response_e - player_response_s, 0);

    info->player_response = yyjson_doc_get_root(info->init_pr_doc);

    if (!info->init_pr_doc)
        return -1;

    char *response_s = strnstr((const char*)buf, " ytInitialData = ", buf_len) + 18;
    size_t response_idx = (const uint8_t *)response_s - buf;
    char *response_e = strnstr(response_s, ";</script>", 
                                      buf_len - response_idx);

    info->init_d_doc = yyjson_read(player_response_s, 
                                   player_response_e - player_response_s, 0);

    info->response = yyjson_doc_get_root(info->init_d_doc);

    if (!info->init_d_doc)
        return 1;

    return 0;
}

int ytdl_info_extract_formats (ytdl_info_ctx_t *info) 
{
    if (!info->player_response)
        return -1;

    yyjson_val *streaming_data = yyjson_obj_get(info->player_response, "streamingData");
    yyjson_val *formats = yyjson_obj_get(streaming_data, "formats");

    info->formats_size = yyjson_arr_size(formats);

    if (info->formats_size == 0)
        return -1;

    info->formats = malloc(sizeof(ytdl_info_format_t*) * info->formats_size);

    if (!info->formats)
        return 1;

    size_t idx, max;
    yyjson_val *val;
    yyjson_arr_foreach(formats, idx, max, val) {
        info->formats[idx] = malloc(sizeof(ytdl_info_format_t));

        if (!info->formats[idx])
            return 1;

        info->formats[idx]->url = NULL;
        info->formats[idx]->val = val;
    }

    return 0;
}

void ytdl_info_set_sig_actions (ytdl_info_ctx_t *info, ytdl_sig_actions_t *sig_actions) 
{
    info->sig_actions = sig_actions;
}

char *ytdl_info_get_format_url (ytdl_info_ctx_t *info, size_t idx) 
{
    if (info->formats[idx]->url)
        return info->formats[idx]->url;

    yyjson_val *cipher_val;
    
    cipher_val = yyjson_obj_get(info->formats[idx]->val, "signatureCipher");
    cipher_val = cipher_val ? cipher_val : yyjson_obj_get(info->formats[idx]->val, "cipher");

    if (cipher_val) {
        UriQueryListA *querylist, **cur;
        int item_count;

        const char *cipher = yyjson_get_str(cipher_val);

        if (uriDissectQueryMallocA(&querylist, &item_count, cipher, 
                                   cipher + strlen(cipher)) != URI_SUCCESS) 
        {
            return NULL;
        }

        char *sig = NULL, 
             *sp  = NULL, 
             *url = NULL;
        cur = &querylist;
        do {
            if ((*cur)->key[0] == 's') {
                if ((*cur)->key[1] == 'p') {
                    sp = strdup((*cur)->value);
                    uriUnescapeInPlaceA(sp);
                } else {
                    sig = strdup((*cur)->value);
                    uriUnescapeInPlaceA(sig);
                }
            } else if ((*cur)->key[0] == 'u' && (*cur)->key[1] == 'r') {
                url = strdup((*cur)->value);
                uriUnescapeInPlaceA(url);
            } 
        } while (((*cur) = querylist->next));

        if (!sig || !url) {
            uriFreeQueryListA(querylist);
            if (sig)
                free(sig);
            if (url)
                free(url);
            if (sp)
                free(sp);
            return NULL;
        }
        
        for (size_t i = 0; i < info->sig_actions->actions_size; i++)
        {
            switch (info->sig_actions->actions[i])
            {
            case YTDL_SIG_ACTION_REVERSE:
                {
                    ytdl__strrev(sig);
                }
                break;
            
            case YTDL_SIG_ACTION_SWAP:
                {
                    char first = sig[0];
                    sig[0] = sig[info->sig_actions->actions_arg[i] % strlen(sig)];
                    sig[info->sig_actions->actions_arg[i]] = first;
                }
                break;

            case YTDL_SIG_ACTION_SPLICE:
            case YTDL_SIG_ACTION_SLICE:
                {
                    size_t sig_len = strlen(sig) - info->sig_actions->actions_arg[i] + 1;

                    for (size_t x = 0; x < sig_len; x++) {
                        sig[x] = sig[info->sig_actions->actions_arg[i] + x];
                    }
                }
                break;

            default:
                perror("Unexpected signature action.");
                break;
            }
        }

        size_t url_len = strlen(url);
        if (sp)
            url_len += strlen(sp) + 2;
        else 
            url_len += sizeof("signature") + 2;
        
        url_len += strlen(sig); 

        url = realloc(url, url_len + 2);

        sprintf(url, "%s?%s=%s", url, sp ? sp : "signature", sig);

        info->formats[idx]->url = url;

        if (sig)
            free(sig);
        if (sp)
            free(sp);

        uriFreeQueryListA(querylist);
    } else { 
        info->formats[idx]->url = strdup(
            yyjson_get_str(
                yyjson_obj_get(info->formats[idx]->val, "url")
            )
        );

        uriUnescapeInPlaceA(info->formats[idx]->url);
    }

    return info->formats[idx]->url;
} 