#ifndef _LIBCLI_H
#define _LIBCLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <libavutil/common.h>
#include <libavutil/log.h>
#include <libavutil/avutil.h>
#include <pthread.h>
#include <wchar.h>

#include "libos.h"
#include "sb.h"
#include "libhelper.h"
#include "libfile.h"

#ifdef __clang__
#define ATTRIBUTE_USED __attribute__((used))
#elif defined(__GNUC__) || defined(__GNUG__)
#define ATTRIBUTE_USED __attribute__((used))
#elif defined(_MSC_VER)
#define ATTRIBUTE_USED
#endif // __GNUC__

typedef enum BUFFER_TYPE BUFFER_TYPE;
typedef struct Handle Handle;
typedef struct CLIState CLIState;
typedef struct Events Events;

#define LEFT_MOUSE_CLICKED (1 << 0)
#define MIDDLE_MOUSE_CLICKED (1 << 1)
#define RIGHT_MOUSE_CLICKED (1 << 2)

#define SHIFT_KEY_PRESSED (1 << 0)
#define CTRL_KEY_PRESSED (1 << 1)
#define ALT_KEY_PRESSED (1 << 2)

typedef struct KeyEvent
{
    bool key_down;
    char acsii_key;
    wchar_t unicode_key;
    uint32_t vk_key;
    uint32_t modifier_key;
} KeyEvent;

typedef struct MouseEvent
{
    int x, y;
    uint32_t state;
    bool double_clicked;
    bool moved;

    bool scrolled;
    int16_t scroll_delta;
} MouseEvent;

typedef struct BufferChangedEvent
{
    int width, height;
} BufferChangedEvent;

typedef enum EventType
{
    KEY_EVENT_TYPE,
    MOUSE_EVENT_TYPE,
    BUFFER_CHANGED_EVENT_TYPE,
    UNKNOWN_EVENT_TYPE,
} EventType;

typedef struct Event
{
    EventType type;
    KeyEvent key_event;
    MouseEvent mouse_event;
    BufferChangedEvent buf_event;
} Event;

typedef struct Vec2
{
    int x, y;
} Vec2;

typedef struct Color
{
    int r, g, b;
} Color;

typedef struct Rect
{
    int x, y;
    int w, h;
} Rect;

typedef enum LineState
{
    LINE_HOVERED,
    LINE_PLAYING,
    LINE_SELECTED,
    LINE_NORMAL,
} LineState;

#if defined(AP_WINDOWS)

#include <windows.h>

#ifndef NOVIRTUALKEYCODES

#define VIRT_LBUTTON VK_LBUTTON
#define VIRT_RBUTTON VK_RBUTTON
#define VIRT_CANCEL VK_CANCEL
#define VIRT_MBUTTON VK_MBUTTON
#define VIRT_XBUTTON1 VK_XBUTTON1
#define VIRT_XBUTTON2 VK_XBUTTON2
#define VIRT_BACK VK_BACK
#define VIRT_TAB VK_TAB
#define VIRT_CLEAR VK_CLEAR
#define VIRT_RETURN VK_RETURN
#define VIRT_SHIFT VK_SHIFT
#define VIRT_CONTROL VK_CONTROL
#define VIRT_MENU VK_MENU
#define VIRT_PAUSE VK_PAUSE
#define VIRT_CAPITAL VK_CAPITAL
#define VIRT_KANA VK_KANA
#define VIRT_HANGEUL VK_HANGEUL
#define VIRT_HANGUL VK_HANGUL
#define VIRT_IME_ON VK_IME_ON
#define VIRT_JUNJA VK_JUNJA
#define VIRT_FINAL VK_FINAL
#define VIRT_HANJA VK_HANJA
#define VIRT_KANJI VK_KANJI
#define VIRT_IME_OFF VK_IME_OFF
#define VIRT_ESCAPE VK_ESCAPE
#define VIRT_CONVERT VK_CONVERT
#define VIRT_NONCONVERT VK_NONCONVERT
#define VIRT_ACCEPT VK_ACCEPT
#define VIRT_MODECHANGE VK_MODECHANGE
#define VIRT_SPACE VK_SPACE
#define VIRT_PRIOR VK_PRIOR
#define VIRT_NEXT VK_NEXT
#define VIRT_END VK_END
#define VIRT_HOME VK_HOME
#define VIRT_LEFT VK_LEFT
#define VIRT_UP VK_UP
#define VIRT_RIGHT VK_RIGHT
#define VIRT_DOWN VK_DOWN
#define VIRT_SELECT VK_SELECT
#define VIRT_PRINT VK_PRINT
#define VIRT_EXECUTE VK_EXECUTE
#define VIRT_SNAPSHOT VK_SNAPSHOT
#define VIRT_INSERT VK_INSERT
#define VIRT_DELETE VK_DELETE
#define VIRT_HELP VK_HELP

