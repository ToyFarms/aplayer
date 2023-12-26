#include "libcli.h"

void cli_get_console_size(CLIState *cst)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(cst->out.handle, &csbi);

    cst->width = csbi.dwSize.X;
    cst->height = csbi.dwSize.Y;
}

void cli_get_cursor_pos(CLIState *cst)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(cst->out.handle, &csbi);

    cst->cursor_x = csbi.dwCursorPosition.X;
    cst->cursor_y = csbi.dwCursorPosition.Y;
}

static HANDLE out;
static HANDLE in;
static DWORD in_mode;

void cli_buffer_switch(BUFFER_TYPE type)
{
    out = GetStdHandle(STD_OUTPUT_HANDLE);

    switch (type)
    {
    case BUF_MAIN:
        if (in && in_mode)
            SetConsoleMode(in, in_mode);

        printf("\x1b[?1049l");
        printf("\x1b[?25h");
        break;
    case BUF_ALTERNATE:
        DWORD out_mode;
        GetConsoleMode(out, &out_mode);
        out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        out_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
        SetConsoleMode(out, out_mode);

        in = GetStdHandle(STD_INPUT_HANDLE);

        GetConsoleMode(in, &in_mode);
        in_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
        in_mode &= ~ENABLE_QUICK_EDIT_MODE;
        SetConsoleMode(in, in_mode);

        printf("\x1b[?1049h");
        printf("\x1b[?25l");
        break;
    }
}

static INPUT_RECORD in_rec[128];
static Event in_event[128];

Events cli_read_in()
{
    if (!in)
    {
        av_log(NULL, AV_LOG_FATAL, "Input is uninitialized.\n");
        return (Events){.event = NULL, .event_size = 0};
    }

    ULONG read;
    BOOL exit;
    ReadConsoleInput(in, in_rec, sizeof(in_rec) / sizeof(INPUT_RECORD), &read);

    for (int i = 0; i < read; i++)
    {
        INPUT_RECORD e = in_rec[i];
        KEY_EVENT_RECORD k = e.Event.KeyEvent;
        MOUSE_EVENT_RECORD m = e.Event.MouseEvent;
        WINDOW_BUFFER_SIZE_RECORD b = e.Event.WindowBufferSizeEvent;

        EventType type;
        KeyEvent key_event;
        MouseEvent mouse_event;
        BufferChangedEvent buf_event;

        switch (e.EventType)
        {
        case KEY_EVENT:
            type = KEY_EVENT_TYPE;

            key_event.key_down = k.bKeyDown;
            key_event.acsii_key = k.uChar.AsciiChar;
            key_event.unicode_key = k.uChar.UnicodeChar;
            key_event.vk_key = k.wVirtualKeyCode;

            DWORD ks = k.dwControlKeyState;

            int control_key_pressed = ks & LEFT_CTRL_PRESSED || ks & RIGHT_CTRL_PRESSED;
            int alt_key_pressed = ks & LEFT_ALT_PRESSED || ks & RIGHT_ALT_PRESSED;

            key_event.modifier_key = ((ks & SHIFT_PRESSED) << 0) | (control_key_pressed << 1) | (alt_key_pressed << 2);

            break;
        case MOUSE_EVENT:
            type = MOUSE_EVENT_TYPE;

            mouse_event.x = m.dwMousePosition.X;
            mouse_event.y = m.dwMousePosition.Y;
            DWORD ms = m.dwButtonState;
            mouse_event.state = ((ms & FROM_LEFT_1ST_BUTTON_PRESSED) << 0) | ((ms & FROM_LEFT_2ND_BUTTON_PRESSED) << 1) | ((ms & FROM_LEFT_3RD_BUTTON_PRESSED) << 2);
            mouse_event.double_clicked = m.dwEventFlags == DOUBLE_CLICK;
            mouse_event.moved = m.dwEventFlags & MOUSE_MOVED;
            mouse_event.scrolled = m.dwEventFlags & MOUSE_WHEELED;
            mouse_event.scroll_delta = GET_WHEEL_DELTA_WPARAM(m.dwButtonState);

            break;
        case WINDOW_BUFFER_SIZE_EVENT:
            type = BUFFER_CHANGED_EVENT_TYPE;
            buf_event.width = b.dwSize.X;
            buf_event.height = b.dwSize.Y;

            break;
        default:
            type = UNKNOWN_EVENT_TYPE;
            break;
        }

        if (type == UNKNOWN_EVENT_TYPE)
            continue;

        in_event[i] = (Event){
            .type = type,
            .key_event = key_event,
            .mouse_event = mouse_event,
            .buf_event = buf_event,
        };
    }

    return (Events){.event = in_event, .event_size = read};
}

Handle cli_get_handle()
{
    return (Handle){.handle = GetStdHandle(STD_OUTPUT_HANDLE)};
}