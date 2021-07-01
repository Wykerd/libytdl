#ifndef YTDL_INFO_H
#define YTDL_INFO_H

#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ytdl/buffer.h>
#include <ytdl/yyjson.h>
#include <ytdl/sig.h>

#define YTDL_INFO_FORMAT_HAS_VID        (1)
#define YTDL_INFO_FORMAT_HAS_AUD        (1 << 1)
#define YTDL_INFO_FORMAT_IS_DASH        (1 << 2)
#define YTDL_INFO_FORMAT_IS_HLS         (1 << 3)
#define YTDL_INFO_FORMAT_IS_LIVE        (1 << 4)

#define YTLD_INFO_AUDIO_QUALITY_LOW     2
#define YTLD_INFO_AUDIO_QUALITY_MEDIUM  1

typedef struct ytdl_info_format_s {
    yyjson_val *val;
    char *url;
    char *mime_type;
    size_t content_length;
 
    int width;//
    int height;
    int bitrate;//
    int fps;//

    char *quality;
    char *quality_label;
    
    int audio_channels; //
    int audio_quality;//

    size_t approx_duration_ms;
    size_t flags;
} ytdl_info_format_t;

typedef struct ytdl_info_ctx_s {
    // // //
    // Formats
    // // // 
    ytdl_info_format_t **formats;
    size_t formats_size;

    // // //
    // JSON Internals
    // // //
    yyjson_doc *init_pr_doc;
    yyjson_doc *init_d_doc;
    yyjson_doc *watch_doc;
    yyjson_val *player_response;
    yyjson_val *response;

    // // //
    // Decipher parameters
    // // //
    char cver[20];
    char player_url[150];
    ytdl_sig_actions_t *sig_actions;
} ytdl_info_ctx_t;

/**
 * Initialize the info context
 * 
 * @param info ytdl_info_ctx_t pointer
 */
#define ytdl_info_ctx_init(info) memset(info, 0, sizeof(ytdl_info_ctx_t))

/**
 * Extracts relevant information from the video's watch page
 * 
 * Finds the `player_response` and `response` json inside the document and parses it
 * @param info the info context. must be initialized with ytdl_info_ctx_init
 * @param buf the content of the webpage `https://youtube.com/watch?v=<id>`
 * @param buf_len length of the contents of `buf`
 * @returns 0 if success. -1 if failed to get `player_response`. 1 if fail to get `response`
 */
int ytdl_info_extract_watch_html (ytdl_info_ctx_t *info, 
                                  const uint8_t *buf, size_t buf_len);

/**
 * Extract the formats from the data acquired from ytdl_info_extract_watch_html
 * 
 * Call this function after populating the context with ytdl_info_extract_watch_html
 * @param info the context
 * @returns 0 on success. -1 if data is invalid in context. 1 if malloc failed.
 */
int ytdl_info_extract_formats (ytdl_info_ctx_t *info);

/**
 * Use the specified sig actions to decipher the formats of the context
 * 
 * @param info the context to which this applies
 * @param sig_actions actions required to decipher code. This value must be freed AFTER disposing of the info context
 */
void ytdl_info_set_sig_actions (ytdl_info_ctx_t *info, ytdl_sig_actions_t *sig_actions);

/**
 * Free memory allocated in info context
 * 
 * @param info the context to destroy
 */
void ytdl_info_ctx_free (ytdl_info_ctx_t *info);

/**
 * Deciphers the format url and returns it
 * 
 * @param info Info context
 * @param idx The index of the format to get
 * @returns The deciphered stream url. The value is freed along with the info. NULL if error
 */ 
char *ytdl_info_get_format_url (ytdl_info_ctx_t *info, size_t idx);

void ytdl_info_sort_formats (ytdl_info_ctx_t *info);

/**
 * Get the path for the video player
 * @param info the ytdl_info_ctx_t*
 */
#define ytdl_info_get_player_url(info) (info)->player_url

#endif