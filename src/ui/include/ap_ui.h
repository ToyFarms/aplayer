#ifndef __AP_UI_H
#define __AP_UI_H

#include "ap_math.h"
#include "sds.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Color
{
    uint8_t r, g, b, a;
} APColor;

#define APCOLOR(r, g, b, a) ((APColor){(r), (g), (b), (a)})
#define APCOLOR_ZERO        ((APColor){0})

typedef enum SizeBehavior
{
    SIZE_FILL,
    SIZE_ABS,
} SizeBehavior;

typedef struct APWidget
{
    Vec2 pos;
    Vec2 size;
    sds draw_cmd;
    // used for intermediate memory allocation, such as utf8 conversion
    sds _conv_utf;
    void (*draw)(struct APWidget *);
    bool resized;
} APWidget;

void ap_widget_init(APWidget *w, Vec2 pos, Vec2 size,
                    void (*draw)(struct APWidget *));

#endif /* __AP_UI_H */
