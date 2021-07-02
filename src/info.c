#include <ytdl/info.h>

#include <ctype.h>
#include <string.h>
#include <stdio.h>

char * strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

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

    char *player_response_s = strnstr((const char *)buf, " ytInitialPlayerResponse = ", buf_len) + 27;
    size_t player_response_idx = (const uint8_t *)player_response_s - buf;
    char *player_response_e_1 = strnstr(player_response_s, ";</script>", 
                                        buf_len - player_response_idx);
    char *player_response_e_2 = strnstr(player_response_s, ";var ", 
                                        buf_len - player_response_idx);

    char *player_response_e = player_response_e_1 < player_response_e_2 ? player_response_e_1 : player_response_e_2;

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

/**
 * Populates the data found in playerReponse json all at once
 */
static void ytdl__populate_pr_dat (ytdl_info_ctx_t *info) 
{
    if (info->is_pr_populated)
        return;
    
    yyjson_val *key, *val;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(info->player_response, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        if (yyjson_equals_str(key, "playabilityStatus"))
            info->ps = val;
        else if (yyjson_equals_str(key, "streamingData"))
            info->sd = val;
    }

    info->is_pr_populated = 1;
}

ytdl_info_playability_status_t ytdl_info_get_playability_status (ytdl_info_ctx_t *info)
{
    if (info->playability_status)
        return info->playability_status;
    
    if (!info->player_response)
        return YTDL_PLAYABILITY_UNKNOWN;

    ytdl__populate_pr_dat(info);

    if (!info->ps)
        return YTDL_PLAYABILITY_UNKNOWN;

    yyjson_val *stat = yyjson_obj_get(info->ps, "status");

    if (!stat) {
        info->playability_status = YTDL_PLAYABILITY_UNKNOWN;
        return info->playability_status;
    }

    info->playability_status = 
        yyjson_equals_str(stat, "OK") ? YTDL_PLAYABILITY_OK :
        yyjson_equals_str(stat, "LOGIN_REQUIRED") ? YTDL_PLAYABILITY_LOGIN_REQUIRED :
        yyjson_equals_str(stat, "UNPLAYABLE") ? YTDL_PLAYABILITY_UNPLAYABLE :
        YTDL_PLAYABILITY_UNKNOWN;

    return info->playability_status;
}

const char *ytdl_info_get_playability_status_message (ytdl_info_ctx_t *info) 
{
    if (info->ps_message)
        return info->ps_message;
    
    if (!info->player_response)
        return NULL;

    
    ytdl__populate_pr_dat(info);

    if (!info->ps)
        return NULL;

    yyjson_val *messages = yyjson_obj_get(info->ps, "messages");

    if (!messages)
        return NULL;

    if (!yyjson_arr_size(messages))
        return NULL;

    return yyjson_get_str(yyjson_arr_get_first(messages));
}

int ytdl_info_extract_formats (ytdl_info_ctx_t *info) 
{
    if (!info->player_response)
        return -1;

    ytdl__populate_pr_dat(info);

    if (info->sd == NULL)
        return -1;

    if (!info->sd_formats || !info->sd_adaptive_formats) 
    {
        yyjson_val *key, *val;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(info->sd, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            val = yyjson_obj_iter_get_val(key);
            if (yyjson_equals_str(key, "formats"))
                info->sd_formats = val;
            else if (yyjson_equals_str(key, "adaptiveFormats"))
                info->sd_adaptive_formats = val;
        }
    }

    if (!info->sd_formats || !info->sd_adaptive_formats)
        return -1;

    size_t adaptive_offset = yyjson_arr_size(info->sd_formats);
    info->formats_size = adaptive_offset + yyjson_arr_size(info->sd_adaptive_formats);

    if (info->formats_size == 0)
        return -1;

    info->formats = malloc(sizeof(ytdl_info_format_t*) * info->formats_size);

    if (!info->formats)
        return 1;

    size_t idx, max;
    yyjson_val *val;
    yyjson_arr_foreach(info->sd_formats, idx, max, val) {
        info->formats[idx] = calloc(1, sizeof(ytdl_info_format_t));

        if (!info->formats[idx])
            return 1;

        info->formats[idx]->url = NULL;
        info->formats[idx]->val = val;
    }

    yyjson_arr_foreach(info->sd_adaptive_formats, idx, max, val) {
        info->formats[adaptive_offset + idx] = calloc(1, sizeof(ytdl_info_format_t));

        if (!info->formats[adaptive_offset + idx])
            return 1;

        info->formats[adaptive_offset + idx]->url = NULL;
        info->formats[adaptive_offset + idx]->val = val;
    }

    // TODO: hls & dash streams 

    return 0;
}

