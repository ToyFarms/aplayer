#include "ui_parser.h"
#include "_math.h"
#include "app.h"
#include "logger.h"
#include "ui_manager.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define DO_HANDLE_CASE(c, caps, id)                                            \
    case TOKEN_##caps:                                                         \
        if (c == NULL)                                                         \
            return "[" #id "] " #caps;                                         \
        return "[" #id "] " #caps "(" #c ")";                                  \
        break;

static const char *token_type_str(enum token_type type)
{
    switch (type)
    {
        TOKEN_TYPE_LIST(DO_HANDLE_CASE);
    default:
        return "TOK_UNKNOWN";
    }

    return "";
}

typedef struct tokenizer_ctx
{
    vec2 loc;
    int ptr;
    const char *source;
    size_t source_len;
    enum token_type *token_map;
    arena_allocator *arena;
} tokenizer_ctx;

#define TOKENIZER_CUR(ctx) (ctx).source[(ctx).ptr]
#define TOKENIZER_EAT(ctx)                                                     \
    do                                                                         \
    {                                                                          \
        (ctx).ptr++;                                                           \
        (ctx).loc.x++;                                                         \
    } while (0)
#define TOKENIZER_EATN(ctx, n)                                                 \
    do                                                                         \
    {                                                                          \
        (ctx).ptr += (n);                                                      \
        (ctx).loc.x += (n);                                                    \
    } while (0)
#define TOKENIZER_LOC_INCR(ctx)                                                \
    do                                                                         \
    {                                                                          \
        (ctx).loc.x = 0;                                                       \
        (ctx).loc.y++;                                                         \
    } while (0)
#define TOKENIZER_LEFT(ctx)    ((ctx).source_len - (ctx).ptr)
#define TOKENIZER_NOT_EOF(ctx) ((ctx).ptr < (ctx).source_len)

static int valid_num_char(char c)
{
    return isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' ||
           c == 'E';
}

static int valid_hex_char(char c)
{
    char l = tolower(c);
    return isdigit(c) || (l >= 'a' && l <= 'f');
}

static int valid_bin_char(char c)
{
    return c == '0' || c == '1';
}

#define SET_STATUS(status, n)                                                  \
    do                                                                         \
    {                                                                          \
        if (status)                                                            \
            *status = n;                                                       \
    } while (0)

static token_t parse_number(tokenizer_ctx *ctx, int *status);
static token_t parse_ident(tokenizer_ctx *ctx);
static token_t parse_comment(tokenizer_ctx *ctx);

static enum token_type token_map[256] = {0};
static int token_map_initialized = 0;

#define DO_INSERT_TOKEN(c, caps, id)                                           \
    if (c != NULL)                                                             \
        token_map[((uint8_t *)c)[0]] = TOKEN_##caps;

array_t ui_tokenize(const char *source, arena_allocator *arena)
{
    if (!token_map_initialized)
    {
        TOKEN_TYPE_LIST(DO_INSERT_TOKEN);
        token_map_initialized = 1;
    }

    tokenizer_ctx ctx = {
        .loc = VEC_ZERO,
        .ptr = 0,
        .source = source,
        .source_len = strlen(source),
        .token_map = token_map,
        .arena = arena,
    };

    array(token_t) tokens = array_create(128, sizeof(token_t));

    while (TOKENIZER_NOT_EOF(ctx))
    {
        char c = TOKENIZER_CUR(ctx);
        char cnext = 0;

        if (TOKENIZER_LEFT(ctx) > 1)
            cnext = ctx.source[ctx.ptr + 1];

        if (c == ' ' || c == '\t')
            TOKENIZER_EAT(ctx);
        else if (c == '\n')
        {
            TOKENIZER_EAT(ctx);
            TOKENIZER_LOC_INCR(ctx);
        }
        else if (ctx.token_map[(uint8_t)c] != 0)
        {
            token_t token = {
                .loc = ctx.loc,
                .type = ctx.token_map[(uint8_t)c],
            };
            array_append(&tokens, &token, 1);
            TOKENIZER_EAT(ctx);
        }
        else if (isdigit(c) || (c == '-' && isdigit(cnext)) ||
                 (c == '+' && isdigit(cnext)) || (c == '.' && isdigit(cnext)))
        {
            int status;
            token_t token = parse_number(&ctx, &status);
            if (status != 0)
                break;

            array_append(&tokens, &token, 1);
        }
        else if (c == '-' && cnext == '-')
        {
            token_t token = parse_comment(&ctx);
            array_append(&tokens, &token, 1);
        }
        else
        {
            token_t token = parse_ident(&ctx);
            array_append(&tokens, &token, 1);
        }
    }
    array_append(&tokens,
                 &(token_t){.type = TOKEN_EOF, .v_str = NULL, .loc = VEC_ZERO},
                 1);

    return tokens;
}

