#ifndef __AP_MATH_H
#define __AP_MATH_H

#define MATH_MIN(x, y)            ((x) > (y) ? (y) : (x))
#define MATH_MAX(x, y)            ((x) > (y) ? (x) : (y))
#define MATH_CLAMP(x, low, high)  ((x) > (high) ? (high) : (x) < (low) ? low : (x))
#define MATH_XYTOIDX(x, y, width) (((y) * (width)) + (x))
#define MATH_ABS(x)               ((x) >= 0 ? (x) : -(x))

typedef struct Vec2
{
    int x, y;
} Vec2;

#define VEC(x, y)            ((Vec2){(x), (y)})
#define VEC_ZERO             (VEC(0, 0))
#define IDXTOVEC(i, width)   (VEC((i) % (width), (i) / (width)))
#define VECTOIDX(vec, width) (XYTOIDX((vec).x, (vec).y, (width)))
#define VEC_MIN(a, b)        (VEC(MATH_MIN((a).x, (b).x), MATH_MIN((a).y, (b).y)))
#define VEC_MAX(a, b)        (VEC(MATH_MAX((a).x, (b).x), MATH_MAX((a).y, (b).y)))
#define VEC_CLAMP(a, low, high)                                                \
    (VEC(MATH_CLAMP((a).x, (low).x, (high).x),                                 \
         MATH_CLAMP((a).y, (low).y, (high).y)))
#define VEC_PRINT(vec)   printf(#vec " = Vec2(x=%d, y=%d)\n", (vec).x, (vec).y)
#define VEC_PRODUCT(vec) ((vec).x * (vec).y)
#define VEC_ADD(a, b)    (VEC((a).x + (b).x, (a).y + (b).y))
#define VEC_SUB(a, b)    (VEC((a).x - (b).x, (a).y - (b).y))
#define VEC_MUL(a, b)    (VEC((a).x * (b).x, (a).y * (b).y))
#define VEC_DIV(a, b)    (VEC((a).x / (b).x, (a).y / (b).y))

#endif /* __AP_MATH_H */
