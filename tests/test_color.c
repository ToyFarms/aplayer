#include "base_test.h"

INCLUDE_BEGIN
#include "color.h"
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/ui/color.c
 src/struct/ds.c
 src/logger.c
 -lm
 */ CFLAGS_END

TEST_BEGIN(rgb_init)
{
    color_t c = COLOR(128, 30, 30);
    ASSERT_INT_EQ(c.r, 128);
    ASSERT_INT_EQ(c.g, 30);
    ASSERT_INT_EQ(c.b, 30);
    ASSERT_INT_EQ(c.a, 255);
}
TEST_END()

TEST_BEGIN(rgb_size)
{
    ASSERT_INT_EQ((int)sizeof(color_t), 4);
}
TEST_END()

TEST_BEGIN(rgb_overflow)
{
    color_t c = COLOR(255, 255, 255);
    c.r += 10;
    c.g += 20;
    c.b += 30;
    c.a += 40;

    ASSERT_INT_EQ(c.r, 10 - 1);
    ASSERT_INT_EQ(c.g, 20 - 1);
    ASSERT_INT_EQ(c.b, 30 - 1);
    ASSERT_INT_EQ(c.a, 40 - 1);
}
TEST_END()

TEST_BEGIN(rgb_from_gray)
{
    color_t c = COLOR_GRAY(69);

    ASSERT_INT_EQ(c.r, 69);
    ASSERT_INT_EQ(c.g, 69);
    ASSERT_INT_EQ(c.b, 69);
    ASSERT_INT_EQ(c.a, 255);
}
TEST_END()

TEST_BEGIN(rgb_from_rgba)
{
    color_t c = COLOR_RGBA(69, 19, 20, 38);

    ASSERT_INT_EQ(c.r, 69);
    ASSERT_INT_EQ(c.g, 19);
    ASSERT_INT_EQ(c.b, 20);
    ASSERT_INT_EQ(c.a, 38);
}
TEST_END()

TEST_BEGIN(rgb_none)
{
    color_t c = COLOR_NONE;

    ASSERT_INT_EQ(c.r, 0);
    ASSERT_INT_EQ(c.g, 0);
    ASSERT_INT_EQ(c.b, 0);
    ASSERT_INT_EQ(c.a, 0);
}
TEST_END()

TEST_BEGIN(hsl_init)
{
    color_hsl_t c = COLOR_HSL(230.0f, 90.0f, 50.0f);

    ASSERT_FLOAT_EQ(c.h, 230.0f);
    ASSERT_FLOAT_EQ(c.s, 90.0f);
    ASSERT_FLOAT_EQ(c.l, 50.0f);
    ASSERT_FLOAT_EQ(c.a, 255.0f);
}
TEST_END()

TEST_BEGIN(hsla_init)
{
    color_hsl_t c = COLOR_HSLA(230.95f, 90.1f, 50.2f, 69.9f);

    ASSERT_FLOAT_EQ(c.h, 230.95f);
    ASSERT_FLOAT_EQ(c.s, 90.1f);
    ASSERT_FLOAT_EQ(c.l, 50.2f);
    ASSERT_FLOAT_EQ(c.a, 69.9f);
}
TEST_END()

TEST_BEGIN(hsl_size)
{
    ASSERT_INT_EQ((int)sizeof(color_hsl_t), (int)sizeof(float) * 4);
}
TEST_END()