void ui_token_print(const token_t *tok)
{
    printf("%d:%d ", tok->loc.y, tok->loc.x);

    switch (tok->type)
    {
    case TOKEN_COMMENT:
        printf("[ COMM ] '");
        print_raw(tok->v_str);
        printf("'\n");
        break;
    case TOKEN_IDENT:
        printf("[ IDENT ] '");
        print_raw(tok->v_str);
        printf("'\n");
        break;
    case TOKEN_INT_LIT:
        printf("'%s' = %lld\n", tok->v_str, tok->v_int);
        break;
    case TOKEN_FLT_LIT:
        printf("'%s' = %f\n", tok->v_str, tok->v_flt);
        break;
    default:
        printf("%s\n", token_type_str(tok->type));
        break;
    }
}

void ui_tokens_print(const array_t *tokens)
{
    if (tokens == NULL || tokens->length == 0)
        return;

    token_t *tok;
    ARR_FOREACH_BYREF(*tokens, tok, i)
    {
        ui_token_print(tok);
    }
}

#define FIND_NUM_BOUNDARY(validator)                                           \
    do                                                                         \
    {                                                                          \
        while (TOKENIZER_NOT_EOF(*ctx) && validator(TOKENIZER_CUR(*ctx)))      \
            TOKENIZER_EAT(*ctx);                                               \
        len = ctx->ptr - len;                                                  \
        memcpy(nstr, base, len > 127 ? 127 : len);                             \
    } while (0)

#define CHECK_CHAR(ch, idx)                                                    \
    if (nstr[i] == ch && (count[idx] += 1) >= 2)                               \
    {                                                                          \
        invalid = 1;                                                           \
        break;                                                                 \
    }

// TODO: Better error reporting
static token_t parse_number(tokenizer_ctx *ctx, int *status)
{
    const char *base = ctx->source + ctx->ptr;
    vec2 base_loc = ctx->loc;

    token_t token = {0};
    token.loc = ctx->loc;
    char *nstr = arena_alloc(ctx->arena, 128);
    token.v_str = nstr;
    int len = ctx->ptr;

    if (TOKENIZER_LEFT(*ctx) >= 2 && base[0] == '0' && tolower(base[1]) == 'x')
    {
        TOKENIZER_EATN(*ctx, 2);
        FIND_NUM_BOUNDARY(valid_hex_char);

        token.type = TOKEN_INT_LIT;

        char *end = NULL;
        token.v_int = strtoll(nstr, &end, 16);
        if (*end != '\0')
            SET_STATUS(status, -1);
    }
    else if (TOKENIZER_LEFT(*ctx) >= 2 && base[0] == '0' &&
             tolower(base[1]) == 'b')
    {
        TOKENIZER_EATN(*ctx, 2);
        FIND_NUM_BOUNDARY(valid_bin_char);

        token.type = TOKEN_INT_LIT;

        char *end = NULL;
        token.v_int = strtoll(nstr + 2, &end, 2);
        if (*end != '\0')
            SET_STATUS(status, -1);
    }
    else
    {
        FIND_NUM_BOUNDARY(valid_num_char);

        int count[3] = {0};
        int invalid = 0;
        for (int i = 0; i < len; i++)
        {
            CHECK_CHAR('.', 0);
            CHECK_CHAR('e', 1);
            CHECK_CHAR('E', 2);
        }

        if (invalid)
        {
            log_error("Invalid number: '%s' %d:%d\n", nstr, base_loc.x + 1,
                      base_loc.y + 1);
            SET_STATUS(status, -1);
            goto exit;
        }

        if (count[0] > 0 || count[1] > 0 || count[2] > 0)
        {
            token.type = TOKEN_FLT_LIT;

            char *end = NULL;
            token.v_flt = strtof(nstr, &end);
            if (*end != '\0')
                SET_STATUS(status, -1);
        }
        else
        {
            token.type = TOKEN_INT_LIT;

            char *end = NULL;
            token.v_int = strtoll(nstr, &end, 10);
            if (*end != '\0')
                SET_STATUS(status, -1);
        }
    }

    SET_STATUS(status, 0);
exit:
    return token;
}

