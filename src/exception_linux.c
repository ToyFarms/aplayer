#include "exception.h"
#include "logger.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

static void (*unrecoverable)() = NULL;
static exc_context *exc_head = NULL;

exc_context *_exception_push_context(exc_context *ctx)
{
    ctx->next = exc_head;
    exc_head = ctx;

    return ctx;
}

int _exception_pop_context()
{
    if (exc_head)
        exc_head = exc_head->next;

    return 1;
}

static void signal_handler(int sig)
{
    log_error("Signal caught: %s (%d)\n", strsignal(sig), sig);
    if (exc_head)
    {
        log_debug("jmp_buf set, recovering...\n");
        siglongjmp(exc_head->ctx, 1);
    }
    else
    {
        log_fatal("No jmp_buf setpoint set, program will crash\n");
        if (unrecoverable)
        {
            log_debug("Run final user-provided cleanup function %p\n",
                      unrecoverable);
            unrecoverable();
        }
        exit(sig);
    }
}

void exception_init()
{
    signal(SIGSEGV, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGILL, signal_handler);
    signal(SIGBUS, signal_handler);
}

void exception_cleanup()
{
    signal(SIGSEGV, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
}

void exception_unrecoverable(void (*handler)())
{
    unrecoverable = handler;
}
