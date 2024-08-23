#include "curses.h"
#include "logger.h"
#include "term.h"
#include "utils.h"

#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static void term_prepare()
{
    initscr();
    noecho();
    nodelay(stdscr, true);
    raw();
    cbreak();
    curs_set(false);
    keypad(stdscr, true);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);

    term_write(TESC TMOUSEENABLE, -1);
}

static void term_reset()
{
    term_write(TESC TMOUSEDISABLE, -1);
    echo();
    noraw();
    nocbreak();
    curs_set(true);
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

int term_get_events(term_event *out, int max)
{
    static int prev_mouse_pos[2] = {0, 0};
    static bool button_state[TERM_MAX_MOUSEKEY] = {0};
    static uint64_t button_last_pressed[TERM_MAX_MOUSEKEY] = {0};
    static int prev_size[2] = {0, 0};

#define INCR_LENGTH(l)                                                         \
    assert(l + 1 <= max);                                                      \
    l++;
#define SHIFT_EVENT(off)                                                       \
    &out[off];                                                                 \
    INCR_LENGTH(off);

    int ev_length = 0;
    memset(out, 0, sizeof(out[0]) * max);
    term_event *ev = NULL;

    int c = getch();
    if (c > 0)
    {
        ev = SHIFT_EVENT(ev_length);

        switch (c)
        {
        case KEY_MOUSE:
            ev->type = TERM_EVENT_MOUSE;

            MEVENT m = {0};
            getmouse(&m);
            ev->as.mouse.x = m.x;
            ev->as.mouse.y = m.y;

            const uint64_t current_time = get_time_ms();

#define CHECK_STATE(m, s) ((m.bstate & s) != 0)

#define CHECK_BUTTON(n)                                                        \
    if (CHECK_STATE(m, BUTTON##n##_PRESSED))                                   \
    {                                                                          \
        button_state[n - 1] = true;                                            \
        if (current_time - button_last_pressed[n - 1] < 200)                   \
            ev->as.mouse.double_clicked = true;                                \
        button_last_pressed[n - 1] = current_time;                             \
    }                                                                          \
    else if (CHECK_STATE(m, BUTTON##n##_RELEASED))                             \
        button_state[n - 1] = false;                                           \
    ev->as.mouse.state[n - 1] = button_state[n - 1]

            CHECK_BUTTON(1);
            CHECK_BUTTON(2);
            CHECK_BUTTON(3);

            if (CHECK_STATE(m, BUTTON4_PRESSED))
                ev->as.mouse.state[3] = true;
            if (CHECK_STATE(m, BUTTON5_PRESSED))
                ev->as.mouse.state[3] = true;

            if (CHECK_STATE(m, BUTTON_SHIFT))
                ev->as.mouse.mod |= TERM_KMOD_SHIFT;
            if (CHECK_STATE(m, BUTTON_CTRL))
                ev->as.mouse.mod |= TERM_KMOD_CTRL;
            if (CHECK_STATE(m, BUTTON_ALT))
                ev->as.mouse.mod |= TERM_KMOD_ALT;

            if (prev_mouse_pos[0] != m.x || prev_mouse_pos[1] != m.y)
                ev->as.mouse.moved = true;

            prev_mouse_pos[0] = m.x;
            prev_mouse_pos[1] = m.y;
            break;
        default:
            ev->type = TERM_EVENT_KEY;

#define CASE_KEY(k)                                                            \
    case KEY_##k:                                                              \
        ev->as.key.virtual = TERM_KEY_##k;                                     \
        break
#define CASE_KEYX(k, x)                                                        \
    case KEY_##k:                                                              \
        ev->as.key.virtual = TERM_KEY_##x;                                     \
        break

#define CASE_KEY_SHIFT(k)                                                      \
    case KEY_##k:                                                              \
        ev->as.key.virtual = TERM_KEY_##k;                                     \
        break;                                                                 \
    case KEY_S##k:                                                             \
        ev->as.key.virtual = TERM_KEY_##k;                                     \
        ev->as.key.mod |= TERM_KMOD_SHIFT;                                     \
        break
#define CASE_KEYX_SHIFT(k, x)                                                  \
    case KEY_##k:                                                              \
        ev->as.key.virtual = TERM_KEY_##x;                                     \
        break;                                                                 \
    case KEY_S##k:                                                             \
        ev->as.key.virtual = TERM_KEY_##x;                                     \
        ev->as.key.mod |= TERM_KMOD_SHIFT;                                     \
        break

            switch (c)
            {
            case '\n':
                ev->as.key.virtual = TERM_KEY_ENTER;
                ev->as.key.ascii = '\n';
                break;
            case '\t':
                ev->as.key.virtual = TERM_KEY_TAB;
                ev->as.key.ascii = '\t';
                break;
            case TERM_KEY_ESC:
                ev->as.key.virtual = TERM_KEY_ESC;
                ev->as.key.ascii = '';
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
                ev->as.key.ascii = c + 'a' - 1;
                ev->as.key.mod |= TERM_KMOD_CTRL;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                ev->as.key.ascii = c;
                ev->as.key.mod |= TERM_KMOD_SHIFT;
            }
            else
            {
                ev->as.key.ascii = c;
            }
        }
    }

    vec2 size = term_size();

    if (prev_size[0] != size.x || prev_size[1] != size.y)
    {
        ev = SHIFT_EVENT(ev_length);
        ev->type = TERM_EVENT_RESIZE;
        ev->as.resize.width = size.x;
        ev->as.resize.height = size.y;
    }

    prev_size[0] = size.x;
    prev_size[1] = size.y;

    refresh();
    return ev_length;
}