#define VIRT_LWIN VK_LWIN
#define VIRT_RWIN VK_RWIN
#define VIRT_APPS VK_APPS
#define VIRT_SLEEP VK_SLEEP
#define VIRT_NUMPAD0 VK_NUMPAD0
#define VIRT_NUMPAD1 VK_NUMPAD1
#define VIRT_NUMPAD2 VK_NUMPAD2
#define VIRT_NUMPAD3 VK_NUMPAD3
#define VIRT_NUMPAD4 VK_NUMPAD4
#define VIRT_NUMPAD5 VK_NUMPAD5
#define VIRT_NUMPAD6 VK_NUMPAD6
#define VIRT_NUMPAD7 VK_NUMPAD7
#define VIRT_NUMPAD8 VK_NUMPAD8
#define VIRT_NUMPAD9 VK_NUMPAD9
#define VIRT_MULTIPLY VK_MULTIPLY
#define VIRT_ADD VK_ADD
#define VIRT_SEPARATOR VK_SEPARATOR
#define VIRT_SUBTRACT VK_SUBTRACT
#define VIRT_DECIMAL VK_DECIMAL
#define VIRT_DIVIDE VK_DIVIDE
#define VIRT_F1 VK_F1
#define VIRT_F2 VK_F2
#define VIRT_F3 VK_F3
#define VIRT_F4 VK_F4
#define VIRT_F5 VK_F5
#define VIRT_F6 VK_F6
#define VIRT_F7 VK_F7
#define VIRT_F8 VK_F8
#define VIRT_F9 VK_F9
#define VIRT_F10 VK_F10
#define VIRT_F11 VK_F11
#define VIRT_F12 VK_F12
#define VIRT_F13 VK_F13
#define VIRT_F14 VK_F14
#define VIRT_F15 VK_F15
#define VIRT_F16 VK_F16
#define VIRT_F17 VK_F17
#define VIRT_F18 VK_F18
#define VIRT_F19 VK_F19
#define VIRT_F20 VK_F20
#define VIRT_F21 VK_F21
#define VIRT_F22 VK_F22
#define VIRT_F23 VK_F23
#define VIRT_F24 VK_F24
#if _WIN32_WINNT >= 0x0604
#define VIRT_NAVIGATION_VIEW VK_NAVIGATION_VIEW
#define VIRT_NAVIGATION_MENU VK_NAVIGATION_MENU
#define VIRT_NAVIGATION_UP VK_NAVIGATION_UP
#define VIRT_NAVIGATION_DOWN VK_NAVIGATION_DOWN
#define VIRT_NAVIGATION_LEFT VK_NAVIGATION_LEFT
#define VIRT_NAVIGATION_RIGHT VK_NAVIGATION_RIGHT
#define VIRT_NAVIGATION_ACCEPT VK_NAVIGATION_ACCEPT
#define VIRT_NAVIGATION_CANCEL VK_NAVIGATION_CANCEL
#endif /* _WIN32_WINNT >= 0x0604 */
#define VIRT_NUMLOCK VK_NUMLOCK
#define VIRT_SCROLL VK_SCROLL
#define VIRT_OEM_NEC_EQUAL VK_OEM_NEC_EQUAL
#define VIRT_OEM_FJ_JISHO VK_OEM_FJ_JISHO
#define VIRT_OEM_FJ_MASSHOU VK_OEM_FJ_MASSHOU
#define VIRT_OEM_FJ_TOUROKU VK_OEM_FJ_TOUROKU
#define VIRT_OEM_FJ_LOYA VK_OEM_FJ_LOYA
#define VIRT_OEM_FJ_ROYA VK_OEM_FJ_ROYA
#define VIRT_LSHIFT VK_LSHIFT
#define VIRT_RSHIFT VK_RSHIFT
#define VIRT_LCONTROL VK_LCONTROL
#define VIRT_RCONTROL VK_RCONTROL
#define VIRT_LMENU VK_LMENU
#define VIRT_RMENU VK_RMENU
#define VIRT_BROWSER_BACK VK_BROWSER_BACK
#define VIRT_BROWSER_FORWARD VK_BROWSER_FORWARD
#define VIRT_BROWSER_REFRESH VK_BROWSER_REFRESH
#define VIRT_BROWSER_STOP VK_BROWSER_STOP
#define VIRT_BROWSER_SEARCH VK_BROWSER_SEARCH
#define VIRT_BROWSER_FAVORITES VK_BROWSER_FAVORITES
#define VIRT_BROWSER_HOME VK_BROWSER_HOME
#define VIRT_VOLUME_MUTE VK_VOLUME_MUTE
#define VIRT_VOLUME_DOWN VK_VOLUME_DOWN
#define VIRT_VOLUME_UP VK_VOLUME_UP
#define VIRT_MEDIA_NEXT_TRACK VK_MEDIA_NEXT_TRACK
#define VIRT_MEDIA_PREV_TRACK VK_MEDIA_PREV_TRACK
#define VIRT_MEDIA_STOP VK_MEDIA_STOP
#define VIRT_MEDIA_PLAY_PAUSE VK_MEDIA_PLAY_PAUSE
#define VIRT_LAUNCH_MAIL VK_LAUNCH_MAIL
#define VIRT_LAUNCH_MEDIA_SELECT VK_LAUNCH_MEDIA_SELECT
#define VIRT_LAUNCH_APP1 VK_LAUNCH_APP1
#define VIRT_LAUNCH_APP2 VK_LAUNCH_APP2
#define VIRT_OEM_1 VK_OEM_1
#define VIRT_OEM_PLUS VK_OEM_PLUS
#define VIRT_OEM_COMMA VK_OEM_COMMA
#define VIRT_OEM_MINUS VK_OEM_MINUS
#define VIRT_OEM_PERIOD VK_OEM_PERIOD
#define VIRT_OEM_2 VK_OEM_2
#define VIRT_OEM_3 VK_OEM_3
#if _WIN32_WINNT >= 0x0604
#define VIRT_GAMEPAD_A VK_GAMEPAD_A
#define VIRT_GAMEPAD_B VK_GAMEPAD_B
#define VIRT_GAMEPAD_X VK_GAMEPAD_X
#define VIRT_GAMEPAD_Y VK_GAMEPAD_Y
#define VIRT_GAMEPAD_RIGHT_SHOULDER VK_GAMEPAD_RIGHT_SHOULDER
#define VIRT_GAMEPAD_LEFT_SHOULDER VK_GAMEPAD_LEFT_SHOULDER
#define VIRT_GAMEPAD_LEFT_TRIGGER VK_GAMEPAD_LEFT_TRIGGER
#define VIRT_GAMEPAD_RIGHT_TRIGGER VK_GAMEPAD_RIGHT_TRIGGER
#define VIRT_GAMEPAD_DPAD_UP VK_GAMEPAD_DPAD_UP
#define VIRT_GAMEPAD_DPAD_DOWN VK_GAMEPAD_DPAD_DOWN
#define VIRT_GAMEPAD_DPAD_LEFT VK_GAMEPAD_DPAD_LEFT
#define VIRT_GAMEPAD_DPAD_RIGHT VK_GAMEPAD_DPAD_RIGHT
#define VIRT_GAMEPAD_MENU VK_GAMEPAD_MENU
#define VIRT_GAMEPAD_VIEW VK_GAMEPAD_VIEW
#define VIRT_GAMEPAD_LEFT_THUMBSTICK_BUTTON VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON
#define VIRT_GAMEPAD_RIGHT_THUMBSTICK_BUTTON VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON
#define VIRT_GAMEPAD_LEFT_THUMBSTICK_UP VK_GAMEPAD_LEFT_THUMBSTICK_UP
#define VIRT_GAMEPAD_LEFT_THUMBSTICK_DOWN VK_GAMEPAD_LEFT_THUMBSTICK_DOWN
#define VIRT_GAMEPAD_LEFT_THUMBSTICK_RIGHT VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT
#define VIRT_GAMEPAD_LEFT_THUMBSTICK_LEFT VK_GAMEPAD_LEFT_THUMBSTICK_LEFT
#define VIRT_GAMEPAD_RIGHT_THUMBSTICK_UP VK_GAMEPAD_RIGHT_THUMBSTICK_UP
#define VIRT_GAMEPAD_RIGHT_THUMBSTICK_DOWN VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN
#define VIRT_GAMEPAD_RIGHT_THUMBSTICK_RIGHT VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT
#define VIRT_GAMEPAD_RIGHT_THUMBSTICK_LEFT VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT
#endif /* _WIN32_WINNT >= 0x0604 */
#define VIRT_OEM_4 VK_OEM_4
#define VIRT_OEM_5 VK_OEM_5
#define VIRT_OEM_6 VK_OEM_6
#define VIRT_OEM_7 VK_OEM_7
#define VIRT_OEM_8 VK_OEM_8
#define VIRT_OEM_AX VK_OEM_AX
#define VIRT_OEM_102 VK_OEM_102
#define VIRT_ICO_HELP VK_ICO_HELP
#define VIRT_ICO_00 VK_ICO_00
#define VIRT_PROCESSKEY VK_PROCESSKEY
#define VIRT_ICO_CLEAR VK_ICO_CLEAR
#define VIRT_PACKET VK_PACKET
#define VIRT_OEM_RESET VK_OEM_RESET
#define VIRT_OEM_JUMP VK_OEM_JUMP
#define VIRT_OEM_PA1 VK_OEM_PA1
#define VIRT_OEM_PA2 VK_OEM_PA2
#define VIRT_OEM_PA3 VK_OEM_PA3
#define VIRT_OEM_WSCTRL VK_OEM_WSCTRL
#define VIRT_OEM_CUSEL VK_OEM_CUSEL
#define VIRT_OEM_ATTN VK_OEM_ATTN
#define VIRT_OEM_FINISH VK_OEM_FINISH
#define VIRT_OEM_COPY VK_OEM_COPY
#define VIRT_OEM_AUTO VK_OEM_AUTO
#define VIRT_OEM_ENLW VK_OEM_ENLW
#define VIRT_OEM_BACKTAB VK_OEM_BACKTAB
#define VIRT_ATTN VK_ATTN
#define VIRT_CRSEL VK_CRSEL
#define VIRT_EXSEL VK_EXSEL
#define VIRT_EREOF VK_EREOF
#define VIRT_PLAY VK_PLAY
#define VIRT_ZOOM VK_ZOOM
#define VIRT_NONAME VK_NONAME
#define VIRT_PA1 VK_PA1
#define VIRT_OEM_CLEAR VK_OEM_CLEAR

