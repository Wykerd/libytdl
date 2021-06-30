#ifndef YTDL_SIG_H
#define YTDL_SIG_H

#include <inttypes.h>
#include <stddef.h>

typedef enum ytdl_sig_action_e {
    YTDL_SIG_ACTION_SWAP = 1, // w
    YTDL_SIG_ACTION_REVERSE,  // r
    YTDL_SIG_ACTION_SLICE,    // s
    YTDL_SIG_ACTION_SPLICE    // p
} ytdl_sig_action_t;

typedef struct ytdl_sig_actions_s {
  ytdl_sig_action_t actions[4];
  int actions_arg[4];
  int actions_size;
} ytdl_sig_actions_t;

/**
 * Extracts the signature deciphering actions (steps) from the youtube player javascript.
 * 
 * @param actions populated by the function
 * @returns 0 on sucess and -1 for any error.
 */
int ytdl_sig_actions_extract (ytdl_sig_actions_t *actions, 
                              uint8_t *buf, size_t buf_len);

#endif