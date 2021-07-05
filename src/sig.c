#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ytdl/libregexp.h>
#include <ytdl/sig-regex.h>
#include <ytdl/buffer.h>
#include <ytdl/sig.h>
#include <ytdl/cutils.h>
#include <ytdl/quickjs.h>

static void js_dump_obj(JSContext *ctx, FILE *f, JSValueConst val)
{
    const char *str;
    
    str = JS_ToCString(ctx, val);
    if (str) {
        fprintf(f, "%s\n", str);
        JS_FreeCString(ctx, str);
    } else {
        fprintf(f, "[exception]\n");
    }
}

static void js_std_dump_error1(JSContext *ctx, JSValueConst exception_val)
{
    JSValue val;
    BOOL is_error;
    
    is_error = JS_IsError(ctx, exception_val);
    js_dump_obj(ctx, stderr, exception_val);
    if (is_error) {
        val = JS_GetPropertyStr(ctx, exception_val, "stack");
        if (!JS_IsUndefined(val)) {
            js_dump_obj(ctx, stderr, val);
        }
        JS_FreeValue(ctx, val);
    }
}

static void js_std_dump_error(JSContext *ctx)
{
    JSValue exception_val;
    
    exception_val = JS_GetException(ctx);
    js_std_dump_error1(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
}

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
    if (actions->bytecode)
        return -1;

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

    uint8_t *var_start = strnstr(buf, ".get(\"n\"))&&(b=", buf_len);
    if (!var_start)
        return -1;
    var_start += 15;

    uint8_t *var_end = strnstr(var_start, "(", buf_len - (var_start - buf));
    if (!var_end)
        return -1;

    char func_prefix[10] = {0};

    snprintf(func_prefix, 10, "%.*s=", var_end - var_start, var_start);

    uint8_t *func_start = strnstr(buf, func_prefix, buf_len);
    if (!func_start)
        return -1;
    func_start += strlen(func_prefix);

    uint8_t *func_end = strnstr(func_start, ".join(\"\")}", buf_len - (func_start - buf));
    if (!func_end)
        return -1;
    func_end += 10;
    
    size_t js_func_size = sizeof(YTDL_SIG_JS_FUNC) + (func_end - func_start);
    char *js_func = malloc(js_func_size);

    snprintf(js_func, js_func_size, YTDL_SIG_JS_FUNC, func_end - func_start, func_start);

    actions->script = JS_Eval(actions->ctx, js_func, strlen(js_func), "<sig-eval>", 
                            JS_EVAL_FLAG_COMPILE_ONLY | JS_EVAL_TYPE_GLOBAL);

    actions->bytecode = JS_WriteObject(actions->ctx, &actions->bc_len, actions->script, JS_WRITE_OBJ_BYTECODE);
    if (!actions->bytecode)
        return -1;

    free(js_func);

    if (JS_IsException(actions->script))
    {
        js_std_dump_error(actions->ctx);
        return -1;
    }

    actions->func = JS_EvalFunction(actions->ctx, actions->script);

    if (JS_IsException(actions->func))
    {
        js_std_dump_error(actions->ctx);
        return -1;
    }

    if (!JS_IsFunction(actions->ctx, actions->func))
    {
        return -1;
    }

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

int ytdl_sig_actions_init (ytdl_sig_actions_t *actions)
{
    memset(actions, 0, sizeof(ytdl_sig_actions_t));
    actions->rt = JS_NewRuntime();
    if (!actions->rt)
        return -1;
    actions->ctx = JS_NewContext(actions->rt);
    if (!actions->ctx)
        return -1;
    actions->script = JS_UNDEFINED;
    actions->func = JS_UNDEFINED;
    actions->bytecode = NULL;
}

void ytdl_sig_actions_free (ytdl_sig_actions_t *actions)
{
    if (!(JS_IsUndefined(actions->func) || JS_IsException(actions->func)))
        JS_FreeValue(actions->ctx, actions->func);

    if (actions->bytecode)
        js_free(actions->ctx, actions->bytecode);

    JS_FreeContext(actions->ctx);
    JS_FreeRuntime(actions->rt);
}

int ytdl_sig_actions_save_file (ytdl_sig_actions_t *actions, FILE *fd)
{

    fwrite(actions, sizeof(ytdl_sig_actions_head_t), 1, fd);
    fputc(sizeof(size_t), fd);
    fwrite(&actions->bc_len, sizeof(size_t), 1, fd);
    fwrite(actions->bytecode, sizeof(uint8_t), actions->bc_len, fd);

    return 0;
}

int ytdl_sig_actions_load_file (ytdl_sig_actions_t *actions, FILE *fd)
{
    if (actions->bytecode)
        return -1;

    int s_size;
    fread(actions, sizeof(ytdl_sig_actions_head_t), 1, fd);
    s_size = fgetc(fd);
    fread(&actions->bc_len, s_size, 1, fd);
    actions->bytecode = js_malloc(actions->ctx, actions->bc_len);
    if (!actions->bytecode)
        return -1;
    fread(actions->bytecode, sizeof(uint8_t), actions->bc_len, fd);

    actions->script = JS_ReadObject(actions->ctx, actions->bytecode, actions->bc_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(actions->script))
    {
        js_std_dump_error(actions->ctx);
        return -1;
    }
    actions->func = JS_EvalFunction(actions->ctx, actions->script);
    if (JS_IsException(actions->func))
    {
        js_std_dump_error(actions->ctx);
        return -1;
    }

    return 0;
}