#endif // NOVIRTUALKEYCODES

typedef enum BUFFER_TYPE
{
    BUF_MAIN,
    BUF_ALTERNATE,
} BUFFER_TYPE;

typedef struct Handle
{
    HANDLE handle;
} Handle;

typedef struct CLIState
{
    pthread_mutex_t mutex;

    File *entries;
    int entry_size;
    int entry_offset;

    int hovered_idx;
    int playing_idx;
    int selected_idx;

    int64_t media_duration;
    int64_t media_timestamp;
    float media_volume;

    bool force_redraw;

    Handle out;
    int width;
    int height;
    int cursor_x;
    int cursor_y;
} CLIState;

typedef struct Events
{
    Event *event;
    int event_size;
} Events;

#elif defined(AP_MACOS)
#elif defined(AP_LINUX)
#endif // AP_WINDOWS

CLIState *cli_state_init();
void cli_state_free(CLIState **cst);

void cli_buffer_switch(BUFFER_TYPE type);
Events cli_read_in();
void cli_draw(CLIState *cst);
void cli_draw_overlay(CLIState *cst);
Handle cli_get_handle(BUFFER_TYPE type);
void cli_get_console_size(CLIState *cst);
void cli_get_cursor_pos(CLIState *cst);

#endif // _LIBCLI_H