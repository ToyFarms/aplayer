#ifndef __AP_TERMINAL_H
#define __AP_TERMINAL_H

#include "ap_math.h"
#include "ap_os.h"
#include "ap_utils.h"
#include "ap_event.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define ESC "\x1b"

#if OS_TYPE == OS_WINDOWS
#    include <windows.h>
typedef struct APHandle
{
    HANDLE handle;
} APHandle;
#elif OS_TYPE == OS_LINUX
#error "Target Build Linux is not Implemented"
#elif OS_TYPE == OS_APPLE
#error "Target Build Apple is not Implemented"
#endif // OS_TYPE == OS_WINDOWS

typedef enum HandleType
{
    HANDLE_STDOUT,
    HANDLE_STDIN,
    HANDLE_STDERR,
} HandleType;

typedef struct APTermContext
{
    bool should_close;
    APHandle handle_out;
    APHandle handle_in;
    Vec2 size;
    Vec2 mouse_pos;
    bool resized;
} APTermContext;

APHandle ap_term_get_std_handle(HandleType type);
void ap_term_switch_main_buf(APHandle h);
void ap_term_switch_alt_buf(APHandle h);
void ap_term_show_cursor(APHandle h, bool visible);
Vec2 ap_term_get_size(APHandle h);
void ap_term_set_cursor(APHandle h, Vec2 pos);
Vec2 ap_term_get_cursor(APHandle h);
void ap_term_write(APHandle h, const char *str, size_t str_len);
void ap_term_writew(APHandle h, const wchar_t *wstr, size_t wstr_len);
void ap_term_get_events(APHandle h_in, APEvent *events_out, int len, int *out_len);

#endif /* __AP_TERMINAL_H */
