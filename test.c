// NOTE: still wip, not functional, still a little messy
// use python version instead (for now)
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// TODO: use dict for params
// TODO: use double linked list for ator
// TODO: convert any string building to using str_t, also arrayof(char *) ->
// arrayof(str_t)

// #define ATOR_TRACE
#define DEBUG

#define STATIC_ARR_LEN(arr) (sizeof(arr) / sizeof(*arr))

#define OS_UNKNOWN 0
#define OS_WIN     1
#define OS_LINUX   2

#if defined(_WIN32) || defined(_WIN64)
#  define OS OS_WIN
#elif defined(__linux__)
#  define OS OS_LINUX
#else
#  define OS OS_UNKNOWN
#endif

#if OS == OS_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  include <io.h>
#endif // OS == OS_LINUX

/* TW: MACRO
 * https://mailund.dk/posts/macro-metaprogramming/
 * https://stackoverflow.com/a/45586169
 * */
#define PP_FIRST(a, ...)     a
#define PP_SECOND(a, b, ...) b

#define PP_EXPR_TO_BIN(expr) PP_SECOND(expr, 0)

#define PP_CONCAT(a, b)  a##b
#define PP_NOT(expr)     PP_EXPR_TO_BIN(PP_CONCAT(PP_NOT_, expr))
#define PP_IS_TRUE(expr) -, 1
#define PP_NOT_0         PP_IS_TRUE()

#define PP_BOOL(expr) PP_NOT(PP_NOT(expr))

#define PP_IF_ELSE_HELPER(expr) PP_CONCAT(PP_IF_, expr)
#define PP_IF_ELSE(expr)        PP_IF_ELSE_HELPER(PP_BOOL(expr))
#define PP_IF_0(...)            PP_ELSE_0
#define PP_IF_1(...)            __VA_ARGS__ PP_ELSE_1
#define PP_ELSE_0(...)          __VA_ARGS__
#define PP_ELSE_1(...)

#define PP_END_OF_ARGUMENTS() 0
#define PP_IS_NULL(...)       PP_EXPR_TO_BIN(PP_FIRST(PP_IS_TRUE __VA_ARGS__)())
#define PP_HAS_ARGS(...)      PP_BOOL(PP_FIRST(PP_END_OF_ARGUMENTS __VA_ARGS__)())

#define PP_EXPAND_1(...)  __VA_ARGS__
#define PP_EXPAND_2(...)  PP_EXPAND_1(PP_EXPAND_1(__VA_ARGS__))
#define PP_EXPAND_4(...)  PP_EXPAND_2(PP_EXPAND_2(__VA_ARGS__))
#define PP_EXPAND_8(...)  PP_EXPAND_4(PP_EXPAND_4(__VA_ARGS__))
#define PP_EXPAND_16(...) PP_EXPAND_8(PP_EXPAND_8(__VA_ARGS__))
#define PP_EXPAND_32(...) PP_EXPAND_16(PP_EXPAND_16(__VA_ARGS__))
#define PP_EXPAND_64(...) PP_EXPAND_32(PP_EXPAND_32(__VA_ARGS__))
#define PP_EVAL           PP_EXPAND_64

#define PP_EMPTY()
#define PP_DEFER2(f) f PP_EMPTY PP_EMPTY()()

#define PP_MAP_HELPER(fun, arg, ...)                                           \
    fun(arg);                                                                  \
    PP_IF_ELSE(PP_HAS_ARGS(__VA_ARGS__))                                       \
    (PP_DEFER2(PP_MAP_HELPER2)()(fun, __VA_ARGS__))()
#define PP_MAP_HELPER2() PP_MAP_HELPER

#define PP_MAP(fun, ...) PP_EVAL(PP_MAP_HELPER(fun, __VA_ARGS__))