static int valid_ident_char(char c, int first)
{
    int is_valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    if (first)
        return !isdigit(c) && is_valid;

    return isdigit(c) || is_valid;
}

static token_t parse_ident(tokenizer_ctx *ctx)
{
    const char *base = ctx->source + ctx->ptr;

    int len = ctx->ptr;
    int first = 1;
    do
    {
        TOKENIZER_EAT(*ctx);
        first = 0;
    } while (TOKENIZER_NOT_EOF(*ctx) &&
             valid_ident_char(TOKENIZER_CUR(*ctx), first));
    len = ctx->ptr - len;

    char *ident = arena_alloc(ctx->arena, len + 1);
    ident[len] = '\0';
    memcpy(ident, base, len);

    return (token_t){.loc = ctx->loc, .type = TOKEN_IDENT, .v_str = ident};
}

static token_t parse_comment(tokenizer_ctx *ctx)
{
    TOKENIZER_EATN(*ctx, 2);

    const char *base = ctx->source + ctx->ptr;
    int len = ctx->ptr;

    while (TOKENIZER_NOT_EOF(*ctx))
    {
        char c = TOKENIZER_CUR(*ctx);
        char cnext = 0;

        if (TOKENIZER_LEFT(*ctx) > 1)
            cnext = ctx->source[ctx->ptr + 1];

        if (c == '-' && cnext == '-')
            break;

        TOKENIZER_EAT(*ctx);
    }
    len = ctx->ptr - len;
    TOKENIZER_EATN(*ctx, 2);

    char *comment = arena_alloc(ctx->arena, len + 1);
    comment[len] = '\0';
    memcpy(comment, base, len);

    return (token_t){.loc = ctx->loc, .type = TOKEN_COMMENT, .v_str = comment};
}

typedef struct ui_header
{
    dict_t links;
} ui_header;

typedef struct parser_ctx
{
    array(token_t) * tokens;
    token_t *prev_tok;
    token_t *cur_tok;
    token_t *cur_ident;
    int idx;
    int err;
    arena_allocator *arena;
    ui_header header;
} parser_ctx;

static int parser_advance(parser_ctx *ctx)
{
    if (ctx->cur_tok->type == TOKEN_EOF)
        return 0;

    token_t *prev = ctx->cur_tok;
    ctx->cur_tok = &ARR_AS(*ctx->tokens, token_t)[ctx->idx++];
    ctx->prev_tok = prev;

    if (ctx->cur_tok->type == TOKEN_IDENT)
        ctx->cur_ident = ctx->cur_tok;

    return 1;
}

