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
        {"test/a/test/b",                                        "test/a/test/b"      },
        {"test/a/test/b/",                                       "test/a/test/b"      },
        {"/test/a/test/b/",                                      "/test/a/test/b"     },
        {"/test/",                                               "/test"              },
        {"/a/",                                                  "/a"                 },
        {"/a",                                                   "/a"                 },
        {"a",                                                    "a"                  },
        {"/a/test/",                                             "/a/test"            },
        {"/a/test",                                              "/a/test"            },
        {"a/test/",                                              "a/test"             },

        {"../test/a/test/b",                                     "../test/a/test/b"   },
        {"../test/a/test/b/",                                    "../test/a/test/b"   },
        {"../test/a/test/b/",                                    "../test/a/test/b"   },
        {"../test/",                                             "../test"            },
        {"../a/",                                                "../a"               },
        {"../a",                                                 "../a"               },
        {"../a",                                                 "../a"               },
        {"../a/test/",                                           "../a/test"          },
        {"../a/test",                                            "../a/test"          },
        {"../a/test/",                                           "../a/test"          },

        {"../test/../a/test/b",                                  "../test/../a/test/b"},
        {"../test/../a/test/b/",                                 "../test/../a/test/b"},
        {"../test/../a/test/b/",                                 "../test/../a/test/b"},
        {"../test/../",                                          "../test/.."         },
        {"../a/../",                                             "../a/.."            },
        {"../a/..",                                              "../a/.."            },
        {"../a",                                                 "../a"               },
        {"../a/test/..",                                         "../a/test/.."       },
        {"../a/test/../",                                        "../a/test/.."       },
        {"../a/test/",                                           "../a/test"          },

        {"../test/../a/test/b/.",                                "../test/../a/test/b"},
        {"../test/../a/././test/b/",                             "../test/../a/test/b"},
        {"../test/../a/test/./b/",                               "../test/../a/test/b"},
        {"/./././././../test/../",                               "/../test/.."        },
        {"./../a/../",                                           "../a/.."            },
        {"./a/..",                                               "a/.."               },
        {"../a/.",                                               "../a"               },
        {"../a/test/../.",                                       "../a/test/.."       },
        {"../a/./test/../",                                      "../a/test/.."       },
        {".././a/test/",                                         "../a/test"          },

        {"../\\/\\//////\\//test///../a/test/b///\\//\\//.",
         "../test/../a/test/b"                                                        },
        {"..////test/../a///././test/b/",                        "../test/../a/test/b"},
        {"../test/../a/test/./b///////////////////////////////",
         "../test/../a/test/b"                                                        },
        {"///////////////////////////./././././../test/../",     "/../test/.."        },
        {"./\\\\\\\\\\\\\\\\\\\\\\\\\\/../a/../",                "../a/.."            },
        {"./a/..",                                               "a/.."               },
        {"..\\\\\\\\\\\\\\/\\\\\\\\\\\\\\/a/.",                  "../a"               },
        {"/..\\a/test/..\\.",                                    "/../a/test/.."      },
        {"../a/./test/../\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\/",     "../a/test/.."       },
        {"..\\.\\a\\test\\",                                     "../a/test"          },

        {"..",                                                   ".."                 },
        {"../",                                                  ".."                 },
        {".",                                                    "."                  },
        {"./",                                                   "."                  },
        {"/.",                                                   "/."                 },
        {"/./",                                                  "/."                 },
        {"",                                                     "."                  },
        {"./..",                                                 ".."                 },
        {"././././././././././././././././././././././././",     "."                  },
        {".\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\.\\",     "."                  },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        path_normalize(&path);
        path_render(&path);
        ASSERT_STR_EQ(path.front.buf, test_cases[i].expected,
                      _MMAX(path.front.len, strlen(test_cases[i].input)));
        path_free(&path);
    }
}
TEST_END()

