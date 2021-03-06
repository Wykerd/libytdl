#ifndef YTDL_INFO_H
#define YTDL_INFO_H

#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ytdl/buffer.h>
#include <ytdl/yyjson.h>
#include <ytdl/sig.h>

#ifdef __cplusplus
extern "C" {
#endif
#define YTDL_INFO_FORMAT_POPULATED      (1)
#define YTDL_INFO_FORMAT_HAS_VID        (1 << 1)
#define YTDL_INFO_FORMAT_HAS_AUD        (1 << 2)
#define YTDL_INFO_FORMAT_IS_DASH        (1 << 3)
#define YTDL_INFO_FORMAT_IS_HLS         (1 << 4)
#define YTDL_INFO_FORMAT_IS_LIVE        (1 << 5)

#define YTLD_INFO_AUDIO_QUALITY_LOW     2
#define YTLD_INFO_AUDIO_QUALITY_MEDIUM  1

typedef struct ytdl_info_format_s {
    yyjson_val *val;
    size_t flags;
    char *url;

    int itag;
    const char *url_untouched;
    const char *cipher;
    const char *mime_type;
    int bitrate;//
    int width;//
    int height;
    long long content_length;
    const char *quality;
    const char *quality_label;
    int fps;//
    int average_bitrate;
    int audio_channels; //
    int audio_quality;//
    long long approx_duration_ms;
} ytdl_info_format_t;

typedef enum ytdl_info_playability_status_e {
    YTDL_PLAYABILITY_OK,
    YTDL_PLAYABILITY_UNKNOWN,
    YTDL_PLAYABILITY_LOGIN_REQUIRED,
    YTDL_PLAYABILITY_LIVE_STREAM_OFFLINE,
    YTDL_PLAYABILITY_UNPLAYABLE
} ytdl_info_playability_status_t;

typedef struct ytdl_info_ctx_s {
    // // //
    // Formats
    // // // 
    ytdl_info_format_t **formats;
    size_t formats_size;

    ytdl_info_playability_status_t playability_status;
    char *ps_message;

    // // //
    // Video details
    // // //
    const char *title;
    const char *length_seconds;
    const char *channel_id;
    const char *short_description;
    double average_rating;
    const char *view_count;
    const char *author;

    // // //
    // JS Internals
    // // //
    yyjson_doc *init_pr_doc;
    yyjson_doc *init_d_doc;

    yyjson_val *player_response;
    yyjson_val *response;
    yyjson_val *video_details;

    /// playability status
    yyjson_val *ps;

    /// streaming data
    yyjson_val *sd;
    yyjson_val *sd_formats;
    yyjson_val *sd_adaptive_formats;
    const char *dash_manifest_url;

    int is_pr_populated;
    int is_fmt_populated;
    int is_details_populated;

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
 * Get the playability status of the video.
 * 
 * Call this function before attemting to extract the formats to check that the video
 * can be played
 * @returns The status `YTDL_PLAYABILITY_OK` means the video can be downloaded. 
 * Else call `ytdl_info_get_playability_status_message` for a reason why not
 */
ytdl_info_playability_status_t ytdl_info_get_playability_status (ytdl_info_ctx_t *info);

/**
 * @returns The status message in case of a non YTDL_PLAYABILITY_OK status code
 */
const char *ytdl_info_get_playability_status_message (ytdl_info_ctx_t *info);

/**
 * Loops through the JSON nodes and populates the video info into the context.
 */
void ytdl_info_extract_video_details (ytdl_info_ctx_t *info);

/**
 * Extract the formats from the data acquired from ytdl_info_extract_watch_html
 * 
 * Call this function after populating the context with ytdl_info_extract_watch_html
 * @param info the context
 * @returns 0 on success. -1 if data is invalid in context. 1 if malloc failed.
 */
int ytdl_info_extract_formats (ytdl_info_ctx_t *info);

/**
 * Populates the data fields of format
 */
void ytdl_info_format_populate (ytdl_info_format_t *format);

/**
 * Use the specified sig actions to decipher the formats of the context
 * 
 * @param info the context to which this applies
 * @param sig_actions actions required to decipher code. 
 * This value must be freed AFTER disposing of the info context
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
 * @param format The format to get
 * @returns The deciphered stream url. NULL if error.
 * The value is freed along with the info. 
 */ 
char *ytdl_info_get_format_url2 (ytdl_info_ctx_t *info, ytdl_info_format_t *format);

/**
 * Deciphers the format url and returns it
 * 
 * @param info Info context
 * @param idx The index of the format to get
 * @returns The deciphered stream url. NULL if error.
 * The value is freed along with the info. 
 */ 
#define ytdl_info_get_format_url(info, idx) ytdl_info_get_format_url2((info), (info)->formats[idx])

/**
 * @returns The index of the best audio format
 */
size_t ytdl_info_get_best_audio_format (ytdl_info_ctx_t *info);

/**
 * @returns The index of the best video format
 */
size_t ytdl_info_get_best_video_format (ytdl_info_ctx_t *info);

/**
 * Get the path for the video player
 * @param info the ytdl_info_ctx_t*
 */
#define ytdl_info_get_player_url(info) (info)->player_url

#ifdef __cplusplus
}
#endif
#endif