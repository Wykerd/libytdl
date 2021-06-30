#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <ytdl/libregexp.h>

#define REGEX_LEN 2

typedef struct ytdl_regex_def_s {
    char *var_name;
    char *regex;
    int regex_len;
    int flags;
} ytdl_regex_def_t;

const ytdl_regex_def_t regex_defs[2] = {
    {
        .var_name = "action_obj_regexp",
        .regex = "var ([a-zA-Z_\\$][a-zA-Z_0-9]*)=\\{((?:(?:(?:[a-zA-Z_\\$][a-zA-Z_0-9]*|(?:'[^'\\\\]*(:?\\\\[\\s\\S][^'\\\\]*)*'|\"[^\"\\\\]*(:?\\\\[\\s\\S][^\"\\\\]*)*\")):function\\(a\\)\\{(?:return )?a\\.reverse\\(\\)\\}|(?:[a-zA-Z_\\$][a-zA-Z_0-9]*|(?:'[^'\\\\]*(:?\\\\[\\s\\S][^'\\\\]*)*'|\"[^\"\\\\]*(:?\\\\[\\s\\S][^\"\\\\]*)*\")):function\\(a,b\\)\\{return a\\.slice\\(b\\)\\}|(?:[a-zA-Z_\\$][a-zA-Z_0-9]*|(?:'[^'\\\\]*(:?\\\\[\\s\\S][^'\\\\]*)*'|\"[^\"\\\\]*(:?\\\\[\\s\\S][^\"\\\\]*)*\")):function\\(a,b\\)\\{a\\.splice\\(0,b\\)\\}|(?:[a-zA-Z_\\$][a-zA-Z_0-9]*|(?:'[^'\\\\]*(:?\\\\[\\s\\S][^'\\\\]*)*'|\"[^\"\\\\]*(:?\\\\[\\s\\S][^\"\\\\]*)*\")):function\\(a,b\\)\\{var c=a\\[0\\];a\\[0\\]=a\\[b(?:%a\\.length)?\\];a\\[b(?:%a\\.length)?\\]=c(?:;return a)?\\}),?\\r?\\n?)+)\\};",
        .regex_len = 645,
        .flags = 0
    },
    {
        .var_name = "action_func_regexp",
        .regex = "function(?: [a-zA-Z_\\$][a-zA-Z_0-9]*)?\\(a\\)\\{a=a\\.split\\((?:''|\"\")\\);\\s*((?:(?:a=)?[a-zA-Z_\\$][a-zA-Z_0-9]*(?:\\.[a-zA-Z_\\$][a-zA-Z_0-9]*|\\[(?:'[^'\\\\]*(:?\\\\[\\s\\S][^'\\\\]*)*'|\"[^\"\\\\]*(:?\\\\[\\s\\S][^\"\\\\]*)*\")\\])\\(a,\\d+\\);)+)return a\\.join\\((?:''|\"\")\\)\\}",
        .regex_len = 247,
        .flags = 0
    }
};

int lre_check_stack_overflow(void *opaque, size_t alloca_size)
{
    return 0;
}

void *lre_realloc(void *opaque, void *ptr, size_t size) 
{
    return realloc(ptr, size);
}

int main (int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s [OUTFILE]\n", argv[0]);
        return 1;
    }

    uint8_t *re_bytecode_buf;
    int re_bytecode_len;
    char error_msg[64];

    FILE *fd = fopen(argv[1], "w");

    fputs("/* Compiled RegExp bytecodes */\n/* Generated file - do not edit */\n\n", fd);

    for (int i = 0; i < REGEX_LEN; i++)
    {
        re_bytecode_buf = lre_compile(&re_bytecode_len, error_msg, sizeof(error_msg), 
            regex_defs[i].regex, regex_defs[i].regex_len, regex_defs[i].flags, NULL);

        if (!re_bytecode_buf) {
            printf("Error: %s\n", error_msg);
            fclose(fd);
            return 1;
        }

        fprintf(fd, "const unsigned char %s[] = {\n    ", regex_defs[i].var_name);
        int i;
        for (i = 0; i < re_bytecode_len; ++i) {
            fprintf(fd, "0x%02x%s",
                   re_bytecode_buf[i],
                   i == re_bytecode_len-1 ? "" : ((i+1) % 15 == 0 ? ",\n    " : ","));
        }
        fputs("\n};\n\n", fd);

        free(re_bytecode_buf);
    }

    fclose(fd);
}