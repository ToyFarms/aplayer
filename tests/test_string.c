#include "base_test.h"

INCLUDE_BEGIN
#include "ds.h"
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/struct/ds.c
 src/logger.c
 */ CFLAGS_END

TEST_BEGIN(init)
{
    str_t str = str_create();
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_NEQ((int)str.capacity, 0);
}
TEST_END()

TEST_BEGIN(alloc)
{
    str_t str = str_alloc(200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
}
TEST_END()

TEST_BEGIN(free)
{
    str_t str = str_alloc(200);
    str_free(&str);

    ASSERT_MEM_EQ(&str, &(str_t){0}, sizeof(str_t));
}
TEST_END()

TEST_BEGIN(free_stack)
{
    str_t str = str_create();
    str_free(&str);

    ASSERT_MEM_EQ(&str, &(str_t){0}, sizeof(str_t));
}
TEST_END()

TEST_BEGIN(resize)
{
    str_t str = str_create();

    str_resize(&str, 200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
}
TEST_END()

TEST_BEGIN(resize_with_item)
{
    str_t str = str_create();
    memcpy(str.buf, (char *){"Hello"}, 5);

    str_resize(&str, 200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
    ASSERT_MEM_EQ(str.buf, "Hello", 5);
}
TEST_END()

TEST_BEGIN(cat)
{
    str_t str = str_create();
    const char string[] = "Hello World!! %d %f %p %% // \\ /\n";
    const int len = sizeof(string) - 1; // no null ptr
    str_cat(&str, string);
    ASSERT_NOTNULL(str.buf);

    ASSERT_INT_EQ((int)str.len, len);
    ASSERT_MEM_EQ(str.buf, string, len);
}
TEST_END()

TEST_BEGIN(cat_many)
{
    str_t str = str_create();
    const char string[] = "Hello World!! %d %f %p %% // \\ /\n";
    const char string2[] = "Hello World 22 2";
    const char string3[] = "Hello World 58";
    const char string4[] = "Hello World test";

    const char combined[] = "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World 22 2"
                            "Hello World 58"
                            "Hello World test";

    str_cat(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string4);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

#define len(s) (sizeof(s) - 1)
    int total_len =
        len(string) + len(string) + len(string2) + len(string3) + len(string4);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_MEM_EQ(str.buf, combined, len(combined));
}
TEST_END()

TEST_BEGIN(cat_many_test_null_terminator)
{
    str_t str = str_create();
    const char string[] = "Hello World!! %d %f %p %% // \\ /\n";
    const char string2[] = "Hello World 22 2";
    const char string3[] = "Hello World 58";
    const char string4[] = "Hello World test";

    const char combined[] = "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World 22 2"
                            "Hello World 58"
                            "Hello World test";

#define len(s) (sizeof(s) - 1)
    int total_len = 0;

    str_cat(&str, string);
    total_len += len(string);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string);
    total_len += len(string);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string2);
    total_len += len(string2);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string3);
    total_len += len(string3);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat(&str, string4);
    total_len += len(string4);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf, combined, len(combined));
}
TEST_END()

TEST_BEGIN(catlen)
{
    str_t str = str_create();
    const char string[] = "1234567890";

    str_catlen(&str, string, 2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catlen(&str, string, 3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catlen(&str, string + 3, 2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catlen(&str, string + 5, 3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

#define len(s) (sizeof(s) - 1)
    ASSERT_INT_EQ((int)str.len, 10);
    ASSERT_MEM_EQ(str.buf, "1212345678", 10);
}
TEST_END()

TEST_BEGIN(catch)
{
    str_t str = str_create();
    const char string[] = "1234567890";

    str_catch(&str, string[0]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[0], '1');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[1]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[1], '2');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[2]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[2], '3');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[3]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[3], '4');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[4]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[4], '5');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[5]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[5], '6');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[6]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[6], '7');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[7]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[7], '8');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[8]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[8], '9');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    str_catch(&str, string[9]);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ(str.buf[9], '0');
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    ASSERT_INT_EQ((int)str.len, 10);
    ASSERT_MEM_EQ(str.buf, string, sizeof(string));
}
TEST_END()

