#ifndef __AP_TERMINAL_H
#define __AP_TERMINAL_H

#include "ap_math.h"
#include "ap_os.h"
#include "ap_utils.h"

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

#define MOUSE_LEFT_CLICKED   (1 << 0)
#define MOUSE_MIDDLE_CLICKED (1 << 1)
#define MOUSE_RIGHT_CLICKED  (1 << 2)

#define SHIFT_KEY_PRESSED (1 << 0)
#define CTRL_KEY_PRESSED  (1 << 1)
#define ALT_KEY_PRESSED   (1 << 2)

typedef struct APTermKeyEvent
{
    bool key_down;
    char ascii_key;
    wchar_t unicode_key;
    uint32_t vk_key;
    uint32_t modifier_key;
} APTermKeyEvent;

typedef struct APTermMouseEvent
{
    int x, y;
    uint32_t state;
    bool double_clicked;
    bool moved;

    bool scrolled;
    int scroll_delta;
} APTermMouseEvent;

typedef struct APTermBufEvent
{
    int to_width, to_height;
} APTermBufEvent;

typedef enum APTermEventType
{
    AP_TERM_KEY_EVENT,
    AP_TERM_MOUSE_EVENT,
    AP_TERM_BUF_EVENT,
    AP_TERM_UNKNOWN_EVENT,
} APTermEventType;

typedef struct APTermEvent
{
    APTermEventType type;
    APTermKeyEvent key_event;
    APTermMouseEvent mouse_event;
    APTermBufEvent buf_event;
} APTermEvent;

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
void ap_term_get_events(APHandle h_in, APTermEvent *events_out, int len, int *out_len);

#endif /* __AP_TERMINAL_H */
