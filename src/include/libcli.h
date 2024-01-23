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
#include "libtime.h"
#include "libplaylist.h"

#define MOUSE_LEFT_CLICKED (1 << 0)
#define MOUSE_MIDDLE_CLICKED (1 << 1)
#define MOUSE_RIGHT_CLICKED (1 << 2)

#define SHIFT_KEY_PRESSED (1 << 0)
#define CTRL_KEY_PRESSED (1 << 1)
#define ALT_KEY_PRESSED (1 << 2)

typedef struct KeyEvent
{
    bool key_down;
    char ascii_key;
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

typedef enum BufferType
{
    BUF_MAIN,
    BUF_ALTERNATE,
} BufferType;

typedef struct UnicodeSymbol
{
    char *volume_mute;
    char *volume_off;
    char *volume_low;
    char *volume_medium;
    char *volume_high;

    char *media_play;
    char *media_pause;
    char *media_next_track;
    char *media_prev_track;
} UnicodeSymbol;

typedef struct Events
{
    Event *event;
    int event_size;
} Events;

typedef enum SortMethod
{
    SORT_CTIME,
    SORT_ALPHABETICALLY,
} SortMethod;

typedef enum SortFlag
{
    SORT_FLAG_ASC,
    SORT_FLAG_DESC,
} SortFlag;

#ifdef AP_WINDOWS

#include <windows.h>

typedef struct Handle
{
    HANDLE handle;
} Handle;

#elif defined(AP_MACOS)
#elif defined(AP_LINUX)
#endif // AP_WINDOWS

typedef struct CLIState
{
    pthread_mutex_t mutex;

    int entry_offset;

    int hovered_idx;
    int selected_idx;

    Playlist *pl;

    bool force_redraw;

    bool is_in_input_mode;
    char *input_buffer;
    int input_buffer_capacity;
    int input_buffer_size;

    Handle out;
    int width;
    int height;
    int cursor_x;
    int cursor_y;

    int mouse_x;
    int mouse_y;

    UnicodeSymbol icon;
    UnicodeSymbol icon_nerdfont;
    UnicodeSymbol *current_icon;

    bool prev_button_hovered;
    bool playback_button_hovered;
    bool next_button_hovered;
} CLIState;

int cli_init(Playlist *pl);
void cli_free();
void cli_event_loop();
void cli_playlist_next();
void cli_playlist_prev();
void cli_playlist_play(int index);
void cli_sort_entry(SortMethod sort, SortFlag flag);
void cli_shuffle_entry();

// Implement for each OS

void cli_buffer_switch(BufferType type);
void cli_get_console_size(CLIState *cst);
void cli_get_cursor_pos(CLIState *cst);
Events cli_read_in();
Handle cli_get_handle();
void cli_write(Handle out, const char *str, int str_len);
void cli_writew(Handle out, const wchar_t *str, int str_len);

#endif // _LIBCLI_H