TEST_BEGIN(cat_str)
{
    str_t str = str_create();
    str_t str2 = str_create();

    str_cat(&str, "Hello World 1!");
    str_cat(&str2, "Hello World 2!");

    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    ASSERT_INT_EQ((int)str.len, 28);
    ASSERT_MEM_EQ(str.buf, "Hello World 1!Hello World 2!", 28);
}
TEST_END()

TEST_BEGIN(cat_str_many)
{
    str_t str = str_create();
    str_t str2 = str_create();

    str_cat(&str, "Hello World 1!");
    str_cat(&str2, "Hello World 2!");

    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_cat_str(&str, &str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    ASSERT_INT_EQ((int)str.len, 14 + (14 * 5));
    ASSERT_MEM_EQ(str.buf,
                  "Hello World 1!Hello World 2!Hello World 2!Hello World "
                  "2!Hello World 2!Hello World 2!",
                  14 + (14 * 5));
}
TEST_END()

TEST_BEGIN(catf)
{
    str_t str = str_create();

    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    const char expected[] =
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n";
    ASSERT_INT_EQ((int)str.len, (int)sizeof(expected) - 1);
    ASSERT_MEM_EQ(str.buf, expected, sizeof(expected) - 1);
}
TEST_END()

TEST_BEGIN(catf_many)
{
    str_t str = str_create();

    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
             123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    const char expected[] =
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n"
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n"
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n"
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n"
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n"
        "Hello % -100 % 0.500000 123(nil) 1048575 16777215 %\n";
    ASSERT_INT_EQ((int)str.len, (int)sizeof(expected) - 1);
    ASSERT_MEM_EQ(str.buf, expected, sizeof(expected) - 1);
}
TEST_END()

TEST_BEGIN(catf_d)
{
    str_t str = str_create();

    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    ASSERT_NOTNULL(str.buf);
    const char expected[] = "-1000000 199 % %%%12345 \n\n\n\n\r\r\r";
    ASSERT_INT_EQ((int)str.len, (int)sizeof(expected) - 1);
    ASSERT_MEM_EQ(str.buf, expected, sizeof(expected) - 1);
}
TEST_END()

TEST_BEGIN(catf_d_many)
{
    str_t str = str_create();

    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 2);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 3);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 4);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199, 12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 5);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    const char expected[] = "-1000000 199 % %%%12345 \n\n\n\n\r\r\r"
                            "-1000000 199 % %%%12345 \n\n\n\n\r\r\r"
                            "-1000000 199 % %%%12345 \n\n\n\n\r\r\r"
                            "-1000000 199 % %%%12345 \n\n\n\n\r\r\r"
                            "-1000000 199 % %%%12345 \n\n\n\n\r\r\r";
    ASSERT_MEM_EQ(str.buf, expected, sizeof(expected) - 1);
}
TEST_END()

TEST_BEGIN(catw, INIT : setlocale)
{
    str_t str = str_create();
    wchar_t string[] = L"おはよう世界!";

    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    wchar_t out[100] = {0};
    mbstowcs(out, str.buf, str.len);

    ASSERT_MEM_EQ(out, string, sizeof(string));
}
TEST_END()

TEST_BEGIN(catw_many, INIT : setlocale)
{
    str_t str = str_create();
    wchar_t string[] = L"おはよう世界!";

    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwcs(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    wchar_t out[256] = {0};
    mbstowcs(out, str.buf, str.len);

    wchar_t expected[] = L"おはよう世界!"
                         L"おはよう世界!"
                         L"おはよう世界!"
                         L"おはよう世界!"
                         L"おはよう世界!";
    ASSERT_MEM_EQ(out, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(catwch, INIT : setlocale)
{
    str_t str = str_alloc(1);

    str_catwch(&str, L'世');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'界');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'は');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'綺');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'麗');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'だ');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    str_catwch(&str, L'ね');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    wchar_t out[100] = {0};
    mbstowcs(out, str.buf, str.len);

    wchar_t expected[] = L"世界は綺麗だね";
    ASSERT_MEM_EQ(out, expected, sizeof(expected));
}
TEST_END()