static ui_unit make_unit(parser_ctx *ctx)
{
    ui_unit unit = {0};

    token_t *tok = ctx->cur_tok;
    if (tok->type == TOKEN_INT_LIT)
    {
        unit.type = OBJ_ATTR_UNIT_INT;
        unit.v_int.v = tok->v_int;
    }
    else if (tok->type == TOKEN_FLT_LIT)
    {
        unit.type = OBJ_ATTR_UNIT_INT;
        unit.v_int.v = (int)tok->v_flt;
    }
    else if (tok->type == TOKEN_IDENT)
    {
        unit.type = OBJ_ATTR_UNIT_STR;
        unit.v_str =
            arena_memdup(ctx->arena, tok->v_str, strlen(tok->v_str) + 1);
    }
    parser_advance(ctx);

    if (ctx->cur_tok && ctx->cur_tok->type == TOKEN_PERCENT &&
        (tok->type == TOKEN_INT_LIT || tok->type == TOKEN_FLT_LIT))
    {
        unit.v_int.is_percent = true;
        parser_advance(ctx);
    }

    return unit;
}

static int parse_attr(parser_ctx *ctx, ui_element *obj)
{
    token_t *attr_ident = ctx->cur_ident;
    char *attr_name = attr_ident->v_str;

    if (ctx->cur_tok->type != TOKEN_COLON)
    {
        log_error("Expected a ':' at %d:%d\n", ctx->cur_tok->loc.y,
                  ctx->cur_tok->loc.x);
        return -1;
    }
    parser_advance(ctx);

    ui_unit unit = {0};

    if (strcmp(attr_name, "x") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "x", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "y") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "y", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "pos") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "x", &unit, sizeof(unit));
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "y", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "w") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "w", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "h") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "h", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "size") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "w", &unit, sizeof(unit));
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "h", &unit, sizeof(unit));
    }
    else if (strcmp(attr_name, "anchor") == 0)
    {
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "anchor-x", &unit, sizeof(unit));
        unit = make_unit(ctx);
        dict_insert_copy(&obj->attr, "anchor-y", &unit, sizeof(unit));
    }
    else
        log_warning("Unknown attribute: %s\n", attr_name);

    return 0;
}

void print_memory(const void *addr, size_t n)
{
    const unsigned char *ptr = addr;

    for (size_t i = 0; i < n; i++)
    {
        if (isprint(ptr[i]))
            fprintf(stderr, "%c ", ptr[i]);
        else
            fprintf(stderr, "%02X ", ptr[i]);
    }
}

static int parse_element(parser_ctx *ctx, ui_element *parent)
{
    bool is_first = true;
    bool at_mod = false;

    while (parser_advance(ctx))
    {
        if (ctx->cur_tok->type == TOKEN_CURLY_CLOSE)
            break;

        if (ctx->cur_tok->type == TOKEN_CURLY_OPEN)
        {
            if (is_first)
            {
                parent->children = array_create(8, sizeof(ui_element));
                is_first = false;
            }

            array_append(&parent->children, &(ui_element){0}, 1);
            ui_element *el = &ARR_AS(parent->children,
                                     ui_element)[parent->children.length - 1];
            el->attr = dict_create();
            el->parent = parent;
            el->name = arena_memdup(ctx->arena, ctx->cur_ident->v_str,
                                    strlen(ctx->cur_ident->v_str) + 1);
            el->type = at_mod ? UI_ELEMENT_WIDGET : UI_ELEMENT_LAYOUT;
            at_mod = false;

            if (el->type == UI_ELEMENT_WIDGET)
            {
                dict_t *links = &ctx->header.links;
                if (!dict_exists(links, el->name))
                {
                    log_fatal("Could not find widget '%s' in links\n",
                              el->name);
                    return -1;
                }

                const char *path = dict_get(links, el->name, NULL);
                if (path == NULL)
                {
                    log_fatal("path is NULL for '%s'\n", el->name);
                    return -1;
                }

                // const char *filename = strrchr(path, '/');
                // filename = filename == NULL ? path : filename + 1;

                app_instance *app = app_get();
                wpl_class *class;
                ARR_FOREACH_BYREF(app->widget_classes, class, i)
                {
                    log_debug("widget: %s, plugin: %s\n", path,
                              class->filepath);
                    // NOTE: Find corresponding widget class given a path, side
                    // tracked by making a path library
                }
            }

            parse_element(ctx, el);
        }
        else if (ctx->cur_tok->type == TOKEN_COLON)
        {
            if (parse_attr(ctx, parent) < 0)
                return -1;
        }
        else if (ctx->cur_tok->type == TOKEN_AT)
            at_mod = true;
    }

    return 0;
}

