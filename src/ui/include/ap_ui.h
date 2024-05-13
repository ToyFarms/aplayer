#ifndef __AP_UI_H
#define __AP_UI_H

#include "ap_math.h"
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include "ap_terminal.h"

typedef struct Color
{
    uint8_t r, g, b, a;
} APColor;

#define APCOLOR(r, g, b, a) ((APColor){(r), (g), (b), (a)})
#define APCOLOR_HEX(hex)    ((APColor){(uint32_t)(hex)})
#define APCOLOR_OP(c1, op, c2)                                                 \
    ((APColor){                                                                \
        MATH_CLAMP((c1).r op(c2).r, 0, 255),                                   \
        MATH_CLAMP((c1).g op(c2).g, 0, 255),                                   \
        MATH_CLAMP((c1).b op(c2).b, 0, 255),                                   \
        MATH_MIN((c1).a, (c2).a),                                              \
    })

#define APCOLOR_ZERO    ((APColor){0})
#define APCOLOR_GRAY(x) ((APColor){(x), (x), (x), 255})
#define APCOLOR_WHITE   ((APColor){255, 255, 255, 255})
#define APCOLOR_BLACK   ((APColor){0, 0, 0, 255})
#define APCOLOR_RED     ((APColor){255, 255, 255, 255})
#define APCOLOR_GREEN   ((APColor){0, 255, 0, 255})
#define APCOLOR_BLUE    ((APColor){0, 0, 255, 255})
#define APCOLOR_ORANGE  ((APColor){255, 165, 0, 255})
#define APCOLOR_YELLOW  ((APColor){255, 255, 0, 255})
#define APCOLOR_MAGENTA ((APColor){255, 0, 255, 255})

#endif /* __AP_UI_H */
