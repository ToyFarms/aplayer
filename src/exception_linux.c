#include "exception.h"
#include "logger.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

static void (*panic)() = NULL;
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
        if (panic)
        {
            log_debug("Run final user-provided cleanup function %p\n",
                      panic);
            panic();
        }
        exit(sig);
    }
}

void exception_init()
{
    struct sigaction act;
    act.sa_handler = signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
}

void exception_panic(void (*handler)())
{
    panic = handler;
}