#define UNWRAP_OR(expr, or)                                                    \
    ({                                                                         \
        errno = 0;                                                             \
        typeof(expr) _v = (expr);                                              \
        if (_v == NULL)                                                        \
        {                                                                      \
            if (errno != 0)                                                    \
                fprintf(stderr, "%s:%d in %s: '%s' is NULL. desc='%s'\n",      \
                        __FILE__, __LINE__, __FUNCTION__, #expr,               \
                        strerror(errno));                                      \
            else                                                               \
                fprintf(stderr, "%s:%d in %s: '%s' is NULL\n", __FILE__,       \
                        __LINE__, __FUNCTION__, #expr);                        \
            or;                                                                \
        }                                                                      \
        _v;                                                                    \
    })

#define UNWRAP_OR_PANIC(expr) UNWRAP_OR(expr, exit(EXIT_FAILURE))

#define TRUTHY_OR(expr, or)                                                    \
    ({                                                                         \
        errno = 0;                                                             \
        typeof(expr) _v = !!(expr);                                            \
        if (!_v)                                                               \
        {                                                                      \
            if (errno != 0)                                                    \
                fprintf(stderr, "%s:%d in %s: '%s' is FALSEY. desc='%s'\n",    \
                        __FILE__, __LINE__, __FUNCTION__, #expr,               \
                        strerror(errno));                                      \
            else                                                               \
                fprintf(stderr, "%s:%d in %s: '%s' is FALSEY\n", __FILE__,     \
                        __LINE__, __FUNCTION__, #expr);                        \
            or;                                                                \
        }                                                                      \
        _v;                                                                    \
    })

#define TRUTHY_OR_PANIC(expr) TRUTHY_OR(expr, exit(EXIT_FAILURE))

typedef struct ator_t ator_t;

typedef struct ator_t
{
    ator_t *head;
    ator_t *tail;
    ator_t *next;

    void *ptr;
    void (*dtor)(void *);

    uint32_t flags;
} ator_t;

#define ATOR_REFPTR  (1 << 0) // dont free the internal ptr
#define ATOR_MEMZERO (1 << 1) // use calloc instead of malloc
#define ATOR_DEBUG   (1 << 2) // set the debug flag for a node
#define ATOR_NONROOT                                                           \
    (1 << 2) // should always be set for non-root, heap allocated node

typedef void (*dtor_fn)(void *);

ator_t *_ator_new(size_t size, dtor_fn dtor, uint32_t flags);
void _ator_ref(ator_t *ator, void *ptr, dtor_fn dtor, uint32_t flags);
void *_ator_alloc(ator_t *ator, size_t size, dtor_fn dtor, uint32_t flags);
void *_ator_memdup(ator_t *ator, void *ptr, size_t size, dtor_fn dtor,
                   uint32_t flags);
void *_ator_strdup(ator_t *ator, const char *str);
ator_t *_ator_free_single(ator_t **ator);
void _ator_free(ator_t *ator);
// free until encounter `end` (inclusive)
void _ator_free_until(ator_t **ator, ator_t *end);
void _ator_join(ator_t *ator, ator_t *new);
void _ator_print(ator_t *ator);
char *_ator_snprintf(ator_t *ator, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

typedef struct _ator_ptr
{
    void *ptr;
    size_t size;
} _ator_ptr;

typedef struct ator_stats
{
    size_t allocated;
    _ator_ptr *pointers;
    size_t len;
    size_t cap;
    int atexit;
} ator_stats;

void _ator_stat_append(ator_stats *stats, _ator_ptr *ptr)
{
    if (stats->pointers == NULL)
    {
        stats->pointers = calloc(128, sizeof(_ator_ptr));
        stats->len = 0;
        stats->cap = 128;
    }
    else if (stats->len + 1 > stats->cap)
    {
        stats->pointers =
            realloc(stats->pointers, stats->cap * 2 * sizeof(_ator_ptr));
        stats->cap *= 2;
        memset(stats->pointers + stats->len, 0,
               (stats->cap - stats->len) * sizeof(_ator_ptr));
    }

    memcpy(stats->pointers + stats->len, ptr, sizeof(_ator_ptr));
    stats->len++;
}

size_t _ator_stat_get(ator_stats *stats, void *ptr)
{
    for (size_t i = 0; i < stats->len; i++)
    {
        if (ptr == stats->pointers[i].ptr)
            return stats->pointers[i].size;
    }

    return 0;
}

ator_stats stats = {0};

void stats_free(void)
{
    if (stats.pointers)
    {
        free(stats.pointers);
        stats.pointers = NULL;
    }
}

#ifdef ATOR_TRACE
#  define WRAP_TRACE(expr)                                                     \
      ({                                                                       \
        printf("%s:%d in %s: \x1b[33m%s\x1b[0m\n", __FILE__, __LINE__,         \
               __FUNCTION__, #expr);                                           \
        expr;                                                                  \
      })
#  define WRAP_ALLOC(_size, expr)                                              \
      ({                                                                       \
        stats.allocated += _size;                                              \
        void *_ptr = expr;                                                     \
        printf("[\x1b[34m%p\x1b[0m] \x1b[32m+%-8zu / %-8zu\x1b[0m bytes "      \
               "%s:%d in %s: "                                                 \
               "\x1b[33m%s\x1b[0m\n",                                          \
               (void *)_ptr, (size_t)_size, stats.allocated, __FILE__,         \
               __LINE__, __FUNCTION__, #expr);                                 \
        if (!stats.atexit)                                                     \
        {                                                                      \
            atexit(stats_free);                                                \
            stats.atexit = 1;                                                  \
        }                                                                      \
        _ator_stat_append(&stats, &(_ator_ptr){.ptr = _ptr, .size = _size});   \
        _ptr;                                                                  \
      })
#  define WRAP_FREE(ator, expr)                                                \
      ({                                                                       \
        size_t s = _ator_stat_get(&stats, (void *)((ator)->ptr));              \
        stats.allocated -= s;                                                  \
        printf(                                                                \
            "[%s%s][\x1b[34m%12p\x1b[0m] \x1b[31m-%-8zu / %-8zu\x1b[0m bytes " \
            "%s:%d in %s: "                                                    \
            "\x1b[33m%s\x1b[0m\n",                                             \
            (ator)->flags & ATOR_MEMZERO ? "MEMZERO" : "",                     \
            (ator)->flags & ATOR_REFPTR ? ",REFPTR" : "",                      \
            (void *)((ator)->ptr), s, stats.allocated, __FILE__, __LINE__,     \
            __FUNCTION__, #expr);                                              \
        expr;                                                                  \
      })
#else

#  define WRAP_TRACE(expr)       expr
#  define WRAP_ALLOC(size, expr) expr
#  define WRAP_FREE(ptr, expr)   expr
#endif // ATOR_TRACE

#define ator_new(size, dtor, flags) WRAP_TRACE(_ator_new(size, dtor, flags))
#define ator_ref(ator, ptr, dtor, flags)                                       \
    WRAP_TRACE(_ator_ref(ator, ptr, dtor, flags))
#define ator_alloc(ator, size, dtor, flags)                                    \
    WRAP_ALLOC(size, _ator_alloc(ator, size, dtor, flags))
#define ator_memdup(ator, ptr, size, dtor, flags)                              \
    WRAP_ALLOC(size, _ator_memdup(ator, ptr, size, dtor, flags))
#define ator_strdup(ator, str)                                                 \
    WRAP_ALLOC(strlen(str), _ator_memdup(ator, (void *)str, strlen(str), NULL, \
                                         ATOR_MEMZERO))
#define ator_strndup(ator, str, n)                                             \
    WRAP_ALLOC(n, _ator_memdup(ator, (void *)str, n, NULL, ATOR_MEMZERO))
#define ator_free_single(ator)     WRAP_FREE(*ator, _ator_free_single(ator))
#define ator_free(ator)            WRAP_TRACE(_ator_free(ator))
#define ator_free_until(ator, end) WRAP_TRACE(_ator_free_until(ator, end))
#define ator_join(ator, new)       WRAP_TRACE(_ator_join(ator, new))
#define ator_print(ator)           WRAP_TRACE(_ator_print(ator))
#define ator_snprintf(ator, fmt, ...)                                          \
    WRAP_TRACE(_ator_snprintf(ator, fmt, ##__VA_ARGS__))

#define reg_dtor(T, name, ator, dtor)                                          \
    ({                                                                         \
        if (ator != NULL)                                                      \
            ator_ref(ator, &name, (void *)dtor, 0);                            \
        (T){0};                                                                \
    })

#define new_queue(name, ator)                                                  \
    ({                                                                         \
        queue_t __Q = reg_dtor(queue_t, name, ator, queue_free);               \
        queue_init(&__Q);                                                      \
        __Q;                                                                   \
    })
#define new_array(name, ator) reg_dtor(array_t, name, ator, array_free)
#define new_dict(name, ator)                                                   \
    ({                                                                         \
        dict_t __Q = reg_dtor(dict_t, name, ator, dict_free);                  \
        dict_init(&__Q);                                                       \
        __Q;                                                                   \
    })
#define new_str(name, ator)                                                    \
    ({                                                                         \
        str_t __Q = reg_dtor(str_t, name, ator, str_free);                     \
        str_init(&__Q);                                                        \
        __Q;                                                                   \
    })
#define new_strn(name, size, ator)                                             \
    ({                                                                         \
        str_t __Q = reg_dtor(str_t, name, ator, str_free);                     \
        str_initn(&__Q, size);                                                 \
        __Q;                                                                   \
    })

#define new(name, ator)                                                        \
    _Generic((name),                                                           \
        queue_t: new_queue(name, ator),                                        \
        array_t: new_array(name, ator),                                        \
        dict_t: new_dict(name, ator),                                          \
        str_t: new_str(name, ator))

#define newn(name, size, ator)                                                 \
    _Generic((name), str_t: new_strn(name, size, ator))

#define arrayof(T) array_t
#define queueof(T) queue_t
#define dictof(T)  dict_t

typedef struct fs_iterator
{
    char *dir;
#if OS == OS_LINUX
    DIR *d;
#else
#  error "NOT IMPLEMENTED"
#endif // OS == OS_LINUX
    int exhausted;
} fs_iterator;

typedef struct fs_entry
{
    char *path;
    struct stat stat;
} fs_entry;

int fs_iter_init(fs_iterator *iter, char *dir, ator_t *ator);
fs_entry *fs_iter_next(fs_iterator *iter, ator_t *ator);
int fs_entry_isdir(const fs_entry *entry);
char *fs_file_read(const char *path, ator_t *ator);

typedef struct array_t
{
    void *items;
    size_t item_size;
    size_t len;
    size_t cap;
} array_t;

#define ARRAY_DEFAULT_SIZE 8

#define ARR_AS(arr, T) ((T *)((arr).items))
#define ARR_FOREACH(arr, elm, i)                                               \
    for (size_t i = 0;                                                         \
         i < (arr).len && (elm = ARR_AS(arr, typeof(elm))[i], 1); i++)
#define ARR_FOREACH_BYREF(arr, elm, i)                                         \
    for (size_t i = 0;                                                         \
         i < (arr).len && (elm = &(ARR_AS(arr, typeof(*elm)))[i], 1); i++)

void array_init(array_t *arr, size_t item_size);
void array_init_n(array_t *arr, size_t item_size, size_t length);
void array_free(array_t *arr);
void array_reserve(array_t *arr, size_t item_size, size_t nb_items);
void array_append(array_t *arr, const void *mem, size_t item_size);
void array_append_many(array_t *arr, const void *mem, size_t item_size,
                       size_t nb_items);
void array_extend(array_t *arr, const array_t *source);
void *array_pop(array_t *arr, int64_t index);
void array_insert(array_t *arr, const void *mem, size_t item_size,
                  size_t index);
void array_overwrite(array_t *arr, const void *mem, size_t item_size,
                     size_t index);

typedef struct path_segment
{
    char *buf;
    size_t size;
    uint32_t flags;
} path_segment;

#define SEGMENT_DRIVE (1 << 0)

#if OS == OS_WIN
#  define PATH_SEP "\\"
#else
#  define PATH_SEP "/"
#endif // OS == OS_WIN

#define path_join(ator, ...)                                                   \
    _path_join(ator, sizeof((void *[]){__VA_ARGS__}) / sizeof(void *),         \
               __VA_ARGS__)

char *_path_join(ator_t *ator, int count, ...);
size_t path_segment_size(const char *path, size_t *out_nb_segments);
size_t path_segments_size(const arrayof(path_segment) * segments);
void path_split(char *path, arrayof(path_segment) * out);
size_t path_skip_drive_segment(char *path, size_t size,
                               path_segment *drive_letter);
int path_is_sep(char c);
char *path_from_segments(const arrayof(path_segment) * segments, ator_t *ator);
void path_segments_print(const arrayof(path_segment) * segments);
const char *path_name(const char *path);
const char *path_stem(ator_t *ator, const char *path);
int path_exists(const char *path);
int path_isdir(const char *path);
int path_mkdir(char *path, mode_t mode, int exists_ok);
int path_touch(const char *path, mode_t mode);

uint64_t siphash24k(const void *src, unsigned long src_sz, const char key[16]);
uint64_t siphash24(const void *src, unsigned long src_sz);
void _print_raw(const char *name, const void *mem, size_t size, int group,
                int columns);
#define print_raw(mem, size) _print_raw(#mem, mem, size, 4, 4);

typedef struct program_args
{
    int64_t jobs;
    char *target;

    int rebuild;
} program_args;

void get_tests_file(arrayof(fs_entry) * out);

typedef struct file_t
{
    FILE *handle;
    const char *path;
    size_t size;

    char *buffer;
    size_t buffer_size;
    size_t buffer_cap;

    pthread_mutex_t mutex;
    int eof;
} file_t;

size_t file_get_size(file_t *file);
void file_free(file_t *file);
void file_open(file_t *file, const char *path, char *mode, ator_t *ator);
void file_prepare_buffer(file_t *file, size_t requested_size);
size_t file_read(file_t *file, size_t nbytes);
char *file_readline(file_t *file, int skip_nl);
int file_seek(file_t *file, size_t off, int whence);
long file_write(file_t *file, const void *data, ssize_t size);

typedef struct filepos
{
    int start, end;
    size_t offset, size;
} filepos;

typedef struct named_filepos
{
    const char *name;
    filepos pos;
} named_filepos;

typedef struct testunit
{
    file_t file;
    arrayof(named_filepos) * config;
    arrayof(testcase) * cases;
} testunit;

typedef struct testcase
{
    file_t *file;
    filepos pos;

    char *test_name;
    arrayof(char *) params;

    char *src_path;
    char *bin_path;
    char *repr_name;
} testcase;

#define PARAM_LIST(DO)                                                         \
    DO(INCLUDE, include)                                                       \
    DO(CFLAGS, cflags)

#define TEST_START_WORD "TEST_BEGIN"
#define TEST_END_WORD   "TEST_END"

int testunit_create(testunit *unit, const char *file, ator_t *ator);

int parse_args(char **args, int argc, program_args *out, ator_t *ator);
char *skip_whitespace(char *s);
size_t skip_whitespace_reversed(char *s);
int is_newline(char *s);
void split(const char *s, const char *delim, arrayof(char *) * out,
           ator_t *ator);
void splitlines(const char *s, arrayof(char *) * out, ator_t *ator);
#if OS == OS_WIN
#  define NLCH "\r\n"
#elif OS == OS_LINUX
#  define NLCH "\n"
#else
#  define NLCH "\n"
#endif // OS == OS_WIN
char *join(const arrayof(char *) * in, const char *delim, ator_t *ator);

#define STRIP(s)                                                               \
    (int)(strlen(skip_whitespace(s)) - (strlen(s) - 1) -                       \
          skip_whitespace_reversed(s)),                                        \
        skip_whitespace(s)

typedef struct queue_entry_t
{
    struct queue_entry_t *next;
    void *data;
} queue_entry_t;

typedef struct queue_t
{
    queue_entry_t *head;
    queue_entry_t *tail;
    size_t len;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} queue_t;

#define QUEUE_IS_EMPTY(q) ((q)->head == NULL || (q)->len <= 0)

void queue_init(queue_t *q);
void queue_free(queue_t *q);
void queue_terminate(queue_t *q);
int queue_push(queue_t *q, void *data);
int queue_push_copy(queue_t *q, const void *data, size_t size);
void *queue_pop(queue_t *q);
void *queue_pop_nonblock(queue_t *q);
void queue_clear(queue_t *q);

typedef struct dict_item
{
    char *key;
    void *data;
    int self_owned;
} dict_item;

typedef uint64_t (*dict_hash_fn)(const char *key, size_t len);
typedef int (*dict_strcmp_fn)(const char *a, const char *b);

typedef struct dict_t
{
    arrayof(dict_item) * buckets;
    int bucket_slot;
    int length;

    dict_hash_fn hash;
    dict_strcmp_fn strcmp;
} dict_t;

#define DICT_FOREACH(dict, key_var, item_var, index_var)                       \
    for (int __i = 0, __attribute__((unused)) index_var = -1;                  \
         __i < (dict).bucket_slot; __i++)                                      \
    {                                                                          \
        __attribute__((unused)) char *key_var = NULL;                          \
        array_t __bkt = (dict).buckets[__i];                                   \
        dict_item *__item;                                                     \
        ARR_FOREACH_BYREF(__bkt, __item, __j)                                  \
        {                                                                      \
            index_var++;                                                       \
            key_var = __item->key;                                             \
            item_var = __item->data;

#define DICT_FOREACH_END()                                                     \
    }                                                                          \
    }

void dict_init(dict_t *dict);
void dict_free(dict_t *dict);
void dict_clear(dict_t *dict);
void dict_resize(dict_t *dict, int new_size);
dict_item *dict_insert(dict_t *dict, const char *key, void *item);
void dict_insert_copy(dict_t *dict, const char *key, void *item, size_t size);
void *dict_get(dict_t *dict, const char *key, void *default_return);
float dict_getload(dict_t *dict);
int dict_exists(dict_t *dict, const char *key);

typedef struct str_t
{
    char *buf;
    size_t len;
    size_t capacity;
} str_t;

typedef struct strview_t
{
    char *start;
    size_t size;
} strview_t;

#define str_c(s)                                                               \
    (str_t)                                                                    \
    {                                                                          \
        .buf = (s), .len = strlen(s), .capacity = strlen(s)                    \
    }

void str_init(str_t *str);
void str_initn(str_t *str, size_t size);
void str_free(str_t *str);
str_t *str_resize(str_t *str, size_t new);
str_t str_from_view(strview_t view, ator_t *ator);

str_t *str_cat(str_t *str, const char *s);
str_t *str_catch(str_t *str, char ch);
str_t *str_catlen(str_t *str, const char *s, size_t len);
str_t *str_cat_str(str_t *dst, const str_t *src);
str_t *str_catf_d(str_t *str, const char *fmt, ...);
str_t *str_catf(str_t *str, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
str_t *str_catfv(str_t *str, const char *fmt, va_list args);
str_t *str_catw(str_t *str, wchar_t *ws);
str_t *str_catwch(str_t *str, wchar_t wc);
str_t *str_pad(str_t *str, char ch, size_t size);
strview_t str_strip(str_t *str);
str_t *str_join(str_t *str, const arrayof(char *) * srcs, const str_t *sep);

typedef struct task_t
{
    void *(*task_fn)(void *arg);
    void *arg;
    struct task_t *next;
} task_t;

typedef struct thread_pool
{
    pthread_mutex_t mutex;
    pthread_cond_t all_done;

    queueof(task_t) tasks;
    ssize_t task_cap;
    size_t task_active;

    arrayof(pthread_t) threads;
} thread_pool;

int pool_init(thread_pool *pool, int nb_threads, ssize_t task_cap);
int pool_enqueue_task(thread_pool *pool, void *(*task)(void *arg), void *arg);
void pool_wait(thread_pool *pool);
void pool_free(thread_pool *pool);

void generate_source(testunit *unit, testcase *tcase);

typedef struct _build_params_tuple
{
    testunit *unit;
    testcase *tcase;
    thread_pool *pool;
    int rebuild;
} _build_params_tuple;

uint64_t testcase_hash(testunit *unit, testcase *tcase);
void gen_cc_args(testunit *unit, testcase *tcase, arrayof(char *) * out,
                 ator_t *ator);
void gen_cc_argstr(str_t *out, arrayof(char *) * args_optional, testunit *unit,
                   testcase *tcase, ator_t *ator);
void *build_and_run(void *arg);
void *run(void *arg);

enum telemetry_event_type
{
    TELEMETRY_EVENT_STATUS,
    TELEMETRY_EVENT_BUILD_TIME,
    TELEMETRY_EVENT_TASK_REGISTER,
    TELEMETRY_EVENT_TASK_UNREGISTER,
    TELEMETRY_EVENT_THREAD_REGISTER,
    TELEMETRY_EVENT_LOG,
};

enum telemetry_status_type
{
    TELEMETRY_STATUS_BUILD,
    TELEMETRY_STATUS_RUN,
    TELEMETRY_STATUS_IDLE,
    TELEMETRY_STATUS_FAIL,
    TELEMETRY_STATUS_LEN,
};

const char *telemetry_status_str[] = {"BUILD", "RUN", "IDLE", "FAIL"};

enum telemetry_task_type
{
    TELEMETRY_TASK_BUILD,
    TELEMETRY_TASK_RUN,
    TELEMETRY_TASK_LEN
};

const char *telemetry_task_str[] = {"BUILD", "RUN"};

typedef struct telemetry_event
{
    unsigned long sender;
    enum telemetry_event_type type;
    union {
        int status;
        uint64_t build_time;
        enum telemetry_task_type task_type;
        char *log; // NOTE: dont forget to free
    };
} telemetry_event;

queueof(telemetry_event) ebus = {0};
pthread_t telemetry_tid = 0;
void *telemetry_thread(void *arg);
#define telemetry_send(...)                                                    \
    _telemetry_send((telemetry_event){.sender = pthread_self(), __VA_ARGS__})
void _telemetry_send(telemetry_event ev);
void __attribute__((format(printf, 1, 2))) telemetry_log(char *fmt, ...);

int program_shutdown = 0;

int main(int argc, char **argv)
{
    if (!path_exists("tests") || !path_isdir("tests"))
    {
        fprintf(stderr, "No tests/ directory found\n");
        return EXIT_FAILURE;
    }

    ator_t ator = {0};
    program_args args = {.rebuild = 0, .target = "*", .jobs = 4};
    if (parse_args(argv, argc, &args, &ator) < 0)
    {
        fprintf(stderr, "Failed to parse arguments\n");
        return EXIT_FAILURE;
    }

    ebus = new (ebus, &ator);
    pthread_create(&telemetry_tid, NULL, telemetry_thread, &ebus);

    arrayof(testunit) units = new (units, &ator);

#if 0
    {
        testunit *unit =
            ator_alloc(&ator, sizeof(testunit), NULL, ATOR_MEMZERO);
        if (testunit_create(unit, "tests/test_ringbuf.c", &ator) < 0)
        {
            fprintf(stderr, "FAILED TO CREATE TEST UNIT\n");
            return EXIT_FAILURE;
        }
        array_append(&units, unit, sizeof(testunit));
    }
#else
    {
        ator_t iter_ator = {0};

        fs_iterator iter = {0};
        fs_iter_init(&iter, "tests", &iter_ator);

        fs_entry *entry;
        while ((entry = fs_iter_next(&iter, &iter_ator)))
        {
            testunit *unit =
                ator_alloc(&ator, sizeof(testunit), NULL, ATOR_MEMZERO);
            if (testunit_create(unit, ator_strdup(&ator, entry->path), &ator) <
                0)
                continue;

            array_append(&units, unit, sizeof(testunit));
        }

        ator_free(&iter_ator);
    }
#endif

    path_mkdir("tests/out/src", 0777, 1);
    path_mkdir("tests/out/bin", 0777, 1);

    testunit *unit;
    ARR_FOREACH_BYREF(units, unit, i)
    {
        testcase *tcase;
        ARR_FOREACH_BYREF(*unit->cases, tcase, j)
        {
            generate_source(unit, tcase);
        }
    }

    thread_pool pool = {0};
    pool_init(&pool, args.jobs, -1);

    ARR_FOREACH_BYREF(units, unit, i)
    {
        testcase *tcase;
        ARR_FOREACH_BYREF(*unit->cases, tcase, j)
        {
            _build_params_tuple *params =
                ator_alloc(&ator, sizeof(*params), NULL, ATOR_MEMZERO);
            params->unit = unit;
            params->tcase = tcase;
            params->pool = &pool;
            params->rebuild = args.rebuild;

            telemetry_send(.type = TELEMETRY_EVENT_TASK_REGISTER,
                           .task_type = TELEMETRY_TASK_BUILD);
            pool_enqueue_task(&pool, build_and_run, params);
        }
    }

    pool_wait(&pool);
    program_shutdown = 1;

    pool_free(&pool);
    queue_terminate(&ebus);
    pthread_join(telemetry_tid, NULL);

    ator_free(&ator);
    return 0;
}

char *telekey(int64_t num, char *buf)
{
    size_t i = 0;
    for (; i < sizeof(num); i++)
    {
        buf[i] = (num >> (i * 8)) & 0xFF;
        if (buf[i] == 0)
            buf[i] = 0xFF;
    }
    buf[i] = '\0';

    return buf;
}

typedef struct thread_state
{
    int thread_id;
    int status;
} thread_state;

typedef struct telemetry_state
{
    int nb_thread;
    dictof(thread_state) threads;
    arrayof(int) nb_tasks;
    char *log;
} telemetry_state;

void catpad(str_t *str, char *s)
{
    // va_list args;
    //
    // va_start(args, fmt);
    // size_t needed = vsnprintf(NULL, 0, fmt, args);
    // va_end(args);
    //
    // char src[needed + 1];
    // memset(src, 0, needed + 1);
    //
    // va_start(args, fmt);
    // vsprintf(src, fmt, args);
    // va_end(args);
    //
    // src[needed] = '\0';
    //
    // if (src[needed - 1] == '\n')
    //     str_catlen(str, src, --needed);
    // else
    //     str_cat(str, src);

    str_cat(str, s);

    struct winsize w = {0};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    str_pad(str, ' ', w.ws_col - strlen(s));
}

void telemetry_update(telemetry_state *state)
{
    ator_t ator = {0};
    str_t str = new (str, &ator);

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    int height = 0;

    if (state->log)
        catpad(&str, state->log);

    str_pad(&str, ' ', w.ws_col - 1);
    str_catch(&str, '\n');
    height++;

    {
        str_t temp = new (temp, NULL);

        thread_state *th;
        size_t start_pos = temp.len;
        DICT_FOREACH(state->threads, key, th, i)
        {
            size_t before_concat = temp.len;
            str_catf(&temp, "[THREAD%d: %s]    ", th->thread_id,
                     telemetry_status_str[th->status]);

            if (temp.len - start_pos > w.ws_col)
            {
                temp.len = before_concat;
                temp.buf[temp.len] = '\0';
                catpad(&str, temp.buf);
                height++;

                temp.len = 0;
                str_catf(&temp, "[THREAD%d: %s]    ", th->thread_id,
                         telemetry_status_str[th->status]);

                start_pos = 0;
            }
        }
        DICT_FOREACH_END()

        if (temp.len > 0)
        {
            catpad(&str, temp.buf);
            str_catch(&str, '\n');
            height++;
        }

        str_free(&temp);
    }

    for (int i = 0; i < TELEMETRY_TASK_LEN; i++)
    {
        str_t temp = new (temp, NULL);

        int nb_task = ARR_AS(state->nb_tasks, int)[i];

        str_catf(&temp, "TASK %s: ", telemetry_task_str[i]);
        str_pad(&temp, '#', nb_task);

        catpad(&str, temp.buf);
        str_catch(&str, '\n');
        height++;

        str_free(&temp);
    }

    str_catf(&str, "\x1b[%dF", height);

    printf("%s", str.buf);
    fflush(stdout);

    ator_free(&ator);
}

void *telemetry_thread(void *arg)
{
    queueof(telemetry_event) *ebus = (queue_t *)arg;

    ator_t ator = {0};
    telemetry_state state = {0};
    state.threads = new (state.threads, &ator);
    state.nb_tasks = new (state.nb_tasks, &ator);
    array_init_n(&state.nb_tasks, sizeof(int),
                 TELEMETRY_TASK_LEN > 32 ? TELEMETRY_TASK_LEN : 32);

    char key[16] = {0};

    for (;;)
    {
        telemetry_event *event = queue_pop(ebus);
        if (event == NULL)
            break;

        thread_state *thread =
            dict_get(&state.threads, telekey(event->sender, key), NULL);

        switch (event->type)
        {
        case TELEMETRY_EVENT_STATUS:
            if (thread == NULL)
            {
                fprintf(stderr, "Thread is not registered\n");
                continue;
            }
            thread->status = event->status;
            break;
        case TELEMETRY_EVENT_BUILD_TIME:
            printf("%lu BUILD TIME: %lu\n", event->sender, event->build_time);
            break;
        case TELEMETRY_EVENT_TASK_REGISTER:
            ARR_AS(state.nb_tasks, int)[event->task_type]++;
            break;
        case TELEMETRY_EVENT_TASK_UNREGISTER:
            if (thread == NULL)
            {
                fprintf(stderr, "Thread is not registered\n");
                continue;
            }
            thread->status = TELEMETRY_STATUS_IDLE;
            ARR_AS(state.nb_tasks, int)[event->task_type]--;
            break;
        case TELEMETRY_EVENT_THREAD_REGISTER:
            dict_insert_copy(&state.threads, telekey(event->sender, key),
                             &(thread_state){.thread_id = state.nb_thread++},
                             sizeof(thread_state));
            break;
        case TELEMETRY_EVENT_LOG:
            state.log = event->log;
            break;
        }

        free(event);

        telemetry_update(&state);
        if (state.log)
        {
            free(state.log);
            state.log = NULL;
        }
    }

    ator_free(&ator);
    return NULL;
}

void _telemetry_send(telemetry_event ev)
{
    telemetry_event *ev_ptr = UNWRAP_OR_PANIC(malloc(sizeof(*ev_ptr)));
    memcpy(ev_ptr, &ev, sizeof(*ev_ptr));

    queue_push(&ebus, ev_ptr);
}

void telemetry_log(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    size_t needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    va_start(args, fmt);

    char *buffer = malloc(needed + 1);
    vsnprintf(buffer, needed + 1, fmt, args);

    va_end(args);

    telemetry_send(.type = TELEMETRY_EVENT_LOG, .log = buffer);
}

// clang-format off
const char *test_base =
"// INCLUDE_BEGIN\n"
"// #include ...\n"
"// INCLUDE_END\n"
"//\n"
"// CFLAGS_BEGIN/*\n"
"// -Isrc/include\n"
"// */CFLAGS_END\n"
"//\n"
"// TEST_BEGIN(init)\n"
"// {\n"
"//     ASSERT_TRUE(0 == 0);\n"
"// }\n"
"// TEST_END()\n"
"\n"
"#ifndef __BASE_H\n"
"#define __BASE_H\n"
"\n"
"#define TEST_BEGIN(name, ...) void testns_##name()\n"
"#define TEST_END(...)\n"
"#define CFLAGS_BEGIN\n"
"#define CFLAGS_END\n"
"#define INCLUDE_BEGIN\n"
"#define INCLUDE_END\n"
"\n"
"// #define __FILENAME__                                                           \\\n"
"//     (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)\n"
"\n"
"#define test_init_setlocale setlocale(LC_ALL, \"\")\n"
"#define test_init_initlogger                                                   \\\n"
"    logger_set_level(LOG_DEBUG);                                               \\\n"
"    logger_add_output(-1, fopen(\"tests/logs/\" __FILE_NAME__ \".log\", \"a\"),      \\\n"
"                      LOG_DEFER_CLOSE);\n"
"\n"
"#include \"logger.h\"\n"
"#include <ctype.h>\n"
"#include <errno.h>\n"
"#include <locale.h>\n"
"#include <math.h>\n"
"#include <stdio.h>\n"
"#include <stdlib.h>\n"
"#include <string.h>\n"
"\n"
"void print_memory(const void *addr, size_t n)\n"
"{\n"
"    const unsigned char *ptr = addr;\n"
"\n"
"    for (size_t i = 0; i < n; i++)\n"
"    {\n"
"        if (isprint(ptr[i]))\n"
"            fprintf(stderr, \"%c \", ptr[i]);\n"
"        else\n"
"            fprintf(stderr, \"%02X \", ptr[i]);\n"
"    }\n"
"}\n"
"\n"
"#define _assert(expr)                                                          \\\n"
"    if (!(expr))                                                               \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, \"Assertion failed: '%s' in %s:%d\", #expr, __FILE__,    \\\n"
"                __LINE__);                                                     \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"\n"
"#define ASSERT_TRUE(a) _assert(a)\n"
"#define _ASSERT_NUM(a, b, fmt, nop, name)                                      \\\n"
"    if ((a)nop(b))                                                             \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, \"%s(%s<%d> %s %s<%d>) %s:%d\", name, #a, (a), #nop, #b, \\\n"
"                (b), __FILE__, __LINE__);                                      \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"\n"
"#define ASSERT_INT_EQ(a, b)  _ASSERT_NUM(a, b, \"%d\", !=, \"INT_EQ\")\n"
"#define ASSERT_INT_NEQ(a, b) _ASSERT_NUM(a, b, \"%d\", ==, \"INT_NEQ\")\n"
"#define ASSERT_INT_GT(a, b)  _ASSERT_NUM(a, b, \"%d\", <=, \"INT_GT\")\n"
"#define ASSERT_INT_GTE(a, b) _ASSERT_NUM(a, b, \"%d\", <, \"INT_GTE\")\n"
"#define ASSERT_INT_LT(a, b)  _ASSERT_NUM(a, b, \"%d\", >=, \"INT_LT\")\n"
"#define ASSERT_INT_LTE(a, b) _ASSERT_NUM(a, b, \"%d\", >, \"INT_LTE\")\n"
"\n"
"#define ASSERT_FLOAT_WITHIN(a, b, within)                                      \\\n"
"    if (fabs((float)(a) - (float)(b)) > (float)(within))                       \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr,                                                        \\\n"
"                \"FLOAT_WITHIN(%s<%f> is NOT within %f of %s<%f>) %s:%d\", #a,   \\\n"
"                (float)(a), (float)(within), #b, (float)(b), __FILE__,         \\\n"
"                __LINE__);                                                     \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"\n"
"#define ASSERT_FLOAT_EQ(a, b)  ASSERT_FLOAT_WITHIN(a, b, 0.001f)\n"
"#define ASSERT_FLOAT_NEQ(a, b) _ASSERT_NUM(a, b, \"%f\", ==, \"FLOAT_NEQ\")\n"
"#define ASSERT_FLOAT_GT(a, b)  _ASSERT_NUM(a, b, \"%f\", <=, \"FLOAT_GT\")\n"
"#define ASSERT_FLOAT_GTE(a, b) _ASSERT_NUM(a, b, \"%f\", <, \"FLOAT_GTE\")\n"
"#define ASSERT_FLOAT_LT(a, b)  _ASSERT_NUM(a, b, \"%f\", >=, \"FLOAT_LT\")\n"
"#define ASSERT_FLOAT_LTE(a, b) _ASSERT_NUM(a, b, \"%f\", >, \"FLOAT_LTE\")\n"
"\n"
"#define ASSERT_MEM_EQ(a, b, n)                                                 \\\n"
"    if (memcmp((a), (b), (n)) != 0)                                            \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, \"MEM_EQ(%s<[\", #a);                                    \\\n"
"        print_memory((a), (n));                                                \\\n"
"        fprintf(stderr, \"]> != %s<[\", #b);                                     \\\n"
"        print_memory((b), (n));                                                \\\n"
"        fprintf(stderr, \"]>) %s:%d\", __FILE__, __LINE__);                      \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"#define ASSERT_STR_EQ(a, b, n)                                                 \\\n"
"    if (strncmp((a), (b), (n)) != 0)                                           \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, \"STR_EQ(%s<\\\"%.*s\\\"> != %s<\\\"%.*s\\\">) %s:%d\", #a,      \\\n"
"                (int)(n), (a), #b, (int)(n), (b), __FILE__, __LINE__);         \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"#define ASSERT_NULL(a)                                                         \\\n"
"    if ((void *)(a) != NULL)                                                   \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, #a \" is not NULL %s:%d\", __FILE__, __LINE__);          \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"#define ASSERT_NOTNULL(a)                                                      \\\n"
"    if ((void *)(a) == NULL)                                                   \\\n"
"    {                                                                          \\\n"
"        fprintf(stderr, #a \" is NULL %s:%d\", __FILE__, __LINE__);              \\\n"
"        exit(EXIT_FAILURE);                                                    \\\n"
"    }\n"
"\n"
"#define _MMIN(a, b) ((a) > (b) ? (b) : (a))\n"
"#define _MMAX(a, b) ((a) > (b) ? (a) : (b))\n"
"\n"
"#endif /* __BASE_H */\n";
// clang-format on

void generate_source(testunit *unit, testcase *tcase)
{
    ator_t ator = {0};

    const char *unit_name = path_stem(&ator, unit->file.path);

    file_seek(tcase->file, tcase->pos.offset, SEEK_SET);

    char *includes = NULL;

    named_filepos cfg;
    ARR_FOREACH(*unit->config, cfg, i)
    {
        if (strcmp(cfg.name, "INCLUDE") == 0)
        {
            file_seek(tcase->file, cfg.pos.offset, SEEK_SET);
            file_read(tcase->file, cfg.pos.size);
            includes = ator_memdup(&ator, tcase->file->buffer,
                                   tcase->file->buffer_size + 1, NULL, 0);
        }
    }

    UNWRAP_OR_PANIC(includes);

    file_t out_file = {0};
    file_open(&out_file, tcase->src_path, "w", &ator);

    file_seek(tcase->file, tcase->pos.offset, SEEK_SET);
    file_read(tcase->file, tcase->pos.size);

    char *source = ator_memdup(&ator, skip_whitespace(tcase->file->buffer),
                               tcase->file->buffer_size + 1, NULL, 0);
    source[skip_whitespace_reversed(source) + 1] = '\0';

    arrayof(char *) source_lines = new (source_lines, &ator);
    splitlines(source, &source_lines, &ator);

    arrayof(char *) source_map = new (source_map, &ator);

    char *line;
    ARR_FOREACH(source_lines, line, i)
    {
        char *sm = ator_snprintf(&ator, "%-80s // SM^ %s:%zu $", line,
                                 tcase->file->path, tcase->pos.start + i + 1);
        array_append(&source_map, &sm, sizeof(char *));
    }

    str_t init = new (init, &ator);

    char *param;
    ARR_FOREACH(tcase->params, param, i)
    {
        param = skip_whitespace(param);
        if (strncmp(param, "INIT", 4) != 0)
            continue;

        while (*param++ != ':') { /* pass */ }
        // param++;
        // param[skip_whitespace_reversed(param) + 1] = '\0';

        str_t param_sb = new (param_sb, NULL);
        while (*param != '\0')
        {
            char c = *param++;
            if (c == ':')
            {
                str_catf(&init, "test_init_%s;\n", param_sb.buf);
                param_sb.len = 0;
            }
            else
                str_catch(&param_sb, c);
        }
        if (param_sb.len > 0)
        {
            str_catf(&init, "test_init_%s;\n", param_sb.buf);
            param_sb.len = 0;
        }

        str_free(&param_sb);
    }

    file_write(&out_file,
               ator_snprintf(&ator,
                             "%s\n"
                             "%s\n"
                             "void _%s_%s()\n"
                             "%s\n"
                             "\n"
                             "int main()\n"
                             "{\n"
                             "    %s\n"
                             "    _%s_%s();\n"
                             "    return 0;\n"
                             "}\n",
                             test_base, includes, unit_name, tcase->test_name,
                             join(&source_map, NLCH, &ator), init.buf,
                             unit_name, tcase->test_name),
               -1);
    fflush(out_file.handle);

    ator_free(&ator);
}

uint64_t testcase_hash(testunit *unit, testcase *tcase)
{
    uint64_t hash = 0;

    pthread_mutex_lock(&tcase->file->mutex);

    file_seek(tcase->file, tcase->pos.offset, SEEK_SET);
    file_read(tcase->file, tcase->pos.size);
    hash = siphash24(tcase->file->buffer, tcase->file->buffer_size);

    pthread_mutex_unlock(&tcase->file->mutex);

    char *param;
    ARR_FOREACH(tcase->params, param, i)
    {
        hash ^= siphash24(param, strlen(param));
    }

    pthread_mutex_lock(&tcase->file->mutex);

    ator_t ator = {0};
    str_t args = new (args, &ator);
    gen_cc_argstr(&args, NULL, unit, tcase, &ator);

    // named_filepos cfg;
    // ARR_FOREACH(*unit->config, cfg, i)
    // {
    //     file_seek(tcase->file, cfg.pos.offset, SEEK_SET);
    //     file_read(tcase->file, cfg.pos.size);
    //     hash ^= siphash24(tcase->file->buffer, tcase->file->buffer_size);
    // }

    pthread_mutex_unlock(&tcase->file->mutex);

    return hash;
}

int testcase_needs_rebuild(testunit *unit, testcase *tcase)
{
    char *cache_dir = "tests/out/cache";
    if (!path_exists(cache_dir))
        return 1;

    ator_t ator = {0};
    int needs_rebuild = 1;

    fs_iterator iter;
    fs_iter_init(&iter, cache_dir, &ator);

    fs_entry *entry;
    while ((entry = fs_iter_next(&iter, &ator)))
    {
        if (fs_entry_isdir(entry))
            continue;

        if (strcmp(path_name(entry->path), tcase->repr_name) == 0)
        {
            char *content = fs_file_read(entry->path, &ator);
            char *endptr = content;
            uint64_t hash = strtoul(content, &endptr, 10);
            uint64_t test_hash = testcase_hash(unit, tcase);
            needs_rebuild = test_hash != hash;
            break;
        }
    }

    ator_free(&ator);
    return needs_rebuild;
}

void testcase_update_hash(testunit *unit, testcase *tcase)
{
    char *cache_dir = "tests/out/cache";
    if (!path_exists(cache_dir))
        path_mkdir("tests/out/cache", 0777, 1);

    ator_t ator = {0};

    char *cache_file = path_join(&ator, cache_dir, tcase->repr_name);

    file_t file;
    file_open(&file, cache_file, "w", &ator);

    uint64_t hash = testcase_hash(unit, tcase);
    char *content = ator_snprintf(&ator, "%" PRIu64, hash);
    file_write(&file, content, strlen(content));

    ator_free(&ator);
}

int spawn_process(arrayof(char *) * args)
{
#if OS == OS_LINUX
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Failed to fork process: %s\n", strerror(errno));
        return -1;
    }
    else if (pid == 0)
    {
        array_append(args, &(char *){NULL}, sizeof(char *));
        int ret = execvp(ARR_AS(*args, char *)[0], args->items);
        if (ret == -1)
        {
            fprintf(stderr, "Failed to execute process: %s\n", strerror(errno));
            return -1;
        }
    }
    else
    {
        int status;
        int ret = waitpid(pid, &status, 0);
        if (ret == -1)
        {
            fprintf(stderr, "Process failed: %s\n", strerror(errno));
            return -1;
        }

        if (WIFEXITED(status))
        {
            if (WEXITSTATUS(status) != 0)
            {
                fprintf(stderr, "Non zero exit status: %d\n",
                        WEXITSTATUS(status));
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "Child process did not exit normally\n");
            return -1;
        }
    }
#else
#  error "Forking process not implemented"
#endif

    return 0;
}

void gen_cc_args(testunit *unit, testcase *tcase, arrayof(char *) * out,
                 ator_t *ator)
{
    ator_t temp = {0};

    array_append(out, &(char *){"cc"}, sizeof(char *));
    array_append(out, &(char *){tcase->src_path}, sizeof(char *));
    array_append(out, &(char *){"-o"}, sizeof(char *));
    array_append(out, &(char *){tcase->bin_path}, sizeof(char *));

    arrayof(char *) cflags = new (cflags, &temp);
    named_filepos cfg;
    ARR_FOREACH(*unit->config, cfg, i)
    {
        if (strcmp(cfg.name, "CFLAGS") == 0)
        {
            file_seek(tcase->file, cfg.pos.offset, SEEK_SET);
            file_read(tcase->file, cfg.pos.size);
            char *flag_string =
                ator_memdup(&temp, tcase->file->buffer,
                            tcase->file->buffer_size + 1, NULL, 0);

            splitlines(flag_string, &cflags, &temp);
            char *flag;
            ARR_FOREACH(cflags, flag, j)
            {
                if (*flag == '\0')
                    continue;

                str_t stripped = str_from_view(str_strip(&str_c(flag)), ator);
                array_append(out, &(char *){stripped.buf}, sizeof(char *));
            }
        }
    }

    ator_free(&temp);
}

void gen_cc_argstr(str_t *out, arrayof(char *) * args_optional, testunit *unit,
                   testcase *tcase, ator_t *ator)
{
    if (args_optional == NULL)
    {
        ator_t temp = {0};

        arrayof(char *) args = new (args, NULL);
        gen_cc_args(unit, tcase, &args, ator);
        str_join(out, &args, &str_c(" "));

        ator_free(&temp);
    }
    else
        str_join(out, args_optional, &str_c(" "));
}

void *build_and_run(void *arg)
{
    _build_params_tuple *params = (_build_params_tuple *)arg;
    testcase *tcase = params->tcase;
    testunit *unit = params->unit;
    ator_t ator = {0};

    TRUTHY_OR_PANIC(tcase->src_path != NULL);

    telemetry_send(.type = TELEMETRY_EVENT_STATUS,
                   .status = TELEMETRY_STATUS_BUILD);

    if (!params->rebuild && !testcase_needs_rebuild(unit, tcase))
    {
        telemetry_log("Using cached version of '%s'", tcase->repr_name);
        goto no_rebuild;
    }

    arrayof(char *) args = new (args, &ator);
    gen_cc_args(unit, tcase, &args, &ator);

    {
        str_t args_str = new (args_str, &ator);
        gen_cc_argstr(&args_str, &args, NULL, NULL, NULL);
        telemetry_log("+ %s", args_str.buf);
    }

    if (spawn_process(&args) < 0)
        telemetry_send(.type = TELEMETRY_EVENT_STATUS,
                       .status = TELEMETRY_STATUS_FAIL);
    else
    {
        testcase_update_hash(unit, tcase);
    no_rebuild:
        telemetry_send(.type = TELEMETRY_EVENT_TASK_REGISTER,
                       .task_type = TELEMETRY_TASK_RUN);
        pool_enqueue_task(params->pool, run, params);
    }

    telemetry_send(.type = TELEMETRY_EVENT_TASK_UNREGISTER,
                   .task_type = TELEMETRY_TASK_BUILD);
    ator_free(&ator);

    return NULL;
}

void *run(void *arg)
{
    _build_params_tuple *params = (_build_params_tuple *)arg;
    testcase *tcase = params->tcase;
    // testunit *unit = params->unit;
    ator_t ator = {0};

    telemetry_send(.type = TELEMETRY_EVENT_STATUS,
                   .status = TELEMETRY_STATUS_RUN);

    telemetry_log("Running '%s'", tcase->repr_name);

    arrayof(char *) args = new (args, &ator);
    array_append(&args, &(char *){tcase->bin_path}, sizeof(char *));

    int expect_fail = 0;
    int debug = 0;
    (void)debug;
    (void)expect_fail;

    char *param;
    ARR_FOREACH(tcase->params, param, i)
    {
        if (strcmp(param, "EXPECT_FAIL") == 0)
            expect_fail = 1;
        else if (strcmp(param, "DEBUG") == 0)
            debug = 1;
    }

    if (spawn_process(&args) < 0)
        telemetry_send(.type = TELEMETRY_EVENT_STATUS,
                       .status = TELEMETRY_STATUS_FAIL);

    telemetry_send(.type = TELEMETRY_EVENT_TASK_UNREGISTER,
                   .status = TELEMETRY_STATUS_RUN);
    ator_free(&ator);

    return NULL;
}

void *consumer_thread(void *arg)
{
    telemetry_send(.type = TELEMETRY_EVENT_THREAD_REGISTER);

    thread_pool *pool = (thread_pool *)arg;
    for (;;)
    {
        task_t *task = queue_pop(&pool->tasks);
        if (task == NULL)
            break;

        if (task)
        {
            pthread_mutex_lock(&pool->mutex);
            pool->task_active++;
            pthread_mutex_unlock(&pool->mutex);

            task->task_fn(task->arg);
            free(task);

            pthread_mutex_lock(&pool->mutex);
            pool->task_active--;
            pthread_cond_broadcast(&pool->all_done);
            pthread_mutex_unlock(&pool->mutex);
        }
    }

    return NULL;
}

int pool_init(thread_pool *pool, int nb_threads, ssize_t task_cap)
{
    pthread_mutex_init(&pool->mutex, NULL);

    queue_init(&pool->tasks);
    pool->task_cap = task_cap;
    pool->task_active = 0;

    pool->threads = (array_t){0};

    for (int i = 0; i < nb_threads; i++)
    {
        pthread_t tid;
        if (pthread_create(&tid, NULL, consumer_thread, pool) != 0)
        {
            pool_free(pool);
            return -1;
        }

        array_append(&pool->threads, (void *)tid, sizeof(tid));
    }

    return 0;
}

int pool_enqueue_task(thread_pool *pool, void *(*task)(void *arg), void *arg)
{
    task_t *next = calloc(1, sizeof(task_t));
    if (next == NULL)
        return -1;

    next->task_fn = task;
    next->arg = arg;
    next->next = NULL;

    pthread_mutex_lock(&pool->mutex);

    if (pool->task_cap != -1)
    {
        while (pool->tasks.len >= (size_t)pool->task_cap && !program_shutdown)
            pthread_cond_wait(&pool->tasks.not_full, &pool->mutex);
    }

    if (program_shutdown)
    {
        pthread_mutex_unlock(&pool->mutex);
        free(next);
        return -1;
    }

    queue_push(&pool->tasks, next);

    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

void pool_wait(thread_pool *pool)
{
    pthread_mutex_lock(&pool->mutex);

    while (pool->tasks.len != 0 || pool->task_active != 0)
        pthread_cond_wait(&pool->all_done, &pool->mutex);

    pthread_mutex_unlock(&pool->mutex);
}

void pool_free(thread_pool *pool)
{
    if (pool == NULL)
        return;

    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->all_done);

    queue_terminate(&pool->tasks);

    pthread_t tid;
    ARR_FOREACH(pool->threads, tid, i)
    {
        pthread_join(tid, NULL);
    }

    array_free(&pool->threads);
    queue_free(&pool->tasks);

    pthread_mutex_unlock(&pool->mutex);

    pthread_cond_destroy(&pool->all_done);
    pthread_mutex_destroy(&pool->mutex);
}

size_t file_get_size(file_t *file)
{
    file_seek(file, 0, SEEK_END);
    size_t size = ftell(file->handle);
    rewind(file->handle);

    return size;
}

void file_open(file_t *file, const char *path, char *mode, ator_t *ator)
{
    PP_MAP(UNWRAP_OR_PANIC, file, path, mode, ator);
    file->handle = UNWRAP_OR_PANIC(fopen(path, mode));
    file->path = path;
    file->size = file_get_size(file);
    file->buffer = NULL;
    pthread_mutex_init(&file->mutex, NULL);

    ator_ref(ator, file, (void *)file_free, 0);
}

void file_free(file_t *file)
{
    if (file == NULL)
        return;

    if (file->handle != NULL)
    {
        fclose(file->handle);
        file->handle = NULL;
    }

    if (file->buffer != NULL)
    {
        free(file->buffer);
        file->buffer = NULL;
    }

    pthread_mutex_destroy(&file->mutex);
}

void file_prepare_buffer(file_t *file, size_t requested_size)
{
    size_t target_size = (float)requested_size * 1.5f;
    if (file->buffer == NULL)
    {
        file->buffer = UNWRAP_OR_PANIC(malloc(target_size));
        file->buffer_cap = target_size;
    }
    else if (requested_size > file->buffer_cap)
    {
        file->buffer = UNWRAP_OR_PANIC(realloc(file->buffer, target_size));
        file->buffer_cap = target_size;
    }
}

size_t file_read(file_t *file, size_t nbytes)
{
    PP_MAP(UNWRAP_OR_PANIC, file);

    if (file->eof)
        return 0;

    file_prepare_buffer(file, nbytes + 1);
    size_t read = fread(file->buffer, 1, nbytes, file->handle);
    if (read == 0 && ferror(file->handle) != 0)
        UNWRAP_OR_PANIC(NULL);
    else if (read == 0)
        file->eof = feof(file->handle);
    else
        file->buffer_size = read;

    file->buffer[nbytes] = '\0';

    return read;
}

char *file_readline(file_t *file, int skip_nl)
{
    PP_MAP(UNWRAP_OR_PANIC, file);

    if (file->eof)
        return NULL;

    file_prepare_buffer(file, 1);

    int ch = 0;
    size_t pos = 0;
    while ((ch = fgetc(file->handle)) != EOF)
    {
        if (pos + 1 > file->buffer_cap)
            file_prepare_buffer(file, file->buffer_cap * 2);

        if (ch == '\n')
        {
            if (!skip_nl)
                file->buffer[pos++] = (char)ch;
            break;
        }
        else if (ch == '\r')
        {
            int next = fgetc(file->handle);
            if (next != '\n' && next != EOF)
                ungetc(next, file->handle);
            if (!skip_nl)
                file->buffer[pos++] = (char)ch;
            break;
        }
        else
            file->buffer[pos++] = (char)ch;
    }

    if (ch == EOF)
        file->eof = 1;

    file->buffer[pos] = '\0';
    file->buffer_size = pos;

    return file->buffer;
}

int file_seek(file_t *file, size_t off, int whence)
{
    PP_MAP(UNWRAP_OR_PANIC, file);

    int ret = fseek(file->handle, off, whence);
    file->eof = feof(file->handle);
    return ret;
}

long file_write(file_t *file, const void *data, ssize_t size)
{
    PP_MAP(UNWRAP_OR_PANIC, file);

    return fwrite(data, 1, size == -1 ? strlen(data) : (size_t)size,
                  file->handle);
}

size_t file_skip_ch(file_t *file, char c)
{
    char currc = 0;
    size_t read = 0;
    while ((currc = fgetc(file->handle)) != EOF)
    {
        if (currc != c)
        {
            ungetc(currc, file->handle);
            break;
        }
        read++;
    }

    return read;
}

size_t file_skip_until_ch(file_t *file, char c)
{
    char currc = 0;
    size_t read = 0;
    while ((currc = fgetc(file->handle)) != EOF)
    {
        if (currc == c)
        {
            ungetc(c, file->handle);
            break;
        }
        read++;
    }

    return read;
}

size_t file_skip_until_one_of_ch(file_t *file, char *cs)
{
    char currc = 0;
    size_t read = 0;
    while ((currc = fgetc(file->handle)) != EOF)
    {
        for (size_t i = 0; i < strlen(cs); i++)
        {
            if (currc == cs[i])
            {
                ungetc(cs[i], file->handle);
                goto end;
            }
        }
        read++;
    }

end:
    return read;
}

size_t file_skip_until_str(file_t *file, const char *str, int skip_str)
{
    char ch = 0;
    size_t read = 0;
    size_t match = 0;

    while ((ch = fgetc(file->handle)) != EOF)
    {
#ifdef DEBUG
        if (ch == '\n')
            fprintf(stderr, "Warning %s: '%s' goes over a newline\n",
                    __FUNCTION__, file->path);
#endif // DEBUG

        read++;
        if (ch != str[match])
        {
            match = 0;
            continue;
        }

        match++;
        if (match == strlen(str))
        {
            if (!skip_str)
            {
                file_seek(file, -strlen(str), SEEK_CUR);
                read -= strlen(str);
            }

            return read;
        }
    }

    return 0;
}

typedef struct _linedelim
{
    const char *name;
    const char *start;
    const char *end;
} _linedelim;

#define STRINGIFY(expr) #expr
#define DO_DEFINE_LIST(_name, lower)                                           \
    (_linedelim){.name = #_name,                                               \
                 .start = STRINGIFY(_name##_BEGIN),                            \
                 .end = STRINGIFY(_name##_END)},
static const _linedelim params[] = {PARAM_LIST(DO_DEFINE_LIST)};

#define FLUSH_PARAM(tcase, p, tmp)                                             \
    ({                                                                         \
        char *__p = ator_strndup(ator, param_temp.items, param_temp.len);      \
        if (tcase->test_name == NULL)                                          \
            tcase->test_name = __p;                                            \
        else                                                                   \
            array_append(&tcase->params, &__p, sizeof(char *));                \
        memset(tmp.items, 0, tmp.len);                                         \
        tmp.len = 0;                                                           \
    })

testcase *testcase_parse_def(testunit *unit, ator_t *ator)
{
    ator_t tmp = {0};
    char stack[128] = {0};
    int sp = 0;

    testcase *tcase = ator_alloc(ator, sizeof(testcase), NULL, ATOR_MEMZERO);
    new (tcase->params, ator);

    tcase->file = &unit->file;
    arrayof(char) param_temp = new (param_temp, &tmp);

    file_skip_ch(&unit->file, ' ');
    file_skip_until_str(&unit->file, "TEST_BEGIN", 1);

    while (file_read(&unit->file, 1))
    {
        char c = unit->file.buffer[0];
        if (isspace(c))
            continue;

        if (c == '(')
        {
            if (sp >= 127)
            {
                fprintf(stderr, "Stack overflow\n");
                goto end;
            }

            if (sp != 0)
                array_append(&param_temp, &(char){c}, 1);

            stack[sp++] = c;
        }
        else if (c == ')')
        {
            if (sp <= 0)
                goto end;

            char open = stack[--sp];
            if (open != '(')
            {
                fprintf(stderr, "Unmatched paren\n");
                goto end;
            }

            if (sp != 0)
                array_append(&param_temp, &(char){c}, 1);

            if (sp == 0)
            {
                FLUSH_PARAM(tcase, param, param_temp);
                goto end;
            }
        }
        else
        {
            if (c == ',')
                FLUSH_PARAM(tcase, param, param_temp);
            else
                array_append(&param_temp, &(char){c}, 1);
        }
    }

end:
    ator_free(&tmp);

    return tcase;
}

void testcase_parse(testunit *unit, filepos pos, ator_t *ator)
{
    size_t off = ftell(unit->file.handle);

    file_seek(&unit->file, pos.offset, SEEK_SET);

    testcase *tcase = testcase_parse_def(unit, ator);
    size_t offset = ftell(unit->file.handle);
    tcase->pos = (filepos){
        .start = pos.start,
        .end = pos.end,
        .offset = offset,
        .size = (pos.offset + pos.size) - offset,
    };

    {
        ator_t tmp = {0};

        const char *unit_name = path_stem(&tmp, unit->file.path);

        // TODO: setting for output path
        char *name =
            ator_snprintf(&tmp, "%s_%s.c", unit_name, tcase->test_name);
        tcase->src_path = path_join(ator, "tests/out/src", name);

        name = ator_snprintf(&tmp, "%s_%s", unit_name, tcase->test_name);
        tcase->bin_path = path_join(ator, "tests/out/bin", name);

        tcase->repr_name =
            ator_snprintf(ator, "%s_%s", unit_name, tcase->test_name);

        ator_free(&tmp);
    }

    array_append(unit->cases, tcase, sizeof(*tcase));

    file_seek(&unit->file, off, SEEK_SET);
}

void testunit_parse(testunit *unit, ator_t *ator)
{
    // init config
    for (unsigned int p = 0; p < STATIC_ARR_LEN(params); p++)
    {
        named_filepos n = {
            .name = params[p].name,
            {.start = -1, .end = -1, .offset = -1, .size = -1},
        };
        array_append(unit->config, &n, sizeof(n));
    }

    file_seek(&unit->file, 0, SEEK_SET);
    int i = 0;
    int test_start_line = -1;
    size_t test_offset = 0;
    size_t line_offset = 0;

    while (file_readline(&unit->file, 1))
    {
        // parse config
        for (unsigned int p = 0; p < STATIC_ARR_LEN(params); p++)
        {
            named_filepos *n = &(ARR_AS(*unit->config, named_filepos)[p]);

            if (strstr(unit->file.buffer, params[p].start) != NULL)
            {
                n->pos.start = i + 1;
                n->pos.offset = ftell(unit->file.handle);
            }
            else if (strstr(unit->file.buffer, params[p].end) != NULL)
            {
                n->pos.end = i;
                n->pos.size = line_offset - n->pos.offset;
            }
        }

        // parse testcase
        if (strncmp(TEST_START_WORD, skip_whitespace(unit->file.buffer),
                    strlen(TEST_START_WORD)) == 0)
        {
            if (test_start_line != -1)
                fprintf(stderr,
                        "\x1b[31m    %s:%d: '%s' have no corresponding "
                        "'%s'\x1b[0m\n",
                        unit->file.path, test_start_line, TEST_START_WORD,
                        TEST_END_WORD);

            test_start_line = i + 1;
            test_offset = line_offset;
        }
        else if (strncmp(TEST_END_WORD, skip_whitespace(unit->file.buffer),
                         strlen(TEST_END_WORD)) == 0)
        {
            if (test_start_line == -1)
                fprintf(stderr,
                        "\x1b[31m    %s:%d: '%s' have no corresponding "
                        "'%s'\x1b[0m\n",
                        unit->file.path, i + 1, TEST_END_WORD, TEST_START_WORD);
            else
            {
                filepos pos = {.start = test_start_line,
                               .end = i,
                               .offset = test_offset,
                               .size = line_offset - test_offset};
                testcase_parse(unit, pos, ator);
            }

            test_start_line = -1;
            test_offset = 0;
        }

        i++;
        line_offset = ftell(unit->file.handle);
    }
    file_seek(&unit->file, 0, SEEK_SET);

    // check for unmatched delim
    for (unsigned int p = 0; p < STATIC_ARR_LEN(params); p++)
    {
        named_filepos *n = &ARR_AS(*unit->config, named_filepos)[p];
        if (n->pos.start == -1 && n->pos.end != -1)
            fprintf(
                stderr,
                "\x1b[31m    %s:%d: '%s' have no corresponding '%s'\x1b[0m\n",
                unit->file.path, n->pos.end, params[p].end, params[p].start);
        else if (n->pos.start != -1 && n->pos.end == -1)
            fprintf(
                stderr,
                "\x1b[31m    %s:%d: '%s' have no corresponding '%s'\x1b[0m\n",
                unit->file.path, n->pos.start, params[p].start, params[p].end);
    }

    if (test_start_line != -1)
        fprintf(stderr,
                "\x1b[31m    %s:%d: '%s' have no corresponding "
                "'%s'\x1b[0m\n",
                unit->file.path, test_start_line, TEST_START_WORD,
                TEST_END_WORD);
}

int testunit_create(testunit *unit, const char *file, ator_t *ator)
{
    if (path_isdir(file))
        return -1;

    unit->config = ator_alloc(ator, sizeof(*unit->config), (void *)array_free,
                              ATOR_MEMZERO);
    unit->cases = ator_alloc(ator, sizeof(*unit->cases), (void *)array_free,
                             ATOR_MEMZERO);

    file_open(&unit->file, file, "r", ator);
    testunit_parse(unit, ator);

    return 0;
}

void print_help(char *program)
{
    const char *message =
        "usage: %s [-h] [-r] [-j <int>] [target ...]\n"
        "\n"
        "positional arguments:\n"
        "  target                target unittest file, default all in 'tests/' "
        "directory\n"
        "\n"
        "options:\n"
        "  -h, --help             show this help message and exit\n"
        "  -r, --rebuild          force rebuild all test\n"
        "  -j <int>, --jobs <int> number of thread to build\n";

    size_t len = strlen(message) + strlen(program) + 1;
    char fmt[len];
    snprintf(fmt, len, message, program);

    printf("%s", fmt);
}

int check_arg(char *arg, const char *shorthand, const char *longhand)
{
    return strcmp(arg, shorthand) == 0 || strcmp(arg, longhand) == 0;
}

int check_arg_value(char *arg, size_t *out_offset, const char *shorthand,
                    const char *longhand)
{
    int singular = strcmp(arg, shorthand) == 0 || strcmp(arg, longhand) == 0;
    int joined_short = strncmp(arg, shorthand, strlen(shorthand)) == 0;
    int joined_long = strncmp(arg, longhand, strlen(longhand)) == 0;

    if (!singular && !(joined_short || joined_long))
        return 0;

    if (singular)
        *out_offset = -1;
    else if (joined_short)
        *out_offset = strlen(shorthand);
    else if (joined_long)
        *out_offset = strlen(longhand);

    return 1;
}

int parse_args(char **args, int argc, program_args *out, ator_t *ator)
{
    PP_MAP(UNWRAP_OR_PANIC, ator, out);

    int i = 0;
    size_t offset = 0;
    while (i < argc)
    {
        char *arg = args[i];
        if (check_arg(arg, "-h", "--help"))
        {
            print_help(args[0]);
            exit(0);
        }
        else if (check_arg(arg, "-r", "--rebuild"))
        {
            out->rebuild = 1;
        }
        else if (check_arg_value(arg, &offset, "-j", "--jobs"))
        {
            int64_t jobs = 0;
            char *end = NULL;
            if (offset != (size_t)-1)
                jobs = strtoll(arg + offset, &end, 10);
            else
            {
                if (i >= argc)
                {
                    fprintf(stderr,
                            "-j/--jobs expected a number, got nothing\n");
                    return -1;
                }

                arg = args[++i];
                jobs = strtoll(arg, &end, 10);
            }

            if (*end != '\0')
            {
                size_t _offset = offset == (size_t)-1 ? 0 : offset;
                fprintf(stderr, "invalid number: '%s'\n", arg + _offset);
                for (size_t j = 0;
                     j < strlen(arg) - strlen(end) +
                             strlen("invalid number: '") - _offset;
                     j++)
                    fprintf(stderr, " ");
                for (size_t j = 0; j < strlen(end); j++)
                    fprintf(stderr, "^");
                fprintf(stderr, "\n");
                return -1;
            }

            out->jobs = jobs;
        }
        i++;
    }

    return 0;
}

char *skip_whitespace(char *s)
{
    while (isspace(*s))
        s++;
    return s;
}

size_t skip_whitespace_reversed(char *s)
{
    size_t end = strlen(s) - 1;
    while (end > 0 && isspace(s[end]))
        end--;

    return end;
}

int is_newline(char *s)
{
    char c = *s++;
    if (c == '\n')
        return 1;
    else if (c == '\0')
        return 0;

    char c2 = *s++;
    if (c == '\r' && c2 == '\n')
        return 2;

    return 0;
}

void split(const char *s, const char *delim, arrayof(char *) * out,
           ator_t *ator)
{
    ator_t tmp = {0};
    char *copy = ator_strdup(&tmp, s);
    char *token = strtok(copy, delim);

    while (token)
    {
        char *chunk = ator_strdup(ator, token);
        array_append(out, &chunk, sizeof(char *));
        token = strtok(NULL, delim);
    }

    ator_free(&tmp);
}

void splitlines(const char *s, arrayof(char *) * out, ator_t *ator)
{
    size_t len = strlen(s);
    size_t start = 0;

    for (size_t i = 0; i < len; i++)
    {
        if (s[i] == '\n' || s[i] == '\r')
        {
            size_t token_len = i - start;
            char *line = ator_strndup(ator, s + start, token_len);
            array_append(out, &line, sizeof(char *));

            if (s[i] == '\r' && (i + 1) < len && s[i + 1] == '\n')
                i++;

            start = i + 1;
        }
    }

    if (start < len)
    {
        size_t token_len = len - start;
        char *line = ator_strndup(ator, s + start, token_len);
        array_append(out, &line, sizeof(char *));
    }
}

char *join(const arrayof(char *) * in, const char *delim, ator_t *ator)
{
    if (in->len == 0)
        return ator_strdup(ator, "");

    size_t delim_len = strlen(delim);
    size_t total_len = 0;

    char *elm;
    ARR_FOREACH(*in, elm, i)
    {
        total_len += strlen(elm);
        if (i < in->len - 1)
            total_len += delim_len;
    }

    char *result = ator_alloc(ator, total_len + 1, NULL, 0);

    size_t pos = 0;
    ARR_FOREACH(*in, elm, i)
    {
        memcpy(result + pos, elm, strlen(elm));
        pos += strlen(elm);

        if (i < in->len - 1)
        {
            memcpy(result + pos, delim, delim_len);
            pos += delim_len;
        }
    }

    result[pos] = '\0';
    return result;
}

void queue_init(queue_t *q)
{
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_free(queue_t *q)
{
    if (q == NULL)
        return;

    pthread_mutex_lock(&q->mutex);

    queue_entry_t *entry = q->head;
    while (entry)
    {
        queue_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }

    q->len = 0;
    q->head = NULL;
    q->tail = NULL;

    pthread_mutex_unlock(&q->mutex);

    queue_terminate(q);

    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->mutex);
}

void queue_terminate(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}

int queue_push(queue_t *q, void *data)
{
    queue_entry_t *entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return -ENOMEM;
    entry->data = data;

    pthread_mutex_lock(&q->mutex);

    if (QUEUE_IS_EMPTY(q))
        q->head = entry;
    else
        q->tail->next = entry;

    q->tail = entry;
    q->len++;

    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int queue_push_copy(queue_t *q, const void *data, size_t size)
{
    void *heap = UNWRAP_OR_PANIC(malloc(size));
    if (heap == NULL)
        return -ENOMEM;

    memcpy(heap, data, size);

    queue_push(q, heap);

    return 0;
}

void *queue_pop(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    while (q->head == NULL && !program_shutdown)
        pthread_cond_wait(&q->not_empty, &q->mutex);

    if (q->head == NULL && program_shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    void *data = NULL;

    if (QUEUE_IS_EMPTY(q))
        goto exit;

    data = q->head->data;

    queue_entry_t *next = q->head->next;
    if (!next)
        q->tail = NULL;

    free(q->head);
    q->head = next;
    q->len--;

    pthread_cond_broadcast(&q->not_full);
exit:
    pthread_mutex_unlock(&q->mutex);

    return data;
}

void *queue_pop_nonblock(queue_t *q)
{
    void *data = NULL;

    if (QUEUE_IS_EMPTY(q))
        goto exit;

    data = q->head->data;

    queue_entry_t *next = q->head->next;
    if (!next)
        q->tail = NULL;

    free(q->head);
    q->head = next;
    q->len--;

exit:
    return data;
}

void queue_clear(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    queue_entry_t *entry = q->head;
    while (entry)
    {
        queue_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }

    q->len = 0;
    q->head = NULL;
    q->tail = NULL;

    pthread_mutex_unlock(&q->mutex);
}

uint64_t hash_djb2(const char *buf, size_t size)
{
    uint64_t hash = 5381;
    for (size_t i = 0; i < size; i++)
        hash = ((hash << 5) + hash) + buf[i];

    return hash;
}

arrayof(dict_item) * buckets_alloc(int length)
{
    arrayof(dict_item) *buckets =
        UNWRAP_OR_PANIC(malloc(length * sizeof(array_t)));

    for (int i = 0; i < length; i++)
    {
        arrayof(dict_item) *bkt = &buckets[i];

        *bkt = new (*bkt, NULL);
    }

    return buckets;
}

void dict_init(dict_t *dict)
{
    dict->strcmp = strcmp;
    dict->hash = hash_djb2;
    dict->bucket_slot = 32;
    dict->buckets = buckets_alloc(dict->bucket_slot);
    if (dict->buckets == NULL)
        errno = -ENOMEM;
}

void dict_item_free(dict_item *item)
{
    if (item == NULL)
        return;

    if (item->self_owned)
        free(item->data);
    item->data = NULL;

    free(item->key);
    item->key = NULL;
}

void dict_clear(dict_t *dict)
{
    assert(dict);

    for (int i = 0; i < dict->bucket_slot; i++)
    {
        arrayof(dict_item) bkt = dict->buckets[i];

        dict_item *item;
        ARR_FOREACH_BYREF(bkt, item, j)
        {
            dict_item_free(item);
        }
    }

    dict->length = 0;
}

void dict_free(dict_t *dict)
{
    if (dict == NULL)
        return;

    dict_clear(dict);

    for (int i = 0; i < dict->bucket_slot; i++)
        array_free(&dict->buckets[i]);

    free(dict->buckets);

    memset(dict, 0, sizeof(*dict));
}

void dict_resize(dict_t *dict, int new_size)
{
    arrayof(dict_item) *new = buckets_alloc(new_size);

    for (int i = 0; i < dict->bucket_slot; i++)
    {
        arrayof(dict_item) *bkt = &dict->buckets[i];
        if (bkt == NULL || bkt->len == 0)
            continue;

        dict_item item;
        ARR_FOREACH(*bkt, item, j)
        {
            int new_index = dict->hash(item.key, strlen(item.key)) % new_size;
            array_append(&new[new_index], &item, sizeof(dict_item));
        }
    }

    free(dict->buckets);
    dict->buckets = new;
    dict->bucket_slot = new_size;
}

array_t *bkt_from_key(dict_t *dict, const char *key)
{
    int index = dict->hash(key, strlen(key)) % dict->bucket_slot;
    return &dict->buckets[index];
}

dict_item *dict_insert(dict_t *dict, const char *key, void *item)
{
    if (dict_getload(dict) > 0.75f)
        dict_resize(dict, dict->bucket_slot * 2);

    arrayof(dict_item) *bkt = bkt_from_key(dict, key);
    dict_item new_entry = (dict_item){.key = strdup(key), .data = item};

    dict_item *itm;
    ARR_FOREACH_BYREF(*bkt, itm, i)
    {
        if (strcmp(itm->key, key) == 0)
        {
            dict_item_free(&ARR_AS(*bkt, dict_item)[i]);
            array_overwrite(bkt, &new_entry, sizeof(new_entry), i);
            return &ARR_AS(*bkt, dict_item)[i];
        }
    }

    array_append(bkt, &new_entry, sizeof(new_entry));
    dict->length += 1;
    return &(ARR_AS(*bkt, dict_item)[bkt->len - 1]);
}

void dict_insert_copy(dict_t *dict, const char *key, void *item, size_t size)
{
    void *heap = UNWRAP_OR_PANIC(malloc(size));
    memcpy(heap, item, size);

    dict_item *itm = dict_insert(dict, key, heap);
    itm->self_owned = 1;
}

void *dict_get(dict_t *dict, const char *key, void *default_return)
{
    arrayof(dict_item) *bkt = bkt_from_key(dict, key);
    if (bkt == NULL || bkt->len == 0)
        goto exit;

    dict_item item;
    ARR_FOREACH(*bkt, item, i)
    {
        if (dict->strcmp(item.key, key) == 0)
            return item.data;
    }

exit:
    errno = -EINVAL;
    return default_return;
}

float dict_getload(dict_t *dict)
{
    if (dict->bucket_slot == 0)
        return 0;

    return (float)dict->length / (float)dict->bucket_slot;
}

int dict_exists(dict_t *dict, const char *key)
{
    arrayof(dict_item) *bkt = bkt_from_key(dict, key);
    if (bkt == NULL || bkt->len == 0)
        return 0;

    dict_item item;
    ARR_FOREACH(*bkt, item, i)
    {
        if (dict->strcmp(item.key, key) == 0)
            return 1;
    }

    return 0;
}

#define stroffset(str) ((str)->buf + (str)->len)

static size_t get_avail(str_t *str)
{
    return str->capacity - str->len;
}

static void ensure_size(str_t *str, size_t required)
{
    if (get_avail(str) >= required)
        return;

    size_t capacity = str->capacity;
    while (capacity - str->len < required)
        capacity = capacity == 0 ? 1 : capacity * 2;

    str_resize(str, capacity);
}

static void append_null(str_t *str)
{
    ensure_size(str, 1);
    str->buf[str->len] = '\0';
}

void str_init(str_t *str)
{
    str->len = 0;
    str->capacity = 256;
    str->buf = UNWRAP_OR_PANIC(calloc(1, str->capacity));
}

void str_initn(str_t *str, size_t size)
{
    str->len = 0;
    str->capacity = size;
    str->buf = UNWRAP_OR_PANIC(calloc(1, str->capacity));
}

void str_free(str_t *str)
{
    if (str == NULL)
        return;

    free(str->buf);
    memset(str, 0, sizeof(str_t));
}

str_t *str_resize(str_t *str, size_t new)
{
    assert(str);

    if (str->capacity == new)
        return str;

    if (str->len > new)
        str->len = new;
    str->capacity = new;
    str->buf = realloc(str->buf, str->capacity);
    if (str->buf == NULL)
        errno = -ENOMEM;

    return str;
}

str_t str_from_view(strview_t view, ator_t *ator)
{
    str_t str = {0};
    str_catlen(&str, view.start, view.size);

    ator_ref(ator, str.buf, free, 0);
    (void)ator;

    return str;
}

str_t *str_cat(str_t *str, const char *s)
{
    assert(str && s);

    size_t len = strlen(s);
    ensure_size(str, len + 1);
    memcpy(stroffset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *str_catch(str_t *str, char ch)
{
    assert(str);

    ensure_size(str, 2);
    str->buf[str->len++] = ch;
    str->buf[str->len] = '\0';

    return str;
}

str_t *str_catlen(str_t *str, const char *s, size_t len)
{
    assert(str && s);

    ensure_size(str, len + 1);
    memcpy(stroffset(str), s, len);

    str->len += len;
    append_null(str);

    return str;
}

str_t *str_cat_str(str_t *dst, const str_t *src)
{
    assert(dst);

    ensure_size(dst, src->len + 1);
    memcpy(stroffset(dst), src->buf, src->len);

    dst->len += src->len;
    append_null(dst);

    return dst;
}

str_t *str_catf(str_t *str, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    size_t needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    va_start(args, fmt);

    ensure_size(str, needed + 1);
    vsprintf(stroffset(str), fmt, args);
    str->len += needed;

    va_end(args);

    return str;
}

str_t *str_catfv(str_t *str, const char *fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    size_t needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    ensure_size(str, needed + 1);

    vsprintf(stroffset(str), fmt, args);
    str->len += needed;

    return str;
}

static int int_to_str(long long int value, char *buffer)
{
    int len = 0;

    if (value < 0)
    {
        buffer[len++] = '-';
        value = -value;
    }

    char temp[68];
    int temp_len = 0;
    do
    {
        temp[temp_len++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    for (int i = temp_len - 1; i >= 0; --i)
        buffer[len++] = temp[i];

    return len;
}

str_t *str_catf_d(str_t *str, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    const char *f = fmt;
    char c = 0;
    char n = 0;
    char digit[68] = {0};

    while ((c = *f))
    {
        n = *(f + 1);

        if (c == '%' && n == 'd')
        {
            int v = va_arg(args, int);
            int len = int_to_str(v, digit);
            str_catlen(str, digit, len);

            f += 2;
        }
        else if (c == '%' && n == '%')
        {
            ensure_size(str, 1);
            str->buf[str->len++] = '%';
            f += 2;
        }
        else if (c == '%')
        {
            ensure_size(str, 2);
            str->buf[str->len++] = '%';
            str->buf[str->len++] = n;
            f += 2;
        }
        else
        {
            ensure_size(str, 1);
            str->buf[str->len++] = c;
            f++;
        }
    }

    append_null(str);
    va_end(args);

    return str;
}

str_t *str_catw(str_t *str, wchar_t *ws)
{
    size_t len = wcstombs(NULL, ws, 0);
    if (len == (size_t)-1)
    {
        fprintf(stderr, "Failed to get the required length for an mbs\n");
        errno = -EINVAL;
        return str;
    }

    ensure_size(str, len + 1);

    size_t length = wcstombs(stroffset(str), ws, len + 1);
    if (length == (size_t)-1)
    {
        fprintf(stderr, "Failed to convert wide string to mbs\n");
        errno = -EINVAL;
        return str;
    }

    str->len += length;

    return str;
}

str_t *str_catwch(str_t *str, wchar_t wc)
{
    ensure_size(str, MB_CUR_MAX + 1);

    int length = wctomb(stroffset(str), wc);
    if (length < 0)
    {
        fprintf(stderr, "Failed to convert wide char to mbs\n");
        errno = -EINVAL;
        return str;
    }

    str->len += length;
    append_null(str);

    return str;
}

str_t *str_pad(str_t *str, char ch, size_t size)
{
    ensure_size(str, size + 1);
    memset(stroffset(str), ch, size);

    str->len += size;
    append_null(str);

    return str;
}

strview_t str_strip(str_t *str)
{
    char *start = str->buf;
    char *end = str->buf + str->len;

    while (start < end && isspace(*start))
        start++;

    while (end > start && isspace(*(end - 1)))
        end--;

    return (strview_t){.start = start, .size = (size_t)(end - start)};
}

str_t *str_join(str_t *str, const arrayof(char *) * srcs, const str_t *sep)
{
    char *src;
    ARR_FOREACH(*srcs, src, i)
    {
        str_cat(str, src);
        if (i != srcs->len - 1)
            str_cat_str(str, sep);
    }

    return str;
}

char *_path_join(ator_t *ator, int count, ...)
{
    va_list args;
    va_start(args, count);

    ator_t tmp = {0};

    arrayof(path_segment) segments = new (segments, &tmp);
    arrayof(path_segment) each_segments = new (each_segments, &tmp);

    for (int i = 0; i < count; i++)
    {
        char *path = va_arg(args, char *);
        if (path == NULL || strcmp(path, "") == 0)
            continue;

        path_split(path, &each_segments);
        array_extend(&segments, &each_segments);
        each_segments.len = 0;
    }

    va_end(args);

    char *path = path_from_segments(&segments, ator);
    ator_free(&tmp);

    return path;
}

size_t path_segment_size(const char *path, size_t *out_nb_segments)
{
    size_t size = 0;
    size_t nb_segments = 0;

    char c = 0;
    int is_last_separator = 1;
    while ((c = *path++))
    {
        switch (c)
        {
        case '/':
        case '\\':
            if (!is_last_separator)
                nb_segments++;
            is_last_separator = 1;
            break;
        default:
            is_last_separator = 0;
            size++;
        }
    }

    if (out_nb_segments)
        *out_nb_segments = nb_segments + (!is_last_separator);

    return size;
}

size_t path_segments_size(const arrayof(path_segment) * segments)
{
    size_t size = 0;
    path_segment *segment;
    ARR_FOREACH_BYREF(*segments, segment, i)
    {
        if (segment->flags & SEGMENT_DRIVE)
            size += 1; // for ':' in 'C:\'

        size += segment->size + (i != segments->len - 1);
    }

    return size;
}

void path_split(char *path, arrayof(path_segment) * out)
{
    size_t size = strlen(path);

    path_segment drive = {0};
    size_t skip = path_skip_drive_segment(path, size, &drive);
    if (skip > 0)
    {
        array_append(out, &drive, sizeof(path_segment));
        path += skip;
    }

    path_segment seg = {.buf = path, .size = 0};
    char c = 0;

    while ((c = *path++))
    {
        switch (c)
        {
        case '/':
        case '\\':
            if (seg.size > 0)
            {
                array_append(out, &seg, sizeof(path_segment));
                seg.buf += seg.size;
                seg.size = 0;
            }
            seg.buf++;
            break;
        default:
            seg.size++;
            break;
        }
    }

    if (seg.size > 0)
        array_append(out, &seg, sizeof(path_segment));
}

size_t path_skip_drive_segment(char *path, size_t size,
                               path_segment *drive_letter)
{
    path_segment seg = {.buf = path, .size = 0, .flags = SEGMENT_DRIVE};
    size_t offset = 0;
    int has_colon = 0;
    int epilogue = 0;

    while (offset < size)
    {
        char c = path[offset];
        switch (c)
        {
        case '/':
        case '\\':
            epilogue = 1;
            break;
        case ':':
            if (epilogue)
                goto end;
            has_colon = 1;
            break;
        default:
            if (epilogue)
                goto end;
            seg.size++;
            break;
        }
        offset++;
    }

end:
    if (!has_colon)
        return 0;

    if (drive_letter)
        *drive_letter = seg;

    return offset;
}

int path_is_sep(char c)
{
    return c == '/' || c == '\\';
}

char *path_from_segments(const arrayof(path_segment) * segments, ator_t *ator)
{
    size_t req = path_segments_size(segments);

    char *path = NULL;
    if (ator)
        path = ator_alloc(ator, req + 1, NULL, 0);
    else
        path = UNWRAP_OR_PANIC(malloc(req + 1));
    path[req] = '\0';
    size_t current = 0;

    path_segment *segment;
    ARR_FOREACH_BYREF(*segments, segment, i)
    {
        assert(current <= req);
        memcpy(path + current, segment->buf, segment->size);
        current += segment->size;

        if (segment->flags & SEGMENT_DRIVE)
            path[current++] = ':';

        if (i != segments->len - 1)
            path[current++] = PATH_SEP[0];
    }

    assert(req == current);

    return path;
}

void path_segments_print(const arrayof(path_segment) * segments)
{
    path_segment *segment;
    ARR_FOREACH_BYREF(*segments, segment, i)
    {
        print_raw(segment->buf, segment->size);
    }
}

const char *path_name(const char *path)
{
    size_t offset = strlen(path);
    while (offset > 1 && !path_is_sep(path[offset - 1]))
        offset--;

    return path + offset;
}

const char *path_stem(ator_t *ator, const char *path)
{
    const char *name = path_name(path);

    const char *from = name;
    while (*from++ != '.') { /* pass */ }

    return ator_strndup(ator, name, (size_t)(from - name) - 1);
}

int path_exists(const char *path)
{
#if OS == OS_LINUX
    return access(path, F_OK) == 0;
#else
#  warning "NOT TESTED"
    return _access(path, 0) == 0;
#endif // OS == OS_LINUX
}

int path_isdir(const char *path)
{
    struct stat s;
    if (stat(path, &s) == -1)
        return 0;

    return S_ISDIR(s.st_mode);
}

int path_mkdir(char *path, mode_t mode, int exists_ok)
{
    ator_t ator = {0};
    arrayof(path_segment) segments = new (segments, &ator);
    arrayof(char) buffer = new (buffer, &ator);

    path_split(path, &segments);

    path_segment segment;
    ARR_FOREACH(segments, segment, i)
    {
        if (segment.flags & SEGMENT_DRIVE)
            continue;

        array_append_many(&buffer, segment.buf, 1, segment.size);
        array_append(&buffer, &(char){'/'}, 1);

        if (mkdir(buffer.items, mode) != 0 && (!exists_ok || errno != EEXIST))
            return -1;
    }

    ator_free(&ator);

    return 0;
}

int path_touch(const char *path, mode_t mode)
{
    int fd = open(path, O_CREAT | O_WRONLY, mode);
    if (fd == -1)
        return -1;
    close(fd);

    return 0;
}

void array_init(array_t *arr, size_t item_size)
{
    UNWRAP_OR_PANIC(arr);
    TRUTHY_OR_PANIC(arr->items == NULL);

    if (arr->items == NULL)
    {
        arr->item_size = item_size;
        arr->len = 0;
        arr->cap = ARRAY_DEFAULT_SIZE;
        arr->items = UNWRAP_OR_PANIC(calloc(ARRAY_DEFAULT_SIZE, item_size));
    }
}

void array_init_n(array_t *arr, size_t item_size, size_t nb_items)
{
    UNWRAP_OR_PANIC(arr);
    TRUTHY_OR_PANIC(arr->items == NULL);

    if (arr->items == NULL)
    {
        arr->item_size = item_size;
        arr->len = 0;
        arr->cap = nb_items;
        arr->items = UNWRAP_OR_PANIC(calloc(nb_items, item_size));
    }
}

void array_free(array_t *arr)
{
    if (arr == NULL)
        return;

    if (arr->items)
    {
        free(arr->items);
        arr->items = 0;
    }

    arr->item_size = 0;
    arr->len = 0;
    arr->cap = 0;
}

void array_reserve(array_t *arr, size_t item_size, size_t nb_items)
{
    PP_MAP(UNWRAP_OR_PANIC, arr);

    if (arr->items == NULL)
        array_init_n(arr, item_size, nb_items);

    arr->items =
        UNWRAP_OR_PANIC(realloc(arr->items, item_size * (arr->cap + nb_items)));
    memset(arr->items + (arr->cap * item_size), 0, nb_items * item_size);

    arr->cap += nb_items;
}

void array_append(array_t *arr, const void *mem, size_t item_size)
{
    PP_MAP(UNWRAP_OR_PANIC, arr, mem);

    if (arr->items == NULL)
        array_init(arr, item_size);

    TRUTHY_OR_PANIC(arr->item_size == item_size);
    if (arr->len + 1 > arr->cap)
    {
        arr->items = UNWRAP_OR_PANIC(
            realloc(arr->items, (arr->cap *= 2) * arr->item_size));
        memset(arr->items + (arr->len * arr->item_size), 0,
               (arr->cap - arr->len) * arr->item_size);
    }

    memcpy(arr->items + (arr->len * arr->item_size), mem, arr->item_size);
    arr->len++;
}

void array_append_many(array_t *arr, const void *mem, size_t item_size,
                       size_t nb_items)
{
    PP_MAP(UNWRAP_OR_PANIC, arr, mem);

    if (arr->items == NULL)
        array_init(arr, item_size);

    TRUTHY_OR_PANIC(arr->item_size == item_size);
    if (arr->len + nb_items > arr->cap)
    {
        size_t new_size = arr->cap * 2;
        while (arr->len + nb_items > new_size)
            new_size *= 2;

        arr->items =
            UNWRAP_OR_PANIC(realloc(arr->items, new_size * arr->item_size));
        arr->cap = new_size;

        memset(arr->items + (arr->len * arr->item_size), 0,
               (arr->cap - arr->len) * arr->item_size);
    }

    memcpy(arr->items + (arr->len * arr->item_size), mem,
           nb_items * arr->item_size);
    arr->len += nb_items;
}

void array_extend(array_t *arr, const array_t *source)
{
    array_append_many(arr, source->items, source->item_size, source->len);
}

void *array_pop(array_t *arr, int64_t _index)
{
    PP_MAP(UNWRAP_OR_PANIC, arr, arr->items);

    uint64_t index = _index % arr->len;

    void *elm = arr->items + index * arr->item_size;

    memmove(arr->items + index * arr->item_size,
            arr->items + (index + 1) * arr->item_size,
            (arr->len - (index + 1)) * arr->item_size);

    return elm;
}

void array_insert(array_t *arr, const void *mem, size_t item_size, size_t index)
{
    PP_MAP(UNWRAP_OR_PANIC, arr, mem);

    if (arr->items == NULL)
        array_init(arr, item_size);

    TRUTHY_OR_PANIC(arr->item_size == item_size);
    TRUTHY_OR_PANIC(index <= arr->len);

    if (arr->len + 1 > arr->cap)
    {
        arr->cap *= 2;
        arr->items =
            UNWRAP_OR_PANIC(realloc(arr->items, arr->cap * arr->item_size));
    }

    memmove(arr->items + (index + 1) * arr->item_size,
            arr->items + index * arr->item_size,
            (arr->len - index) * arr->item_size);

    memcpy(arr->items + index * arr->item_size, mem, arr->item_size);

    arr->len++;
}

void array_overwrite(array_t *arr, const void *mem, size_t item_size,
                     size_t index)
{
    PP_MAP(UNWRAP_OR_PANIC, arr, mem);

    if (arr->items == NULL)
        array_init(arr, item_size);

    TRUTHY_OR_PANIC(arr->item_size == item_size);
    TRUTHY_OR_PANIC(index < arr->len);

    memcpy(arr->items + index * arr->item_size, mem, arr->item_size);
}

void _print_raw(const char *name, const void *mem, size_t size, int group,
                int columns)
{
    printf("\n    * %s (%zu bytes)\n", name, size);
    const uint8_t *bytes = (const uint8_t *)mem;
    size_t offset = 0;

    while (offset < size)
    {
        printf("[0x%08lx]   ", offset);

        for (int col = 0; col < columns; col++)
        {
            if (offset + col * group >= size)
            {
                for (int i = 0; i < group * 2 + 1; i++)
                    putchar(' ');
            }
            else
            {
                for (int byte = 0; byte < group; byte++)
                {
                    size_t idx = offset + col * group + byte;
                    if (idx < size)
                    {
                        switch (bytes[idx])
                        {
                        case '\n':
                            printf("\\n");
                            break;
                        case '\t':
                            printf("\\t");
                            break;
                        case '\r':
                            printf("\\r");
                            break;
                        case '\x1b':
                            printf("\\x1b");
                            break;
                        default:
                            if (isprint(bytes[idx]))
                                printf("%c  ", bytes[idx]);
                            else
                                printf("%02X ", bytes[idx]);
                            break;
                        }
                    }
                    else
                        printf("   ");
                }
                printf("    ");
            }
        }

        putchar('\n');
        offset += group * columns;
    }
}

#if OS == OS_LINUX
int fs_iter_init(fs_iterator *iter, char *dir, ator_t *ator)
{
    PP_MAP(UNWRAP_OR_PANIC, iter, dir, ator);

    DIR *d = UNWRAP_OR_PANIC(opendir(dir));
    ator_ref(ator, d, (void *)closedir, ATOR_MEMZERO);

    iter->d = d;
    iter->dir = dir;
    iter->exhausted = 0;

    return 0;
}

fs_entry *fs_iter_next(fs_iterator *iter, ator_t *ator)
{
    PP_MAP(UNWRAP_OR_PANIC, iter, ator);

    if (iter->exhausted)
        return NULL;

    fs_entry *entry = ator_alloc(ator, sizeof(*entry), NULL, ATOR_MEMZERO);

    struct dirent *dent = NULL;
    do
    {
        dent = readdir(iter->d);
        if (dent == NULL)
        {
            if (errno != 0)
            {
                fprintf(stderr, "%s: Failed to read the next entry: %s\n",
                        __FUNCTION__, strerror(errno));
            }
            iter->exhausted = 1;
            return NULL;
        }
    } while (dent && (strcmp(dent->d_name, ".") == 0 ||
                      strcmp(dent->d_name, "..") == 0));

    entry->path = path_join(ator, iter->dir, dent->d_name);

    TRUTHY_OR_PANIC(stat(entry->path, &entry->stat) != -1);

    return entry;
}

int fs_entry_isdir(const fs_entry *entry)
{
    return S_ISDIR(entry->stat.st_mode);
}
#else
#  error "Implement fs"
#endif // OS == OS_LINUX

void _ator_join(ator_t *ator, ator_t *new)
{
    if (new == NULL)
        return;

    if (ator->head == NULL)
        ator->head = new;
    else
        ator->tail->next = new;

    ator->tail = new;
    if (ator->next == NULL)
        ator->next = new;
}

void _ator_print(ator_t *ator)
{
    ator_t *node = ator;
    printf("{\n    ");
    while (node)
    {
        char *s = node->flags & ATOR_DEBUG ? "\x1b[36m" : "\x1b[33m";
        char *n = "\x1b[38;2;200;200;200;4m";
        char *e = node->flags & ATOR_DEBUG ? "\x1b[0m" : "\x1b[0m";
        printf(
            "%sator(ptr%s=%s%p%s, %sflags%s=%s%d%s, %sdtor%s=%s%p%s%s)%s -> ",
            s, e, n, node->ptr, e, s, e, n, node->flags, e, s, e, n,
            (void *)node->dtor, e, s, e);
        node = node->next;
    }
    printf("\n}\n");
}

char *_ator_snprintf(ator_t *ator, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    size_t needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    va_start(args, fmt);

    char *buffer = ator_alloc(ator, needed + 1, NULL, 0);
    vsnprintf(buffer, needed + 1, fmt, args);

    va_end(args);

    return buffer;
}

ator_t *_ator_new(size_t size, dtor_fn dtor, uint32_t flags)
{
    ator_t *ator = malloc(sizeof(ator_t));

    if (flags & ATOR_MEMZERO)
        ator->ptr = UNWRAP_OR_PANIC(calloc(1, sizeof(ator_t) + size));
    else
        ator->ptr = UNWRAP_OR_PANIC(malloc(sizeof(ator_t) + size));

    ator->dtor = dtor;
    ator->flags = flags | ATOR_NONROOT;
    ator->next = NULL;

    return ator;
}

void _ator_ref(ator_t *ator, void *ptr, dtor_fn dtor, uint32_t flags)
{
    ator_t *next = UNWRAP_OR_PANIC(calloc(1, sizeof(*next)));

    next->ptr = ptr;
    next->dtor = dtor;
    next->flags |= flags | ATOR_REFPTR | ATOR_NONROOT;
    next->next = NULL;

    ator_join(ator, next);
}

void *_ator_alloc(ator_t *ator, size_t size, dtor_fn dtor, uint32_t flags)
{
    ator_t *next = ator_new(size, dtor, flags);
    ator_join(ator, next);

    return next->ptr;
}

void *_ator_memdup(ator_t *ator, void *ptr, size_t size, dtor_fn dtor,
                   uint32_t flags)
{
    ator_t *next = ator_new(size, dtor, flags);
    memcpy(next->ptr, ptr, size);

    ator_join(ator, next);

    return next->ptr;
}

ator_t *_ator_free_single(ator_t **_ator)
{
    if (_ator == NULL || *_ator == NULL)
        return NULL;

    ator_t *ator = *_ator;

    if ((ator->flags & ATOR_NONROOT) == 0)
        return ator->next;

    if (ator->dtor)
        ator->dtor(ator->ptr);

    if (!(ator->flags & ATOR_REFPTR))
        free(ator->ptr);
    ator->ptr = NULL;
    ator->dtor = NULL;

    ator_t *next = ator->next;

    free(ator);
    *_ator = NULL;

    return next;
}

ator_t *_ator_reverse(ator_t *ator)
{
    ator_t *prev = NULL, *current = ator, *next = NULL;

    while (current)
    {
        next = current->next;
        current->next = prev;

        prev = current;
        current = next;
    }

    return prev;
}

void _ator_free(ator_t *ator)
{
    if (ator == NULL)
        return;

    ator_t *item = _ator_reverse(ator);
    while ((item = ator_free_single(&item))) { /* pass */ }
}

void _ator_free_until(ator_t **start, ator_t *end)
{
    if (start == NULL || *start == NULL)
        return;

    ator_t *item = *start;
    while (item)
    {
        item = ator_free_single(&item);
        if (item == end)
        {
            item = ator_free_single(&item);
            break;
        }

        if (*start == end)
            break;
    }

    *start = NULL;
}

char *fs_file_read(const char *path, ator_t *ator)
{
    FILE *fd = UNWRAP_OR(fopen(path, "r"), return NULL);
    char *buf = NULL;

    fseek(fd, 0, SEEK_END);
    ssize_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    if (size == -1)
        goto error;

    buf = ator_alloc(ator, size + 1, NULL, 0);
    size_t read = fread(buf, 1, size, fd);
    if (read != (size_t)size)
    {
        fprintf(stderr, "fread() failed: %zu\n", read);
        goto error;
    }
    buf[size] = '\0';

    fclose(fd);

    return buf;

error:
    if (buf)
        free(buf);
    if (fd)
        fclose(fd);
    return NULL;
}

/* SIPHASH algorithm from
 * https://github.com/majek/csiphash/blob/master/csiphash.c
 */

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&             \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define _le64toh(x) ((uint64_t)(x))
#elif defined(_WIN32)
/* Windows is always little endian, unless you're on xbox360
   http://msdn.microsoft.com/en-us/library/b0084kay(v=vs.80).aspx */
#  define _le64toh(x) ((uint64_t)(x))
#elif defined(__APPLE__)
#  include <libkern/OSByteOrder.h>
#  define _le64toh(x) OSSwapLittleToHostInt64(x)
#else

/* See: http://sourceforge.net/p/predef/wiki/Endianness/ */
#  if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#    include <sys/endian.h>
#  else
#    include <endian.h>
#  endif
#  if defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) &&                     \
      __BYTE_ORDER == __LITTLE_ENDIAN
#    define _le64toh(x) ((uint64_t)(x))
#  else
#    define _le64toh(x) le64toh(x)
#  endif

#endif

#define ROTATE(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define HALF_ROUND(a, b, c, d, s, t)                                           \
    a += b;                                                                    \
    c += d;                                                                    \
    b = ROTATE(b, s) ^ a;                                                      \
    d = ROTATE(d, t) ^ c;                                                      \
    a = ROTATE(a, 32);

#define DOUBLE_ROUND(v0, v1, v2, v3)                                           \
    HALF_ROUND(v0, v1, v2, v3, 13, 16);                                        \
    HALF_ROUND(v2, v1, v0, v3, 17, 21);                                        \
    HALF_ROUND(v0, v1, v2, v3, 13, 16);                                        \
    HALF_ROUND(v2, v1, v0, v3, 17, 21);

uint64_t siphash24(const void *src, unsigned long src_sz)
{
    return siphash24k(src, src_sz, "0123456789abcdef");
}

uint64_t siphash24k(const void *src, unsigned long src_sz, const char key[16])
{
    const uint64_t *_key = (uint64_t *)key;
    uint64_t k0 = _le64toh(_key[0]);
    uint64_t k1 = _le64toh(_key[1]);
    uint64_t b = (uint64_t)src_sz << 56;
    const uint64_t *in = (uint64_t *)src;

    uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
    uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
    uint64_t v3 = k1 ^ 0x7465646279746573ULL;

    while (src_sz >= 8)
    {
        uint64_t mi = _le64toh(*in);
        in += 1;
        src_sz -= 8;
        v3 ^= mi;
        DOUBLE_ROUND(v0, v1, v2, v3);
        v0 ^= mi;
    }

    uint64_t t = 0;
    uint8_t *pt = (uint8_t *)&t;
    uint8_t *m = (uint8_t *)in;
    switch (src_sz)
    {
    case 7:
        pt[6] = m[6];
        __attribute__((fallthrough));
    case 6:
        pt[5] = m[5];
        __attribute__((fallthrough));
    case 5:
        pt[4] = m[4];
        __attribute__((fallthrough));
    case 4:
        *((uint32_t *)&pt[0]) = *((uint32_t *)&m[0]);
        break;
    case 3:
        pt[2] = m[2];
        __attribute__((fallthrough));
    case 2:
        pt[1] = m[1];
        __attribute__((fallthrough));
    case 1:
        pt[0] = m[0];
    }
    b |= _le64toh(t);

    v3 ^= b;
    DOUBLE_ROUND(v0, v1, v2, v3);
    v0 ^= b;
    v2 ^= 0xff;
    DOUBLE_ROUND(v0, v1, v2, v3);
    DOUBLE_ROUND(v0, v1, v2, v3);
    return (v0 ^ v1) ^ (v2 ^ v3);
}
