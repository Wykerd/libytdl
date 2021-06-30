#include <ytdl/net.h>
#include <uriparser/Uri.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

const char *yt_hosts[6] = {
    "youtube.com",
    "www.youtube.com",
    "youtu.be",
    "m.youtube.com",
    "music.youtube.com",
    "gaming.youtube.com"
};

const char yt_valid_id_map[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
    1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void ytdl_net_get_watch_url (char url[YTDL_WATCH_URL_SIZE], char id[YTDL_ID_SIZE])
{
    sprintf(url, "%s%s", "https://www.youtube.com/watch?v=", id);
}

int ytdl_net_get_id_from_url (const char *url_str, char id[YTDL_ID_SIZE]) 
{
    UriUriA uri;
    const char * error_pos;
    UriQueryListA * queryList, **cur;
    int itemCount;

    if (uriParseSingleUriA(&uri, url_str, &error_pos) != URI_SUCCESS)
        return -1;

    for (int i = 0; i < 6; i++)
        if (!strncmp(uri.hostText.first, yt_hosts[i],
                     uri.hostText.afterLast - uri.hostText.first)) 
        {
            goto valid_host;
        }

fail:
    uriFreeUriMembersA(&uri);
    return 1;
valid_host:

    if (uri.pathTail->text.afterLast - uri.pathTail->text.first == YTDL_ID_LEN) 
    {
        for (int i = 0; i < YTDL_ID_LEN; i++) {
            if (!yt_valid_id_map[uri.pathTail->text.first[i]])
                goto invalid_path_tail;
        }

        memcpy(id, uri.pathTail->text.first, YTDL_ID_LEN);
        id[YTDL_ID_LEN] = 0;
        goto suffix_found;
    }

invalid_path_tail:
    if (uriDissectQueryMallocA(&queryList, &itemCount, uri.query.first,
                               uri.query.afterLast) != URI_SUCCESS) 
    {
        goto fail;    
    }

    cur = &queryList;
    do {
        if (((*cur)->key[0] == 'v') && ((*cur)->key[1] == 0)) {
            if (strlen((*cur)->value) == YTDL_ID_LEN) {
                for (int i = 0; i < YTDL_ID_LEN; i++) {
                    if (!yt_valid_id_map[(*cur)->value[i]])
                        goto no_option;
                    }

                    memcpy(id, (*cur)->value, YTDL_ID_LEN);
                    id[YTDL_ID_LEN] = 0;
                    goto v_param_found;
                }
        }
    } while (((*cur) = queryList->next));

no_option:
    uriFreeQueryListA(queryList);
    uriFreeUriMembersA(&uri);
    return 1;
v_param_found:
    uriFreeQueryListA(queryList);
suffix_found:
    uriFreeUriMembersA(&uri);
    return 0;
};
