#include "clock.h"
#include "logger.h"
#include "ncurses.h"
#include "term.h"
#include "utils.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void term_prepare()
{
    term_write(TESC TALTBUF, -1);
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
    endwin();
    term_write(TESC TMAINBUF, -1);
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

vec2 term_size_update(term_state *term)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    term->width = w.ws_col;
    term->height = w.ws_row;

    if (w.ws_xpixel > 0 && w.ws_ypixel > 0)
    {
        term->capability.cell_width = w.ws_xpixel / w.ws_col;
        term->capability.cell_height = w.ws_ypixel / w.ws_row;
    }

    return VEC(w.ws_col, w.ws_row);
}

void term_get_events(queue_t *out)
{
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

static enum term_color_mode detect_color_mode(void)
{
    const char *colorterm = getenv("COLORTERM");
    const char *term = getenv("TERM");

    if (colorterm && (!strcasecmp(colorterm, "truecolor") ||
                      !strcasecmp(colorterm, "24bit")))
    {
        return TERM_COLOR_24BIT;
    }

    if (term && (strstr(term, "direct") || strstr(term, "truecolor")))
    {
        return TERM_COLOR_24BIT;
    }

    if (term && strstr(term, "256color"))
        return TERM_COLOR_256;

    return TERM_COLOR_MONO;
}

static void read_response(str_t *out)
{
    int ch;

    while ((ch = getch()) != ERR)
        str_catch(out, ch);
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
    struct winsize w = {0};

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_xpixel > 0 &&
        w.ws_ypixel > 0 && w.ws_col > 0 && w.ws_row > 0)
    {
        *width = (int)(w.ws_xpixel / w.ws_col);
        *height = (int)(w.ws_ypixel / w.ws_row);
        return;
    }

    flushinp();
    term_write("\x1b[16t", -1);
    refresh();
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
                log_error("Failed to parse height: %s\n", strerror(errno));
                failed = true;
                break;
            }
        }
        else if (i == 2)
        {
            token.len -= 1; // strip trailing 't'
            errno = 0;
            *width = str_parse(token, 10);
            if (errno != 0)
            {
                log_error("Failed to parse width: %s\n", strerror(errno));
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
    flushinp();
    term_write("\x1b[c", -1);
    refresh();

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

static bool supports_tgp(str_t *resp, bool is_tmux)
{
    flushinp();

    if (is_tmux)
    {
        term_write("\x1bPtmux;\x1b"
                   "\x1b_Gi=1;\x1b\\"
                   "\x1b[c"
                   "\\\x1b\\\x1b[c",
                   -1);
    }
    else
    {
        term_write("\x1b_Gi=1;\x1b\\\x1b[c", -1);
    }

    refresh();
    read_response(resp);

    return contains_bytes(resp, "\x1b_G", 3);
}

static void begin_term_probe(void)
{
    initscr();
    cbreak();
    noecho();
    timeout(100);
    flushinp();
}

static void end_term_probe(void)
{
    flushinp();
    endwin();
}

term_capability term_query_capability(void)
{
    term_capability cap = {0};
    str_t resp = str_create();

    begin_term_probe();

    cap.is_tmux = getenv("TMUX") != NULL;
    cap.color = detect_color_mode();

    cap.supports_tgp = supports_tgp(&resp, cap.is_tmux);
    resp.len = 0;

    get_cell_size(&resp, &cap.cell_width, &cap.cell_height);
    resp.len = 0;

    cap.supports_sixel = supports_sixel(&resp);
    resp.len = 0;

    str_free(&resp);
    end_term_probe();

    return cap;
}
