#ifndef __UI_PARSER_H
#define __UI_PARSER_H

#include "arena_allocator.h"
#include "array.h"
#include "ui_manager.h"

#define TOKEN_TYPE_LIST(DO)                                                    \
    DO(NULL, UNKNOWN, 0)                                                       \
    DO(NULL, IDENT, 1)                                                         \
    DO("{", CURLY_OPEN, 2)                                                     \
    DO("}", CURLY_CLOSE, 3)                                                    \
    DO("@", AT, 4)                                                             \
    DO(";", SEMICOLON, 5)                                                      \
    DO(":", COLON, 6)                                                          \
    DO("%", PERCENT, 7)                                                        \
    DO(NULL, EOF, 8)                                                           \
    DO(NULL, COMMENT, 9)                                                       \
    DO(NULL, STR_LIT, 10)                                                      \
    DO(NULL, INT_LIT, 11)                                                      \
    DO(NULL, FLT_LIT, 12)                                                      \
    DO("=", EQUAL, 13)

#define DO_DEFINE_TOKEN_TYPE(c, caps, id) TOKEN_##caps = id,
enum token_type
{
    TOKEN_TYPE_LIST(DO_DEFINE_TOKEN_TYPE)
};

typedef struct token_t
{
    vec2 loc;
    enum token_type type;
    char *v_str;

    union {
        long long int v_int;
        float v_flt;
    };
} token_t;

array(token_t) ui_tokenize(const char *source, arena_allocator *arena);
void ui_tokens_print(const array_t *tokens);
void ui_token_print(const token_t *token);
ui_scene ui_scene_from_tokens(array_t *tokens, arena_allocator *arena);

#endif /* __UI_PARSER_H */
