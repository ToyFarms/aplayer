#include "clock.h"
#include "logger.h"
#include "ncurses.h"
#include "term.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void term_prepare()
{
    initscr();
    noecho();
    nodelay(stdscr, true);
    raw();
    nonl();
    cbreak();
    curs_set(false);
    keypad(stdscr, true);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    term_write(TESC TMOUSEENABLE, -1);
    term_write("\x1b[?7l", -1);
}

static void term_reset()
{
    term_write("\x1b[?7h", -1);
    term_write(TESC TMOUSEDISABLE, -1);
    echo();
    noraw();
    nocbreak();
    curs_set(true);
    delwin(stdscr);
    endwin();
}

handle_t term_handle(enum handle_type type)
{
    switch (type)
    {
    case HANDLE_STDOUT:
        return (handle_t){.fd = STDOUT_FILENO};
        break;
    case HANDLE_STDIN:
        return (handle_t){.fd = STDIN_FILENO};
        break;
    case HANDLE_STDERR:
        return (handle_t){.fd = STDERR_FILENO};
        break;
    default:
        log_error("Unknown handle type: %d\n", type);
        return (handle_t){0};
    }
}

void term_write(char *str, int size)
{
    write(STDOUT_FILENO, str, size >= 0 ? size : strlen(str));
}

void term_altbuf()
{
    term_prepare();
}

void term_mainbuf()
{
    term_reset();
}

vec2 term_size()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    return VEC(w.ws_col, w.ws_row);
}

