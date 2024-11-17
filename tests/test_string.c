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
    string_t str = string_create();
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_NEQ((int)str.capacity, 0);
}
TEST_END()

TEST_BEGIN(alloc)
{
    string_t str = string_alloc(200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
}
TEST_END()

TEST_BEGIN(free)
{
    string_t str = string_alloc(200);
    string_free(&str);

    ASSERT_MEM_EQ(&str, &(string_t){0}, sizeof(string_t));
}
TEST_END()

TEST_BEGIN(free_stack)
{
    string_t str = string_create();
    string_free(&str);

    ASSERT_MEM_EQ(&str, &(string_t){0}, sizeof(string_t));
}
TEST_END()

TEST_BEGIN(resize)
{
    string_t str = string_create();

    string_resize(&str, 200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
}
TEST_END()

TEST_BEGIN(resize_with_item)
{
    string_t str = string_create();
    memcpy(str.buf, (char *){"Hello"}, 5);

    string_resize(&str, 200);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 0);
    ASSERT_INT_EQ((int)str.capacity, 200);
    ASSERT_MEM_EQ(str.buf, "Hello", 5);
}
TEST_END()

TEST_BEGIN(cat)
{
    string_t str = string_create();
    const char string[] = "Hello World!! %d %f %p %% // \\ /\n";
    const int len = sizeof(string) - 1; // no null ptr
    string_cat(&str, string);
    ASSERT_NOTNULL(str.buf);

    ASSERT_INT_EQ((int)str.len, len);
    ASSERT_MEM_EQ(str.buf, string, len);
}
TEST_END()

TEST_BEGIN(cat_many)
{
    string_t str = string_create();
    const char string[] = "Hello World!! %d %f %p %% // \\ /\n";
    const char string2[] = "Hello World 22 2";
    const char string3[] = "Hello World 58";
    const char string4[] = "Hello World test";

    const char combined[] = "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World!! %d %f %p %% // \\ /\n"
                            "Hello World 22 2"
                            "Hello World 58"
                            "Hello World test";

    string_cat(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string4);
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
    string_t str = string_create();
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

    string_cat(&str, string);
    total_len += len(string);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string);
    total_len += len(string);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string2);
    total_len += len(string2);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string3);
    total_len += len(string3);
    ASSERT_INT_EQ((int)str.len, total_len);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat(&str, string4);
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
    string_t str = string_create();
    const char string[] = "1234567890";

    string_catlen(&str, string, 2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catlen(&str, string, 3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catlen(&str, string + 3, 2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catlen(&str, string + 5, 3);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

#define len(s) (sizeof(s) - 1)
    ASSERT_INT_EQ((int)str.len, 10);
    ASSERT_MEM_EQ(str.buf, "1212345678", 10);
}
TEST_END()

TEST_BEGIN(cat_str)
{
    string_t str = string_create();
    string_t str2 = string_create();

    string_cat(&str, "Hello World 1!");
    string_cat(&str2, "Hello World 2!");

    string_cat_str(&str, str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);

    ASSERT_INT_EQ((int)str.len, 28);
    ASSERT_MEM_EQ(str.buf, "Hello World 1!Hello World 2!", 28);
}
TEST_END()

TEST_BEGIN(cat_str_many)
{
    string_t str = string_create();
    string_t str2 = string_create();

    string_cat(&str, "Hello World 1!");
    string_cat(&str2, "Hello World 2!");

    string_cat_str(&str, str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat_str(&str, str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat_str(&str, str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat_str(&str, str2);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_cat_str(&str, str2);
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
    string_t str = string_create();

    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
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
    string_t str = string_create();

    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
                123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
                123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
                123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
                123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
                123, NULL, 0xFFFFFUL, 0xFFFFFFULL);
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf(&str, "Hello %% %d %% %f %d%p %zu %llu %%\n", -100, 0.3f + 0.2f,
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
    string_t str = string_create();

    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    ASSERT_NOTNULL(str.buf);
    const char expected[] = "-1000000 199 % %%%12345 \n\n\n\n\r\r\r";
    ASSERT_INT_EQ((int)str.len, (int)sizeof(expected) - 1);
    ASSERT_MEM_EQ(str.buf, expected, sizeof(expected) - 1);
}
TEST_END()

TEST_BEGIN(catf_d_many)
{
    string_t str = string_create();

    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 2);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 3);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_EQ((int)str.len, 31 * 4);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catf_d(&str, "%d %d %% %%%%%%%d \n\n\n\n\r\r\r", -1000000, 199,
                  12345);
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
    string_t str = string_create();
    wchar_t string[] = L"おはよう世界!";

    string_catw(&str, string);
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
    string_t str = string_create();
    wchar_t string[] = L"おはよう世界!";

    string_catw(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catw(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catw(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catw(&str, string);
    ASSERT_NOTNULL(str.buf);
    ASSERT_INT_NEQ((int)str.len, 0);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catw(&str, string);
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
    string_t str = string_alloc(1);

    string_catwch(&str, L'世');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'界');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'は');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'綺');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'麗');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'だ');
    ASSERT_NOTNULL(str.buf);
    ASSERT_MEM_EQ(str.buf + str.len, "\0", 1);
    string_catwch(&str, L'ね');
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
    string_t str = string_create();
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

        string_t temp = string_create();
        string_catf(&temp, "%lc %ls %d!", L'時', L"あの時は。。。", -10000);

        string_cat(&str, a);
        string_catlen(&str, b, 1);
        string_catf(&str, "%d%d %f %p %%!", -120, 69, 0.3f + 0.2f, NULL);
        string_catf_d(&str, "%d%d%%%%%d!", 100, -100, -6969);
        string_catwch(&str, L'空');
        string_catw(&str, L"HELLO 世界!");
        string_cat_str(&str, temp);

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
        string_free(&temp);
    }

    ASSERT_INT_EQ((int)str.len, len);
    ASSERT_MEM_EQ(str.buf, expected, len);
}
TEST_END()