TEST_BEGIN(normalize_stress)
{
    str_t s = str_create();
    const int iter = 1000;
    for (int i = 0; i < iter; i++)
    {
        str_cat(&s, "./\\///\\///\\\\.\\");
    }

    path_t path = path_create(s.buf);
    path_normalize(&path);
    path_render(&path);

    const char *expected = ".";
    ASSERT_STR_EQ(path.front.buf, expected,
                  _MMAX(path.front.len, strlen(expected)));
}
TEST_END()

TEST_BEGIN(normalize_stress2)
{
    str_t s = str_create();
    const int iter = 1000;
    for (int i = 0; i < iter; i++)
    {
        str_cat(&s, "/\\/\\./");
    }

    path_t path = path_create(s.buf);
    path_normalize(&path);
    path_render(&path);

    const char *expected = "/.";
    ASSERT_STR_EQ(path.front.buf, expected,
                  _MMAX(path.front.len, strlen(expected)));
}
TEST_END()

TEST_BEGIN(resolve)
{
    char cwd[1024];
#ifdef __linux__
#  include <unistd.h>
    getcwd(cwd, 1024);
#else
#  error "getcwd() not implemented"
#endif // __linux__
    array_t cwd_seg = array_create(16, sizeof(strview_t));
    path_segmentize(cwd, &cwd_seg);

    struct
    {
        char *input;
        int len;
        char *ext;
    } test_cases[] = {
        {"",                                   cwd_seg.length,     ""                },
        {".",                                  cwd_seg.length,     ""                },
        {"..",                                 cwd_seg.length - 1, ""                },
        {"...",                                cwd_seg.length,     "..."             },
        {"....",                               cwd_seg.length,     "...."            },

        {"/",                                  0,                  ""                },
        {"./",                                 cwd_seg.length,     ""                },
        {"../",                                cwd_seg.length - 1, ""                },
        {".../",                               cwd_seg.length,     "..."             },
        {"..../",                              cwd_seg.length,     "...."            },

        {"/.",                                 0,                  ""                },
        {"./.",                                cwd_seg.length,     ""                },
        {"../.",                               cwd_seg.length - 1, ""                },
        {".../.",                              cwd_seg.length,     "..."             },
        {"..../.",                             cwd_seg.length,     "...."            },

        {"/..",                                0,                  ""                },
        {"./..",                               cwd_seg.length - 1, ""                },
        {"../..",                              cwd_seg.length - 2, ""                },
        {".../..",                             cwd_seg.length,     ""                },
        {"..../..",                            cwd_seg.length,     ""                },

        {"../test",                            cwd_seg.length - 1, "test"            },
        {"..////test/",                        cwd_seg.length - 1, "test"            },
        {"../.././././a/b/",                   cwd_seg.length - 2, "a/b"             },
        {"a/b/c/",                             cwd_seg.length,     "a/b/c"           },
        {"\\a/b/c/",                           0,                  "a/b/c"           },
        {"\\///a/b/c/\\//\\.//",               0,                  "a/b/c"           },
        {"\\///a/b/c/\\//\\.//..",             0,                  "a/b"             },
        {"\\///.a/b/c/\\//\\.//..",            0,                  ".a/b"            },
        {"\\///..a/.b/...c/\\//\\.//..",       0,                  "..a/.b"          },
        {".\\///..a/.b/...c/\\//\\.//..",      cwd_seg.length,     "..a/.b"          },
        {".\\///..a/.b/...c/\\//\\.//../..",   cwd_seg.length,     "..a"             },
        {".\\///..a/.b/...c/\\..//\\.//../..", cwd_seg.length,     ""                },
        {"../a/../b/../c/../d/../e",           cwd_seg.length - 1, "e"               },
        {"/../a/../b/../c/../d/../e",          0,                  "e"               },
        {"/../a!!!!/b/?!!!*@&*",               0,                  "a!!!!/b/?!!!*@&*"},
        {"//////////////////////////////////", 0,                  ""                },
        {"\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\", 0,                  ""                },
        {"/././././././././././././././././.", 0,                  ""                },
        {"./././././././././././././././././", cwd_seg.length,     ""                },
        {"././././././././././././././././..", cwd_seg.length - 1, ""                },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        path_resolve(&path);
        path_render(&path);

        size_t offset = path.is_abs;
        for (int j = 0; j < test_cases[i].len; j++)
        {
            strview_t view = ARR_AS(cwd_seg, strview_t)[j];
            ASSERT_STR_EQ(path.front.buf + offset, view.buf, view.len);
            offset += view.len + 1;
        }

        ASSERT_STR_EQ(path.front.buf + offset, test_cases[i].ext,
                      _MMAX(path.front.len, strlen(test_cases[i].ext)));

        path_free(&path);
    }
}
TEST_END()

TEST_BEGIN(absolute)
{
    struct
    {
        char *input;
        bool is_abs;
    } test_cases[] = {
        {"./",              false},
        {"/",               true },
        {"./",              false},
        {"/",               true },
        {"../",             false},
        {"",                false},
        {"/home/user",      true },
        {"home/user",       false},
        {"/home/./user",    true },
        {"../user",         false},
        {"/..",             true },
        {".",               false},
        {"/.",              true },
        {"/user/../bin",    true },
        {"user/./docs",     false},
        {"//server/share",  true },
        {"/tmp///file",     true },
        {"~/Documents",     false},
        {"path/to/file",    false},
        {"/path/to/./file", true },
        {"../..",           false},
        {"/var/../usr",     true },
        {"file.txt",        false},
        {"/etc/passwd",     true },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        ASSERT_TRUE(path.is_abs == test_cases[i].is_abs);

        path_free(&path);
    }
}
TEST_END()

TEST_BEGIN(preserve_input)
{
    struct
    {
        char *input;
        char *expected;
    } test_cases[] = {
        {"./",              "./"             },
        {"/",               "/"              },
        {"./",              "./"             },
        {"/",               "/"              },
        {"../",             "../"            },
        {"",                ""               },
        {"/home/user",      "/home/user"     },
        {"home/user",       "home/user"      },
        {"/home/./user",    "/home/./user"   },
        {"../user",         "../user"        },
        {"/..",             "/.."            },
        {".",               "."              },
        {"/.",              "/."             },
        {"/user/../bin",    "/user/../bin"   },
        {"user/./docs",     "user/./docs"    },
        {"//server/share",  "//server/share" },
        {"/tmp///file",     "/tmp///file"    },
        {"~/Documents",     "~/Documents"    },
        {"path/to/file",    "path/to/file"   },
        {"/path/to/./file", "/path/to/./file"},
        {"../..",           "../.."          },
        {"/var/../usr",     "/var/../usr"    },
        {"file.txt",        "file.txt"       },
        {"/etc/passwd",     "/etc/passwd"    },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        ASSERT_STR_EQ(path.front.buf, test_cases[i].expected,
                      _MMAX(path.front.len, strlen(test_cases[i].expected)));

        path_free(&path);
    }
}
TEST_END()

TEST_BEGIN(render)
{
    struct
    {
        char *input;
        char *expected;
    } test_cases[] = {
        {"./",              "."              },
        {"/",               "/"              },
        {"./",              "."              },
        {"/",               "/"              },
        {"../",             ".."             },
        {"",                ""               },
        {"/home/user",      "/home/user"     },
        {"home/user",       "home/user"      },
        {"/home/./user",    "/home/./user"   },
        {"../user",         "../user"        },
        {"/..",             "/.."            },
        {".",               "."              },
        {"/.",              "/."             },
        {"/user/../bin",    "/user/../bin"   },
        {"user/./docs",     "user/./docs"    },
        {"//server/share",  "/server/share"  },
        {"/tmp///file",     "/tmp/file"      },
        {"~/Documents",     "~/Documents"    },
        {"path/to/file",    "path/to/file"   },
        {"/path/to/./file", "/path/to/./file"},
        {"../..",           "../.."          },
        {"/var/../usr",     "/var/../usr"    },
        {"file.txt",        "file.txt"       },
        {"/etc/passwd",     "/etc/passwd"    },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        path_render(&path);
        ASSERT_STR_EQ(path.front.buf, test_cases[i].expected,
                      _MMAX(path.front.len, strlen(test_cases[i].expected)));

        path_free(&path);
    }
}
TEST_END()