TEST_BEGIN(rgb_from_hex)
{
    uint32_t hex_col[] = {
        0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFFFF0000,
        0x00FFFF00, 0x0000FFFF, 0xFF0000FF, 0xFFFFFF00, 0x00FFFFFF,
        0xFF00FFFF, 0xFFFF00FF, 0xFFFFFFFF,
    };

    color_t expected[] = {
        COLOR_RGBA(255, 0, 0, 0),       COLOR_RGBA(0, 255, 0, 0),
        COLOR_RGBA(0, 0, 255, 0),       COLOR_RGBA(0, 0, 0, 255),
        COLOR_RGBA(255, 255, 0, 0),     COLOR_RGBA(0, 255, 255, 0),
        COLOR_RGBA(0, 0, 255, 255),     COLOR_RGBA(255, 0, 0, 255),
        COLOR_RGBA(255, 255, 255, 0),   COLOR_RGBA(0, 255, 255, 255),
        COLOR_RGBA(255, 0, 255, 255),   COLOR_RGBA(255, 255, 0, 255),
        COLOR_RGBA(255, 255, 255, 255),
    };

    int len = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < len; i++)
    {
        color_t out = color_hex(hex_col[i]);
        ASSERT_MEM_EQ(&out, &expected[i], sizeof(color_t));
    }
}
TEST_END()

TEST_BEGIN(rgb_from_hsl)
{
    color_hsl_t hsl_col[] = {
        COLOR_HSL(0.0f, 0.0f, 0.0f),      COLOR_HSL(0.0f, 0.0f, 100.0f),
        COLOR_HSL(0.0f, 100.0f, 50.0f),   COLOR_HSL(120.0f, 100.0f, 50.0f),
        COLOR_HSL(240.0f, 100.0f, 50.0f), COLOR_HSL(60.0f, 100.0f, 50.0f),
        COLOR_HSL(180.0f, 100.0f, 50.0f), COLOR_HSL(300.0f, 100.0f, 50.0f),
        COLOR_HSL(0.0f, 0.0f, 75.0f),     COLOR_HSL(0.0f, 0.0f, 50.0f),
        COLOR_HSL(0.0f, 100.0f, 25.0f),   COLOR_HSL(60.0f, 100.0f, 25.0f),
        COLOR_HSL(120.0f, 100.0f, 25.0f), COLOR_HSL(300.0f, 100.0f, 25.0f),
        COLOR_HSL(180.0f, 100.0f, 25.0f), COLOR_HSL(240.0f, 100.0f, 25.0f),
    };

    color_t expected[] = {
        COLOR(0, 0, 0),       COLOR(255, 255, 255), COLOR(255, 0, 0),
        COLOR(0, 255, 0),     COLOR(0, 0, 255),     COLOR(255, 255, 0),
        COLOR(0, 255, 255),   COLOR(255, 0, 255),   COLOR(191, 191, 191),
        COLOR(128, 128, 128), COLOR(128, 0, 0),     COLOR(128, 128, 0),
        COLOR(0, 128, 0),     COLOR(128, 0, 128),   COLOR(0, 128, 128),
        COLOR(0, 0, 128),
    };

    int len = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < len; i++)
    {
        color_t out = color_hsl(hsl_col[i]);
        ASSERT_MEM_EQ(&out, &expected[i], sizeof(color_t));
    }
}
TEST_END()

