#ifndef YTDL_NET_H
#define YTDL_NET_H

#define YTDL_WATCH_URL_SIZE sizeof("https://www.youtube.com/watch?v=") + 11
#define YTDL_ID_SIZE 12
#define YTDL_ID_LEN 11

#include <ytdl/buffer.h>

/**
 * Not very accurate but fast way to find youtube video url
 * 
 * May match invalid urls!
 * @param url_str The URL cstring to get the ID from
 * @param id ID string to populate
 * @returns 0 if successful. -1 if invalid url and 1 if not matched.
 */
int ytdl_net_get_id_from_url (const char *url_str, char id[YTDL_ID_SIZE]);

/**
 * Get the watch url for the specified id
 * 
 * @param url URL to populate
 * @param id ID to use while populating url
 */
void ytdl_net_get_watch_url (char url[YTDL_WATCH_URL_SIZE], char id[YTDL_ID_SIZE]);

/**
 * @returns The HTTP/1.1 GET request for the `watch` page's html
 */
void ytdl_net_request_watch_html (ytdl_buf_t *buf, const char id[12]);

/**
 * @returns The HTTP/1.1 GET request for the youtube player javascript file
 */
void ytdl_net_request_player_js (ytdl_buf_t *buf, const char* player_path);

#endif