TEST_BEGIN(stress, INIT : setlocale)
{
    str_t str = str_create();
#define MAX  (1024 * 1024)
#define ITER (5000)
    char expected[MAX] = {0};
    int len = 0;

    for (int i = 0; i < ITER; i++)
    {
        char *a = malloc(2);
        a[0] = i;
        if (a[0] == '\0')
            a[0] = '\1';
        a[1] = '\0';

        char *b = malloc(1);
        b[0] = i;

        str_t temp = str_create();
        str_catf(&temp, "%lc %ls %d!", L'時', L"あの時は。。。", -10000);

        str_cat(&str, a);
        str_catch(&str, b[0]);
        str_catf(&str, "%d%d %f %p %%!", -120, 69, 0.3f + 0.2f, NULL);
        str_catf_d(&str, "%d%d%%%%%d!", 100, -100, -6969);
        str_catwch(&str, L'空');
        str_catwcs(&str, L"HELLO 世界!");
        str_cat_str(&str, &temp);

        // ------------------------------

        expected[len++] = a[0];
        expected[len++] = b[0];
        len += sprintf(expected + len, "%d%d %f %p %%!", -120, 69, 0.3f + 0.2f,
                       NULL);
        len += sprintf(expected + len, "%d%d%%%%%d!", 100, -100, -6969);
        len += sprintf(expected + len, "%lc", L'空');
        len += sprintf(expected + len, "%ls", L"HELLO 世界!");
        len += sprintf(expected + len, "%s", temp.buf);

        free(a);
        free(b);
        str_free(&temp);
    }

    ASSERT_INT_EQ((int)str.len, len);
    ASSERT_MEM_EQ(str.buf, expected, len);
}
TEST_END()

TEST_BEGIN(encode, INIT : setlocale)
{
    str_t s = str_encode(L"ハロ");
    ASSERT_INT_GTE((int)s.capacity, 1);
    ASSERT_NOTNULL(s.buf);
    const char expected[] = {0xe3, 0x83, 0x8f, 0xe3, 0x83, 0xad};
    ASSERT_INT_EQ((int)s.len, (int)sizeof(expected));
    ASSERT_MEM_EQ(s.buf, expected, sizeof(expected));
    ASSERT_INT_EQ(s.buf[s.len], '\0');
}
TEST_END()

TEST_BEGIN(encode_ascii)
{
    str_t s = str_encode(L"HELLO!");
    ASSERT_INT_GTE((int)s.capacity, 1);
    ASSERT_NOTNULL(s.buf);
    const char expected[] = "HELLO!";
    ASSERT_INT_EQ((int)s.len, (int)strlen(expected));
    ASSERT_MEM_EQ(s.buf, expected, sizeof(expected));
    ASSERT_INT_EQ(s.buf[s.len], '\0');
}
TEST_END()

TEST_BEGIN(decode, INIT : setlocale)
{
    str_t s = str_create();
    const char utf8[] = {0xe3, 0x83, 0x8f, 0xe3, 0x83, 0xad, 0x0};
    str_cat(&s, utf8);

    wchar_t *res = str_decode(&s);
    ASSERT_NOTNULL(res);
    const wchar_t expected[] = L"ハロ";
    ASSERT_MEM_EQ(res, expected, sizeof(expected));
    ASSERT_INT_EQ(s.buf[s.len], '\0');
}
TEST_END()

TEST_BEGIN(decode2, INIT : setlocale)
{
    str_t s = str_create();
    str_cat(&s, "HELLO!");

    wchar_t *res = str_decode(&s);
    ASSERT_NOTNULL(res);
    const wchar_t expected[] = L"HELLO!";
    ASSERT_MEM_EQ(res, expected, sizeof(expected));
    ASSERT_INT_EQ(s.buf[s.len], '\0');
}
TEST_END()

TEST_BEGIN(repeat_char)
{
    str_t s = str_create();
    str_repeat_char(&s, 'A', 5, NULL);
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 5);
    ASSERT_INT_GTE((int)s.capacity, (int)s.len + 1);
    ASSERT_MEM_EQ(s.buf, "AAAAA", 5);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_char_zero)
{
    str_t s = str_create();
    str_repeat_char(&s, 'Z', 0, "-");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 0);
    ASSERT_INT_GTE((int)s.capacity, 1);
    ASSERT_MEM_EQ(s.buf, "", 0);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_char_sep)
{
    str_t s = str_create();
    str_repeat_char(&s, 'X', 3, ":");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 5);
    ASSERT_INT_GTE((int)s.capacity, 6);
    ASSERT_MEM_EQ(s.buf, "X:X:X", 5);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_char_multisep)
{
    str_t s = str_create();
    str_repeat_char(&s, 'X', 3, "abc");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 9);
    ASSERT_INT_GTE((int)s.capacity, 9);
    ASSERT_MEM_EQ(s.buf, "XabcXabcX", 9);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_wchar)
{
    setlocale(LC_ALL, "");
    str_t s = str_create();
    str_repeat_wchar(&s, L'世', 3, L"-");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_GTE((int)s.capacity, (int)s.len + 1);
    wchar_t out[16] = {0};
    mbstowcs(out, s.buf, s.len + 1);
    wchar_t exp[] = L"世-世-世";
    ASSERT_INT_EQ((int)wcslen(out), (int)wcslen(exp));
    ASSERT_MEM_EQ(out, exp, sizeof(exp));
}
TEST_END()

