#ifndef __EXCEPTION_H
#define __EXCEPTION_H

#include <setjmp.h>

typedef struct exc_context
{
    jmp_buf ctx;
    struct exc_context *next;
} exc_context;

#define try                                                                    \
    if (sigsetjmp(_exception_push_context(&(exc_context){0})->ctx, 1) == 0)    \
    {
#define except                                                                 \
    }                                                                          \
    else if (_exception_pop_context())
#define except_ignore                                                          \
    }                                                                          \
    else _exception_pop_context()

exc_context *_exception_push_context(exc_context *ctx);
int _exception_pop_context();

void exception_init();
void exception_cleanup();

void exception_unrecoverable(void (*handler)());

#endif /* __EXCEPTION_H */
