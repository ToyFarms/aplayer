#include "layout.h"
#include "_math.h"
#include "logger.h"
#include <stdint.h>

static void compute_layout(lo_ctx *ctx);
static void finish_layout(lo_ctx *ctx);

lo_ctx lo_create()
{
    lo_ctx ctx = {0};

    return ctx;
}

void lo_frame_clear(lo_frame *root)
{
    if (root == NULL)
        return;

    lo_frame *frame;
    ARR_FOREACH_BYREF(root->children, frame, i)
    {
        lo_frame_clear(frame);
    }
    array_free(&root->children);
}

void lo_free(lo_ctx *ctx)
{
    if (ctx == NULL)
        return;

    lo_frame_clear(&ctx->root);
}

void lo_frame_add_to_current(lo_ctx *ctx, lo_frame_def def)
{
    lo_frame frame = {.def = def,
                      .children = array_create(4, sizeof(lo_frame))};
    if (def.__is_root)
    {
        if (def.size.x.mode == LO_SIZING_ABS &&
            def.size.y.mode == LO_SIZING_ABS)
        {
            frame.size.x = def.size.x.preferred;
            frame.size.y = def.size.y.preferred;
        }
        ctx->root = frame;
        ctx->_current = &ctx->root;
        return;
    }

    lo_frame *parent = ctx->_current ? ctx->_current : &ctx->root;
    frame.parent = parent;

    array_append(&parent->children, &frame, 1);
    lo_frame *child =
        &ARR_AS(parent->children, lo_frame)[parent->children.length - 1];

    ctx->_current = child;
}

void lo_frame_finish_current(lo_ctx *ctx)
{
    if (ctx->_current->def.__is_root)
    {
        ctx->_current->def.__has_children = true;
        compute_layout(ctx);
        finish_layout(ctx);
        return;
    }

    if (ctx->_current->parent == NULL)
        return;

    ctx->_current->parent->def.__has_children = true;

    compute_layout(ctx);
    ctx->_current = ctx->_current->parent;
}

enum axis
{
    AXIS_X,
    AXIS_Y,
};

static int eval_unit(lo_frame *frame, enum axis axis)
{
    int size = 0;
    lo_unit unit = axis == AXIS_X ? frame->def.size.x : frame->def.size.y;

    switch (unit.mode)
    {
    case LO_SIZING_ABS:
        size = unit.preferred;
        break;
    case LO_SIZING_GROW:
        break;
    case LO_SIZING_FIT:
        if ((axis == AXIS_X &&
             frame->def.direction == LO_DIRECTION_HORIZONTAL) ||
            (axis == AXIS_Y && frame->def.direction == LO_DIRECTION_VERTICAL))
            size += frame->def.gap * (frame->children.length - 1);
        break;
    case LO_SIZING_PERCENT: {
        lo_frame *parent = frame->parent;
        while (
            parent != NULL &&
            (axis == AXIS_X ? parent->def.size.x : parent->def.size.y).mode ==
                LO_SIZING_FIT)
        {
            parent = parent->parent;
        }
        if (parent == NULL ||
            (axis == AXIS_X ? parent->def.size.x : parent->def.size.y).mode ==
                LO_SIZING_FIT)
            break;

        int parent_size = axis == AXIS_X ? parent->size.x : parent->size.y;
        size = unit.percentage * (float)parent_size;
    }
    break;
    }

    if (axis == AXIS_X)
        size += frame->def.padding.x + frame->def.padding.w;
    else
        size += frame->def.padding.y + frame->def.padding.h;

    return size;
}

