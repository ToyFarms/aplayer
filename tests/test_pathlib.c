#include "base_test.h"

INCLUDE_BEGIN
#include "pathlib.h"
INCLUDE_END

CFLAGS_BEGIN /*
 -Isrc/include
 src/struct/pathlib.c
 src/logger.c
 src/struct/ds.c
 src/struct/array.c
 */ CFLAGS_END

TEST_BEGIN(normalize)
{
    struct
    {
        char *input;
        char *expected;
    } test_cases[] = {
        {"test/a/test/b",                                        "test/a/test/b/"      },
        {"test/a/test/b/",                                       "test/a/test/b/"      },
        {"/test/a/test/b/",                                      "/test/a/test/b/"     },
        {"/test/",                                               "/test/"              },
        {"/a/",                                                  "/a/"                 },
        {"/a",                                                   "/a/"                 },
        {"a",                                                    "a/"                  },
        {"/a/test/",                                             "/a/test/"            },
        {"/a/test",                                              "/a/test/"            },
        {"a/test/",                                              "a/test/"             },

        {"../test/a/test/b",                                     "../test/a/test/b/"   },
        {"../test/a/test/b/",                                    "../test/a/test/b/"   },
        {"../test/a/test/b/",                                    "../test/a/test/b/"   },
        {"../test/",                                             "../test/"            },
        {"../a/",                                                "../a/"               },
        {"../a",                                                 "../a/"               },
        {"../a",                                                 "../a/"               },
        {"../a/test/",                                           "../a/test/"          },
        {"../a/test",                                            "../a/test/"          },
        {"../a/test/",                                           "../a/test/"          },

        {"../test/../a/test/b",                                  "../test/../a/test/b/"},
        {"../test/../a/test/b/",                                 "../test/../a/test/b/"},
        {"../test/../a/test/b/",                                 "../test/../a/test/b/"},
        {"../test/../",                                          "../test/../"         },
        {"../a/../",                                             "../a/../"            },
        {"../a/..",                                              "../a/../"            },
        {"../a",                                                 "../a/"               },
        {"../a/test/..",                                         "../a/test/../"       },
        {"../a/test/../",                                        "../a/test/../"       },
        {"../a/test/",                                           "../a/test/"          },

        {"../test/../a/test/b/.",                                "../test/../a/test/b/"},
        {"../test/../a/././test/b/",                             "../test/../a/test/b/"},
        {"../test/../a/test/./b/",                               "../test/../a/test/b/"},
        {"/./././././../test/../",                               "/../test/../"        },
        {"./../a/../",                                           "../a/../"            },
        {"./a/..",                                               "a/../"               },
        {"../a/.",                                               "../a/"               },
        {"../a/test/../.",                                       "../a/test/../"       },
        {"../a/./test/../",                                      "../a/test/../"       },
        {".././a/test/",                                         "../a/test/"          },

        {"../\\/\\//////\\//test///../a/test/b///\\//\\//.",
         "../test/../a/test/b/"                                                        },
        {"..////test/../a///././test/b/",                        "../test/../a/test/b/"},
        {"../test/../a/test/./b///////////////////////////////",
         "../test/../a/test/b/"                                                        },
        {"///////////////////////////./././././../test/../",     "/../test/../"        },
        {"./\\\\\\\\\\\\\\\\\\\\\\\\\\/../a/../",                "../a/../"            },
        {"./a/..",                                               "a/../"               },
        {"..\\\\\\\\\\\\\\/\\\\\\\\\\\\\\/a/.",                  "../a/"               },
        {"/..\\a/test/..\\.",                                    "/../a/test/../"      },
        {"../a/./test/../\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\/",     "../a/test/../"       },
        {"..\\.\\a\\test\\",                                     "../a/test/"          },

        {"..",                                                   "../"                 },
        {"../",                                                  "../"                 },
        {".",                                                    "./"                  },
        {"./",                                                   "./"                  },
        {"/.",                                                   "/./"                 },
        {"/./",                                                  "/./"                 },
        {"",                                                     "./"                  },
        {"./..",                                                 "../"                 },
        {"././././././././././././././././././././././././",     "./"                  },
        {".\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\",     "./"                  },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        path_normalize(&path);
        ASSERT_STR_EQ(path.front.buf, test_cases[i].expected,
                      strlen(test_cases[i].expected));
        path_free(&path);
    }
}
TEST_END()

TEST_BEGIN(normalize_stress)
{
    string_t s = string_create();
    const int iter = 1000 * 1000;
    for (int i = 0; i < iter; i++)
    {
        string_cat(&s, "./");
    }

    path_t path = path_create(s.buf);
    path_normalize(&path);

    const char *expected = "./";
    ASSERT_STR_EQ(path.front.buf, expected, strlen(expected));
}
TEST_END()

TEST_BEGIN(normalize_stress2)
{
    string_t s = string_create();
    const int iter = 1000 * 1000;
    for (int i = 0; i < iter; i++)
    {
        string_cat(&s, "/\\/\\./");
    }

    path_t path = path_create(s.buf);
    path_normalize(&path);

    const char *expected = "/./";
    ASSERT_STR_EQ(path.front.buf, expected, strlen(expected));
}
TEST_END()
