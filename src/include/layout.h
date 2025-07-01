#ifndef __LAYOUT_H
#define __LAYOUT_H

// TODO: not used right now

#include "_math.h"
#include "array.h"
#include <stdbool.h>
#include <stdlib.h>

enum lo_sizing
{
    LO_SIZING_ABS,
    LO_SIZING_GROW,
    LO_SIZING_FIT,
    LO_SIZING_PERCENT,
};

#define LO_ABS(n)                                                              \
    (lo_unit)                                                                  \
    {                                                                          \
        .mode = LO_SIZING_ABS, .preferred = (n)                                \
    }
#define LO_GROW()                                                              \
    (lo_unit)                                                                  \
    {                                                                          \
        .mode = LO_SIZING_GROW                                                 \
    }
#define LO_FIT()                                                               \
    (lo_unit)                                                                  \
    {                                                                          \
        .mode = LO_SIZING_FIT                                                  \
    }
#define LO_PERCENT(n)                                                          \
    (lo_unit)                                                                  \
    {                                                                          \
        .mode = LO_SIZING_PERCENT, .percentage = (n)                           \
    }

enum lo_alignment
{
    LO_ALIGNMENT_START,
    LO_ALIGNMENT_CENTER,
    LO_ALIGNMENT_END,
};

#define LO_CENTER {LO_ALIGNMENT_CENTER, LO_ALIGNMENT_CENTER}

enum lo_direction
{
    LO_DIRECTION_VERTICAL,
    LO_DIRECTION_HORIZONTAL,
};

typedef struct
{
    enum lo_alignment x;
    enum lo_alignment y;
} lo_alignment;

typedef struct
{
    int min;
    int max;
} lo_constraint;

typedef struct
{
    lo_constraint x;
    lo_constraint y;
} lo_constraint2;

typedef struct
{
    int x, y, w, h;
} lo_scalar4;

typedef int (*lo_fit_callback)(void *userdata);

typedef struct
{
    lo_fit_callback fn;
    void *userdata;
} lo_fit_callback_with_data;

typedef struct
{
    union {
        int preferred;
        float percentage;
        lo_fit_callback_with_data callback;
    };
    enum lo_sizing mode;
} lo_unit;

typedef struct
{
    lo_unit x;
    lo_unit y;
} lo_unit2;

typedef struct
{
    int id; // user defined
    lo_unit2 size;
    lo_scalar4 margin;
    lo_scalar4 padding;
    lo_alignment alignment;
    enum lo_direction direction;
    lo_constraint2 size_constraint;
    int gap;
    bool __is_root;
    bool __has_children; // aka not a leaf node
    vec2 __growing_child;
} lo_frame_def;

typedef struct lo_frame
{
    lo_frame_def def;

    vec2 pos;
    vec2 size;

    array(struct lo_frame) children;
    struct lo_frame *parent;
} lo_frame;

typedef struct
{
    lo_frame root;
    lo_frame *_current;
} lo_ctx;

lo_ctx lo_create();
void lo_free(lo_ctx *ctx);
void lo_frame_clear(lo_frame *root);

void lo_frame_add_to_current(lo_ctx *ctx, lo_frame_def def);
void lo_frame_finish_current(lo_ctx *ctx);

static inline void lo_frame_add_root(lo_ctx *ctx, vec2 size, lo_frame_def def)
{
    if (ctx->root.children.data != NULL)
        lo_frame_clear(&ctx->root);

    def.size.x = LO_ABS(size.x);
    def.size.y = LO_ABS(size.y);
    def.__is_root = true;

    lo_frame_add_to_current(ctx, def);
}

#define LO_FRAME(...)                                                          \
    for (int __i =                                                             \
             (lo_frame_add_to_current(ctx, (lo_frame_def)__VA_ARGS__), 0);     \
         __i == 0; __i = (lo_frame_finish_current(ctx), 1))

#define LO_ROOT(width, height, ...)                                            \
    for (int __i = (lo_frame_add_root(ctx, VEC(width, height),                 \
                                      (lo_frame_def)__VA_ARGS__),              \
                    0);                                                        \
         __i == 0; __i = 1, lo_frame_finish_current(ctx))

#endif /* __LAYOUT_H */
