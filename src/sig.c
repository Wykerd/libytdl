#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ytdl/libregexp.h>
#include <ytdl/sig-regex.h>
#include <ytdl/buffer.h>
#include <ytdl/sig.h>

static 
int str2int(const char* str, int len)
{
    int i;
    int ret = 0;
    for(i = 0; i < len; ++i)
    {
        ret = ret * 10 + (str[i] - '0');
    }
    return ret;
}

int lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    // TODO: check for overflow?
    return 0;
}

void *lre_realloc(void *opaque, void *ptr, size_t size) 
{
    return realloc(ptr, size);
}

/* Returns 1 if match 0 if no match and -1 if error 
   DO NOT FORGET TO FREE capture */
static 
int ytdl__regex_exec (const uint8_t *re_bytecode, uint8_t *buf, size_t buf_len, 
                      int64_t last_index, uint8_t ***capture, int *capture_count) 
{
    uint8_t *str_buf;
    int shift, re_flags;
    int ret;

    re_flags = lre_get_flags(re_bytecode);
    if ((re_flags & (LRE_FLAG_GLOBAL | LRE_FLAG_STICKY)) == 0) {
        last_index = 0;
    }

    *capture_count = lre_get_capture_count(re_bytecode);
    *capture = NULL;
    if (*capture_count > 0) {
        *capture = malloc(sizeof((*capture)[0]) * (*capture_count) * 2);
        if (!*capture)
            return -1;
    }

    if (last_index > buf_len)
        goto fail;

    // TODO: check widechar
    shift = 0;

    ret = lre_exec(*capture, re_bytecode,
                   buf, last_index, buf_len,
                   shift, NULL);

    return ret;
fail:
    free(*capture);
    return -1;
}

int ytdl_sig_actions_extract (ytdl_sig_actions_t *actions, 
                              uint8_t *buf, size_t buf_len) 
{
    int result, capture_count;
    uint8_t **capture;

    // // //
    result = ytdl__regex_exec(action_obj_regexp, buf, buf_len, 0, &capture, &capture_count);

    if ((result != 1) || (capture[2 * 1] == NULL) || (capture[2 * 2] == NULL))
        goto fail;

    ytdl_buf_t obj;
    if (!ytdl_buf_alloc(&obj, capture[2 * 1 + 1] - capture[2 * 1]))
        goto fail;

    memcpy(obj.base, capture[2 * 1], obj.size);
    obj.len = obj.size;

    ytdl_buf_t obj_body;
    if (!ytdl_buf_alloc(&obj_body, capture[2 * 2 + 1] - capture[2 * 2]))
        goto fail_malloc2;

    memcpy(obj_body.base, capture[2 * 2], obj_body.size);
    obj_body.len = obj_body.size;

    free(capture);

    // // //
    result = ytdl__regex_exec(action_func_regexp, buf, buf_len, 0, &capture, &capture_count);

    if ((result != 1) || (capture[2 * 1] == NULL))
        goto fail1;

    ytdl_buf_t func_body;
    
    if (!ytdl_buf_alloc(&func_body, capture[2 * 1 + 1] - capture[2 * 1]))
        goto fail_malloc3;

    memcpy(func_body.base, capture[2 * 1], func_body.size);
    func_body.len = func_body.size;
 
    free(capture);

    size_t kstart = 0, kend = 0;

    size_t swap_s = 0,
           swap_e = 0,
           splice_s = 0,
           splice_e = 0,
           reverse_s = 0,
           reverse_e = 0,
           slice_s = 0,
           slice_e = 0;
    for (size_t idx = 0; idx < obj_body.len; idx++) {
        if (obj_body.base[idx] == ':') {
            kend = idx++;
            
            if (!strncmp("function(a,b){var c=a[0]", obj_body.base + idx, 24)) {
                swap_s = kstart;
                swap_e = kend;
            } else if (!strncmp("function(a,b){a.splice(0", obj_body.base + idx, 24)) {
                splice_s = kstart;
                splice_e = kend;
            } else if (!strncmp("function(a){a.reverse()}", obj_body.base + idx, 24)) {
                reverse_s = kstart;
                reverse_e = kend;
            } else if (!strncmp("function(a){return a.rev", obj_body.base + idx, 24)) {
                reverse_s = kstart;
                reverse_e = kend;
            } else if (!strncmp("function(a,b){return a.s", obj_body.base + idx, 24)) {
                slice_s = kstart;
                slice_e = kend;
            }
        }
        if (obj_body.base[idx] == '\n')
            kstart = ++idx;
    }

    size_t vStart = 0,
           vEnd   = 0,
           nStart = 0,
           nEnd   = 0;
    char   propType = 1,
           action_idx = 0;

    for (size_t idx = 0; idx < func_body.len; idx++) {
        if (func_body.base[idx] == '.') {
            vStart = ++idx;
            propType = 1;
        } else if (func_body.base[idx] == '(') {
            if (propType)
                vEnd = idx;
            propType = 1;
        } else if (func_body.base[idx] == '[') {
            idx++;
            vStart = ++idx;
            propType = 0;
        } else if ((func_body.base[idx] == '"') || (func_body.base[idx] == '\'')) {
            vEnd = idx;
        } else if (func_body.base[idx] == ',') {
            nStart = ++idx;
        } else if (func_body.base[idx] == ')') {
            nEnd = idx;
            size_t vlen = vEnd - vStart;
            if (action_idx == 4) {
                perror("Too many signature actions.");
                goto overflow;
            } else if (swap_e && ((swap_e - swap_s) == (vlen)) && 
                !strncmp(func_body.base + vStart, obj_body.base + swap_s, vlen)) 
            {
                actions->actions[action_idx] = YTDL_SIG_ACTION_SWAP;
                actions->actions_arg[action_idx] = str2int(func_body.base + nStart,
                                                           nEnd - nStart);
                action_idx++;
            } else if (splice_e && ((splice_e - splice_s) == (vlen)) && 
                !strncmp(func_body.base + vStart, obj_body.base + splice_s, vlen)) 
            {
                actions->actions[action_idx] = YTDL_SIG_ACTION_SPLICE;
                actions->actions_arg[action_idx] = str2int(func_body.base + nStart, 
                                                           nEnd - nStart);
                action_idx++;
            } else if (slice_e && ((slice_e - slice_s) == (vlen)) && 
                !strncmp(func_body.base + vStart, obj_body.base + slice_s, vlen)) 
            {
                actions->actions[action_idx] = YTDL_SIG_ACTION_SLICE;
                actions->actions_arg[action_idx] = str2int(func_body.base + nStart, 
                                                           nEnd - nStart);
                action_idx++;
            } else if (reverse_e && ((reverse_e - reverse_s) == (vlen)) && 
                !strncmp(func_body.base + vStart, obj_body.base + reverse_s, vlen)) 
            {
                actions->actions[action_idx] = YTDL_SIG_ACTION_REVERSE;
                actions->actions_arg[action_idx] = 0;
                action_idx++;
            }
        }
    }

    actions->actions_size = action_idx;

    ytdl_buf_free(&func_body);
    ytdl_buf_free(&obj);
    ytdl_buf_free(&obj_body);
    return 0;
overflow:
    ytdl_buf_free(&func_body);
    ytdl_buf_free(&obj);
    ytdl_buf_free(&obj_body);
    return -1;
fail1:
    ytdl_buf_free(&obj);
    ytdl_buf_free(&obj_body);

    return -1;

fail_malloc3:
    ytdl_buf_free(&obj_body);
fail_malloc2:
    ytdl_buf_free(&obj);
fail:
    free(capture);
    return -1;
}
