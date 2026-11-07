#ifndef __EXCEPTION_H
#define __EXCEPTION_H

#include <setjmp.h>

#ifdef _WIN32
typedef jmp_buf exc_jmp_buf;
#  define exc_setjmp(buf) setjmp(buf)
#  define exc_longjmp(buf, val) longjmp(buf, val)
#else
typedef sigjmp_buf exc_jmp_buf;
#  define exc_setjmp(buf) sigsetjmp(buf, 1)
#  define exc_longjmp(buf, val) siglongjmp(buf, val)
#endif // _WIN32

typedef struct exc_context
{
    exc_jmp_buf ctx;
    struct exc_context *next;
} exc_context;

#define try                                                                    \
    if (exc_setjmp(_exception_push_context(&(exc_context){0})->ctx) == 0)      \
    {
#define except                                                                 \
    }                                                                          \
    else if (_exception_pop_context())

exc_context *_exception_push_context(exc_context *ctx);
int _exception_pop_context();

void exception_init();

void exception_panic(void (*handler)());

#endif /* __EXCEPTION_H */
