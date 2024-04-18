#include "ap_terminal.h"

typedef CONSOLE_SCREEN_BUFFER_INFO CSBI;

static CSBI os_term_get_csbi(APHandle h);

APHandle ap_term_get_std_handle(HandleType type)
{
    switch (type)
    {
    case HANDLE_STDOUT:
        return (APHandle){GetStdHandle(STD_OUTPUT_HANDLE)};
        break;
    case HANDLE_STDIN:
        return (APHandle){GetStdHandle(STD_INPUT_HANDLE)};
        break;
    case HANDLE_STDERR:
        return (APHandle){GetStdHandle(STD_ERROR_HANDLE)};
        break;
    default:
        return (APHandle){0};
        break;
    }
}

void ap_term_switch_alt_buf(APHandle h)
{
    DWORD out_mode;
    GetConsoleMode(h.handle, &out_mode);
    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    out_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;

    SetConsoleMode(h.handle, out_mode);

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);

    DWORD in_mode;
    GetConsoleMode(in, &in_mode);
    in_mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    in_mode &= ~ENABLE_QUICK_EDIT_MODE;

    SetConsoleMode(in, in_mode);

    ap_term_show_cursor(h, false);
    printf(ESC "[?1049h");
}

static CSBI os_term_get_csbi(APHandle h)
{
    CSBI csbi;
    GetConsoleScreenBufferInfo(h.handle, &csbi);
    return csbi;
}

Vec2 ap_term_get_size(APHandle h)
{
    CSBI csbi = os_term_get_csbi(h);
    return VEC(csbi.dwSize.X, csbi.dwSize.Y);
}

Vec2 ap_term_get_cursor(APHandle h)
{
    CSBI csbi = os_term_get_csbi(h);
    return VEC(csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y);
}

void ap_term_write(APHandle h, const char *str, size_t str_len)
{
    WriteConsole(h.handle, str, str_len, NULL, NULL);
}

void ap_term_writew(APHandle h, const wchar_t *wstr, size_t wstr_len)
{
    WriteConsoleW(h.handle, wstr, wstr_len, NULL, NULL);
}

#define EVENT_RAW_SIZE 128

void ap_term_get_events(APHandle h_in, APTermEvent *events_out, int len, int *out_len)
{
    static INPUT_RECORD event_raw[EVENT_RAW_SIZE];
    ULONG event_read;
    ReadConsoleInputW(h_in.handle, event_raw, EVENT_RAW_SIZE, &event_read);

    for (int i = 0; i < MATH_MIN(event_read, len); i++)
    {
        INPUT_RECORD e = event_raw[i];
        KEY_EVENT_RECORD k = e.Event.KeyEvent;
        MOUSE_EVENT_RECORD m = e.Event.MouseEvent;
        WINDOW_BUFFER_SIZE_RECORD b = e.Event.WindowBufferSizeEvent;

        APTermEventType type = {0};
        APTermKeyEvent key_event = {0};
        APTermMouseEvent mouse_event = {0};
        APTermBufEvent buf_event = {0};

        switch (e.EventType)
        {
        case KEY_EVENT:
            type = AP_TERM_KEY_EVENT;

            key_event.key_down = k.bKeyDown;
            key_event.ascii_key = k.uChar.AsciiChar;
            key_event.unicode_key = k.uChar.UnicodeChar;
            key_event.vk_key = k.wVirtualKeyCode;

            DWORD ks = k.dwControlKeyState;

            int control_key_pressed =
                (ks & LEFT_CTRL_PRESSED) || (ks & RIGHT_CTRL_PRESSED);
            int alt_key_pressed =
                (ks & LEFT_ALT_PRESSED) || (ks & RIGHT_ALT_PRESSED);

            key_event.modifier_key = ((ks & SHIFT_PRESSED) << 0) |
                                     (control_key_pressed << 1) |
                                     (alt_key_pressed << 2);

            break;
        case MOUSE_EVENT:
            type = AP_TERM_MOUSE_EVENT;

            mouse_event.x = m.dwMousePosition.X;
            mouse_event.y = m.dwMousePosition.Y;
            DWORD ms = m.dwButtonState;
            mouse_event.state = ((ms & FROM_LEFT_1ST_BUTTON_PRESSED) << 0) |
                                ((ms & FROM_LEFT_2ND_BUTTON_PRESSED) << 1) |
                                ((ms & FROM_LEFT_3RD_BUTTON_PRESSED) << 2);
            mouse_event.double_clicked = m.dwEventFlags == DOUBLE_CLICK;
            mouse_event.moved = m.dwEventFlags & MOUSE_MOVED;
            mouse_event.scrolled = m.dwEventFlags & MOUSE_WHEELED;
            mouse_event.scroll_delta = GET_WHEEL_DELTA_WPARAM(m.dwButtonState);

            break;
        case WINDOW_BUFFER_SIZE_EVENT:
            type = AP_TERM_BUF_EVENT;
            buf_event.to_width = b.dwSize.X;
            buf_event.to_height = b.dwSize.Y;

            break;
        default:
            type = AP_TERM_UNKNOWN_EVENT;
            break;
        }

        events_out[i] = (APTermEvent){
            .type = type,
            .key_event = key_event,
            .mouse_event = mouse_event,
            .buf_event = buf_event,
        };
    }

    if (out_len)
        *out_len = event_read;
}
