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
    // TODO: Config for these modes
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
    ap_term_write(h, ESC TCMD_ALTBUF, -1);
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

void ap_term_write(APHandle h, const char *str, int str_len)
{
    WriteConsole(h.handle, str, str_len > 0 ? str_len : strlen(str), NULL,
                 NULL);
}

void ap_term_writew(APHandle h, const wchar_t *wstr, int wstr_len)
{
    WriteConsoleW(h.handle, wstr, wstr_len > 0 ? wstr_len : wcslen(wstr), NULL,
                  NULL);
}

#define EVENT_RAW_SIZE 128

void ap_term_get_events(APHandle h_in, APEvent *events_out, int len,
                        int *out_len)
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

        switch (e.EventType)
        {
        case KEY_EVENT:
            APEventKey key_ev = {0};

            key_ev.keydown = k.bKeyDown;
            key_ev.ascii = k.uChar.AsciiChar;
            key_ev.unicode = k.uChar.UnicodeChar;
            key_ev.virtual = k.wVirtualKeyCode;

            DWORD ks = k.dwControlKeyState;

            int control_key_pressed =
                (ks & LEFT_CTRL_PRESSED) || (ks & RIGHT_CTRL_PRESSED);
            int alt_key_pressed =
                (ks & LEFT_ALT_PRESSED) || (ks & RIGHT_ALT_PRESSED);

            key_ev.mod = ((ks & SHIFT_PRESSED) << 0) |
                         (control_key_pressed << 1) | (alt_key_pressed << 2);

            events_out[i] = (APEvent){AP_EVENT_KEY, .event.key = key_ev};
            break;
        case MOUSE_EVENT:
            APEventMouse mouse_ev = {0};

            mouse_ev.x = m.dwMousePosition.X;
            mouse_ev.y = m.dwMousePosition.Y;
            DWORD ms = m.dwButtonState;
            mouse_ev.state = ((ms & FROM_LEFT_1ST_BUTTON_PRESSED) << 0) |
                             ((ms & FROM_LEFT_2ND_BUTTON_PRESSED) << 1) |
                             ((ms & FROM_LEFT_3RD_BUTTON_PRESSED) << 2);
            mouse_ev.double_clicked = m.dwEventFlags == DOUBLE_CLICK;
            mouse_ev.moved = m.dwEventFlags & MOUSE_MOVED;
            mouse_ev.scrolled = m.dwEventFlags & MOUSE_WHEELED;
            mouse_ev.scroll_delta = GET_WHEEL_DELTA_WPARAM(m.dwButtonState);

            events_out[i] = (APEvent){AP_EVENT_MOUSE, .event.mouse = mouse_ev};
            break;
        case WINDOW_BUFFER_SIZE_EVENT:
            APEventBuffer buf_ev = {0};
            buf_ev.width = b.dwSize.X;
            buf_ev.height = b.dwSize.Y;

            events_out[i] = (APEvent){AP_EVENT_BUFFER, .event.buf = buf_ev};
            break;
        default:
            events_out[i] = (APEvent){AP_EVENT_UNKNOWN};
            break;
        }

        if (out_len)
            *out_len = event_read;
    }
}
