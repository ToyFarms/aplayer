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
        {"/a/b/c",    "/a/b/c/"  },
        {"/a/b/../c", "/a/c/"    },
        {"/../",      "/"        },
        {"/./",       "/"        },
        {"/",         "/"        },
        {"./",        "./"       },

        {"a/b/c",     "a/b/c/"   },
        {"a/./b",     "a/b/"     },
        {"a/b/../c",  "a/c/"},
        {"./a/b",     "a/b/"     },
        {"../a/b",    "../a/b/"  },

        {"a/b\\c",    "a/b/c/"   },
        {"/a\\b\\c",  "/a/b/c/"  },

        {"a///b/c",   "a/b/c/"   },
        {"/a//b/",    "/a/b/"    },

        {".",         "./"       },
        {"..",        "../"      },
        {"",          "./"       },
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
    {
        path_t path = path_create(test_cases[i].input);
        ASSERT_STR_EQ(path.front.buf, test_cases[i].expected, path.front.len);
    }
}
TEST_END()
