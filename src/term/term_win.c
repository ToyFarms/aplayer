#include "clock.h"
#include "logger.h"
#include "term.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE g_stdout = INVALID_HANDLE_VALUE;
static HANDLE g_stdin = INVALID_HANDLE_VALUE;
static DWORD g_orig_out_mode = 0;
static DWORD g_orig_in_mode = 0;
static bool g_have_orig_modes = false;

static void win_handles_init(void)
{
    if (g_stdout == INVALID_HANDLE_VALUE)
        g_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_stdin == INVALID_HANDLE_VALUE)
        g_stdin = GetStdHandle(STD_INPUT_HANDLE);
}

handle_t term_handle(enum handle_type type)
{
    win_handles_init();

    switch (type)
    {
    case HANDLE_STDOUT:
        return (handle_t){.fd = _fileno(stdout)};
    case HANDLE_STDIN:
        return (handle_t){.fd = _fileno(stdin)};
    case HANDLE_STDERR:
        return (handle_t){.fd = _fileno(stderr)};
    default:
        log_error("Unknown handle type: %d\n", type);
        return (handle_t){0};
    }
}

void term_write(char *str, int size)
{
    win_handles_init();
    DWORD len = (DWORD)(size >= 0 ? size : strlen(str));

    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
    WCHAR *wbuf = malloc(wlen * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, str, (int)len, wbuf, wlen);

    DWORD written = 0;
    WriteConsoleW(g_stdout, wbuf, wlen, &written, NULL);
    free(wbuf);
}

static void term_prepare(void)
{
    win_handles_init();

    if (!g_have_orig_modes)
    {
        GetConsoleMode(g_stdout, &g_orig_out_mode);
        GetConsoleMode(g_stdin, &g_orig_in_mode);
        g_have_orig_modes = true;
    }

    DWORD out_mode = g_orig_out_mode;
    out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    out_mode |= DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(g_stdout, out_mode);

    DWORD in_mode = 0;
    in_mode |= ENABLE_WINDOW_INPUT;
    in_mode |= ENABLE_MOUSE_INPUT;
    in_mode |= ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(g_stdin, in_mode);

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    term_write(TESC TALTBUF, -1);
    term_write(TESC TCURSORHIDE, -1);
    term_write("\x1b[?7l", -1);
}

static void term_reset(void)
{
    term_write("\x1b[?7h", -1);
    term_write(TESC TCURSORSHOW, -1);
    term_write(TESC TMAINBUF, -1);

    if (g_have_orig_modes)
    {
        SetConsoleMode(g_stdout, g_orig_out_mode);
        SetConsoleMode(g_stdin, g_orig_in_mode);
    }
}

void term_altbuf(void)
{
    term_prepare();
}

void term_mainbuf(void)
{
    term_reset();
}

void term_exit(void)
{
    term_reset();
}

vec2 term_size(void)
{
    win_handles_init();

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(g_stdout, &info))
        return VEC(0, 0);

    int width = info.srWindow.Right - info.srWindow.Left + 1;
    int height = info.srWindow.Bottom - info.srWindow.Top + 1;

    return VEC(width, height);
}

vec2 term_size_update(term_state *term)
{
    win_handles_init();

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(g_stdout, &info))
        return VEC(term->width, term->height);

    int width = info.srWindow.Right - info.srWindow.Left + 1;
    int height = info.srWindow.Bottom - info.srWindow.Top + 1;

    term->width = width;
    term->height = height;

    return VEC(width, height);
}

static uint32_t vk_to_term_key(WORD vk)
{
    switch (vk)
    {
    case VK_RETURN:
        return TERM_KEY_ENTER;
    case VK_TAB:
        return TERM_KEY_TAB;
    case VK_ESCAPE:
        return TERM_KEY_ESC;
    case VK_F1:
        return TERM_KEY_F1;
    case VK_F2:
        return TERM_KEY_F2;
    case VK_F3:
        return TERM_KEY_F3;
    case VK_F4:
        return TERM_KEY_F4;
    case VK_F5:
        return TERM_KEY_F5;
    case VK_F6:
        return TERM_KEY_F6;
    case VK_F7:
        return TERM_KEY_F7;
    case VK_F8:
        return TERM_KEY_F8;
    case VK_F9:
        return TERM_KEY_F9;
    case VK_F10:
        return TERM_KEY_F10;
    case VK_F11:
        return TERM_KEY_F11;
    case VK_F12:
        return TERM_KEY_F12;
    case VK_UP:
        return TERM_KEY_UP;
    case VK_DOWN:
        return TERM_KEY_DOWN;
    case VK_LEFT:
        return TERM_KEY_LEFT;
    case VK_RIGHT:
        return TERM_KEY_RIGHT;
    case VK_BACK:
        return TERM_KEY_BACKSPACE;
    case VK_HOME:
        return TERM_KEY_HOME;
    case VK_END:
        return TERM_KEY_END;
    case VK_INSERT:
        return TERM_KEY_INS;
    case VK_DELETE:
        return TERM_KEY_DEL;
    case VK_PRIOR:
        return TERM_KEY_PAGEUP;
    case VK_NEXT:
        return TERM_KEY_PAGEDOWN;
    case VK_NUMPAD7:
        return TERM_KEY_PAD_7;
    case VK_NUMPAD9:
        return TERM_KEY_PAD_9;
    case VK_NUMPAD5:
        return TERM_KEY_PAD_5;
    case VK_NUMPAD1:
        return TERM_KEY_PAD_1;
    case VK_NUMPAD3:
        return TERM_KEY_PAD_3;
    default:
        return 0;
    }
}

