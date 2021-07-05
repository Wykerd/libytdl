#ifndef YTDL_SIG_H
#define YTDL_SIG_H

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <ytdl/quickjs.h>

typedef enum ytdl_sig_action_e {
    YTDL_SIG_ACTION_SWAP = 1, // w
    YTDL_SIG_ACTION_REVERSE,  // r
    YTDL_SIG_ACTION_SLICE,    // s
    YTDL_SIG_ACTION_SPLICE    // p
} ytdl_sig_action_t;

#define YTDL_SIG_JS_FUNC                            \
    "function y (z){"                               \
    "const s=z.split('&');"                         \
    "const i=s.findIndex(e=>e.startsWith('n='));"   \
    "const n=s[i].substr(2);"                       \
    "s.splice(i,1,'n='+(%.*s)(n));"                 \
    "return s.join('&');"                           \
    "};y;"

#define YTDL_SIG_ACTIONS          \
    ytdl_sig_action_t actions[4]; \
    int actions_arg[4];           \
    int actions_size;             

typedef struct ytdl_sig_actions_head_s {
    YTDL_SIG_ACTIONS
} ytdl_sig_actions_head_t;

typedef struct ytdl_sig_actions_s {
    YTDL_SIG_ACTIONS
    uint8_t *bytecode;
    size_t bc_len;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue script;
    JSValue func;
} ytdl_sig_actions_t;

int ytdl_sig_actions_init (ytdl_sig_actions_t *actions);
void ytdl_sig_actions_free (ytdl_sig_actions_t *actions);
int ytdl_sig_actions_save_file (ytdl_sig_actions_t *actions, FILE *fd);
int ytdl_sig_actions_load_file (ytdl_sig_actions_t *actions, FILE *fd);
/**
 * Extracts the signature deciphering actions (steps) from the youtube player javascript.
 * 
 * @param actions populated by the function
 * @returns 0 on sucess and -1 for any error.
 */
int ytdl_sig_actions_extract (ytdl_sig_actions_t *actions, 
                              uint8_t *buf, size_t buf_len);

#endif