TEST_BEGIN(rgb_normalize)
{
    color_t col[] = {
        COLOR(0, 0, 0),       COLOR(255, 255, 255), COLOR(255, 0, 0),
        COLOR(0, 255, 0),     COLOR(0, 0, 255),     COLOR(255, 255, 0),
        COLOR(0, 255, 255),   COLOR(255, 0, 255),   COLOR(188, 188, 188),
        COLOR(128, 128, 128), COLOR(128, 0, 0),     COLOR(128, 128, 0),
        COLOR(0, 128, 0),     COLOR(128, 0, 128),   COLOR(0, 128, 128),
        COLOR(0, 0, 128),
    };
    color_norm_t expected[] = {
        (color_norm_t){0,     0,     0,     1},
        (color_norm_t){1,     1,     1,     1},
        (color_norm_t){1,     0,     0,     1},
        (color_norm_t){0,     1,     0,     1},
        (color_norm_t){0,     0,     1,     1},
        (color_norm_t){1,     1,     0,     1},
        (color_norm_t){0,     1,     1,     1},
        (color_norm_t){1,     0,     1,     1},
        (color_norm_t){0.74f, 0.74f, 0.74f, 1},
        (color_norm_t){0.50f, 0.50f, 0.50f, 1},
        (color_norm_t){0.50f, 0,     0,     1},
        (color_norm_t){0.50f, 0.50f, 0,     1},
        (color_norm_t){0,     0.50f, 0,     1},
        (color_norm_t){0.50f, 0,     0.50f, 1},
        (color_norm_t){0,     0.50f, 0.50f, 1},
        (color_norm_t){0,     0,     0.50f, 1},
    };

    int len = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < len; i++)
    {
        color_norm_t out = color_normalize(col[i]);
        ASSERT_FLOAT_WITHIN(out.r, expected[i].r, 0.01f);
        ASSERT_FLOAT_WITHIN(out.g, expected[i].g, 0.01f);
        ASSERT_FLOAT_WITHIN(out.b, expected[i].b, 0.01f);
        ASSERT_FLOAT_WITHIN(out.a, expected[i].a, 0.01f);
    }
}
TEST_END()

TEST_BEGIN(rgb_scale)
{
    color_norm_t col[] = {
        (color_norm_t){0,     0,     0,     1   },
        (color_norm_t){1,     1,     1,     0.1f},
        (color_norm_t){1,     0,     0,     0.1f},
        (color_norm_t){0,     1,     0,     1   },
        (color_norm_t){0,     0,     1,     1   },
        (color_norm_t){1,     1,     0,     1   },
        (color_norm_t){0,     1,     1,     1   },
        (color_norm_t){1,     0,     1,     1   },
        (color_norm_t){0.74f, 0.74f, 0.74f, 1   },
        (color_norm_t){0.50f, 0.50f, 0.50f, 1   },
        (color_norm_t){0.50f, 0,     0,     1   },
        (color_norm_t){0.50f, 0.50f, 0,     0.8f},
        (color_norm_t){0,     0.50f, 0,     1   },
        (color_norm_t){0.50f, 0,     0.50f, 1   },
        (color_norm_t){0,     0.50f, 0.50f, 0.9f},
        (color_norm_t){0,     0,     0.50f, 5.0f},
    };
    color_t expected[] = {
        COLOR(0, 0, 0),
        COLOR_RGBA(255, 255, 255, 26),
        COLOR_RGBA(255, 0, 0, 26),
        COLOR(0, 255, 0),
        COLOR(0, 0, 255),
        COLOR(255, 255, 0),
        COLOR(0, 255, 255),
        COLOR(255, 0, 255),
        COLOR(189, 189, 189),
        COLOR(128, 128, 128),
        COLOR(128, 0, 0),
        COLOR_RGBA(128, 128, 0, 204),
        COLOR(0, 128, 0),
        COLOR(128, 0, 128),
        COLOR_RGBA(0, 128, 128, 230),
        COLOR_RGBA(0, 0, 128, 255),
    };

    int len = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < len; i++)
    {
        color_t out = color_scale(col[i]);
        ASSERT_MEM_EQ(&out, &expected[i], sizeof(color_t));
    }
}
TEST_END()