TEST_BEGIN(repeat_cstr)
{
    str_t s = str_create();
    str_repeat_cstr(&s, "hi", 4, " ");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 11);
    ASSERT_INT_GTE((int)s.capacity, 12);
    ASSERT_MEM_EQ(s.buf, "hi hi hi hi", 11);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_cstr_empty_src)
{
    str_t s = str_create();
    str_repeat_cstr(&s, "", 5, ",");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 4);
    ASSERT_INT_GTE((int)s.capacity, 4);
    ASSERT_MEM_EQ(s.buf, ",,,,", 4);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
}
TEST_END()

TEST_BEGIN(repeat_wcs)
{
    setlocale(LC_ALL, "");
    str_t s = str_create();
    str_repeat_wcs(&s, L"あ", 4, L"・");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_GTE((int)s.capacity, (int)s.len + 1);
    wchar_t *out = str_decode(&s);
    wchar_t exp[] = L"あ・あ・あ・あ";
    ASSERT_MEM_EQ(out, exp, sizeof(exp));
}
TEST_END()

TEST_BEGIN(repeat_empty_wcs)
{
    setlocale(LC_ALL, "");
    str_t s = str_create();
    str_repeat_wcs(&s, L"", 4, L"・");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_GTE((int)s.capacity, (int)s.len + 1);
    wchar_t *out = str_decode(&s);
    wchar_t exp[] = L"・・・";
    ASSERT_MEM_EQ(out, exp, sizeof(exp));
}
TEST_END()

TEST_BEGIN(repeat_str_t)
{
    str_t base = str_new("abc");
    str_t s = str_create();
    str_repeat_str(&s, &base, 3, "-");
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, 11);
    ASSERT_INT_GTE((int)s.capacity, 12);
    ASSERT_MEM_EQ(s.buf, "abc-abc-abc", 11);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
    str_free(&base);
}
TEST_END()

TEST_BEGIN(repeat_stress)
{
    const size_t N = 10000;
    str_t s = str_create();
    str_repeat_char(&s, 'x', N, NULL);
    ASSERT_NOTNULL(s.buf);
    ASSERT_INT_EQ((int)s.len, (int)N);
    ASSERT_INT_GTE((int)s.capacity, (int)N + 1);
    ASSERT_MEM_EQ(&s.buf[0], "x", 1);
    ASSERT_MEM_EQ(&s.buf[N / 2], "x", 1);
    ASSERT_MEM_EQ(&s.buf[N - 1], "x", 1);
    ASSERT_MEM_EQ(s.buf + s.len, "\0", 1);
    str_free(&s);
}
TEST_END()

TEST_BEGIN(tokenizer)
{
    str_t s = str_new("1,2,3");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '1');

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '2');

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '3');

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_multi_len)
{
    str_t s = str_new("1000,2000,3");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 4);
    ASSERT_STR_EQ(token.buf, "1000", 4);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 4);
    ASSERT_STR_EQ(token.buf, "2000", 4);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '3');

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_no_delim)
{
    str_t s = str_new("1,2,3");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ";");

    strview_t token = {0};

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_empty_string)
{
    str_t s = str_new("");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_trailing_delim)
{
    str_t s = str_new("1,2,3,");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '1');

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '2');

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 1);
    ASSERT_INT_EQ(token.buf[0], '3');

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_empty_between_delim)
{
    str_t s = str_new("12,,,34");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 2);
    ASSERT_STR_EQ(token.buf, "12", 2);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_TRUE(token.buf == s.buf + 3);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_TRUE(token.buf == s.buf + 4);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 2);
    ASSERT_STR_EQ(token.buf, "34", 2);

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_only_delim)
{
    str_t s = str_new(",,,");

    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, ",");

    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_TRUE(token.buf == s.buf + 0);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_TRUE(token.buf == s.buf + 1);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_TRUE(token.buf == s.buf + 2);

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_macro)
{
    str_t s = str_new("1,2,3,");

    int i = 1;
    STR_SPLIT(s, token, ",")
    {
        ASSERT_INT_EQ((int)token.len, 1);
        ASSERT_INT_EQ(token.buf[0], '0' + i);
        i++;
    }

    ASSERT_INT_EQ(i, 4);
}
TEST_END()