static void handle_key_event(KEY_EVENT_RECORD *k, queue_t *out)
{
    term_event ev = {0};
    ev.type = TERM_EVENT_KEY;
    ev.key.keydown = k->bKeyDown;

    DWORD cks = k->dwControlKeyState;
    if (cks & SHIFT_PRESSED)
        ev.key.mod |= TERM_KMOD_SHIFT;
    if (cks & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        ev.key.mod |= TERM_KMOD_CTRL;
    if (cks & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
        ev.key.mod |= TERM_KMOD_ALT;

    uint32_t mapped = vk_to_term_key(k->wVirtualKeyCode);
    WCHAR ch = k->uChar.UnicodeChar;

    if (!k->bKeyDown)
        return;

    if (mapped != 0)
    {
        ev.key.virtual = mapped;
        if (mapped == TERM_KEY_ENTER)
            ev.key.ascii = '\n';
        else if (mapped == TERM_KEY_TAB)
            ev.key.ascii = '\t';
        else if (mapped == TERM_KEY_ESC)
            ev.key.ascii = '\x1b';
    }
    else if (ch != 0)
    {
        if (ch < 128)
        {
            ev.key.ascii = (char)ch;

            if (ch >= 1 && ch <= 26)
            {
                ev.key.ascii = (char)(ch + 'a' - 1);
                ev.key.mod |= TERM_KMOD_CTRL;
            }
        }
        else
            return;
    }
    else
    {
        return;
    }

    queue_push_copy(out, &ev, sizeof(ev));
}

static void handle_mouse_event(MOUSE_EVENT_RECORD *m, queue_t *out)
{
    static int prev_x = -1, prev_y = -1;
    static bool button_state[TERM_MAX_MOUSEKEY] = {0};
    static uint64_t button_last_pressed[TERM_MAX_MOUSEKEY] = {0};

    term_event ev = {0};
    ev.type = TERM_EVENT_MOUSE;
    ev.mouse.x = m->dwMousePosition.X;
    ev.mouse.y = m->dwMousePosition.Y;

    if (m->dwControlKeyState & SHIFT_PRESSED)
        ev.mouse.mod |= TERM_KMOD_SHIFT;
    if (m->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        ev.mouse.mod |= TERM_KMOD_CTRL;
    if (m->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
        ev.mouse.mod |= TERM_KMOD_ALT;

    if (m->dwEventFlags & MOUSE_WHEELED)
    {
        ev.mouse.scrolled = true;
        ev.mouse.scroll_delta = (HIWORD(m->dwButtonState) & 0x8000) ? -1 : 1;
    }

    const uint64_t now = gclock_now_ns() / 1000000;

    static const DWORD buttons[3] = {
        FROM_LEFT_1ST_BUTTON_PRESSED,
        RIGHTMOST_BUTTON_PRESSED,
        FROM_LEFT_2ND_BUTTON_PRESSED,
    };

    for (int i = 0; i < 3; i++)
    {
        bool pressed = (m->dwButtonState & buttons[i]) != 0;

        if (pressed && !button_state[i])
        {
            if (now - button_last_pressed[i] < 200)
                ev.mouse.double_clicked = true;
            button_last_pressed[i] = now;
        }

        button_state[i] = pressed;
        ev.mouse.state[i] = pressed;
    }

    if (prev_x != ev.mouse.x || prev_y != ev.mouse.y)
        ev.mouse.moved = true;

    prev_x = ev.mouse.x;
    prev_y = ev.mouse.y;

    queue_push_copy(out, &ev, sizeof(ev));
}

void term_get_events(queue_t *out)
{
    win_handles_init();

    static int prev_size[2] = {0, 0};

    DWORD pending = 0;
    while (GetNumberOfConsoleInputEvents(g_stdin, &pending) && pending > 0)
    {
        INPUT_RECORD rec;
        DWORD read = 0;

        if (!ReadConsoleInputW(g_stdin, &rec, 1, &read) || read == 0)
            break;

        switch (rec.EventType)
        {
        case KEY_EVENT:
            handle_key_event(&rec.Event.KeyEvent, out);
            break;
        case MOUSE_EVENT:
            handle_mouse_event(&rec.Event.MouseEvent, out);
            break;
        case WINDOW_BUFFER_SIZE_EVENT: {
            term_event ev = {0};
            ev.type = TERM_EVENT_RESIZE;
            ev.resize.width = rec.Event.WindowBufferSizeEvent.dwSize.X;
            ev.resize.height = rec.Event.WindowBufferSizeEvent.dwSize.Y;
            queue_push_copy(out, &ev, sizeof(ev));

            prev_size[0] = ev.resize.width;
            prev_size[1] = ev.resize.height;
            break;
        }
        default:
            break;
        }
    }

    vec2 size = term_size();
    if (prev_size[0] != size.x || prev_size[1] != size.y)
    {
        term_event ev = {0};
        ev.type = TERM_EVENT_RESIZE;
        ev.resize.width = size.x;
        ev.resize.height = size.y;
        queue_push_copy(out, &ev, sizeof(ev));
    }
    prev_size[0] = size.x;
    prev_size[1] = size.y;
}

static void read_response(str_t *out)
{
    for (int tries = 0; tries < 50; tries++)
    {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(g_stdin, &pending) || pending == 0)
        {
            Sleep(2);
            continue;
        }

        INPUT_RECORD rec;
        DWORD read = 0;
        if (!ReadConsoleInputW(g_stdin, &rec, 1, &read) || read == 0)
            break;

        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
        {
            WCHAR ch = rec.Event.KeyEvent.uChar.UnicodeChar;
            if (ch != 0 && ch < 128)
                str_catch(out, (char)ch);
        }
    }
}

static bool contains_bytes(const str_t *s, const char *needle, size_t nlen)
{
    if (!s || !needle || nlen == 0 || s->len < nlen)
        return false;

    for (size_t i = 0; i + nlen <= s->len; ++i)
    {
        if (memcmp(s->buf + i, needle, nlen) == 0)
            return true;
    }

    return false;
}

static void get_cell_size(str_t *resp, int *width, int *height)
{
    term_write("\x1b[16t", -1);
    read_response(resp);

    if (resp->len < 4 || resp->buf[0] != '\x1b' || resp->buf[1] != '[' ||
        resp->buf[resp->len - 1] != 't')
    {
        log_error(
            "Unexpected response from terminal, defaulting size to 10x20\n");
        *width = 10;
        *height = 20;
        return;
    }

    int i = 0;
    bool failed = false;

    STR_SPLIT(*resp, token, ";")
    {
        if (i == 0)
        {
            i++;
            continue;
        }
        else if (i == 1)
        {
            errno = 0;
            *height = str_parse(token, 10);
            if (errno != 0)
            {
                failed = true;
                break;
            }
        }
        else if (i == 2)
        {
            token.len -= 1;
            errno = 0;
            *width = str_parse(token, 10);
            if (errno != 0)
            {
                failed = true;
                break;
            }
        }

        i++;
    }

    if (failed)
    {
        *width = 10;
        *height = 20;
    }
}

static bool supports_sixel(str_t *resp)
{
    term_write("\x1b[c", -1);
    read_response(resp);

    if (resp->len < 3)
        return false;

    if (resp->buf[0] != '\x1b' || resp->buf[1] != '[' ||
        resp->buf[resp->len - 1] != 'c')
    {
        return false;
    }

    strview_t v = (strview_t){.buf = resp->buf + 2, .len = resp->len - 3};

    STR_SPLIT(strv(v), token, ";")
    {
        if (token.len > 0 && token.buf[0] == '4')
            return true;
    }

    return false;
}

static bool supports_tgp(str_t *resp)
{
    term_write("\x1b_Gi=1;\x1b\\\x1b[c", -1);
    read_response(resp);

    return contains_bytes(resp, "\x1b_G", 3);
}

term_capability term_query_capability(void)
{
    term_capability cap = {0};
    str_t resp = str_create();

    win_handles_init();
    DWORD saved_in_mode;
    GetConsoleMode(g_stdin, &saved_in_mode);
    SetConsoleMode(g_stdin, 0);

    DWORD saved_out_mode;
    GetConsoleMode(g_stdout, &saved_out_mode);
    SetConsoleMode(g_stdout,
                   saved_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    cap.is_tmux = false;

    cap.supports_tgp = supports_tgp(&resp);
    resp.len = 0;

    get_cell_size(&resp, &cap.cell_width, &cap.cell_height);
    resp.len = 0;

    cap.supports_sixel = supports_sixel(&resp);
    resp.len = 0;

    str_free(&resp);
    SetConsoleMode(g_stdin, saved_in_mode);

    return cap;
}