static void compute_layout(lo_ctx *ctx)
{
    lo_frame *current = ctx->_current;
    if (current == NULL)
        return;
    lo_frame *parent = ctx->_current->parent;
    if (parent == NULL)
        goto compute_last;

    int width, height;
    width = eval_unit(current, AXIS_X);
    height = eval_unit(current, AXIS_Y);

    if (current->def.__has_children)
    {
        current->size.x += width;
        current->size.y += height;
    }
    else
    {
        current->size.x = width;
        current->size.y = height;
    }

    if (!parent->def.__is_root)
    {
        if (parent->def.direction == LO_DIRECTION_HORIZONTAL)
        {
            if (parent->def.size.x.mode != LO_SIZING_ABS)
                parent->size.x += width;
            if (parent->def.size.y.mode != LO_SIZING_ABS)
                parent->size.y = MATH_MAX(parent->size.y, height);
        }
        else
        {
            if (parent->def.size.x.mode != LO_SIZING_ABS)
                parent->size.x = MATH_MAX(parent->size.x, width);
            if (parent->def.size.y.mode != LO_SIZING_ABS)
                parent->size.y += height;
        }
    }

    if (current->def.size.x.mode == LO_SIZING_GROW)
        parent->def.__growing_child.x++;
    if (current->def.size.y.mode == LO_SIZING_GROW)
        parent->def.__growing_child.y++;

compute_last:
    if (current->def.__growing_child.x > 0)
    {
        int used = current->def.padding.x + current->def.padding.w;
        lo_frame *child;
        int index = 0;
        ARR_FOREACH_BYREF(current->children, child, i)
        {
            if (child->def.size.x.mode == LO_SIZING_GROW)
            {
                index = i;
                continue;
            }

            used += child->def.margin.x + child->def.margin.w;
            if (current->def.direction == LO_DIRECTION_HORIZONTAL)
                used += child->size.x;
        }
        if (current->def.direction == LO_DIRECTION_HORIZONTAL)
            used += current->def.gap * (current->children.length - 1);

        int left = current->size.x - used;
        ARR_AS(current->children, lo_frame)[index].size.x = left;
    }
    if (current->def.__growing_child.y > 0)
    {
        int used = current->def.padding.y + current->def.padding.h;
        lo_frame *child;
        int index = 0;
        ARR_FOREACH_BYREF(current->children, child, i)
        {
            if (child->def.size.y.mode == LO_SIZING_GROW)
            {
                index = i;
                continue;
            }

            used += child->def.margin.y + child->def.margin.h;
            if (current->def.direction == LO_DIRECTION_VERTICAL)
                used += child->size.y;
        }
        if (current->def.direction == LO_DIRECTION_VERTICAL)
            used += current->def.gap * (current->children.length - 1);

        int left = current->size.y - used;
        ARR_AS(current->children, lo_frame)[index].size.y = left;
    }
}

static void calculate_position(lo_frame *frame)
{
    vec2 content_origin = {frame->pos.x + frame->def.padding.x,
                           frame->pos.y + frame->def.padding.y};
    vec2 content_size = {
        frame->size.x - (frame->def.padding.x + frame->def.padding.w),
        frame->size.y - (frame->def.padding.y + frame->def.padding.h)};

    int n = frame->children.length;
    int sum = 0;
    int margins = 0;
    int total_gaps = (n > 1) ? (n - 1) * frame->def.gap : 0;

    lo_frame *child;
    ARR_FOREACH_BYREF(frame->children, child, i)
    {
        if (frame->def.direction == LO_DIRECTION_HORIZONTAL)
        {
            sum += child->size.x;
            margins += child->def.margin.x + child->def.margin.w;
        }
        else
        {
            sum += child->size.y;
            margins += child->def.margin.y + child->def.margin.h;
        }
    }

    int track_len = sum + margins + total_gaps;
    int free_space =
        ((frame->def.direction == LO_DIRECTION_HORIZONTAL) ? content_size.x
                                                           : content_size.y) -
        track_len;

    enum lo_alignment main_align =
        (frame->def.direction == LO_DIRECTION_HORIZONTAL
             ? frame->def.alignment.x
             : frame->def.alignment.y);

    int offset = 0;
    switch (main_align)
    {
    case LO_ALIGNMENT_START:
        offset = 0;
        break;
    case LO_ALIGNMENT_CENTER:
        offset = free_space / 2;
        break;
    case LO_ALIGNMENT_END:
        offset = free_space;
        break;
    }

    if (offset < 0)
        offset = 0;

    vec2 cursor = content_origin;
    if (frame->def.direction == LO_DIRECTION_HORIZONTAL)
    {
        cursor.x += offset;
        ARR_FOREACH_BYREF(frame->children, child, i)
        {
            cursor.x += child->def.margin.x;
            child->pos.x = cursor.x;

            switch (frame->def.alignment.y)
            {
            case LO_ALIGNMENT_START:
                child->pos.y = content_origin.y + child->def.margin.y;
                break;
            case LO_ALIGNMENT_CENTER:
                child->pos.y =
                    content_origin.y + (content_size.y - child->size.y) * 0.5f;
                break;
            case LO_ALIGNMENT_END:
                child->pos.y = content_origin.y + content_size.y -
                               child->size.y - child->def.margin.h;
                break;
            }

            cursor.x += child->size.x + child->def.margin.w;
            cursor.x += frame->def.gap;

            calculate_position(child);
        }
    }
    else
    {
        cursor.y += offset;
        ARR_FOREACH_BYREF(frame->children, child, i)
        {
            cursor.y += child->def.margin.y;
            child->pos.y = cursor.y;

            switch (frame->def.alignment.x)
            {
            case LO_ALIGNMENT_START:
                child->pos.x = content_origin.x + child->def.margin.x;
                break;
            case LO_ALIGNMENT_CENTER:
                child->pos.x =
                    content_origin.x + (content_size.x - child->size.x) * 0.5f;
                break;
            case LO_ALIGNMENT_END:
                child->pos.x = content_origin.x + content_size.x -
                               child->size.x - child->def.margin.w;
                break;
            }

            cursor.y += child->size.y + child->def.margin.h;
            cursor.y += frame->def.gap;

            calculate_position(child);
        }
    }
}

static void finish_layout(lo_ctx *ctx)
{
    calculate_position(&ctx->root);
}