void ytdl_info_set_sig_actions (ytdl_info_ctx_t *info, ytdl_sig_actions_t *sig_actions) 
{
    info->sig_actions = sig_actions;
}

static void ytdl__info_format_populate (ytdl_info_ctx_t *info, size_t i) 
{
    if (info->formats[i]->flags & YTDL_INFO_FORMAT_POPULATED)
        return;
    
    yyjson_val *key, *val;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(info->formats[i]->val, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        if (yyjson_equals_str(key, "itag"))
            info->formats[i]->itag = yyjson_get_int(val);
        else if (yyjson_equals_str(key, "url"))
            info->formats[i]->url_untouched = yyjson_get_str(val);
        else if (yyjson_equals_str(key, "mimeType"))
            info->formats[i]->mime_type = yyjson_get_str(val);
        else if (yyjson_equals_str(key, "signatureCipher") || yyjson_equals_str(key, "cipher"))
            info->formats[i]->cipher = yyjson_get_str(val);
        else if (yyjson_equals_str(key, "bitrate"))
            info->formats[i]->bitrate = yyjson_get_int(val);
        else if (yyjson_equals_str(key, "width"))
        {
            info->formats[i]->flags |= YTDL_INFO_FORMAT_HAS_VID;
            info->formats[i]->width = yyjson_get_int(val);
        }
        else if (yyjson_equals_str(key, "height"))
            info->formats[i]->height = yyjson_get_int(val);
        else if (yyjson_equals_str(key, "contentLength"))
            info->formats[i]->content_length = atoll(yyjson_get_str(val));
        else if (yyjson_equals_str(key, "quality"))
            info->formats[i]->quality = yyjson_get_str(val);
        else if (yyjson_equals_str(key, "qualityLabel"))
            info->formats[i]->quality_label = yyjson_get_str(val);
        else if (yyjson_equals_str(key, "fps"))
            info->formats[i]->fps = yyjson_get_int(val);
        else if (yyjson_equals_str(key, "averageBitrate"))
            info->formats[i]->average_bitrate = yyjson_get_int(val);
        else if (yyjson_equals_str(key, "audioChannels"))
        {
            info->formats[i]->flags |= YTDL_INFO_FORMAT_HAS_AUD;
            info->formats[i]->audio_channels = yyjson_get_int(val);
        }
        else if (yyjson_equals_str(key, "approxDurationMs"))
            info->formats[i]->approx_duration_ms = atoll(yyjson_get_str(val));
        else if (yyjson_equals_str(key, "audioQuality"))
        {
            info->formats[i]->audio_quality = 
                yyjson_get_str(val)[yyjson_get_len(val) - 1] == 'W' ? 
                    YTLD_INFO_AUDIO_QUALITY_LOW : 
                    YTLD_INFO_AUDIO_QUALITY_MEDIUM;
        }
    }

    info->formats[i]->flags |= YTDL_INFO_FORMAT_POPULATED;
}

static void ytdl__info_format_populate_all(ytdl_info_ctx_t *info)
{
    if (info->is_fmt_populated)
        return;

    for (size_t i = 0; i < info->formats_size; i++)
        ytdl__info_format_populate(info, i);

    info->is_fmt_populated = 1;
}

size_t ytdl_info_get_best_video_format (ytdl_info_ctx_t *info)
{
    ytdl__info_format_populate_all(info);
    size_t idx = 0;
    int score = 0;
    for (size_t i = 0; i < info->formats_size; i++)
    {
        if (!(info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID))
            continue;

        int a_score = !!((info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD) & (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID));
            a_score = a_score ? a_score : (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID) ? 2 : 0;

        a_score += info->formats[i]->width + info->formats[i]->fps + info->formats[i]->bitrate;

        if (a_score > score) 
        {
            idx = i;
            score = a_score;
        }
    };

    return idx;
}