void term_get_events(queue_t *out)
{
    // BUG: on xterm, or atleast any terminal on linux, mouse button is highly
    // likely to get stuck when moving, and clicking at the same time, because
    // apparently xterm can't do it, nothing i can do about it (ask x
    // server/wayland for mouse state?)
    static int prev_mouse_pos[2] = {0, 0};
    static bool button_state[TERM_MAX_MOUSEKEY] = {0};
    static uint64_t button_last_pressed[TERM_MAX_MOUSEKEY] = {0};
    static int prev_size[2] = {0, 0};

    int c;
    while ((c = getch()) != ERR)
    {
        term_event ev = {0};

        if (c > 0)
        {
            switch (c)
            {
            case KEY_MOUSE:
                ev.type = TERM_EVENT_MOUSE;

                MEVENT m = {0};
                getmouse(&m);
                ev.mouse.x = m.x;
                ev.mouse.y = m.y;

                const uint64_t current_time = gclock_now_ns() / 1000000;

#define CHECK_STATE(m, s) ((m.bstate & s) != 0)

#define CHECK_BUTTON(n)                                                        \
    if (CHECK_STATE(m, BUTTON##n##_PRESSED))                                   \
    {                                                                          \
        button_state[n - 1] = true;                                            \
        if (current_time - button_last_pressed[n - 1] < 200)                   \
            ev.mouse.double_clicked = true;                                    \
        button_last_pressed[n - 1] = current_time;                             \
    }                                                                          \
    else if (CHECK_STATE(m, BUTTON##n##_RELEASED))                             \
        button_state[n - 1] = false;                                           \
    ev.mouse.state[n - 1] = button_state[n - 1]

                CHECK_BUTTON(1);
                CHECK_BUTTON(2);
                CHECK_BUTTON(3);

                if (CHECK_STATE(m, BUTTON4_PRESSED))
                    ev.mouse.state[3] = true;
                if (CHECK_STATE(m, BUTTON5_PRESSED))
                    ev.mouse.state[3] = true;

                if (CHECK_STATE(m, BUTTON_SHIFT))
                    ev.mouse.mod |= TERM_KMOD_SHIFT;
                if (CHECK_STATE(m, BUTTON_CTRL))
                    ev.mouse.mod |= TERM_KMOD_CTRL;
                if (CHECK_STATE(m, BUTTON_ALT))
                    ev.mouse.mod |= TERM_KMOD_ALT;

                if (prev_mouse_pos[0] != m.x || prev_mouse_pos[1] != m.y)
                    ev.mouse.moved = true;

                prev_mouse_pos[0] = m.x;
                prev_mouse_pos[1] = m.y;
                break;
            default:
                ev.type = TERM_EVENT_KEY;

#define CASE_KEY(k)                                                            \
    case KEY_##k:                                                              \
        ev.key.virtual = TERM_KEY_##k;                                         \
        break
#define CASE_KEYX(k, x)                                                        \
    case KEY_##k:                                                              \
        ev.key.virtual = TERM_KEY_##x;                                         \
        break

#define CASE_KEY_SHIFT(k)                                                      \
    case KEY_##k:                                                              \
        ev.key.virtual = TERM_KEY_##k;                                         \
        break;                                                                 \
    case KEY_S##k:                                                             \
        ev.key.virtual = TERM_KEY_##k;                                         \
        ev.key.mod |= TERM_KMOD_SHIFT;                                         \
        break
#define CASE_KEYX_SHIFT(k, x)                                                  \
    case KEY_##k:                                                              \
        ev.key.virtual = TERM_KEY_##x;                                         \
        break;                                                                 \
    case KEY_S##k:                                                             \
        ev.key.virtual = TERM_KEY_##x;                                         \
        ev.key.mod |= TERM_KMOD_SHIFT;                                         \
        break

                switch (c)
                {
                case KEY_ENTER:
                case '\r':
                    ev.key.virtual = TERM_KEY_ENTER;
                    ev.key.ascii = '\n';
                    break;
                case '\n': // ctrl+j
                    ev.key.ascii = 'j';
                    ev.key.mod |= TERM_KMOD_CTRL;
                    break;
                case '\t':
                    ev.key.virtual = TERM_KEY_TAB;
                    ev.key.ascii = '\t';
                    break;
                case TERM_KEY_ESC:
                    ev.key.virtual = TERM_KEY_ESC;
                    ev.key.ascii = '';
                    break;
                    CASE_KEY(F(1));
                    CASE_KEY(F(2));
                    CASE_KEY(F(3));
                    CASE_KEY(F(4));
                    CASE_KEY(F(5));
                    CASE_KEY(F(6));
                    CASE_KEY(F(7));
                    CASE_KEY(F(8));
                    CASE_KEY(F(9));
                    CASE_KEY(F(10));
                    CASE_KEY(F(11));
                    CASE_KEY(F(12));
                    CASE_KEY(UP);
                    CASE_KEY(DOWN);
                    CASE_KEY_SHIFT(LEFT);
                    CASE_KEY_SHIFT(RIGHT);
                    CASE_KEY(BACKSPACE);
                    CASE_KEYX(A1, PAD_7);
                    CASE_KEYX(A3, PAD_9);
                    CASE_KEYX(B2, PAD_5);
                    CASE_KEYX(C1, PAD_1);
                    CASE_KEYX(C3, PAD_3);
                    CASE_KEY_SHIFT(HOME);
                    CASE_KEY_SHIFT(END);
                    CASE_KEYX_SHIFT(IC, INS);
                    CASE_KEYX_SHIFT(DC, DEL);
                    CASE_KEYX(PPAGE, PAGEUP);
                    CASE_KEYX(NPAGE, PAGEDOWN);
                default:
                    goto continue_check;
                }
                break;

            continue_check:
                if (c >= 1 && c <= 26)
                {
                    ev.key.ascii = c + 'a' - 1;
                    ev.key.mod |= TERM_KMOD_CTRL;
                }
                else if (c >= 'A' && c <= 'Z')
                {
                    ev.key.ascii = c;
                    ev.key.mod |= TERM_KMOD_SHIFT;
                }
                else
                {
                    ev.key.ascii = c;
                }
            }
        }

        if (ev.type != TERM_EVENT_UNKNOWN)
            queue_push_copy(out, &ev, sizeof(ev));
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
    int ch;
    while ((ch = getch()) != ERR)
        str_catch(out, ch);
}

static void get_cell_size(str_t *resp, int *width, int *height)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    if (w.ws_xpixel > 0 && w.ws_ypixel > 0)
    {
        *width = w.ws_xpixel / w.ws_col;
        *height = w.ws_ypixel / w.ws_row;
    }
    else
    {
        term_write("\x1b[16t", -1);
        refresh();

        read_response(resp);

        bool failed = false;
        failed |= resp->len >= 2 && resp->buf[0] != '\x1b';
        failed |= resp->len >= 2 && resp->buf[1] != '[';
        failed |= resp->len >= 1 && resp->buf[resp->len - 1] != 't';

        if (failed)
        {
            log_error("Unexpected response from terminal, defaulting size to "
                      "10,20\n");
            *width = 10;
            *height = 20;
        }
        else
        {
            int i = 0;
            STR_SPLIT(*resp, token, ";")
            {
                if (i == 0)
                    continue;
                else if (i == 1)
                    *height = str_parse(token, 10);
                else if (i == 2)
                {
                    token.len -= 1; // for the last t
                    *width = str_parse(token, 10);
                }

                if (errno != 0)
                {
                    log_error("Failed to parse number: %s\n", strerror(errno));
                    failed = true;
                    break;
                }

                i++;
            }

            if (failed)
            {
                *width = 10;
                *height = 20;
            }
        }
    }
}

static bool supports_sixel(str_t *resp)
{
    term_write("\x1b[c", -1);
    refresh();

    read_response(resp);
    if (resp->len <= 3)
        return false;

    if (resp->buf[0] != '\x1b' || resp->buf[1] != '[' ||
        resp->buf[resp->len - 1] != 'c')
        return false;

    // skip \x1b[? and c
    strview_t v = (strview_t){.buf = resp->buf + 2, .len = resp->len - 1};
    STR_SPLIT(strv(v), token, ";")
    {
        if (token.len == 1 && token.buf[0] == 4)
            return true;
    }

    return false;
}

static bool supports_tgp(str_t *resp, bool is_tmux)
{
    if (is_tmux)
        term_write("\x1bPtmux;\x1b"
                   "\x1b_Gi=1;\x1b\\\x1b[c"
                   "\\\x1b\\\x1b[c",
                   -1);
    else
        term_write("\x1b_Gi=1;\x1b\\\x1b[c", -1);
    refresh();

    read_response(resp);
    return strstr(resp->buf, "\x1b_G") != NULL;
}

term_capability term_query_capability()
{
    term_capability cap = {0};

    initscr();
    cbreak();
    noecho();
    timeout(100);

    cap.is_tmux = getenv("TMUX") != NULL;

    if (!has_colors())
        cap.color = TERM_COLOR_MONO;
    else
    {
        start_color();
        if (COLORS >= 256 && getenv("COLORTERM"))
            cap.color = TERM_COLOR_24BIT;
        else if (COLORS >= 256)
            cap.color = TERM_COLOR_256;
        else
            cap.color = TERM_COLOR_MONO;
    }

    str_t resp = str_create();

    cap.supports_tgp = supports_tgp(&resp, cap.is_tmux);
    resp.len = 0;

    get_cell_size(&resp, &cap.cell_width, &cap.cell_height);
    resp.len = 0;

    cap.supports_sixel = supports_sixel(&resp);
    resp.len = 0;

    // endwin();
    str_free(&resp);

    return cap;
}
