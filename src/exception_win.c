#include "exception.h"
#include "logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#  define THREAD_LOCAL __declspec(thread)
#else
#  define THREAD_LOCAL _Thread_local
#endif

static void (*panic)() = NULL;
static THREAD_LOCAL exc_context *exc_head = NULL;
static PVOID veh_handle = NULL;

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

static const char *exc_code_name(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
    default:
        return "UNKNOWN_EXCEPTION";
    }
}

static int is_handled_code(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
        return 1;
    default:
        return 0;
    }
}

static LONG CALLBACK vectored_handler(PEXCEPTION_POINTERS info)
{
    DWORD code = info->ExceptionRecord->ExceptionCode;

    if (!is_handled_code(code))
        return EXCEPTION_CONTINUE_SEARCH;

    log_error("Exception caught: %s (0x%08lx)\n", exc_code_name(code), code);

    if (exc_head)
    {
        log_debug("jmp_buf set, recovering...\n");
        exc_longjmp(exc_head->ctx, 1);
    }
    else
    {
        log_fatal("No jmp_buf setpoint set, program will crash\n");
        if (panic)
        {
            log_debug("Run panic handler %p\n", panic);
            panic();
        }
        exit((int)code);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void exception_init()
{
    veh_handle = AddVectoredExceptionHandler(1, vectored_handler);
    if (veh_handle == NULL)
        log_error("Failed to install vectored exception handler\n");
}

void exception_panic(void (*handler)())
{
    panic = handler;
}