static int parse_header(parser_ctx *ctx)
{
    string_t link_target = string_alloc(32);

    if (ctx->cur_tok->type != TOKEN_CURLY_OPEN)
    {
        log_error("%d:%d Expected '{' to start the header. Got '%s'\n",
                  ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                  token_type_str(ctx->cur_tok->type));
        return -1;
    }

    parser_advance(ctx);

    while (ctx->cur_tok->type != TOKEN_CURLY_CLOSE)
    {
        if (ctx->cur_tok->type != TOKEN_AT)
        {
            log_error("%d:%d Expected '@' in link definition. Got '%s'\n",
                      ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                      token_type_str(ctx->cur_tok->type));
            break;
        }

        parser_advance(ctx);

        if (ctx->cur_tok->type != TOKEN_IDENT)
        {
            log_error("%d:%d Expected identifier after '@'. Got '%s'\n",
                      ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                      token_type_str(ctx->cur_tok->type));
            break;
        }

        if (strcmp(ctx->cur_tok->v_str, "link") != 0)
        {
            log_error("%d:%d Expected 'link' after '@'. Got '%s'\n",
                      ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                      ctx->cur_tok->v_str);
            break;
        }

        parser_advance(ctx);

        if (ctx->cur_tok->type != TOKEN_IDENT)
        {
            log_error("%d:%d Expected an identifier after '@link'. Got '%s'\n",
                      ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                      token_type_str(ctx->cur_tok->type));
            break;
        }

        const char *link_ident = ctx->cur_tok->v_str;
        parser_advance(ctx);

        if (ctx->cur_tok->type != TOKEN_EQUAL)
        {
            log_error("%d:%d Expected '=' after '@link <ident>'. Got '%s'\n",
                      ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                      token_type_str(ctx->cur_tok->type));
            break;
        }

        while (parser_advance(ctx) && ctx->cur_tok->type != TOKEN_SEMICOLON)
        {
            if (ctx->cur_tok->type != TOKEN_IDENT)
            {
                log_error(
                    "%d:%d Expected an identifier after '@link <ident> = '. "
                    "Got '%s'\n",
                    ctx->cur_tok->loc.y, ctx->cur_tok->loc.x,
                    token_type_str(ctx->cur_tok->type));
                break;
            }

            string_cat(&link_target, ctx->cur_tok->v_str);
        }

        parser_advance(ctx);

        dict_insert_copy(&ctx->header.links, link_ident, link_target.buf,
                         link_target.len);
        link_target.len = 0;
    }

    string_free(&link_target);
    return 0;
}

ui_scene ui_scene_from_tokens(array_t *tokens, arena_allocator *arena)
{
    assert(tokens && tokens->length != 0 && arena);

    ui_scene scene = {0};
    ui_element *body = &scene.body;

    *body = (ui_element){.name = "body"};

    parser_ctx ctx = {.tokens = tokens,
                      .prev_tok = NULL,
                      .cur_tok = &ARR_AS(*tokens, token_t)[0],
                      .idx = 0,
                      .arena = arena,
                      .header = (ui_header){.links = dict_create()}};

    parser_advance(&ctx);
    parse_header(&ctx);
    parse_element(&ctx, body);

    return scene;
}
