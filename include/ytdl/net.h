#ifndef YTDL_NET_H
#define YTDL_NET_H

#define YTDL_WATCH_URL_SIZE sizeof("https://www.youtube.com/watch?v=") + 11
#define YTDL_ID_SIZE 12
#define YTDL_ID_LEN 11

#define YTDL_DL_CHUNK_SIZE (1024 * 1024 * 10)
#define YTDL_DL_USER_AGENT "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.77 Safari/537.36"

#define YTDL_DL_MAX_PLAYER_PATH 150
#define YTDL_DL_PLAYER_JS_REQUEST_SIZE 94 + sizeof(YTDL_DL_USER_AGENT) + YTDL_DL_MAX_PLAYER_PATH

#define YTDL_DL_WATCH_HTML_REQUEST_SIZE 135 + sizeof(YTDL_DL_USER_AGENT) + YTDL_ID_SIZE

#include <ytdl/buffer.h>

/**
 * Sanitize filename inplace
 */
void ytdl_sanitize_filename_inplace (char *filepath);

/**
 * Not very accurate but fast way to find youtube video url
 * 
 * May match invalid urls!
 * @param url_buf The URL buffer to get the ID from
 * @param url_len The length of the url buffer
 * @param id ID string to populate
 * @returns 0 if successful. -1 if invalid url and 1 if not matched.
 */
int ytdl_net_get_id_from_url (const char *url_buf, size_t url_len, char id[YTDL_ID_SIZE]);

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
 * @returns The HTTP/1.1 GET request for the innertube json
 */
void ytdl_net_request_innertube_player (ytdl_buf_t *buf, const char id[YTDL_ID_SIZE], const char *cver);

/**
 * @returns The HTTP/1.1 GET request for the youtube player javascript file
 */
void ytdl_net_request_player_js (ytdl_buf_t *buf, const char* player_path);

/**
 * Should be used if format contains only audio or video not both. 
 * 
 * Can be checked with !(format->flags & YTDL_INFO_FORMAT_HAS_AUD) || !(format->flags & YTDL_INFO_FORMAT_HAS_VID) 
 * 
 * `end_idx - start_idx` should be `<= YTDL_DL_CHUNK_SIZE` to avoid throttling
 * @returns The HTTP/1.1 GET request for media chunk
 */
void ytdl_net_request_media_chunk (ytdl_buf_t *buf, const char* path_buf, size_t path_len, 
                                   const char* query_buf, size_t query_len, 
                                   const char* host_buf, size_t host_len, 
                                   size_t start_idx, size_t end_idx);

/**
 * Should be used if format contains both audio and video. 
 * 
 * Can be checked with (format->flags & (YTDL_INFO_FORMAT_HAS_AUD | YTDL_INFO_HAS_VID))
 * @returns The HTTP/1.1 GET request for full media
 */
void ytdl_net_request_media (ytdl_buf_t *buf, const char* path_buf, size_t path_len, 
                             const char* query_buf, size_t query_len, 
                             const char* host_buf, size_t host_len);

#endif