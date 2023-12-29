#ifndef _LIBHOOK_H
#define _LIBHOOK_H

#include <libavutil/log.h>
#include <stdint.h>
#include <stdbool.h>

#include "libos.h"
#include "libcli.h"

#ifdef AP_WINDOWS
#include <windows.h>
#elif defined(AP_MACOS)
#elif defined(AP_LINUX)
#endif // AP_WINDOWS

void *keyboard_hooks(void *arg);

#endif /* _LIBHOOK_H */ 