TEST_BEGIN(xterm256_pallete)
{
    color_t col[] = {
        COLOR(0, 0, 0),       COLOR(0, 0, 95),      COLOR(0, 0, 135),
        COLOR(0, 0, 175),     COLOR(0, 0, 215),     COLOR(0, 0, 255),
        COLOR(0, 95, 0),      COLOR(0, 95, 95),     COLOR(0, 95, 135),
        COLOR(0, 95, 175),    COLOR(0, 95, 215),    COLOR(0, 95, 255),
        COLOR(0, 135, 0),     COLOR(0, 135, 95),    COLOR(0, 135, 135),
        COLOR(0, 135, 175),   COLOR(0, 135, 215),   COLOR(0, 135, 255),
        COLOR(0, 175, 0),     COLOR(0, 175, 95),    COLOR(0, 175, 135),
        COLOR(0, 175, 175),   COLOR(0, 175, 215),   COLOR(0, 175, 255),
        COLOR(0, 215, 0),     COLOR(0, 215, 95),    COLOR(0, 215, 135),
        COLOR(0, 215, 175),   COLOR(0, 215, 215),   COLOR(0, 215, 255),
        COLOR(0, 255, 0),     COLOR(0, 255, 95),    COLOR(0, 255, 135),
        COLOR(0, 255, 175),   COLOR(0, 255, 215),   COLOR(0, 255, 255),
        COLOR(95, 0, 0),      COLOR(95, 0, 95),     COLOR(95, 0, 135),
        COLOR(95, 0, 175),    COLOR(95, 0, 215),    COLOR(95, 0, 255),
        COLOR(95, 95, 0),     COLOR(95, 95, 95),    COLOR(95, 95, 135),
        COLOR(95, 95, 175),   COLOR(95, 95, 215),   COLOR(95, 95, 255),
        COLOR(95, 135, 0),    COLOR(95, 135, 95),   COLOR(95, 135, 135),
        COLOR(95, 135, 175),  COLOR(95, 135, 215),  COLOR(95, 135, 255),
        COLOR(95, 175, 0),    COLOR(95, 175, 95),   COLOR(95, 175, 135),
        COLOR(95, 175, 175),  COLOR(95, 175, 215),  COLOR(95, 175, 255),
        COLOR(95, 215, 0),    COLOR(95, 215, 95),   COLOR(95, 215, 135),
        COLOR(95, 215, 175),  COLOR(95, 215, 215),  COLOR(95, 215, 255),
        COLOR(95, 255, 0),    COLOR(95, 255, 95),   COLOR(95, 255, 135),
        COLOR(95, 255, 175),  COLOR(95, 255, 215),  COLOR(95, 255, 255),
        COLOR(135, 0, 0),     COLOR(135, 0, 95),    COLOR(135, 0, 135),
        COLOR(135, 0, 175),   COLOR(135, 0, 215),   COLOR(135, 0, 255),
        COLOR(135, 95, 0),    COLOR(135, 95, 95),   COLOR(135, 95, 135),
        COLOR(135, 95, 175),  COLOR(135, 95, 215),  COLOR(135, 95, 255),
        COLOR(135, 135, 0),   COLOR(135, 135, 95),  COLOR(135, 135, 135),
        COLOR(135, 135, 175), COLOR(135, 135, 215), COLOR(135, 135, 255),
        COLOR(135, 175, 0),   COLOR(135, 175, 95),  COLOR(135, 175, 135),
        COLOR(135, 175, 175), COLOR(135, 175, 215), COLOR(135, 175, 255),
        COLOR(135, 215, 0),   COLOR(135, 215, 95),  COLOR(135, 215, 135),
        COLOR(135, 215, 175), COLOR(135, 215, 215), COLOR(135, 215, 255),
        COLOR(135, 255, 0),   COLOR(135, 255, 95),  COLOR(135, 255, 135),
        COLOR(135, 255, 175), COLOR(135, 255, 215), COLOR(135, 255, 255),
        COLOR(175, 0, 0),     COLOR(175, 0, 95),    COLOR(175, 0, 135),
        COLOR(175, 0, 175),   COLOR(175, 0, 215),   COLOR(175, 0, 255),
        COLOR(175, 95, 0),    COLOR(175, 95, 95),   COLOR(175, 95, 135),
        COLOR(175, 95, 175),  COLOR(175, 95, 215),  COLOR(175, 95, 255),
        COLOR(175, 135, 0),   COLOR(175, 135, 95),  COLOR(175, 135, 135),
        COLOR(175, 135, 175), COLOR(175, 135, 215), COLOR(175, 135, 255),
        COLOR(175, 175, 0),   COLOR(175, 175, 95),  COLOR(175, 175, 135),
        COLOR(175, 175, 175), COLOR(175, 175, 215), COLOR(175, 175, 255),
        COLOR(175, 215, 0),   COLOR(175, 215, 95),  COLOR(175, 215, 135),
        COLOR(175, 215, 175), COLOR(175, 215, 215), COLOR(175, 215, 255),
        COLOR(175, 255, 0),   COLOR(175, 255, 95),  COLOR(175, 255, 135),
        COLOR(175, 255, 175), COLOR(175, 255, 215), COLOR(175, 255, 255),
        COLOR(215, 0, 0),     COLOR(215, 0, 95),    COLOR(215, 0, 135),
        COLOR(215, 0, 175),   COLOR(215, 0, 215),   COLOR(215, 0, 255),
        COLOR(215, 95, 0),    COLOR(215, 95, 95),   COLOR(215, 95, 135),
        COLOR(215, 95, 175),  COLOR(215, 95, 215),  COLOR(215, 95, 255),
        COLOR(215, 135, 0),   COLOR(215, 135, 95),  COLOR(215, 135, 135),
        COLOR(215, 135, 175), COLOR(215, 135, 215), COLOR(215, 135, 255),
        COLOR(215, 175, 0),   COLOR(215, 175, 95),  COLOR(215, 175, 135),
        COLOR(215, 175, 175), COLOR(215, 175, 215), COLOR(215, 175, 255),
        COLOR(215, 215, 0),   COLOR(215, 215, 95),  COLOR(215, 215, 135),
        COLOR(215, 215, 175), COLOR(215, 215, 215), COLOR(215, 215, 255),
        COLOR(215, 255, 0),   COLOR(215, 255, 95),  COLOR(215, 255, 135),
        COLOR(215, 255, 175), COLOR(215, 255, 215), COLOR(215, 255, 255),
        COLOR(255, 0, 0),     COLOR(255, 0, 95),    COLOR(255, 0, 135),
        COLOR(255, 0, 175),   COLOR(255, 0, 215),   COLOR(255, 0, 255),
        COLOR(255, 95, 0),    COLOR(255, 95, 95),   COLOR(255, 95, 135),
        COLOR(255, 95, 175),  COLOR(255, 95, 215),  COLOR(255, 95, 255),
        COLOR(255, 135, 0),   COLOR(255, 135, 95),  COLOR(255, 135, 135),
        COLOR(255, 135, 175), COLOR(255, 135, 215), COLOR(255, 135, 255),
        COLOR(255, 175, 0),   COLOR(255, 175, 95),  COLOR(255, 175, 135),
        COLOR(255, 175, 175), COLOR(255, 175, 215), COLOR(255, 175, 255),
        COLOR(255, 215, 0),   COLOR(255, 215, 95),  COLOR(255, 215, 135),
        COLOR(255, 215, 175), COLOR(255, 215, 215), COLOR(255, 215, 255),
        COLOR(255, 255, 0),   COLOR(255, 255, 95),  COLOR(255, 255, 135),
        COLOR(255, 255, 175), COLOR(255, 255, 215), COLOR(255, 255, 255),
        COLOR(8, 8, 8),       COLOR(30, 30, 30),    COLOR(100, 100, 50),
        COLOR(100, 100, 100),
    };
    int expected[] = {
        16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
        30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
        44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,
        58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
        72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,
        86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,
        100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
        114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
        142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155,
        156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
        170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
        198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211,
        212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225,
        226, 227, 228, 229, 230, 231, 232, 234, 239, 241,
    };

    ASSERT_INT_EQ((int)sizeof(col), (int)sizeof(expected));
    int len = sizeof(expected) / sizeof(expected[0]);
    for (int i = 0; i < len; i++)
    {
        int out = color_term_pallete(col[i]);
        ASSERT_INT_EQ(out, expected[i]);
    }
}
TEST_END()