TEST_BEGIN(tokenizer_utf8_multi_byte_delim)
{
    str_t s = str_new("あ、い、う");
    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, "、");
    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 3);
    ASSERT_STR_EQ(token.buf, "あ", 3);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 3);
    ASSERT_STR_EQ(token.buf, "い", 3);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 3);
    ASSERT_STR_EQ(token.buf, "う", 3);

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(tokenizer_emoji)
{
    str_t s = str_new("😀|😂|👍");
    str_tokenizer_t tok = {0};
    str_tokenizer_init(&tok, &s, "|");
    strview_t token = {0};

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 4);
    ASSERT_STR_EQ(token.buf, "😀", 4);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 4);
    ASSERT_STR_EQ(token.buf, "😂", 4);

    ASSERT_TRUE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 4);
    ASSERT_STR_EQ(token.buf, "👍", 4);

    ASSERT_FALSE(str_tokenizer_next(&tok, &token));
    ASSERT_INT_EQ((int)token.len, 0);
    ASSERT_NULL(token.buf);
}
TEST_END()

TEST_BEGIN(parse)
{
    str_t s = str_new("0");
    long x = str_parse(view(s), 10);

    ASSERT_INT_EQ((int)x, 0);
}
TEST_END()

TEST_BEGIN(parse_10)
{
    str_t s = str_new("10");
    long x = str_parse(view(s), 10);

    ASSERT_INT_EQ((int)x, 10);
}
TEST_END()

TEST_BEGIN(parse_negative)
{
    str_t s = str_new("-10");
    long x = str_parse(view(s), 10);

    ASSERT_INT_EQ((int)x, -10);
}
TEST_END()

TEST_BEGIN(parse_negative_zero)
{
    str_t s = str_new("-0");
    long x = str_parse(view(s), 10);

    ASSERT_INT_EQ((int)x, 0);
}
TEST_END()

TEST_BEGIN(parse_hex)
{
    str_t s = str_new("FF");
    long x = str_parse(view(s), 16);

    ASSERT_INT_EQ((int)x, 255);
}
TEST_END()

TEST_BEGIN(parse_bin)
{
    str_t s = str_new("1100");
    long x = str_parse(view(s), 2);

    ASSERT_INT_EQ((int)x, 12);
}
TEST_END()

TEST_BEGIN(parse_float)
{
    str_t s = str_new("10.099");
    float x = str_parsef(view(s));

    ASSERT_FLOAT_EQ(x, 10.099f);
}
TEST_END()

TEST_BEGIN(parse_negative_float)
{
    str_t s = str_new("-10.099");
    float x = str_parsef(view(s));

    ASSERT_FLOAT_EQ(x, -10.099f);
}
TEST_END()

TEST_BEGIN(parse_big_float)
{
    str_t s = str_new("912391923919239.127391283");
    float x = str_parsef(view(s));

    ASSERT_FLOAT_EQ(x, 912391923919239.127391283f);
}
TEST_END()

TEST_BEGIN(parse_longdouble)
{
    str_t s = str_new("9123919239192391920310293.127391283");
    long double x = str_parseld(view(s));

    ASSERT_TRUE(x == 9123919239192391920310293.127391283L);
}
TEST_END()

TEST_BEGIN(parse_negative_longdouble)
{
    str_t s = str_new("-9123919239192391920310293.127391283");
    long double x = str_parseld(view(s));

    ASSERT_TRUE(x == -9123919239192391920310293.127391283L);
}
TEST_END()

TEST_BEGIN(parse_scientific_notation)
{
    str_t s = str_new("1e9");
    long double x = str_parseld(view(s));

    ASSERT_TRUE(x == 1000000000L);
}
TEST_END()