size_t ytdl_info_get_best_audio_format (ytdl_info_ctx_t *info)
{
    ytdl__info_format_populate_all(info);
    size_t idx = 0;
    int score = 0;
    for (size_t i = 0; i < info->formats_size; i++)
    {
        if (!(info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD))
            continue;
        
        int a_score = !!((info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD) & (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID));
            a_score = a_score ? a_score : (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID) ? 2 : 0;

        if (a_score)
            a_score += info->formats[i]->width + info->formats[i]->fps + info->formats[i]->bitrate;
        else
            a_score -= (info->formats[i]->bitrate * info->formats[i]->audio_channels) / info->formats[i]->audio_quality;

        if (a_score < score)
        {
            score = a_score;
            idx = i;
        }
    };

    return idx;
}

/**
 * decode a percent-encoded C string with optional path normalization
 *
 * The buffer pointed to by @dst must be at least strlen(@src) bytes.
 * Decoding stops at the first character from @src that decodes to null.
 *
 * @param dst       destination buffer
 * @param src       source buffer
 * @return          number of valid characters in @dst
 * @author          Johan Lindh <johan@linkdata.se>
 * @legalese        BSD licensed (http://opensource.org/licenses/BSD-2-Clause)
 */
ptrdiff_t urldecode(char* dst, const char* src)
{
    char* org_dst = dst;
    char ch, a, b;
    do {
        ch = *src++;
        if (ch == '%' && isxdigit(a = src[0]) && isxdigit(b = src[1])) {
            if (a < 'A') a -= '0';
            else if(a < 'a') a -= 'A' - 10;
            else a -= 'a' - 10;
            if (b < 'A') b -= '0';
            else if(b < 'a') b -= 'A' - 10;
            else b -= 'a' - 10;
            ch = 16 * a + b;
            src += 2;
        }
        *dst++ = ch;
    } while(ch);
    return (dst - org_dst) - 1;
}

char *ytdl_info_get_format_url (ytdl_info_ctx_t *info, size_t idx) 
{
    if (info->formats[idx]->url)
        return info->formats[idx]->url;

    ytdl__info_format_populate(info, idx);

    if (info->formats[idx]->cipher) {
        char *cipher = strdup(info->formats[idx]->cipher);

        char *cipher_end = cipher + strlen(cipher);

        //
        char *sig_start = strstr(cipher, "s=");
        if (!sig_start)
        {
            free(cipher);
            return NULL;
        }
        sig_start += 2;

        char *sig_end = strstr(sig_start, "&");
        if (!sig_end)
            sig_end = cipher_end;
        
        //
        char *sp_start = strstr(cipher, "sp=");
        if (!sp_start)
        {
            free(cipher);
            return NULL;
        }
        sp_start += 3;

        char *sp_end = strstr(sp_start, "&");
        if (!sp_end)
            sp_end = cipher_end;
        
        //
        char *url_start = strstr(cipher, "url=");
        if (!url_start)
        {
            free(cipher);
            return NULL;
        }
        url_start += 4;

        char *url_end = strstr(url_start, "&");
        if (!url_end)
            url_end = cipher_end;

        *sig_end = 0;
        *sp_end = 0;
        *url_end = 0;

        char *sig = calloc(sig_end - sig_start + 1, 1), 
             *sp  = calloc(sp_end - sp_start + 1, 1), 
             *url = calloc(url_end - url_start + 1, 1);

        urldecode(sig, sig_start);
        urldecode(sp, sp_start);
        urldecode(url, url_start);
        
        free(cipher);

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
    } 
    else if (info->formats[idx]->url_untouched) 
    {
        info->formats[idx]->url = calloc(strlen(info->formats[idx]->url_untouched) + 1, 1);

        urldecode(info->formats[idx]->url, info->formats[idx]->url_untouched);
    }
    else return NULL;

    // TODO: check live dash and hls support



    return info->formats[idx]->url;
} 